#pragma once
#include <sqlite3.h>
#include <string>
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
    long long buyId,sellId;
    double price;
    int qty;
};

struct Request{
    ReqType type;
    Order order;
    long long cancelId=0;
};

class Database{
    sqlite3* db{};
public:
    Database(string file="exchange.db"){
        sqlite3_open(file.c_str(),&db);
        sqlite3_exec(db,"CREATE TABLE IF NOT EXISTS Orders(id INTEGER,side INTEGER,type INTEGER,price REAL,qty INTEGER);",0,0,0);
        sqlite3_exec(db,"CREATE TABLE IF NOT EXISTS Trades(buyId INTEGER,sellId INTEGER,price REAL,qty INTEGER);",0,0,0);
    }

    ~Database(){ if(db) sqlite3_close(db); }

    void saveOrder(const Order&o){
        string q="INSERT INTO Orders VALUES("+to_string(o.id)+","+to_string((int)o.side)+","+to_string((int)o.type)+","+to_string(o.price)+","+to_string(o.qty)+");";
        sqlite3_exec(db,q.c_str(),0,0,0);
    }

    void saveTrade(const Trade&t){
        string q="INSERT INTO Trades VALUES("+to_string(t.buyId)+","+to_string(t.sellId)+","+to_string(t.price)+","+to_string(t.qty)+");";
        sqlite3_exec(db,q.c_str(),0,0,0);
    }
};
