#include<iostream>
#include <thread>
#include <string>
#include"../hdcs_networking.h"

using namespace std;
using namespace hdcs::networking;

/* hdcs::networking::server have three interface.
 * interface 1 : construction 
 * interface 2 : start
 * interface 3 : run
 * interface 4 : send
 */

class test_class{
public:
    test_class(string ip_address, string port_num, int s_num, int thd_num):
        echo_server(new server(ip_address, port_num, s_num, thd_num)){
    }

    ~test_class(){
        cout<<"test_class will destroy!!!!"<<endl;
    }

    void run(){
        cout<<"....run echo server...."<<endl;
        echo_server->start( [ this ](void* p, string s){ //
                handle_request(p, s);
                });
        echo_server->sync_run();
    }

    void handle_request(void* s_id, string receive_buffer){
        //std::cout<<"hdcs_hanlde_request: msg is "<<receive_buffer<<std::endl;
        // produce 4k message content.

        string xx("hello, i'm server......");

        echo_server->send((Session*)s_id, xx, NULL);
    }
private:
    server* echo_server;
};


int main(){

    int session_num = 10; // one session for one io_service
    int thd_num_of_one_ios = 20;
    uint64_t run_time = 100000;

    string ip_address("127.0.0.1");
    string port_num("6666");

    test_class test(ip_address, port_num, session_num, thd_num_of_one_ios);
    test.run();

    /*
    server echo_server("127.0.0.1", "6666", 1, 1);

    echo_server.start( [](void* p, string s){ //
        handle_request(p, s);
        });


    echo_server.sync_run();
    */

    //test_for_stop_interface(ip_address, port_num, 1, 1);

    return 0;
} 
