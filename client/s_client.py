import socket
import struct
import time


def login_request(sock, user_id, password):
    """
    Send login request to server.

    Message Structure:
    - Header: seq(H) + type(B) + length(H)
    - Payload: user_id(20s) + password(20s)
    """
    header = struct.pack('<HBH', 0, ord('L'), 45)  # seq=0, type='L', length=45
    login_req = struct.pack(
        '<20s20s',
        f"{user_id}".encode().ljust(20),
        f"{password}".encode().ljust(20)
    )
    sock.send(header + login_req)
    print("Sent login request")

    try:
        data = sock.recv(1024)
        if len(data) >= 5:
            seq, msg_type, length = struct.unpack('<HBH', data[:5])
            if chr(msg_type) == 'R':
                status, _ = struct.unpack('<B50s', data[5:])
                if status == 1:
                    print("Login successful")
                else:
                    print("Login failed")
                    return
    except Exception as e:
        print(f"Error receiving data: {e}")


def send_new_order(sock):
    """
    Send new order request to server.

    Message Structure:
    - Header: seq(H) + type(B) + length(H)
    - Payload: client_order_id(Q) + symbol(10s) + side(B) + type(B) + price(d) + quantity(Q)
    """
    print("Sending new order request")
    header = struct.pack('<HBH', 1, ord('N'), 41)  # seq=1, type='N', length=41
    new_order_req = struct.pack(
        '<Q10sBBdQ',
        1,                           # client_order_id
        b"AAPL".ljust(10),          # symbol
        0,                           # side (0=Buy, 1=Sell)
        1,                           # type (0=Market, 1=Limit)
        150.00,                      # price
        100                          # quantity
    )
    sock.send(header + new_order_req)
    print("Sent new order request")

    data = sock.recv(1024)
    if len(data) >= 5:
        print(f"\n[Client] Received {len(data)} bytes.")
        seq, mtype, mlen = struct.unpack('<HBH', data[:5])
        print(f"Header -> Type: {chr(mtype)}")

        if chr(mtype) == 'E':  # Execution Report
            # Body Structure (from protocol.h):
            # client_order_id (Q) + execution_id (Q) + symbol (10s) + side (B) +
            # price (d) + quantity (Q) + filled_qty (Q) + status (B)
            # Format: <QQ10sBdQQB

            payload = data[5:]
            try:
                (
                    cid, exec_id, sym, side, px, qty, filled, status
                ) = struct.unpack('<QQ10sBdQQB', payload)

                print("\n>>> EXECUTION REPORT <<<")
                print(f"Client Order ID: {cid}")
                print(f"Execution ID:    {exec_id}")
                print(f"Symbol:          {sym.decode().strip(chr(0))}")
                print(f"Side:            {'Buy' if side == 0 else 'Sell'}")
                print(f"Price:           {px}")
                print(f"Quantity:        {qty}")
                print(f"Filled Qty:      {filled}")
                print(f"Status:          {status} (0=New, 2=Filled, 1=Partially Filled, 3=Rejected)")
            except struct.error as e:
                print(f"Unpack Error: {e} - check protocol alignment!")
    time.sleep(1)


def send_market_data_request(sock):
    """
    Send market data request to server.

    Message Structure:
    - Header: seq(H) + type(B) + length(H)
    - Payload: symbol(10s)
    - Total length: 15 bytes (5 header + 10 symbol)
    """
    print("Sending market data request")
    header = struct.pack('<HBH', 2, ord('M'), 15)  # seq=2, type='M', length=15
    md_req = struct.pack('<10s', b"AAPL".ljust(10))  # symbol="AAPL"
    sock.send(header + md_req)
    print("Sent market data request")

    data = sock.recv(1024)
    if len(data) >= 5:
        print(f"\n[Client] Received {len(data)} bytes.")
        seq, mtype, mlen = struct.unpack('<HBH', data[:5])
        print(f"Header -> Type: {chr(mtype)}")

        if chr(mtype) == 'S':  # Market Data Snapshot
            # Body Structure (from protocol.h):
            # symbol (10s) + best_bid_price (d) + best_bid_qty (Q) +
            # best_ask_price (d) + best_ask_qty (Q)
           
            fmt = '<10sII' + ('dQ' * 5) + ('dQ' * 5)
            payload = data[5:]
            try:
                unpacked_data = struct.unpack(fmt, payload)
                
                # Extract fields
                symbol = unpacked_data[0].decode().strip(chr(0))
                num_bids = unpacked_data[1]
                num_asks = unpacked_data[2]
                
                print("\n>>> MARKET DATA SNAPSHOT <<<")
                print(f"Symbol:   {symbol}")
                print(f"Num Bids: {num_bids}")
                print(f"Num Asks: {num_asks}")

                # Slice the tuple to get bids and asks
                # unpacked_data[0:3] are header fields
                # unpacked_data[3:13] are bids (5 pairs of price,qty)
                # unpacked_data[13:23] are asks (5 pairs of price,qty)
                
                print("--- Bids ---")
                for i in range(5):
                    if i < num_bids:
                        idx = 3 + (i * 2)
                        price = unpacked_data[idx]
                        qty = unpacked_data[idx+1]
                        print(f"Bid {i+1}: {price:.2f} @ {qty}")

                print("--- Asks ---")
                for i in range(5):
                    if i < num_asks:
                        idx = 13 + (i * 2)
                        price = unpacked_data[idx]
                        qty = unpacked_data[idx+1]
                        print(f"Ask {i+1}: {price:.2f} @ {qty}")

            except struct.error as e:
                print(f"Unpack Error: {e}")
                print(f"Payload len: {len(payload)}, Expected: {struct.calcsize(fmt)}")


def main():
    """Main entry point for the client."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(("localhost", 8080))
    print("Connected to server")

    login_request(sock, "user123", "password123")
    time.sleep(0.5)
    send_new_order(sock)
    time.sleep(0.5)
    send_market_data_request(sock)

    sock.close()


if __name__ == "__main__":
    main()




