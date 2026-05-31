# High-Performance Order Matching Engine

## Overview

This project is a simplified electronic exchange matching engine implemented in C++17. The engine maintains an order book, processes incoming buy and sell orders, matches compatible orders using price-time priority, executes trades, and persists order/trade information using SQLite.

The objective was to build a systems-oriented project combining concepts from Data Structures & Algorithms, Operating Systems, DBMS, Concurrency, and basic Financial Engineering.

---

## Features

### Supported Order Types

* Limit Buy Order
* Limit Sell Order
* Market Buy Order
* Market Sell Order
* Order Cancellation

### Matching Rules

* Price-Time Priority
* Partial Fills
* Full Fills
* Trade Execution Logging

### Persistence

* SQLite database storage
* Order history
* Trade history

### Concurrency

* Producer-Consumer architecture
* Multiple producer threads
* Single matching engine thread
* Thread-safe request queue
* Mutex and condition variable synchronization

### Performance Metrics

* Orders processed
* Trades executed
* Total traded volume
* Average order processing latency

---

## System Architecture

```text
Producer Threads
       |
       v
+------------------+
| Thread Safe Queue|
+------------------+
       |
       v
+------------------+
| Matching Engine  |
+------------------+
       |
       v
+------------------+
| SQLite Database  |
+------------------+
```

---

## Order Book Design

### Bid Side

Maintained in descending price order.

```text
Highest Price First
```

Example:

```text
101 | 20
100 | 40
```

### Ask Side

Maintained in ascending price order.

```text
Lowest Price First
```

Example:

```text
102 | 15
103 | 25
```

---

## Data Structures Used

### Bid Book

```cpp
map<double, deque<Order>, greater<double>>
```

Purpose:

* Highest bid lookup
* FIFO execution within a price level

### Ask Book

```cpp
map<double, deque<Order>>
```

Purpose:

* Lowest ask lookup
* FIFO execution within a price level

### Order Lookup

```cpp
unordered_map<long long, pair<Side,double>>
```

Purpose:

* Fast order cancellation
* Order tracking

### Request Queue

```cpp
queue<Request>
```

Purpose:

* Producer-consumer communication

---

## Matching Algorithm

### Buy Order

A buy order matches against the lowest available ask price.

Conditions:

```text
LIMIT BUY:
Buy Price >= Best Ask

MARKET BUY:
Matches until quantity becomes zero
or ask book becomes empty
```

### Sell Order

A sell order matches against the highest available bid price.

Conditions:

```text
LIMIT SELL:
Sell Price <= Best Bid

MARKET SELL:
Matches until quantity becomes zero
or bid book becomes empty
```

### Trade Execution

```text
Trade Price = Resting Order Price
Trade Quantity = Minimum(Remaining Quantities)
```

---

## Complexity Analysis

Let:

```text
P = Number of Price Levels
K = Orders at a Price Level
```

### Add Order

```text
O(log P)
```

### Best Bid / Ask Lookup

```text
O(1)
```

### Cancel Order

```text
O(log P + K)
```

### Match Order

```text
O(Number of Executions)
```

---

## Operating System Concepts Demonstrated

### Multithreading

Implemented using:

```cpp
std::thread
```

### Synchronization

Implemented using:

```cpp
std::mutex
std::condition_variable
```

### Producer-Consumer Pattern

Producer threads generate orders and submit them to a shared queue.

The matching engine thread consumes requests and updates the order book.

### Graceful Shutdown

The matching thread is terminated using a stop request and joined safely before program exit.

---

## Database Design

### Orders Table

```sql
CREATE TABLE Orders(
    id INTEGER,
    side INTEGER,
    type INTEGER,
    price REAL,
    qty INTEGER
);
```

### Trades Table

```sql
CREATE TABLE Trades(
    buyId INTEGER,
    sellId INTEGER,
    price REAL,
    qty INTEGER
);
```

---

## Sample Output

```text
TRADE 20@101
TRADE 10@100

========== ASKS ==========
102 | 40

========== BIDS ==========
100 | 40

Orders Processed : 4
Trades Executed  : 2
Volume           : 30
Avg Latency(ns)  : 25050
```

---

## Tech Stack

### Core Engine

* C++17

### Database

* SQLite

### Concurrency

* std::thread
* mutex
* condition_variable

### Build System

* CMake

---

## Future Improvements

- Support multiple trading symbols
- Export trades to CSV for analysis
- Add more realistic market simulations
- Improve performance under larger workloads
- Add recovery from saved order book snapshots
- Build a simple analytics dashboard

---

## Key Learnings

This project provided hands-on experience with:

* Order book design
* Matching engine architecture
* Concurrent programming and Synchronization primitives
* Producer-consumer systems
* SQLite persistence
* Performance measurement
* System design tradeoffs

The project demonstrates practical applications of concepts from Operating Systems, DBMS, Data Structures & Algorithms, and Concurrent Systems while simulating the core functionality of a financial exchange.
