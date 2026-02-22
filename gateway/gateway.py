import queue

import grpc
from concurrent import futures
import socket
import struct
import threading

import trading_pb2
import trading_pb2_grpc

# -----Engine Config and Protocol---- #

OME_HOST = '127.0.0.1'
OME_PORT = 8080

LOGIN_FMT = '<20s20s'
HEADER_FMT = '<HBH'  # seq_num(2), type(1), msg(2)
NEW_ORDER_FMT = '<Q10sBBdQ'  # id(8), sym(10), side(1), type(1), px(8), qty(8)
SUB_REQ_FMT = '<10sB    '
MSG_NEW_ORDER = b'N'[0]
MSG_SUBSCRIBE = b'Q'[0]
MSG_SNAPSHOT = b'S'[0]
class GatewayService(trading_pb2_grpc.TradingGatewayServicer):
    def __init__(self):
        self.ome_sock = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        self.md_streams = []
        self.order_id_counter = 0
        self.connect_to_ome()
        threading.Thread(target=self.tcp_listener_loop, daemon=True).start()
    def send_to_ome(self,msg_type,pay_load):
        msg_len = 5 + len(pay_load)
        header = struct.pack(HEADER_FMT,0,msg_type,msg_len)
        self.ome_sock.sendall(header + pay_load)
    def connect_to_ome(self):
        try:
            self.ome_sock.connect((OME_HOST,OME_PORT))

            login_payload = struct.pack(LOGIN_FMT,b'Gateway'.ljust(20,b'\0'),b'Pass'.ljust(20,b'\0'))
            self.send_to_ome(b'L'[0],login_payload)
            print("[Gateway] connected to server.")
        except Exception as e:
            print(f"[Gateway] Connection failed: {e}, make sure server is running")
    def get_order_id(self):
        self.order_id_counter += 1
        return self.order_id_counter
    def tcp_listener_loop(self):
        buffer =b''
        while True:
            try:
                data = self.ome_sock.recv(4096)
                print(f"[gateway] DEBUG: Received {len(data)} bytes from C++. Raw: {data[:20]}")
                if not data: break
                buffer += data
                while len(buffer) >= 5:
                    seq_num,msg_type, msg_len = struct.unpack(HEADER_FMT,buffer[:5])
                    if len(buffer) < msg_len:
                        break

                    payload = buffer[5:msg_len]
                    if msg_type == MSG_SNAPSHOT:
                        self.handle_snapshot(payload)
                    buffer = buffer[msg_len:]
            except Exception as e:
                print(f"[Gateway] TCP Listener Error:  {e}")
                break

    def handle_snapshot(self,payload):
        sym_raw,n_bids,n_asks = struct.unpack('<10sII',payload[:18])
        symbol = sym_raw.split(b'\x00')[0].decode('ascii', 'ignore')
        snapshot = trading_pb2.MarketDataSnapshot(symbol = symbol)
        print("DEBUG: received snapshot")
        offset = 18
        for i in range(5):  # Bids
            p, q = struct.unpack('<dQ', payload[offset:offset + 16])
            if q > 0 : snapshot.bids.append(trading_pb2.PriceLevel(price = p, quantity = q))
            offset += 16

        offset = 18 + (5 * 16)
        for i in range(5):  # Asks
            p, q = struct.unpack('<dQ', payload[offset:offset + 16])
            if q > 0 : snapshot.asks.append(trading_pb2.PriceLevel(price = p, quantity = q))
            offset += 16
        for q in self.md_streams:
            q.put(snapshot)
    def PlaceOrder(self,request,context):
        print("[Gateway] received new order, parsing...")

        side_int = 0 if request.side == trading_pb2.BUY else 1
        type_int = 0 if request.type == trading_pb2.MARKET else 1
        order_id = self.get_order_id()

        pay_load = struct.pack(NEW_ORDER_FMT,
                               order_id,
                               request.symbol.encode().ljust(20),
                               side_int,
                               type_int,
                               float(request.price),
                               int(request.quantity)
        )
        try:
            self.send_to_ome(MSG_NEW_ORDER, pay_load)
            return trading_pb2.OrderResponse(
                success = True,
                message = "Order Forwarded to Engine.",
                order_id = order_id
            )
        except Exception as e:
            return trading_pb2.OrderResponse(
                success=False,
                message=f"Failed to send Order: {e}",
                order_id=0
            )
    def StreamMarketData(self, request, context):
        print(f"[Gateway] Client subscribed to Market Data for {request.symbol}")

        sub_payload = struct.pack(SUB_REQ_FMT,request.symbol.encode().ljust(10,b'\0'),1)
        self.send_to_ome(MSG_SUBSCRIBE,sub_payload)

        client_queue = queue.Queue()
        self.md_streams.append(client_queue)
        try:
            while context.is_active():
                try:
                    snapshot = client_queue.get(timeout=1.0)
                    if snapshot.symbol == request.symbol:
                        yield snapshot
                except queue.Empty:
                        pass
        finally:
            if   client_queue in self.md_streams:
                self.md_streams.remove(client_queue)
            print(f"[Gateway] UI Client disconnected from Market Data.")

    def RequestMarketDataSnapshot(self, request, context):
        print(f"[Gateway] Forwarding manual Market Data Request for {request.symbol}...")
        temp_queue = queue.Queue()
        self.md_streams.append(temp_queue)

        try:
            payload = struct.pack('<10s', request.symbol.encode().ljust(10, b'\0'))
            self.send_to_ome(b'M'[0], payload)
            while True:
                # Wait up to 3 seconds for the C++ engine to reply
                snapshot = temp_queue.get(timeout=3.0)
                if snapshot.symbol == request.symbol:
                    return snapshot  # Return the snapshot directly to the UI
        except queue.Empty:
            # Handle the case where the C++ engine never responds
            context.set_details(f"Engine timeout requesting snapshot for {request.symbol}")
            context.set_code(grpc.StatusCode.DEADLINE_EXCEEDED)
            return trading_pb2.MarketDataSnapshot(symbol=request.symbol)

        finally:
            # 4. Cleanup: remove the temporary queue so it doesn't leak memory
            if temp_queue in self.md_streams:
                self.md_streams.remove(temp_queue)
def run():
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    trading_pb2_grpc.add_TradingGatewayServicer_to_server(GatewayService(),server)

    # listen on port 500051
    server.add_insecure_port('[::]:50051')
    server.start()

    print("[Gateway] gRPC API listening on port 50051....")
    server.wait_for_termination()

if __name__ == '__main__':
    run()








