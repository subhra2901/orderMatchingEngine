import tkinter as tk
from tkinter import ttk, simpledialog, messagebox
import grpc
import threading
import queue
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime
import trading_pb2
import trading_pb2_grpc

GATEWAY_HOST = '127.0.0.1:50051'

class AuthManager:
    def __init__(self):
        self.token = None
    def get_metadata(self):
        if self.token:
            return (('authorization', f'Bearer {self.token}'),)
        return None

# --- Phase 1: Communication Layer ---
class GrpcClient:
    """Manages gRPC channel, stubs, and background threading."""
    def __init__(self, host, ui_queue):
        self.host = host
        self.ui_queue = ui_queue
        self.channel = None
        self.stub = None
        self.connected = False
        self.executor = ThreadPoolExecutor(max_workers=5)
        self.active_users = set()
        self.auth = AuthManager()
        self.logged_in_user = None
        self.connect()

    def connect(self):
        try:
            self.channel = grpc.insecure_channel(self.host)
            self.stub = trading_pb2_grpc.TradingGatewayStub(self.channel)
            self.connected = True
            self.ui_queue.put({"type": "LOG", "msg": f"Connected to Gateway at {self.host}"})
        except Exception as e:
            self.connected = False
            self.ui_queue.put({"type": "LOG", "msg": f"Failed to connect: {e}"})

    def login(self,username: str,password: str):
        if not self.connected: return False
        try:
            req = trading_pb2.LoginRequest(username = username, password = password)
            resp = self.stub.Login(req)
            print(f"[DEBUG] Gateway Response: success={resp.success}, msg='{resp.message}'")
            if resp.success:
                self.auth.token = resp.token
                self.logged_in_user = username
                self.ui_queue.put({"type": "LOG", "msg": "Login successful!"})
                return True, "Success"
            else:
                self.ui_queue.put({"type": "LOG", "msg": f"Login failed: {resp.message}"})
                return False, resp.message
        except grpc.RpcError as e:
            self.ui_queue.put({"type": "LOG", "msg": f"Auth RPC Error: {e.details()}"})
            return False, f"gRPC Error: {e.details()}"
        except Exception as e:
            return False, f"Python Client Error: {str(e)}"
    def submit_order_async(self, req):
        if not self.connected: return
        def _call():
            try:
                resp = self.stub.PlaceOrder(req,metadata=self.auth.get_metadata())
                self.ui_queue.put({"type": "LOG", "msg": f"Order Ack: {resp.message} (ID: {resp.order_id})"})
            except grpc.RpcError as e:
                self.ui_queue.put({"type": "LOG", "msg": f"Gateway Error: {e.details()}"})
        self.executor.submit(_call)

    def cancel_order_async(self,order_id,symbol,side_str):
        if not self.connected: return
        def _call():
            try:
                side_enum = trading_pb2.BUY if side_str == 'BUY' else trading_pb2.SELL
                req = trading_pb2.CancelOrderRequest(
                    order_id=int(order_id),
                    symbol=symbol,
                    side=side_enum
                )
                resp = self.stub.CancelOrder(req,metadata=self.auth.get_metadata())
                self.ui_queue.put({"type": "LOG", "msg": f"Cancel Request Ack: {resp.message}"})
            except grpc.RpcError as e:
                self.ui_queue.put({"type": "LOG", "msg": f"Cancel Error: {e.details()}"})
        self.executor.submit(_call)
    def request_snapshot_async(self, symbol):
        if not self.connected: return
        def _call():
            req = trading_pb2.MarketDataRequest(symbol=symbol)
            try:
                snapshot = self.stub.RequestMarketDataSnapshot(req,metadata=self.auth.get_metadata())
                self.ui_queue.put({"type": "L2_UPDATE", "data": snapshot})
                self.ui_queue.put({"type": "LOG", "msg": f"Snapshot loaded for {symbol}."})
            except grpc.RpcError as e:
                self.ui_queue.put({"type": "LOG", "msg": f"Snapshot Error: {e.details()}"})
        self.executor.submit(_call)

    def stream_market_data(self, symbol):
        if not self.connected: return
        def _stream():
            req = trading_pb2.MarketDataRequest(symbol=symbol)
            try:
                for snapshot in self.stub.StreamMarketData(req,metadata=self.auth.get_metadata()):
                    self.ui_queue.put({"type": "L2_UPDATE", "data": snapshot})
            except grpc.RpcError as e:
                self.ui_queue.put({"type": "LOG", "msg": f"MD Stream closed: {e.details()}"})
        threading.Thread(target=_stream, daemon=True).start()

    def ensure_execution_stream(self, user_id):
        if not self.connected or user_id in self.active_users: return
        self.active_users.add(user_id)
        def _stream():
            req = trading_pb2.SessionRequest(user_id=0)# Not required, handled by gateway

            try:
                for report in self.stub.StreamExecutions(req,metadata=self.auth.get_metadata()):

                    self.ui_queue.put({"type": "EXEC_UPDATE", "data": report})
            except grpc.RpcError as e:
                self.ui_queue.put({"type": "LOG", "msg": f"Exec Stream closed: {e.details()}"})
                self.active_users.discard(user_id)
        threading.Thread(target=_stream, daemon=True).start()

