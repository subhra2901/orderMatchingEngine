import time
import random
import struct
import threading
import sys
try:    
    from s_client import TradingShell, Colors, HEADER_FMT, HEADER_SIZE
except ImportError:
    print("s_client module not found. Please ensure s_client.py is in the same directory.")

class MarketMaker(TradingShell):
    def __init__(self, symbol = 'AAPL', startPrice = 100, spread = 0.5, interval = 10.0):
        super().__init__()
        self.symbol = symbol
        self.price = startPrice
        self.quantity = 100
        self.spread = spread
        self.interval = interval
        self.running = True
        # List to keep track of active orders
        self.active_orders = []
        #Stats
        self.pnl = 0.0
        self.position = 0
    def start(self):
        if not self.connect():
            print(f"{Colors.RED}Not connected to server. Please connect first.{Colors.RESET}")
            return
        time.sleep(1)  # Give some time for connection to establish
        print(f"{Colors.GREEN}[Bot] Starting market maker for {self.symbol}...{Colors.RESET}")
        print(f"{Colors.GREEN}[Bot] Initial Price: {self.price}, Spread: {self.spread}, Interval: {self.interval} seconds{Colors.RESET}")
        
        try:
            while self.running:
                #Cancel existing orders
                if self.active_orders:
                    for order_id,side in self.active_orders:
                        self.sendCancel(order_id, self.symbol, 'BUY' if side == 0 else 'SELL')
                    self.active_orders.clear()
                time.sleep(0.5)  # Short delay to ensure cancellations are processed
                #Update price randomly within a range
                self.price += random.choices([-0.25,0.0,0.25], weights=[1, 8, 1])[0]
                self.price = max(50,min(150,self.price))  # Keep price within reasonable bounds
                
                #Place new buy and sell orders
                buy_price = round(self.price - self.spread/2, 2)
                ask_price = round(self.price + self.spread/2, 2)
                
                bid_id =self._get_seq()
                self.sendOrderLimit(bid_id, self.symbol, 'BUY', buy_price, self.quantity)   
                self.active_orders.append((bid_id, 0))
                print(f"{Colors.BLUE}[Bot] Placed BUY order: {self.quantity} @ {buy_price} (ID: {bid_id}){Colors.RESET}")
                
                ask_id =self._get_seq()
                self.sendOrderLimit(ask_id, self.symbol, 'SELL', ask_price, self.quantity)   
                self.active_orders.append((ask_id, 1))
                print(f"{Colors.BLUE}[Bot] Placed SELL order: {self.quantity} @ {ask_price} (ID: {ask_id}){Colors.RESET}")
                
                time.sleep(self.interval)
        except KeyboardInterrupt:
            print(f"{Colors.YELLOW}\n[Bot] Stopping market maker...{Colors.RESET}")
            self.running = False
            # Cancel any remaining active orders
            self.cancel_all_orders()
            self.sock.close()
            print(f"{Colors.GREEN}[Bot] Market maker stopped.{Colors.RESET}")
    def sendOrderLimit(self, order_id, symbol, side, price, quantity):
        #side: 0 for buy, 1 for sell
        side_byte = 0 if side == 'BUY' else 1
        payload = struct.pack('<Q10sBBdQ', order_id, symbol.encode().ljust(10), side_byte, 1, price, quantity)
        self.send_packet('N', payload)
    def sendCancel(self, order_id, symbol, side):
        side_byte = 0 if side == 'BUY' else 1
        payload = struct.pack('<Q10sB', order_id, symbol.encode().ljust(10), side_byte)
        self.send_packet('C', payload)
    def cancel_all_orders(self):
        for order_id,side in self.active_orders:
            self.sendCancel(order_id, self.symbol, 'BUY' if side == 0 else 'SELL')
        self.active_orders.clear()
if __name__ == "__main__":
    bot = MarketMaker(symbol='AAPL', startPrice=100, spread=0.5, interval=1.0)
    bot.start()
        

                
                