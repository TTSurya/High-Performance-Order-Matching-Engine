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
#include <atomic>
#include <numeric>
#include "database.h"

using namespace std;

template<class T>
class TSQ{
    queue<T> q;
    mutex m;
    condition_variable cv;
public:
    void push(const T& x){ {lock_guard<mutex> lk(m); q.push(x);} cv.notify_one(); }
    T pop(){ unique_lock<mutex> lk(m); cv.wait(lk,[&]{return !q.empty();}); T x=q.front(); q.pop(); return x; }
};

struct Stats{
    long long orders=0,trades=0,volume=0;
    vector<long long> latency;
};

class MatchingEngine{
    map<double,deque<Order>,greater<double>> bids;
    map<double,deque<Order>> asks;
    unordered_map<long long,pair<Side,double>> pos;

public:
    vector<Trade> process(Order in){
        vector<Trade> tr;

        if(in.side==Side::BUY){
            while(in.qty && !asks.empty()){
                auto it=asks.begin();
                if(in.type==OrderType::LIMIT && in.price<it->first) break;

                auto &r=it->second.front();
                int fill=min(in.qty,r.qty);
                tr.push_back({in.id,r.id,it->first,fill});

                in.qty-=fill; r.qty-=fill;

                if(!r.qty){
                    pos.erase(r.id);
                    it->second.pop_front();
                    if(it->second.empty()) asks.erase(it);
                }
            }
            if(in.qty && in.type==OrderType::LIMIT){
                bids[in.price].push_back(in);
                pos[in.id]={Side::BUY,in.price};
            }
        }else{
            while(in.qty && !bids.empty()){
                auto it=bids.begin();
                if(in.type==OrderType::LIMIT && in.price>it->first) break;

                auto &r=it->second.front();
                int fill=min(in.qty,r.qty);
                tr.push_back({r.id,in.id,it->first,fill});

                in.qty-=fill; r.qty-=fill;

                if(!r.qty){
                    pos.erase(r.id);
                    it->second.pop_front();
                    if(it->second.empty()) bids.erase(it);
                }
            }
            if(in.qty && in.type==OrderType::LIMIT){
                asks[in.price].push_back(in);
                pos[in.id]={Side::SELL,in.price};
            }
        }
        return tr;
    }

    bool cancel(long long id){
        if(!pos.count(id)) return false;
        auto [side,price]=pos[id];

        auto rem=[&](auto& book){
            auto lvl=book.find(price);
            if(lvl==book.end()) return false;
            auto &dq=lvl->second;

            for(auto it=dq.begin();it!=dq.end();++it){
                if(it->id==id){
                    dq.erase(it);
                    if(dq.empty()) book.erase(lvl);
                    pos.erase(id);
                    return true;
                }
            }
            return false;
        };

        return side==Side::BUY?rem(bids):rem(asks);
    }

    void printBook(){
        cout<<"\n========== ASKS ==========\n";
        for(auto &[p,q]:asks) cout<<p<<" | "<<q.front().qty<<"\n";

        cout<<"\n========== BIDS ==========\n";
        for(auto &[p,q]:bids) cout<<p<<" | "<<q.front().qty<<"\n";
    }
};

int main(){
    Database db;
    TSQ<Request> q;
    MatchingEngine eng;
    Stats st;
    atomic<bool> running=true;

    thread matcher([&]{
        while(running){
            Request r=q.pop();

            if(r.type==ReqType::STOP) break;

            if(r.type==ReqType::CANCEL){
                eng.cancel(r.cancelId);
                continue;
            }

            auto t1=chrono::high_resolution_clock::now();
            auto trades=eng.process(r.order);
            auto t2=chrono::high_resolution_clock::now();

            st.latency.push_back(
                chrono::duration_cast<chrono::nanoseconds>(t2-t1).count()
            );

            st.orders++;
            db.saveOrder(r.order);

            for(auto &t:trades){
                st.trades++;
                st.volume+=t.qty;

                db.saveTrade(t);
                cout<<"TRADE "<<t.qty<<"@"<<t.price<<"\n";
            }
        }
    });

    thread p1([&]{
        q.push({ReqType::NEW_ORDER,{1,Side::BUY,OrderType::LIMIT,100,50}});
        q.push({ReqType::NEW_ORDER,{2,Side::BUY,OrderType::LIMIT,101,20}});
    });

    thread p2([&]{
        q.push({ReqType::NEW_ORDER,{3,Side::SELL,OrderType::LIMIT,99,30}});
        q.push({ReqType::NEW_ORDER,{4,Side::SELL,OrderType::LIMIT,102,40}});
    });

    p1.join();
    p2.join();

    this_thread::sleep_for(chrono::seconds(1));

    running=false;
    q.push({ReqType::STOP,{0,Side::BUY,OrderType::LIMIT,0,0}});

    matcher.join();

    eng.printBook();

    long long avg=0;
    if(!st.latency.empty())
        avg=accumulate(st.latency.begin(),st.latency.end(),0LL)/st.latency.size();

    cout<<"\nOrders Processed : "<<st.orders<<"\n";
    cout<<"Trades Executed  : "<<st.trades<<"\n";
    cout<<"Volume           : "<<st.volume<<"\n";
    cout<<"Avg Latency(ns)  : "<<avg<<"\n";
}