# --- Phase 2: UI Components ---
class OrderEntryPanel(ttk.LabelFrame):
    def __init__(self, parent, on_submit):
        super().__init__(parent, text="Order Entry", padding=10)
        self.on_submit = on_submit

        # Removed var_uid completely
        self.var_sym = tk.StringVar(value="AAPL")
        self.var_px = tk.StringVar(value="150.50")
        self.var_qty = tk.StringVar(value="100")

        ttk.Label(self, text="Symbol:").grid(row=0, column=0, padx=5)
        ttk.Entry(self, textvariable=self.var_sym, width=10).grid(row=0, column=1, padx=5)
        ttk.Label(self, text="Price:").grid(row=0, column=2, padx=5)
        ttk.Entry(self, textvariable=self.var_px, width=10).grid(row=0, column=3, padx=5)
        ttk.Label(self, text="Qty:").grid(row=0, column=4, padx=5)
        ttk.Entry(self, textvariable=self.var_qty, width=10).grid(row=0, column=5, padx=5)
        ttk.Button(self, text="BUY", width=10, command=lambda: self._trigger_submit("BUY")).grid(row=0, column=6,
                                                                                                 padx=15)
        ttk.Button(self, text="SELL", width=10, command=lambda: self._trigger_submit("SELL")).grid(row=0, column=7,
                                                                                                   padx=5)

    def _trigger_submit(self, side_str):
        # No longer passing a user ID from the UI
        self.on_submit(
            self.var_sym.get().strip().upper(),
            side_str,
            float(self.var_px.get()),
            int(self.var_qty.get())
        )

class MarketDataPanel(ttk.LabelFrame):
    def __init__(self, parent, on_stream, on_snapshot):
        super().__init__(parent, text="Level 2 Order Book", padding=5)
        ctrl_frame = tk.Frame(self)
        ctrl_frame.pack(fill="x", pady=(0, 5))
        self.var_sym = tk.StringVar(value="AAPL")
        ttk.Label(ctrl_frame, text="Symbol:").pack(side="left", padx=5)
        ttk.Entry(ctrl_frame, textvariable=self.var_sym, width=10).pack(side="left", padx=5)
        ttk.Button(ctrl_frame, text="Subscribe Live", command=lambda: on_stream(self.var_sym.get().strip().upper())).pack(side="left", padx=5)
        ttk.Button(ctrl_frame, text="Pull Snapshot", command=lambda: on_snapshot(self.var_sym.get().strip().upper())).pack(side="left", padx=5)

        book_split = tk.Frame(self)
        book_split.pack(fill="both", expand=True)
        self.tree_bids = ttk.Treeview(book_split, columns=("Qty", "Price"), show="headings", height=8)
        self.tree_bids.heading("Qty", text="Bid Qty")
        self.tree_bids.heading("Price", text="Bid Price")
        self.tree_bids.pack(side="left", fill="both", expand=True)

        self.tree_asks = ttk.Treeview(book_split, columns=("Price", "Qty"), show="headings", height=8)
        self.tree_asks.heading("Price", text="Ask Price")
        self.tree_asks.heading("Qty", text="Ask Qty")
        self.tree_asks.pack(side="right", fill="both", expand=True)

    def update_ladder(self, snapshot):
        for row in self.tree_bids.get_children(): self.tree_bids.delete(row)
        for row in self.tree_asks.get_children(): self.tree_asks.delete(row)
        for bid in snapshot.bids:
            self.tree_bids.insert("", "end", values=(bid.quantity, f"{bid.price:.2f}"))
        for ask in snapshot.asks:
            self.tree_asks.insert("", "end", values=(f"{ask.price:.2f}", ask.quantity))

