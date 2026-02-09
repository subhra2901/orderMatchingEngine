import socket
import struct
import time

def main():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    s.connect(("localhost", 8080))
    print("Connected to server")


    # Header:  seq(H) + type(B) + length(H)
    # LoginRequest: Header + user_id(I) + password(20s)
    # NewOrderRequest: Header + user_id(I) + symbol(8s) + side(B) + price(Q) + quantity(I)
    #1. Login
    header = struct.pack('<HBH', 0,ord('L'),45)  # seq=0, type='L', length=45
    login_req = struct.pack('<20s20s', b"Trder1".ljust(20), b"pass".ljust(20))  # user_id=1, password="password123"
    s.send(header + login_req)

    print (" Sent login request")
    try:
        data = s.recv(1024)
        if len(data) >=5 :
            seq, msg_type, length = struct.unpack('<HBH', data[:5])
            if(chr(msg_type) == 'R'):
                status, _ = struct.unpack('<B50s', data[5:])
                if(status == 1):
                    print("Login successful")
                else:
                    print("Login failed")
                    return             
    except Exception as e:
        print(f"Error receiving data: {e}")
    #2. Send New Order
    time.sleep(1)  # Wait a bit before sending the order

    print (" Sending new order request")

    header = struct.pack('<HBH', 1, ord('N'), 41)  # seq=1, type='N', length=41
    new_order_req = struct.pack('<Q10sBBdQ', 
                                1,
                                b"AAPL".ljust(10),
                                0,  # side = 'B' (Buy)
                                1, #market order
                                150.00, 
                                100)  # price=150.00, quantity=100
    s.send(header + new_order_req)

    print (" Sent new order request")

    data = s.recv(1024)
    
    if len(data) >= 5:
        print(f"\n[Client] Received {len(data)} bytes.")
        seq, mtype, mlen = struct.unpack('<HBH', data[:5])
        
        print(f"Header -> Type: {chr(mtype)}")

        if chr(mtype) == 'E': # Execution Report
            # Body Structure (from protocol.h):
            # client_order_id (Q)
            # execution_id    (Q)
            # symbol          (10s)
            # side            (B)
            # price           (d)
            # quantity        (Q)
            # filled_qty      (Q)
            # status          (B)
            # Format: <QQ10sBdQQB
            
            payload = data[5:]
            try:
                (cid, exec_id, sym, side, px, qty, filled, status) = struct.unpack('<QQ10sBdQQB', payload)
                
                print("\n>>> EXECUTION REPORT <<<")
                print(f"Client Order ID: {cid}")
                print(f"Execution ID:    {exec_id}")
                print(f"Symbol:          {sym.decode().strip(chr(0))}")
                print(f"Side:            {'Buy' if side == 0 else 'Sell'}")
                print(f"Price:           {px}")
                print(f"Quantity:        {qty}")
                print(f"Filled Qty:      {filled}")
                print(f"Status:          {status} (2=Filled)")
            except struct.error as e:
                print(f"Unpack Error: {e} - check protocol alignment!")
    time.sleep(1)
    s.close()
if __name__ == "__main__":
    main()




