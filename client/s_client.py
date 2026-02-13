import socket
import struct
import threading
import sys
import time

# --- CONFIGURATION ---
HOST = 'localhost'
PORT = 8080
USER = "Trader1"
PASS = "password"

# --- COLORS ---
class Colors:
    HEADER = '\033[95m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    RESET = '\033[0m'
    RED = '\033[91m'
    YELLOW = '\033[93m'

# --- PROTOCOL ---
HEADER_FMT = '<HBH'
HEADER_SIZE = 5

class TradingShell:
    def __init__(self):
        self.sock = None
        self.running = True
        self.seq_num = 0
        self.base_prompt = f"{Colors.BOLD}Command (O/M/S/X/C): {Colors.ENDC}"
        self.current_prompt = self.base_prompt 
        self.lock = threading.Lock() 

    # --- UI HELPERS ---
    def clean_print(self, text, color=Colors.ENDC):
        """
        Wipes current line, prints async message, restores ACTIVE prompt.
        """
        with self.lock:
            # 1. Clear current line
            sys.stdout.write('\r' + ' ' * 80 + '\r')
            
            # 2. Print the async message
            if text:
                print(f"{color}{text}{Colors.ENDC}")
            
            # 3. Restore whatever prompt the user was staring at
            sys.stdout.write(self.current_prompt)
            sys.stdout.flush()

    def smart_input(self, prompt_text):
        """
        Sets current_prompt for clean_print to use, then waits for input.
        """
        with self.lock:
            self.current_prompt = prompt_text
            sys.stdout.write('\r' + ' ' * 80 + '\r') # Clear debris
            sys.stdout.write(prompt_text)
            sys.stdout.flush()
        
        # Read standard input (Blocking)
        return sys.stdin.readline().strip()

    def _get_seq(self):
        self.seq_num += 1
        return self.seq_num

    # --- NETWORKING ---
    def connect(self):
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((HOST, PORT))
            self.clean_print(f"[System] Connected to {HOST}:{PORT}", Colors.BLUE)
            
            t = threading.Thread(target=self.listen_loop, daemon=True)
            t.start()
            
            self.send_login()
            return True
        except Exception as e:
            print(f"{Colors.FAIL}[Error] Connect failed: {e}{Colors.ENDC}")
            return False

    def send_login(self):
        payload = struct.pack('<20s20s', USER.encode().ljust(20), PASS.encode().ljust(20))
        self.send_packet('L', payload)

    def send_packet(self, msg_type, payload):
        try:
            header = struct.pack(HEADER_FMT, self._get_seq(), ord(msg_type), HEADER_SIZE + len(payload))
            self.sock.sendall(header + payload)
        except Exception as e:
            print(f"{Colors.FAIL}[Error] Failed to send packet: {e}{Colors.ENDC}")
            self.running = False

    # --- LISTENER ---
    def listen_loop(self):
        while self.running:
            try:
                header_data = self.sock.recv(HEADER_SIZE)
                if not header_data: break
                
                seq, mtype_int, mlen = struct.unpack(HEADER_FMT, header_data)
                body_len = mlen - HEADER_SIZE
                payload = b''
                while len(payload) < body_len:
                    chunk = self.sock.recv(body_len - len(payload))
                    if not chunk: break
                    payload += chunk
                
                self.parse_message(chr(mtype_int), payload)
            except:
                break
        self.clean_print("[System] Disconnected.", Colors.FAIL)
        self.running = False

    def parse_message(self, mtype, data):
        if mtype == 'R': # Login
            st, msg = struct.unpack('<B50s', data)
            msg = msg.decode().strip('\x00')
            self.clean_print(f"[Login] {msg}", Colors.GREEN if st==1 else Colors.FAIL)

        elif mtype == 'T': # Ticker
            sym, px, qty,ts,side = struct.unpack('<10sdQQB', data)
            sym = sym.decode().strip('\x00')
            self.clean_print(f"[TICKER] {sym} {qty} @ ${px:.2f}", Colors.CYAN)

        elif mtype == 'E': # Execution
            cid, eid, sym, side, px, qty, filled, stat = struct.unpack('<QQ10sBdQQB', data)
            sym = sym.decode().strip('\x00')
            side_str = "BUY" if side == 0 else "SELL"
            status_map = {0:"New", 1:"Partial", 2:"FILLED", 3:"Cancelled"}
            st_str = status_map.get(stat, "Unknown")
            
            # Format: [EXEC] BUY AAPL | Filled: 100/100 @ 150.00 | Status: FILLED
            msg = f">>> EXEC: {side_str} {sym} | {filled}/{qty} @ {px:.2f} | {st_str}"
            self.clean_print(msg, Colors.GREEN if stat==2 else Colors.WARNING)

        elif mtype == 'S': # Snapshot
            sym, n_bids, n_asks = struct.unpack('<10sII', data[:18])
            sym = sym.decode().strip('\x00')
            lines = [f"\n--- BOOK: {sym} ---"]
            
            offset = 18
            for i in range(5): # Bids
                p, q = struct.unpack('<dQ', data[offset:offset+16])
                if i < n_bids: lines.append(f"{Colors.GREEN}BID: {q} @ {p:.2f}{Colors.ENDC}")
                offset += 16
                
            offset = 18 + (5*16)
            for i in range(5): # Asks
                p, q = struct.unpack('<dQ', data[offset:offset+16])
                if i < n_asks: lines.append(f"{Colors.FAIL}ASK: {q} @ {p:.2f}{Colors.ENDC}")
                offset += 16
            
            self.clean_print("\n".join(lines))

    # --- MAIN LOOP ---
    def run(self):
        if not self.connect(): return
        time.sleep(0.5)

        while self.running:
            try:
                # Use smart_input instead of standard input/print
                cmd = self.smart_input(self.base_prompt).upper()
                
                if not cmd: continue

                if cmd == 'X':
                    self.running = False
                    self.sock.close()
                    break

                elif cmd == 'O':
                    try:
                        # Sub-menu prompts are now protected from interruptions
                        sym = self.smart_input(f"{Colors.HEADER}  Symbol: {Colors.ENDC}")
                        side = 0 if self.smart_input(f"{Colors.HEADER}  Side (B/S): {Colors.ENDC}").upper() == 'B' else 1
                        type_ = 0 if self.smart_input(f"{Colors.HEADER}  Type (M/L): {Colors.ENDC}").upper() == 'M' else 1
                        px = float(self.smart_input(f"{Colors.HEADER}  Price: {Colors.ENDC}"))
                        qty = int(self.smart_input(f"{Colors.HEADER}  Qty: {Colors.ENDC}"))
                        
                        payload = struct.pack('<Q10sBBdQ', self._get_seq(), sym.encode().ljust(10), side, type_, px, qty)
                        self.send_packet('N', payload)
                        self.clean_print("Order Sent.", Colors.GREEN)
                    except ValueError:
                        self.clean_print("Invalid Input.", Colors.FAIL)

                elif cmd == 'S':
                    sym = self.smart_input("Symbol to Subscribe: ")
                    payload = struct.pack('<10sB', sym.encode().ljust(10), 1) # 1 for subscribe
                    self.send_packet('Q', payload)
                    self.clean_print(f"Subscribed to {sym}", Colors.GREEN)
                
                elif cmd == 'M':
                    sym = self.smart_input("Symbol to View: ")
                    payload = struct.pack('<10s', sym.encode().ljust(10))
                    self.send_packet('M', payload)
                elif cmd == 'C':
                    oid = int(self.smart_input("Order ID to Cancel: "))
                    sym = self.smart_input("Symbol to Cancel: ")
                    side = 0 if self.smart_input("Side (B/S): ").upper() == 'B' else 1
                    payload = struct.pack('<Q10sB', oid, sym.encode().ljust(10), side)
                    self.send_packet('C', payload)
                    self.clean_print(f"Cancel Sent for Order ID {oid} on Symbol {sym}", Colors.WARNING)

            except KeyboardInterrupt:
                break
        
        print("Goodbye.")

if __name__ == "__main__":
    TradingShell().run()