class BlotterPanel(ttk.LabelFrame):
    def __init__(self, parent, on_cancel):
        super().__init__(parent, text="Working Orders & Trades", padding=5)
        self.on_cancel = on_cancel
        self.tree_orders = ttk.Treeview(self, columns=("ID", "Sym", "Side", "Px", "Qty", "Status"), show="headings", height=6)
        for col in self.tree_orders["columns"]:
            self.tree_orders.heading(col, text=col)
            self.tree_orders.column(col, anchor="center", width=60)
        self.tree_orders.pack(fill="both", expand=True, pady=(0, 5))
        # Add the Cancel Button right below the orders tree
        btn_frame = tk.Frame(self)
        btn_frame.pack(fill="x", pady=(0, 5))
        ttk.Button(btn_frame, text="❌ Cancel Selected Order", command=self._trigger_cancel).pack(side="left")

        self.tree_trades = ttk.Treeview(self, columns=("ExecID", "Sym", "Side", "Px", "Qty"), show="headings", height=4)
        for col in self.tree_trades["columns"]:
            self.tree_trades.heading(col, text=col)
            self.tree_trades.column(col, anchor="center", width=60)
        self.tree_trades.pack(fill="both", expand=True)

    def _trigger_cancel(self):
        selected = self.tree_orders.selection()
        if not selected: return
        item = self.tree_orders.item(selected[0])
        vals = item['values']
        # Don't try to cancel orders that are already dsoead
        if vals[5] in ["Canceled", "Filled", "Reject"]:
            return

        self.on_cancel(vals[0], vals[1], vals[2])
    def update_execution(self, report):
        status_map = {0: "New", 1: "Partial", 2: "Filled", 3: "Canceled", 4: "Reject"}
        status_str = status_map.get(int(report.status), "Unknown")
        side_str = "BUY" if report.side == 0 else "SELL"
        qty_display = f"{report.filled_quantity} / {report.quantity}"

        # 1. Update the Trades table if a fill occurred
        if int(report.status) in [1, 2] and report.execution_id != 0:
            self.tree_trades.insert("", 0, values=(
                report.execution_id, report.symbol, side_str, f"{report.price:.2f}", report.filled_quantity
            ))

        # 2. Handle the Working Orders Blotter
        terminal_statuses = [2, 3, 4]  # 2=Filled, 3=Canceled, 4=Reject

        # Find the order in the treeview
        found_item = None
        for item in self.tree_orders.get_children():
            if str(self.tree_orders.item(item, 'values')[0]) == str(report.order_id):
                found_item = item
                break

        if found_item:
            # If the order is dead, remove it from the working orders list
            if int(report.status) in terminal_statuses:
                self.tree_orders.delete(found_item)
            else:
                # Otherwise, update its state (e.g., Partial Fill)
                self.tree_orders.item(found_item, values=(
                    report.order_id, report.symbol, side_str, f"{report.price:.2f}", qty_display, status_str
                ))
        else:
            # If we haven't seen this order yet, only insert it if it's actually alive
            if int(report.status) not in terminal_statuses:
                self.tree_orders.insert("", "end", values=(
                    report.order_id, report.symbol, side_str, f"{report.price:.2f}", qty_display, status_str
                ))

    def clear(self):
        """Utility to wipe the blotter on sign out."""
        for row in self.tree_orders.get_children(): self.tree_orders.delete(row)
        for row in self.tree_trades.get_children(): self.tree_trades.delete(row)
