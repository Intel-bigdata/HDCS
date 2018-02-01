#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include "../hdcs_networking.h"


using namespace std;
using namespace hdcs::networking;

// record sending request times
atomic<uint64_t> send_index(0);
// receive ack times.
atomic<uint64_t> receive_index(0);

mutex send_lock;
mutex receive_lock;

namespace hdcs{
namespace networking{

ssize_t handle_request(void* arg, string receive_buffer){
    ++receive_index;
   std::cout<<"hdcs handle msg, msg is : "<<receive_buffer<<std::endl;
   return 1;
}

}
}


int main(){

    Connection* echo_client = new Connection(
            [](void* s, string p){handle_request(s,p);},
            1,
            1
            );


    string ip_address("127.0.0.1");
    string port_num("6666");

    echo_client->connect(ip_address, port_num);


    cout<<"******************************connection success***********************************"<<endl;

    sleep(2);
    ++send_index;
    echo_client->aio_communicate("shang_de_hao_from_client_side_xxxxxxxxxxxxxxxx");
    sleep(1);
    ++send_index;
    echo_client->aio_communicate("shang_de_hao_from_client_side_yyyyyyyyyyyyyyyy");
    sleep(1);
    ++send_index;
    echo_client->aio_communicate("shang_de_hao_from_client_side_zzzzzzzzzzzzzzzz");
    sleep(1);
    ++send_index;
    echo_client->aio_communicate("shang_de_hao_from_client_side_cccccccccccccccc");
    sleep(1);
    ++send_index;
    echo_client->aio_communicate("shang_de_hao_from_client_side_kkkkkkkkkkkkkkkk");
    sleep(1);

    cout<<"*****************************aio_communicate success********************************"<<endl;

    for(int i=0; i<200000; i++){
        //sleep(1);
        while((send_index-receive_index)>64){
            std::cout<<"waiting....."<<std::endl;
        }

        ++send_index;
        echo_client->aio_communicate("shang_de_hao_from_client_side_xxxxxxxxxxxxxxxx_over");
        ++send_index;
        echo_client->aio_communicate("shang_de_hao_from_client_side_yyyyyyyyyyyyyyyy_over");
        ++send_index;
        echo_client->aio_communicate("shang_de_hao_from_client_side_zzzzzzzzzzzzzzzz_over");
        ++send_index;
        echo_client->aio_communicate("shang_de_hao_from_client_side_cccccccccccccccc_over");
        ++send_index;
        echo_client->aio_communicate("shang_de_hao_from_client_side_kkkkkkkkkkkkkkkk_over");
    }

    sleep(3);

    std::cout<<"+++++++++++++++++++++++++++++++"<<std::endl;

    if(send_index == receive_index){
        cout<<"send_index==receive_index"<<endl;
    }else{
        cout<<"send_index!=receive_index !!!!!!!!!!!!!!!!!!!!!"<<endl;
    }
    cout<<send_index<<endl;
    cout<<receive_index<<endl;


    cout<<"1000s, then over"<<endl;

    sleep(1000);

    return 1;
}
