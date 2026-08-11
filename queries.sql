-- Run with: sqlite3 exchange.db < queries.sql

.headers on
.mode box

-- Every order and its current matching state.
.print ''
.print 'ORDERS'
SELECT
    id AS "Order ID",
    side AS "Side",
    type AS "Order Type",
    price AS "Price",
    originalQty AS "Original Quantity",
    remainingQty AS "Remaining Quantity",
    status AS "Status"
FROM Orders
ORDER BY id DESC
LIMIT 20;
.print ''

-- Every completed trade.
.print 'TRADES'
SELECT
    id AS "Trade ID",
    buyId AS "Buy Order ID",
    sellId AS "Sell Order ID",
    price AS "Price",
    qty AS "Quantity"
FROM Trades
ORDER BY id DESC
LIMIT 20;
.print ''

-- Only orders still available in the order book.
.print 'ACTIVE ORDERS'
SELECT
    id AS "Order ID",
    side AS "Side",
    type AS "Order Type",
    price AS "Price",
    originalQty AS "Original Quantity",
    remainingQty AS "Remaining Quantity",
    status AS "Status"
FROM Orders
WHERE status IN ('RESTING', 'PARTIALLY_FILLED')
ORDER BY id DESC
LIMIT 20;
.print ''

-- Summary totals.
.print 'SUMMARY'
SELECT
    COUNT(*) AS "Total Orders",
    SUM(CASE WHEN status = 'FILLED' THEN 1 ELSE 0 END) AS "Filled Orders",
    SUM(CASE WHEN status = 'RESTING' THEN 1 ELSE 0 END) AS "Resting Orders",
    SUM(CASE WHEN status = 'PARTIALLY_FILLED' THEN 1 ELSE 0 END) AS "Partially Filled Orders"
FROM Orders;
