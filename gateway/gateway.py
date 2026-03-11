import queue
import grpc
from concurrent import futures
import socket
import struct
import threading
import jwt
from datetime import datetime, timedelta, timezone
from dotenv import load_dotenv
import os

import trading_pb2
import trading_pb2_grpc

load_dotenv()

OME_HOST = os.getenv("OME_HOST")
OME_PORT = int(os.getenv("OME_PORT"))
SECRET_KEY = os.getenv("SECRET_KEY")

LOGIN_FMT = '<20s20s'
HEADER_FMT = '<HBH'
NEW_ORDER_FMT = '<QQ10sBBdQ'
SUB_REQ_FMT = '<10sB'
EXECUTION_FMT = '<QQQ10sBdQQB'
CANCEL_REQ_FMT = '<QQ10sB'

MSG_NEW_ORDER = b'N'[0]
MSG_SUBSCRIBE = b'Q'[0]
MSG_SNAPSHOT = b'S'[0]
MSG_EXEC_REPORT = b'E'[0]
MSG_CANCEL_ORDER = b'C'[0]

class GatewayService(trading_pb2_grpc.TradingGatewayServicer):
    def __init__(self):
        self.ome_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.md_streams = []
        self.order_id_counter = 0
        self.user_queues = {}
        self.connect_to_ome()
        threading.Thread(target=self.tcp_listener_loop, daemon=True).start()

    def get_user_from_context(self, context):
        """Extracts and validates the JWT from gRPC metadata."""
        metadata = dict(context.invocation_metadata())
        token = metadata.get('authorization')
        if not token or not token.startswith('Bearer'):
            context.abort(grpc.StatusCode.UNAUTHENTICATED, "Missing or invalid token")
        try:
            token = token.split(' ')[1]
            payload = jwt.decode(token, SECRET_KEY, algorithms="HS256")
            return int(payload["user_id"])
        except jwt.ExpiredSignatureError:
            context.abort(grpc.StatusCode.UNAUTHENTICATED, "Token expired")
        except jwt.InvalidTokenError:
            context.abort(grpc.StatusCode.UNAUTHENTICATED, "Invalid token")

    def send_to_ome(self, msg_type, pay_load):
        msg_len = 5 + len(pay_load)
        header = struct.pack(HEADER_FMT, 0, msg_type, msg_len)
        self.ome_sock.sendall(header + pay_load)

    def connect_to_ome(self):
        try:
            self.ome_sock.connect((OME_HOST, OME_PORT))
            login_payload = struct.pack(LOGIN_FMT, b'Gateway'.ljust(20, b'\0'), b'Pass'.ljust(20, b'\0'))
            self.send_to_ome(b'L'[0], login_payload)
            print("[Gateway] Connected to C++ Matching Engine.")
        except Exception as e:
            print(f"[Gateway] Connection failed: {e}. Ensure the C++ engine is running on {OME_HOST}:{OME_PORT}")

    def Login(self, request, context):
        print(f"[Gateway] Login attempt for user {request.username}")

        # Minimalist db for now
        mock_db = {"Trader1": ("pass1", 1), "Trader2": ("pass2", 2), "Trader3": ("pass3", 3)}
        if request.username in mock_db and mock_db[request.username][0] == request.password:
            user_id = mock_db[request.username][1]
            # JWT valid for 15 minutes only
            payload = {
                "user_id": user_id,
                "exp": datetime.now(timezone.utc) + timedelta(minutes=15)
            }
            token = jwt.encode(payload, SECRET_KEY, algorithm="HS256")
            return trading_pb2.LoginResponse(success=True, token=token, message="Login successful")
        return trading_pb2.LoginResponse(success=False, token="", message="Invalid credentials")

    def get_order_id(self):
        self.order_id_counter += 1
        return self.order_id_counter

    def tcp_listener_loop(self):
        buffer = b''
        while True:
            try:
                data = self.ome_sock.recv(4096)
                if not data: break
                buffer += data
                while len(buffer) >= 5:
                    seq_num, msg_type, msg_len = struct.unpack(HEADER_FMT, buffer[:5])
                    if len(buffer) < msg_len:
                        break

                    payload = buffer[5:msg_len]
                    if msg_type == MSG_SNAPSHOT:
                        self.handle_snapshot(payload)
                    elif msg_type == MSG_EXEC_REPORT:
                        self.handle_execution(payload)
                    buffer = buffer[msg_len:]
            except Exception as e:
                print(f"[Gateway] TCP Listener Error: {e}")
                break

    def handle_snapshot(self, payload):
        sym_raw, n_bids, n_asks = struct.unpack('<10sII', payload[:18])
        symbol = sym_raw.split(b'\x00')[0].decode('ascii', 'ignore')
        snapshot = trading_pb2.MarketDataSnapshot(symbol=symbol)

        offset = 18
        for i in range(5):
            p, q = struct.unpack('<dQ', payload[offset:offset + 16])
            if q > 0: snapshot.bids.append(trading_pb2.PriceLevel(price=p, quantity=q))
            offset += 16

        offset = 18 + (5 * 16)
        for i in range(5):
            p, q = struct.unpack('<dQ', payload[offset:offset + 16])
            if q > 0: snapshot.asks.append(trading_pb2.PriceLevel(price=p, quantity=q))
            offset += 16

        for q in self.md_streams:
            q.put(snapshot)

    def handle_execution(self, payload):
        if len(payload) < 60: return
        (client_order_id, user_id, exec_id, sym_raw, side, price,
         qty, filled_qty, status) = struct.unpack(EXECUTION_FMT, payload[:60])
        symbol = sym_raw.split(b'\x00')[0].decode('ascii', 'ignore')
        report = trading_pb2.ExecutionReport(
            order_id=client_order_id,
            execution_id=exec_id,
            user_id=user_id,
            symbol=symbol,
            side=side,
            price=price,
            quantity=qty,
            filled_quantity=filled_qty,
            status=str(status)
        )

        if user_id not in self.user_queues:
            self.user_queues[user_id] = queue.Queue()
        self.user_queues[user_id].put(report)

    def StreamExecutions(self, request, context):
        user_id = self.get_user_from_context(context)
        if user_id not in self.user_queues:
            self.user_queues[user_id] = queue.Queue()

        print(f"[Gateway] User {user_id} connected to execution stream.")
        try:
            while context.is_active():
                try:
                    report = self.user_queues[user_id].get(timeout=1.0)
                    yield report
                except queue.Empty:
                    pass
        finally:
            print(f"[Gateway] User {user_id} execution stream disconnected.")

    def PlaceOrder(self, request, context):
        side_int = 0 if request.side == trading_pb2.BUY else 1
        type_int = 0 if request.type == trading_pb2.MARKET else 1
        order_id = self.get_order_id()
        user_id = self.get_user_from_context(context)
        pay_load = struct.pack(NEW_ORDER_FMT,
                               order_id,
                               user_id,
                               request.symbol.encode().ljust(10, b'\0'),
                               side_int,
                               type_int,
                               float(request.price),
                               int(request.quantity))
        try:
            self.send_to_ome(MSG_NEW_ORDER, pay_load)
            return trading_pb2.OrderResponse(success=True, message="Order Forwarded", order_id=order_id)
        except Exception as e:
            return trading_pb2.OrderResponse(success=False, message=f"Failed to send: {e}", order_id=0)

    def StreamMarketData(self, request, context):
        sub_payload = struct.pack(SUB_REQ_FMT, request.symbol.encode().ljust(10, b'\0'), 1)
        self.send_to_ome(MSG_SUBSCRIBE, sub_payload)

        client_queue = queue.Queue()
        self.md_streams.append(client_queue)
        print(f"[Gateway] Stream active for {request.symbol}.")

        try:
            while context.is_active():
                try:
                    snapshot = client_queue.get(timeout=1.0)
                    if snapshot.symbol == request.symbol:
                        yield snapshot
                except queue.Empty:
                    pass
        finally:
            if client_queue in self.md_streams:
                self.md_streams.remove(client_queue)
            print(f"[Gateway] Stream disconnected for {request.symbol}.")

    def RequestMarketDataSnapshot(self, request, context):
        temp_queue = queue.Queue()
        self.md_streams.append(temp_queue)

        try:
            payload = struct.pack('<10s', request.symbol.encode().ljust(10, b'\0'))
            self.send_to_ome(b'M'[0], payload)
            while True:
                snapshot = temp_queue.get(timeout=3.0)
                if snapshot.symbol == request.symbol:
                    return snapshot
        except queue.Empty:
            context.set_details(f"Engine timeout requesting snapshot for {request.symbol}")
            context.set_code(grpc.StatusCode.DEADLINE_EXCEEDED)
            return trading_pb2.MarketDataSnapshot(symbol=request.symbol)
        finally:
            if temp_queue in self.md_streams:
                self.md_streams.remove(temp_queue)
    def CancelOrder(self, request, context):
        user_id = self.get_user_from_context(context)
        side_int = 0 if request.side == trading_pb2.BUY else 1
        # Pack: order_id (uint64), user_id (uint64), symbol (10 chars), side (uint8)
        pay_load = struct.pack(CANCEL_REQ_FMT,
                               request.order_id,
                               user_id,
                               request.symbol.encode().ljust(10, b'\0'),
                               side_int)
        try:
            self.send_to_ome(MSG_CANCEL_ORDER, pay_load)
            return trading_pb2.OrderResponse(success=True, message="Cancel Forwarded", order_id=request.order_id)
        except Exception as e:
            return trading_pb2.OrderResponse(success=False, message=f"Failed to send: {e}", order_id=request.order_id)
def run():
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    trading_pb2_grpc.add_TradingGatewayServicer_to_server(GatewayService(), server)
    server.add_insecure_port('[::]:50051')
    server.start()
    print("[Gateway] gRPC API listening on port 50051...")
    server.wait_for_termination()


if __name__ == '__main__':
    run()
