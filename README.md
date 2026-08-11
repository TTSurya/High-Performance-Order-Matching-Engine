# High-Performance Order Matching Engine

A C++17 limit-order-book simulator that matches buy and sell orders using
**price-time priority**, records the results in SQLite, restores pending orders
after a restart, and prints core market-data metrics.

This project is a compact systems-programming exercise combining data
structures, multithreading, database persistence, and financial-market logic.

## Highlights

- Limit and market buy/sell orders.
- Price-time-priority matching.
- Partial fills, full fills, and resting orders.
- Cancellation of orders that are still in the book.
- Producer-consumer design with multiple producers and one matching thread.
- Thread-safe request queue using a mutex and condition variable.
- SQLite storage for orders and trades.
- Pending orders restored from the database on restart.
- Automatic order and trade IDs.
- Best bid/ask, spread, mid-price, liquidity, imbalance, last trade, and VWAP.

## Tech Stack

| Area | Technology |
|---|---|
| Language | C++17 |
| Order-book structures | `std::map`, `std::deque`, `std::unordered_map` |
| Concurrency | `std::thread`, `std::mutex`, `std::condition_variable` |
| Persistence | SQLite 3 |
| Build | `g++` and `run.bat` |
| Querying | SQLite CLI and `queries.sql` |
| Version control | Git and GitHub |

## Architecture

```text
Buyer / seller producer threads
             |
             v
   Thread-safe request queue
             |
             v
      Single matching thread
             |
     +-------+-------+
     v               v
In-memory order book  SQLite database
```

Producer threads create requests. They never modify the order book directly.
The single matcher consumes one request at a time, updates the book, creates
trades, and writes the latest state to SQLite. This gives deterministic book
updates without requiring a lock around the matching algorithm itself.

## Order Book Design

The engine maintains two books:

```cpp
map<double, deque<Order>, greater<double>> bids;
map<double, deque<Order>> asks;
```

| Book | Map order | First entry | Why |
|---|---|---|---|
| Bids | Descending | Highest buy price | Best bid is immediately available |
| Asks | Ascending | Lowest sell price | Best ask is immediately available |

Each map key is a price level. The `deque<Order>` at that price holds every
order waiting at the level.

### Price-Time Priority

Priority is decided in two stages:

1. **Price priority:** a buy order takes the lowest ask it can afford; a sell
   order takes the highest bid it can accept.
2. **Time priority:** when orders have the same price, the lowest order ID
   fills first. Order IDs are assigned by the single matching thread, so a
   lower ID represents an earlier accepted request.

For example, a buy at `101` executes before a buy at `100`, even if the `100`
order has an earlier ID. Price wins first; ID breaks ties only at the same price.

### Fast Cancellation Lookup

```cpp
unordered_map<long long, pair<Side, double>> positions;
```

This maps an active order ID to its side and price. A cancellation can therefore
jump directly to the correct price level instead of scanning the entire book.
It then scans only the queue at that price to remove the target order.

## Matching Logic

### Limit Orders

A limit order sets the worst price the trader accepts:

- Buy limit order: matches only when `buy price >= best ask`.
- Sell limit order: matches only when `sell price <= best bid`.

Any unfilled limit quantity remains in the book as a `RESTING` or
`PARTIALLY_FILLED` order.

### Market Orders

A market order takes the best prices currently available until its quantity is
filled or the opposite book becomes empty. Any remainder is cancelled because a
market order does not rest in the book.

### Execution Price and Quantity

Each trade executes at the resting order's price:

```text
trade quantity = min(incoming quantity, resting quantity)
trade price    = resting order price
```

## Thread Safety

`TSQ<Request>` is a thread-safe queue shared by all producers and the matcher.

- `push()` locks the queue, adds a request, unlocks it, then wakes one waiting
  consumer with `notify_one()`.
- `pop()` waits on a condition variable until a request exists, then safely
  removes and returns it.
- The matcher is the only thread that changes the book and database. This
  serializes matching decisions and prevents race conditions in order state.

The producers can run at the same time, but the queue determines the order in
which requests are accepted by the matcher.

## Database Design

SQLite stores the result of every accepted order and execution in `exchange.db`.

### Orders

| Column | Meaning |
|---|---|
| `id` | Automatically assigned order ID |
| `side` | `BUY` or `SELL` |
| `type` | `LIMIT` or `MARKET` |
| `price` | Limit price supplied with the order |
| `originalQty` | Quantity when the order was created |
| `remainingQty` | Quantity not yet filled or cancelled |
| `status` | Current order state |

Order statuses:

- `RESTING`: no quantity has traded; the order is waiting in the book.
- `PARTIALLY_FILLED`: some quantity traded; the remainder is waiting.
- `FILLED`: all quantity traded.
- `CANCELLED`: the remaining quantity was removed or an unfilled market-order
  remainder was discarded.

### Trades

| Column | Meaning |
|---|---|
| `id` | Automatically assigned trade ID |
| `buyId` | Buy order participating in the trade |
| `sellId` | Sell order participating in the trade |
| `price` | Execution price |
| `qty` | Executed quantity |

### Restart Recovery

At startup, the engine reads every `RESTING` and `PARTIALLY_FILLED` order from
SQLite and rebuilds the in-memory bid/ask books with its `remainingQty`.
Because restoration uses order ID order at each price, time priority continues
across program restarts.

Order and trade counters begin at the current table row count plus one, allowing
new records to continue after the existing rows in the database.

## Market Data

After matching, the program prints a snapshot of the remaining book:

| Metric | Meaning |
|---|---|
| Best bid | Highest active buy price |
| Best ask | Lowest active sell price |
| Bid-ask spread | `best ask - best bid` |
| Mid-price | `(best bid + best ask) / 2` |
| Bid liquidity | Total remaining buy quantity |
| Ask liquidity | Total remaining sell quantity |
| Order-book imbalance | `(bid liquidity - ask liquidity) / total liquidity` |
| Last trade price | Price of the final execution in the current run |
| VWAP | Volume-weighted average execution price in the current run |

Positive order-book imbalance means there is more displayed buy quantity; a
negative value means more displayed sell quantity. It is an order-book snapshot,
not a guaranteed price prediction.

## Run the Project

Requirements:

- A C++17 compiler (`g++`)
- SQLite development library and command-line tool (`sqlite3`)

From the project folder:

```powershell
.\run.bat
```

The program generates random demo limit-order prices and quantities on each run.
It creates or updates `exchange.db` in the project folder.

To start from an empty database:

```powershell
Remove-Item exchange.db
.\run.bat
```

## Inspect the Database

Run the included script after running the program.

In **PowerShell**:

```powershell
Get-Content queries.sql | sqlite3 exchange.db
```

In **Command Prompt** (`cmd`):

```bat
sqlite3 exchange.db < queries.sql
```

The script prints the 20 most recent orders, 20 most recent trades, active
orders, and an order-status summary.

## Complexity

Let `P` be the number of price levels and `K` the number of orders at one price
level.

| Operation | Complexity |
|---|---|
| Find best bid or ask | `O(1)` from the first map entry |
| Add a resting order | `O(log P + K)` because same-price orders are kept ID-ordered |
| Match an execution | `O(1)` plus book cleanup |
| Cancel an order | `O(log P + K)` |
| Restore pending orders | Sum of the insertion cost for each pending order |

## Limitations and Next Steps

This is an educational single-symbol engine. Real exchanges additionally need
validation, durable transactions, unique database constraints, risk checks,
multiple symbols, better ID generation, market-data feeds, event replay, and
production-grade performance testing.
