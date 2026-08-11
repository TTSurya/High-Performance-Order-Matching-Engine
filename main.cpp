#include <iostream>
#include <map>
#include <deque>
#include <unordered_map>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <algorithm>
#include <functional>
#include <iomanip>
#include <random>
#include "database.h"

using namespace std;

template<class T>
class TSQ{
    queue<T> queue;
    mutex lockMutex;
    condition_variable ready;

public:
    void push(const T& item){
        {
            lock_guard<mutex> lock(lockMutex);
            queue.push(item);
        }
        ready.notify_one();
    }

    T pop(){
        unique_lock<mutex> lock(lockMutex);
        ready.wait(lock,[&]{ return !queue.empty(); });

        T item=queue.front();
        queue.pop();
        return item;
    }
};

struct Stats{
    long long orders=0;
    long long trades=0;
    long long volume=0;
    long long totalLatency=0;
    double tradedValue=0;
    double lastTradePrice=0;
    bool hasTrade=false;
};

struct MarketData{
    bool hasBid=false;
    bool hasAsk=false;
    double bestBid=0;
    double bestAsk=0;
    long long bidQuantity=0;
    long long askQuantity=0;
};

int randomInt(int minimum,int maximum){
    static thread_local mt19937 generator(random_device{}());
    uniform_int_distribution<int> distribution(minimum,maximum);
    return distribution(generator);
}

class MatchingEngine{
    map<double,deque<Order>,greater<double>> bids;
    map<double,deque<Order>> asks;
    unordered_map<long long,pair<Side,double>> positions;

    long long levelQuantity(const deque<Order>& orders) const{
        long long quantity=0;
        for(const Order& order:orders) quantity+=order.qty;
        return quantity;
    }

    void addRestingOrder(const Order& order){
        if(order.side==Side::BUY){
            deque<Order>& level=bids[order.price];
            auto place=level.begin();
            while(place!=level.end() && place->id<order.id) place++;
            level.insert(place,order);
        }else{
            deque<Order>& level=asks[order.price];
            auto place=level.begin();
            while(place!=level.end() && place->id<order.id) place++;
            level.insert(place,order);
        }
        positions[order.id]={order.side,order.price};
    }

public:
    void restore(const Order& order){
        addRestingOrder(order);
    }

    vector<Trade> process(Order& order){
        vector<Trade> trades;

        if(order.side==Side::BUY){
            while(order.qty && !asks.empty()){
                auto level=asks.begin();
                if(order.type==OrderType::LIMIT && order.price<level->first) break;

                Order& resting=level->second.front();
                int quantity=min(order.qty,resting.qty);
                order.qty-=quantity;
                resting.qty-=quantity;
                trades.push_back({0,order.id,resting.id,level->first,quantity,order.qty,resting.qty});

                if(resting.qty==0){
                    positions.erase(resting.id);
                    level->second.pop_front();
                    if(level->second.empty()) asks.erase(level);
                }
            }

            if(order.qty && order.type==OrderType::LIMIT){
                addRestingOrder(order);
            }
        }else{
            while(order.qty && !bids.empty()){
                auto level=bids.begin();
                if(order.type==OrderType::LIMIT && order.price>level->first) break;

                Order& resting=level->second.front();
                int quantity=min(order.qty,resting.qty);
                order.qty-=quantity;
                resting.qty-=quantity;
                trades.push_back({0,resting.id,order.id,level->first,quantity,resting.qty,order.qty});

                if(resting.qty==0){
                    positions.erase(resting.id);
                    level->second.pop_front();
                    if(level->second.empty()) bids.erase(level);
                }
            }

            if(order.qty && order.type==OrderType::LIMIT){
                addRestingOrder(order);
            }
        }
        return trades;
    }

    int remainingQuantity(long long id) const{
        auto position=positions.find(id);
        if(position==positions.end()) return 0;

        if(position->second.first==Side::BUY){
            auto level=bids.find(position->second.second);
            for(const Order& order:level->second){
                if(order.id==id) return order.qty;
            }
        }else{
            auto level=asks.find(position->second.second);
            for(const Order& order:level->second){
                if(order.id==id) return order.qty;
            }
        }
        return 0;
    }

    bool cancel(long long id){
        auto position=positions.find(id);
        if(position==positions.end()) return false;

        auto removeOrder=[&](auto& book){
            auto level=book.find(position->second.second);
            for(auto order=level->second.begin();order!=level->second.end();++order){
                if(order->id==id){
                    level->second.erase(order);
                    if(level->second.empty()) book.erase(level);
                    positions.erase(position);
                    return true;
                }
            }
            return false;
        };

        if(position->second.first==Side::BUY) return removeOrder(bids);
        return removeOrder(asks);
    }

