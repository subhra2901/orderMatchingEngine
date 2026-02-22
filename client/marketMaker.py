import grpc
import time
import random
import threading
import trading_pb2
import trading_pb2_grpc

GATEWAY_HOST = '127.0.0.1:50051'
SYMBOL = "AAPL"
BASE_PRICE = 150.00


def run_market_maker():
    print(f"Connecting Market Maker Bot to Gateway at {GATEWAY_HOST}...")
    channel = grpc.insecure_channel(GATEWAY_HOST)
    stub = trading_pb2_grpc.TradingGatewayStub(channel)

    print(f"Injecting liquidity for {SYMBOL}...")

    try:
        while True:
            # 1. Generate random Bid (Buy) and Ask (Sell) prices around the base price
            # Bids are lower than base, Asks are higher than base (maintaining a spread)
            bid_px = round(BASE_PRICE - random.uniform(0.05, 0.50), 2)
            ask_px = round(BASE_PRICE + random.uniform(0.05, 0.50), 2)

            qty = random.choice([100, 200, 500, 1000])

            # 2. Send the BUY Limit Order
            stub.PlaceOrder(trading_pb2.NewOrderRequest(
                symbol=SYMBOL,
                side=trading_pb2.BUY,
                type=trading_pb2.LIMIT,
                price=bid_px,
                quantity=qty
            ))

            # 3. Send the SELL Limit Order
            stub.PlaceOrder(trading_pb2.NewOrderRequest(
                symbol=SYMBOL,
                side=trading_pb2.SELL,
                type=trading_pb2.LIMIT,
                price=ask_px,
                quantity=qty
            ))

            print(f"[Market Maker] Quoted {qty} @ ${bid_px} (BID) | ${ask_px} (ASK)")

            # Wait a fraction of a second before sending more
            time.sleep(0.5)

    except KeyboardInterrupt:
        print("\nMarket Maker Bot stopped.")
    except Exception as e:
        print(f"Market Maker Error: {e}")


if __name__ == "__main__":
    run_market_maker()
