# Order Matching Engine

A compact C++17 example of a price-time-priority matching engine backed by SQLite.

## What it does

- Accepts limit and market buy/sell orders.
- Matches buys with the lowest ask and sells with the highest bid.
- At the same price, matches the lowest order ID first (the earliest accepted request).
- Prints best bid, best ask, spread, mid-price, liquidity, imbalance, last trade price, and VWAP.
- Supports cancellation of resting orders.
- Stores orders and trades in `exchange.db`.
- Restores pending orders from `exchange.db` when the program starts.
- Uses producer threads, a thread-safe queue, and one matching thread.
- Generates random limit-order prices and quantities for each demo run.

## Run

Requirements: a C++17 compiler and SQLite development library (`sqlite3`).

```powershell
.\run.bat
```

The batch file stops if compilation fails. Running the program creates or updates
`exchange.db` in the project folder.

## Database

`Orders` stores the original quantity, remaining quantity, and current status:

- `RESTING`: not filled yet.
- `PARTIALLY_FILLED`: some quantity traded and some remains.
- `FILLED`: all quantity traded.
- `CANCELLED`: remaining quantity was cancelled.

Both orders and trades receive IDs automatically. At startup, each next ID begins
at the current row count plus one, so new records continue after existing rows.
Orders with status `RESTING` or `PARTIALLY_FILLED` are loaded back into the order
book using their saved remaining quantity before new requests are processed.

## Market data

After matching, the program prints a market snapshot:

- **Best bid / best ask:** highest active buy and lowest active sell.
- **Bid-ask spread:** `best ask - best bid`.
- **Mid-price:** average of best bid and best ask.
- **Bid/ask liquidity:** total remaining quantity on each book side.
- **Order-book imbalance:** `(bid liquidity - ask liquidity) / total liquidity`.
  Positive values indicate more displayed buy liquidity; negative values indicate more sell liquidity.
- **Last trade price:** price of the most recent execution in the current run.
- **VWAP:** volume-weighted average price of executions in the current run.

## View results

Run the included query script after running the program:

```powershell
Get-Content queries.sql | sqlite3 exchange.db
```

It displays the 20 most recent orders, 20 most recent trades, active orders, and
an order-status summary.

To track the script in Git:

```powershell
git add queries.sql
```