    void printBook() const{
        cout<<"\nASKS\n";
        for(const auto& [price,orders]:asks) cout<<price<<" | "<<levelQuantity(orders)<<"\n";

        cout<<"\nBIDS\n";
        for(const auto& [price,orders]:bids) cout<<price<<" | "<<levelQuantity(orders)<<"\n";
    }

    MarketData marketData() const{
        MarketData data;
        data.hasBid=!bids.empty();
        data.hasAsk=!asks.empty();

        if(data.hasBid) data.bestBid=bids.begin()->first;
        if(data.hasAsk) data.bestAsk=asks.begin()->first;

        for(const auto& [price,orders]:bids) data.bidQuantity+=levelQuantity(orders);
        for(const auto& [price,orders]:asks) data.askQuantity+=levelQuantity(orders);
        return data;
    }
};

int main(){
    Database db;
    TSQ<Request> requests;
    MatchingEngine engine;
    Stats stats;

    for(const Order& order:db.loadPendingOrders()){
        engine.restore(order);
    }

    thread matcher([&]{
        while(true){
            Request request=requests.pop();
            if(request.type==ReqType::STOP) break;

            if(request.type==ReqType::CANCEL){
                int remaining=engine.remainingQuantity(request.cancelId);
                if(engine.cancel(request.cancelId)){
                    db.updateOrder(request.cancelId,remaining,"CANCELLED");
                }
                continue;
            }

            db.saveNewOrder(request.order);

            auto start=chrono::high_resolution_clock::now();
            vector<Trade> trades=engine.process(request.order);
            auto end=chrono::high_resolution_clock::now();

            stats.orders++;
            stats.totalLatency+=chrono::duration_cast<chrono::nanoseconds>(end-start).count();

            for(Trade& trade:trades){
                stats.trades++;
                stats.volume+=trade.qty;
                stats.tradedValue+=trade.price*trade.qty;
                stats.lastTradePrice=trade.price;
                stats.hasTrade=true;
                db.saveTrade(trade);
                db.updateOrderAfterMatch(trade.buyId,trade.buyRemaining);
                db.updateOrderAfterMatch(trade.sellId,trade.sellRemaining);
                cout<<"TRADE "<<trade.qty<<"@"<<trade.price<<"\n";
            }

            if(request.order.qty>0 && request.order.type==OrderType::MARKET){
                db.updateOrder(request.order.id,request.order.qty,"CANCELLED");
            }else if(request.order.qty>0){
                db.updateOrderAfterMatch(request.order.id,request.order.qty);
            }
        }
    });

    thread buyers([&]{
        requests.push({ReqType::NEW_ORDER,{0,Side::BUY,OrderType::LIMIT,double(randomInt(100,105)),randomInt(20,100)}});
        requests.push({ReqType::NEW_ORDER,{0,Side::BUY,OrderType::LIMIT,double(randomInt(98,103)),randomInt(20,100)}});
    });

    thread sellers([&]{
        requests.push({ReqType::NEW_ORDER,{0,Side::SELL,OrderType::LIMIT,double(randomInt(95,100)),randomInt(20,100)}});
        requests.push({ReqType::NEW_ORDER,{0,Side::SELL,OrderType::LIMIT,double(randomInt(106,110)),randomInt(20,100)}});
    });

    buyers.join();
    sellers.join();
    requests.push({ReqType::STOP,{}});
    matcher.join();

    engine.printBook();
    MarketData market=engine.marketData();
    long long averageLatency=stats.orders ? stats.totalLatency/stats.orders : 0;

    cout<<fixed<<setprecision(2);
    cout<<"\nMARKET DATA\n";
    if(market.hasBid) cout<<"Best bid: "<<market.bestBid<<"\n";
    if(market.hasAsk) cout<<"Best ask: "<<market.bestAsk<<"\n";
    if(market.hasBid && market.hasAsk){
        double spread=market.bestAsk-market.bestBid;
        double midPrice=(market.bestAsk+market.bestBid)/2;
        cout<<"Bid-ask spread: "<<spread<<"\n";
        cout<<"Mid-price: "<<midPrice<<"\n";
    }
    cout<<"Bid liquidity: "<<market.bidQuantity<<"\n";
    cout<<"Ask liquidity: "<<market.askQuantity<<"\n";
    if(market.bidQuantity+market.askQuantity>0){
        double imbalance=double(market.bidQuantity-market.askQuantity);
        imbalance/=market.bidQuantity+market.askQuantity;
        cout<<"Order-book imbalance: "<<imbalance<<"\n";
    }
    if(stats.hasTrade){
        cout<<"Last trade price: "<<stats.lastTradePrice<<"\n";
        cout<<"VWAP: "<<stats.tradedValue/stats.volume<<"\n";
    }

    cout<<"\nOrders: "<<stats.orders<<"\n";
    cout<<"Trades: "<<stats.trades<<"\n";
    cout<<"Volume: "<<stats.volume<<"\n";
    cout<<"Average latency (ns): "<<averageLatency<<"\n";
}
