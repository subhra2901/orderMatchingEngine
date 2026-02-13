import socket
import struct
import time
import threading
import random
import sys

# --- CONFIGURATION ---
HOST = '127.0.0.1'
PORT = 8080
CLIENTS_COUNT = 5       # Number of concurrent threads
ORDERS_PER_CLIENT = 2000 # Orders per thread
SYMBOL = "AAPL"

# Protocol Format (Little Endian)
# Header: Seq(H), Type(B), Len(H) -> 5 bytes
HEADER_FMT = '<HBH'
HEADER_SIZE = 5

# Body: ID(Q), Sym(10s), Side(B), Type(B), Price(d), Qty(Q) -> 36 bytes
# (Adjust if you added TimeInForce byte!)
BODY_FMT = '<Q10sBBdQ' 

class StressClient(threading.Thread):
    def __init__(self, client_id):
        super().__init__()
        self.client_id = client_id
        self.sock = None
        self.orders_sent = 0
        self.running = True

    def connect(self):
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((HOST, PORT))
            
            # Login immediately
            user = f"Stress{self.client_id}"
            pwd = "password"
            # Type 'L' = Login
            payload = struct.pack('<20s20s', user.encode().ljust(20), pwd.encode().ljust(20))
            self.send_packet('L', payload)
            
            # Give server a tiny bit to process login
            time.sleep(0.1)
            return True
        except Exception as e:
            print(f"[Client {self.client_id}] Connection Failed: {e}")
            return False

    def send_packet(self, msg_type, payload):
        # SeqNum is 0 for stress test (we don't care about ack tracking here)
        msg_len = HEADER_SIZE + len(payload)
        header = struct.pack(HEADER_FMT, 0, ord(msg_type), msg_len)
        self.sock.sendall(header + payload)

    def run(self):
        if not self.connect(): return

        print(f"[Client {self.client_id}] Starting blast of {ORDERS_PER_CLIENT} orders...")
        
        start_time = time.time()
        
        for i in range(ORDERS_PER_CLIENT):
            # Alternating Buy/Sell to trigger matches (Testing Engine Logic)
            # or just Buys to fill book (Testing Memory)
            side = i % 2 # 0=Buy, 1=Sell
            price = 100.0 + (i % 10) # Price between 100.0 and 110.0
            qty = 10
            
            # Pack 'New Order'
            # ID is purely client-side logic here. 
            # We use a huge number to avoid collisions: (ClientID * 1M) + i
            oid = (self.client_id * 1_000_000) + i
            
            try:
                payload = struct.pack(BODY_FMT, 
                    oid, 
                    SYMBOL.encode().ljust(10), 
                    side, 
                    1, # Limit
                    float(price), 
                    int(qty)
                )
                self.send_packet('N', payload)
                self.orders_sent += 1
                
                # OPTIONAL: Flood Control (Uncomment if server chokes)
                # if i % 100 == 0: time.sleep(0.001)

            except Exception as e:
                print(f"[Client {self.client_id}] Send Error: {e}")
                break
        
        duration = time.time() - start_time
        ops = self.orders_sent / duration
        print(f"[Client {self.client_id}] Finished. {self.orders_sent} orders in {duration:.2f}s ({ops:.0f} orders/sec)")
        
        self.sock.close()

def run_stress_test():
    print(f"=== LAUNCHING STRESS TEST ===")
    print(f"Target: {HOST}:{PORT}")
    print(f"Threads: {CLIENTS_COUNT} | Orders/Thread: {ORDERS_PER_CLIENT}")
    print(f"Total Orders: {CLIENTS_COUNT * ORDERS_PER_CLIENT}")
    print("--------------------------------")

    threads = []
    global_start = time.time()

    # 1. Launch Threads
    for i in range(CLIENTS_COUNT):
        t = StressClient(i + 1)
        threads.append(t)
        t.start()

    # 2. Wait for all to finish
    for t in threads:
        t.join()

    total_time = time.time() - global_start
    total_orders = CLIENTS_COUNT * ORDERS_PER_CLIENT
    global_ops = total_orders / total_time

    print("--------------------------------")
    print(f"=== TEST COMPLETE ===")
    print(f"Total Time: {total_time:.4f} seconds")
    print(f"Throughput: {global_ops:.0f} ORDERS PER SECOND")
    print("--------------------------------")

if __name__ == "__main__":
    run_stress_test()