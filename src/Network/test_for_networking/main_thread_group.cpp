#include <iostream>
#include<string>
#include "../io_service/thread_group.h"

using namespace std;

void print1(){
    cout<<"-----"<<pthread_self()<<"----"<<endl;
}

void print2(){
    cout<<"-----"<<pthread_self()<<"----"<<endl;
}

void print3(string temp){
    cout<<temp<<endl;
    cout<<"-----"<<pthread_self()<<"----"<<endl;
}

int main(){

    hdcs::networking::ThreadGroup thread_pool(10);

    thread_pool.post(print1);
    thread_pool.dispatch(print2);
    thread_pool.dispatch(print2);
    thread_pool.post(print1);
    thread_pool.post(print1);
    thread_pool.post(print1);
    thread_pool.dispatch(print2);
    thread_pool.post(print1);
    thread_pool.post(print1);
    thread_pool.post(print1);
    thread_pool.post(print1);
    thread_pool.dispatch(print2);
    thread_pool.dispatch(print2);
    thread_pool.post(print1);

    thread_pool.post(boost::bind(&print3, "this is test 3"));

    sleep(10);

    return 0;
}
