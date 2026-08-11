#pragma once
#include <sqlite3.h>
#include <string>
#include <vector>
using namespace std;

enum class Side{BUY,SELL};
enum class OrderType{LIMIT,MARKET};
enum class ReqType{NEW_ORDER,CANCEL,STOP};

struct Order{
    long long id;
    Side side;
    OrderType type;
    double price;
    int qty;
};

struct Trade{
    long long id;
    long long buyId;
    long long sellId;
    double price;
    int qty;
    int buyRemaining;
    int sellRemaining;
};

struct Request{
    ReqType type;
    Order order;
    long long cancelId=0;
};

class Database{
    sqlite3* db{};
    long long nextOrderId=1;
    long long nextTradeId=1;

    void run(const string& query){
        sqlite3_exec(db,query.c_str(),nullptr,nullptr,nullptr);
    }

    long long rowCount(const string& table){
        sqlite3_stmt* statement=nullptr;
        sqlite3_prepare_v2(db,("SELECT COUNT(*) FROM "+table+";").c_str(),-1,&statement,nullptr);
        sqlite3_step(statement);
        long long count=sqlite3_column_int64(statement,0);
        sqlite3_finalize(statement);
        return count;
    }

    string sideName(Side side){
        return side==Side::BUY ? "BUY" : "SELL";
    }

    string typeName(OrderType type){
        return type==OrderType::LIMIT ? "LIMIT" : "MARKET";
    }

public:
    Database(){
        sqlite3_open("exchange.db",&db);
        run("CREATE TABLE IF NOT EXISTS Orders(id INTEGER, side TEXT, type TEXT, price REAL, originalQty INTEGER, remainingQty INTEGER, status TEXT);");
        run("CREATE TABLE IF NOT EXISTS Trades(id INTEGER, buyId INTEGER, sellId INTEGER, price REAL, qty INTEGER);");

        // Makes an older Trades table work with the new Trade ID column.
        run("ALTER TABLE Trades ADD COLUMN id INTEGER;");
        run("UPDATE Trades SET id=rowid WHERE id IS NULL;");

        nextOrderId=rowCount("Orders")+1;
        nextTradeId=rowCount("Trades")+1;
    }

    ~Database(){
        if(db) sqlite3_close(db);
    }

    void saveNewOrder(Order& order){
        order.id=nextOrderId++;
        string query="INSERT INTO Orders (id,side,type,price,originalQty,remainingQty,status) VALUES(";
        query+=to_string(order.id)+",'"+sideName(order.side)+"','"+typeName(order.type)+"',";
        query+=to_string(order.price)+","+to_string(order.qty)+","+to_string(order.qty)+",'OPEN');";
        run(query);
    }

    void updateOrder(long long id,int remaining,const string& status){
        run("UPDATE Orders SET remainingQty="+to_string(remaining)+", status='"+status+"' WHERE id="+to_string(id)+";");
    }

    void updateOrderAfterMatch(long long id,int remaining){
        string query="UPDATE Orders SET remainingQty="+to_string(remaining)+", status=CASE ";
        query+="WHEN "+to_string(remaining)+"=0 THEN 'FILLED' ";
        query+="WHEN "+to_string(remaining)+" < originalQty THEN 'PARTIALLY_FILLED' ";
        query+="ELSE 'RESTING' END WHERE id="+to_string(id)+";";
        run(query);
    }

    void saveTrade(Trade& trade){
        trade.id=nextTradeId++;
        string query="INSERT INTO Trades (id,buyId,sellId,price,qty) VALUES(";
        query+=to_string(trade.id)+","+to_string(trade.buyId)+","+to_string(trade.sellId)+",";
        query+=to_string(trade.price)+","+to_string(trade.qty)+");";
        run(query);
    }

    vector<Order> loadPendingOrders(){
        sqlite3_stmt* statement=nullptr;
        string query="SELECT id,side,type,price,remainingQty FROM Orders ";
        query+="WHERE status IN ('RESTING','PARTIALLY_FILLED') ORDER BY id;";
        sqlite3_prepare_v2(db,query.c_str(),-1,&statement,nullptr);

        vector<Order> orders;
        while(sqlite3_step(statement)==SQLITE_ROW){
            string side=(const char*)sqlite3_column_text(statement,1);
            string type=(const char*)sqlite3_column_text(statement,2);
            orders.push_back({
                sqlite3_column_int64(statement,0),
                side=="BUY" ? Side::BUY : Side::SELL,
                type=="LIMIT" ? OrderType::LIMIT : OrderType::MARKET,
                sqlite3_column_double(statement,3),
                sqlite3_column_int(statement,4)
            });
        }
        sqlite3_finalize(statement);
        return orders;
    }
};