# --- Phase 3: Main Application Shell ---
class TradingTerminal:
    def __init__(self, root):
        self.root = root

        # 1. Hide the massive main window immediately
        self.root.withdraw()
        self.root.title("HFT Trading Terminal")
        self.root.geometry("1000x850")
        self.root.configure(padx=10, pady=10)

        self.ui_queue = queue.Queue()
        self.client = GrpcClient(GATEWAY_HOST, self.ui_queue)

        self.prompt_login()

        # 2. Success! Un-hide the main window
        self.root.deiconify()
        user_info_frame = tk.Frame(self.root)
        user_info_frame.pack(fill="x", pady=(0, 10))

        # We will use a dynamic string variable so we can update it on re-login
        self.var_user_label = tk.StringVar(value=f"👤 Trader Profile: {self.client.logged_in_user.upper()}")
        user_label = ttk.Label(
            user_info_frame,
            textvariable=self.var_user_label,
            font=("Arial", 11, "bold"),
            foreground="#2E86C1"  # A nice professional blue
        )
        user_label.pack(side="left")
        ttk.Button(user_info_frame, text="Sign Out", command=self.handle_signout).pack(side="right")
        # Build UI
        self.order_panel = OrderEntryPanel(self.root, self.handle_submit)
        self.order_panel.pack(fill="x", pady=(0, 10))

        mid_pane = tk.Frame(self.root)
        mid_pane.pack(fill="both", expand=True, pady=(0, 10))
        self.md_panel = MarketDataPanel(mid_pane, self.client.stream_market_data, self.client.request_snapshot_async)
        self.md_panel.pack(side="left", fill="both", expand=True, padx=(0, 5))
        self.blotter_panel = BlotterPanel(mid_pane,self.handle_cancel)
        self.blotter_panel.pack(side="right", fill="both", expand=True, padx=(5, 0))

        log_frame = ttk.LabelFrame(self.root, text="System Logs", padding=5)
        log_frame.pack(fill="both", expand=True)
        self.log_area = tk.Text(log_frame, height=8, bg="black", fg="lightgreen", font=("Consolas", 10))
        self.log_area.pack(fill="both", expand=True)

        self.poll_queue()

    def prompt_login(self):
        while True:
            username = simpledialog.askstring("Login","Username",parent=self.root)
            if not username: self.root.destroy(); return
            password = simpledialog.askstring("Login", "Password:", parent=self.root, show="*")
            if not password: self.root.destroy(); return
            success, error_msg = self.client.login(username, password)
            if success:
                break
            else:
                messagebox.showerror("Login Failed", f"Reason: {error_msg}")
                #messagebox.showerror("Error", "Invalid credentials")
    def handle_submit(self, sym, side_str, px, qty):
        self.client.ensure_execution_stream(self.client.logged_in_user)
        side_enum = trading_pb2.BUY if side_str == "BUY" else trading_pb2.SELL
        req = trading_pb2.NewOrderRequest(
            user_id=0, symbol=sym, side=side_enum, type=trading_pb2.LIMIT, price=px, quantity=qty
        )
        self.client.submit_order_async(req)
        self.log(f"Submitting Order: {side_str} {qty} @ {px} for {sym}")
    def handle_cancel(self, order_id, symbol, side_str):
        self.log(f"Requesting cancellation for Order {order_id} ({symbol})")
        self.client.cancel_order_async(order_id, symbol, side_str)

    def handle_signout(self):
        self.log("Signing out...")
        # Clear auth state
        self.client.auth.token = None
        self.client.logged_in_user = None
        self.client.active_users.clear()

        # Clear UI state
        self.blotter_panel.clear()
        # Optionally clear MD panel trees here too

        # Hide main window and re-prompt
        self.root.withdraw()
        self.prompt_login()

        # Update UI for new user upon return
        self.var_user_label.set(f"👤 Trader Profile: {self.client.logged_in_user.upper()}")
        self.root.deiconify()
    def poll_queue(self):
        try:
            while True:
                msg = self.ui_queue.get_nowait()
                msg_type = msg.get("type")
                if msg_type == "LOG":
                    self.log(msg["msg"])
                elif msg_type == "L2_UPDATE":
                    self.md_panel.update_ladder(msg["data"])
                elif msg_type == "EXEC_UPDATE":
                    self.blotter_panel.update_execution(msg["data"])
        except queue.Empty:
            pass
        finally:
            self.root.after(50, self.poll_queue)

    def log(self, msg):
        ts = datetime.now().strftime("%H:%M:%S")
        self.log_area.insert("end", f"[{ts}] {msg}\n")
        self.log_area.see("end")

if __name__ == "__main__":
    root = tk.Tk()
    app = TradingTerminal(root)
    root.mainloop()
