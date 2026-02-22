import tkinter as tk
from tkinter import ttk
import grpc
import threading
import queue
from datetime import datetime
import trading_pb2
import trading_pb2_grpc

GATEWAY_HOST = '127.0.0.1:50051'


class TradingTerminal:
    def __init__(self, root):
        self.root = root
        self.root.title("HFT Trading Terminal - gRPC Client")
        self.root.geometry("1000x850")  # Slightly taller to fit new controls
        self.root.configure(padx=10, pady=10)

        # --- gRPC Setup ---
        self.ui_queue = queue.Queue()
        self.stream_active = False
        try:
            self.channel = grpc.insecure_channel(GATEWAY_HOST)
            self.stub = trading_pb2_grpc.TradingGatewayStub(self.channel)
            self.connected = True
        except Exception as e:
            self.connected = False
            print(f"Failed to connect to Gateway: {e}")

        # Build the UI Components
        self.setup_order_entry()
        self.setup_data_controls()  # <--- NEW PANEL
        self.setup_middle_pane()
        self.setup_bottom_pane()

        # Start the UI polling loop
        self.poll_queue()

        if self.connected:
            self.log("Connected to API Gateway. Waiting for user actions.")

    def setup_order_entry(self):
        """ZONE 1: ORDER ENTRY"""
        frame = ttk.LabelFrame(self.root, text="Order Entry", padding=10)
        frame.pack(fill="x", pady=(0, 5))

        self.var_sym = tk.StringVar(value="AAPL")
        self.var_px = tk.StringVar(value="150.50")
        self.var_qty = tk.StringVar(value="100")

        ttk.Label(frame, text="Symbol:").grid(row=0, column=0, padx=5)
        ttk.Entry(frame, textvariable=self.var_sym, width=10).grid(row=0, column=1, padx=5)
        ttk.Label(frame, text="Price:").grid(row=0, column=2, padx=5)
        ttk.Entry(frame, textvariable=self.var_px, width=10).grid(row=0, column=3, padx=5)
        ttk.Label(frame, text="Qty:").grid(row=0, column=4, padx=5)
        ttk.Entry(frame, textvariable=self.var_qty, width=10).grid(row=0, column=5, padx=5)
        ttk.Button(frame, text="BUY", width=10, command=lambda: self.place_order("BUY")).grid(row=0, column=6, padx=15)
        ttk.Button(frame, text="SELL", width=10, command=lambda: self.place_order("SELL")).grid(row=0, column=7, padx=5)

    def setup_data_controls(self):
        """ZONE 1.5: DATA CONTROLS (NEW)"""
        frame = ttk.LabelFrame(self.root, text="Market Data Controls", padding=10)
        frame.pack(fill="x", pady=(0, 10))

        self.var_data_sym = tk.StringVar(value="AAPL")

        ttk.Label(frame, text="Symbol:").grid(row=0, column=0, padx=5)
        ttk.Entry(frame, textvariable=self.var_data_sym, width=10).grid(row=0, column=1, padx=5)

        ttk.Button(frame, text="1. Subscribe to Live Stream", command=self.start_subscription).grid(row=0, column=2,
                                                                                                    padx=15)
        ttk.Button(frame, text="2. Request Snapshot (Pull L2)", command=self.request_snapshot).grid(row=0, column=3,
                                                                                                    padx=5)

    # ... (Keep your setup_middle_pane and setup_bottom_pane exactly the same) ...
    def setup_middle_pane(self):
        pane = tk.Frame(self.root)
        pane.pack(fill="both", expand=True, pady=(0, 10))

        # --- LEFT: MARKET DATA (THE LADDER) ---
        md_frame = ttk.LabelFrame(pane, text="Level 2 Order Book (Public Data)", padding=5)
        md_frame.pack(side="left", fill="both", expand=True, padx=(0, 5))

        book_split = tk.Frame(md_frame)
        book_split.pack(fill="both", expand=True)

        self.tree_bids = ttk.Treeview(book_split, columns=("Qty", "Price"), show="headings", height=8)
        self.tree_bids.heading("Qty", text="Bid Qty")
        self.tree_bids.heading("Price", text="Bid Price")
        self.tree_bids.column("Qty", anchor="center", width=80)
        self.tree_bids.column("Price", anchor="center", width=80)
        self.tree_bids.pack(side="left", fill="both", expand=True)

        self.tree_asks = ttk.Treeview(book_split, columns=("Price", "Qty"), show="headings", height=8)
        self.tree_asks.heading("Price", text="Ask Price")
        self.tree_asks.heading("Qty", text="Ask Qty")
        self.tree_asks.column("Price", anchor="center", width=80)
        self.tree_asks.column("Qty", anchor="center", width=80)
        self.tree_asks.pack(side="right", fill="both", expand=True)

        # --- RIGHT: WORKING ORDERS ---
        wo_frame = ttk.LabelFrame(pane, text="My Working Orders (Private Data)", padding=5)
        wo_frame.pack(side="right", fill="both", expand=True, padx=(5, 0))

        self.tree_orders = ttk.Treeview(wo_frame, columns=("ID", "Sym", "Side", "Px", "Qty", "Status"), show="headings",
                                        height=8)
        for col in self.tree_orders["columns"]:
            self.tree_orders.heading(col, text=col)
            self.tree_orders.column(col, anchor="center", width=60)
        self.tree_orders.pack(fill="both", expand=True)

    def setup_bottom_pane(self):
        pane = tk.Frame(self.root)
        pane.pack(fill="both", expand=True)

        trades_frame = ttk.LabelFrame(pane, text="Session Trades / Fills", padding=5)
        trades_frame.pack(side="left", fill="both", expand=True, padx=(0, 5))

        self.tree_trades = ttk.Treeview(trades_frame, columns=("ExecID", "Sym", "Side", "Px", "Qty"), show="headings",
                                        height=6)
        for col in self.tree_trades["columns"]:
            self.tree_trades.heading(col, text=col)
            self.tree_trades.column(col, anchor="center", width=60)
        self.tree_trades.pack(fill="both", expand=True)

        log_frame = ttk.LabelFrame(pane, text="System Logs", padding=5)
        log_frame.pack(side="right", fill="both", expand=True, padx=(5, 0))

        self.log_area = tk.Text(log_frame, height=6, bg="black", fg="lightgreen", font=("Consolas", 10))
        self.log_area.pack(fill="both", expand=True)

    # --- NEW BUTTON ACTIONS ---

    def start_subscription(self):
        if not self.connected or self.stream_active: return
        symbol = self.var_data_sym.get().strip().upper()
        self.log(f"Opening live stream for {symbol}...")
        self.stream_active = True
        threading.Thread(target=self.listen_to_market_data, args=(symbol,), daemon=True).start()


    def listen_to_market_data(self, symbol):
        req = trading_pb2.MarketDataRequest(symbol=symbol)
        try:
            for snapshot in self.stub.StreamMarketData(req):
                self.ui_queue.put({"type": "L2_UPDATE", "data": snapshot})
        except grpc.RpcError as e:
            self.ui_queue.put({"type": "LOG", "msg": f"Stream closed: {e.details()}"})
            self.stream_active = False

    def place_order(self, side_str):
        if not self.connected: return
        symbol = self.var_sym.get().strip().upper()
        price = float(self.var_px.get())
        qty = int(self.var_qty.get())
        side_enum = trading_pb2.BUY if side_str == "BUY" else trading_pb2.SELL

        req = trading_pb2.NewOrderRequest(
            symbol=symbol, side=side_enum, type=trading_pb2.LIMIT, price=price, quantity=qty
        )
        try:
            resp = self.stub.PlaceOrder(req)
            self.log(f"Order Sent: {side_str} {qty} @ {price}. Gateway Ack: {resp.message}")
        except grpc.RpcError as e:
            self.log(f"Gateway Error: {e.details()}")

    def request_snapshot(self):
        if not self.connected: return
        symbol = self.var_data_sym.get().strip().upper()
        self.log(f"Pinging C++ Engine for current L2 Snapshot ({symbol})...")
        try:
            req = trading_pb2.MarketDataSnapshot(symbol=symbol)
            snapshot = self.stub.RequestMarketDataSnapshot(req)
            self.ui_queue.put({"type": "L2_UPDATE", "data": snapshot})
            self.log(f"Manual snapshot received and loaded for {symbol}.")
        except grpc.RpcError as e:
            self.log(f"Gateway Error: {e.details()}")
    def poll_queue(self):
        try:
            while True:
                msg = self.ui_queue.get_nowait()
                print("[DEBUG] Received msg in frontend " + msg["type"])
                if msg["type"] == "LOG":
                    self.log(msg["msg"])
                elif msg["type"] == 'L2_UPDATE':
                    snapshot = msg["data"]
                    print(f"[DEBUG] Received object type: {type(snapshot)}")
                    print(f"[DEBUG] Object contents: {snapshot}")
                    # Clear ladder
                    for row in self.tree_bids.get_children(): self.tree_bids.delete(row)
                    for row in self.tree_asks.get_children(): self.tree_asks.delete(row)
                    # Redraw
                    for bid in snapshot.bids:
                        self.tree_bids.insert("", "end", values=(bid.quantity, f"{bid.price:.2f}"))
                    for ask in snapshot.asks:
                        self.tree_asks.insert("", "end", values=(f"{ask.price:.2f}", ask.quantity))
        except queue.Empty:
            pass
        finally:
            self.root.after(100, self.poll_queue)

    def log(self, msg):
        ts = datetime.now().strftime("%H:%M:%S")
        formatted_msg = f"[{ts}] {msg}\n"
        self.log_area.insert("end", formatted_msg)
        self.log_area.see("end")
        print(formatted_msg.strip())


if __name__ == "__main__":
    root = tk.Tk()
    app = TradingTerminal(root)
    root.mainloop()
