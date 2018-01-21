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
    std::cout<<"main_client: handle_request...."<<std::endl;
    std::cout<<"ack is "<<receive_buffer<<std::endl;
    /*
    receive_lock.lock();
    receive_index++;
    receive_lock.unlock();
    //if(receive_index%100000==0){
    // just print 35 bytes of every message.
        cout<<"*echo client* ACK ID is : "<<receive_index<<" . Msg is : "<<string(receive_buffer.begin(),receive_buffer.begin()+35)<<endl;
    }
    // print complete message.  
    if( receive_index%1000000==0){
        cout<<endl;
        cout<<endl;
        cout<<endl;
        cout<<"========================="<<endl;
        cout<<endl;
        cout<<receive_buffer<<endl;
        cout<<endl;
        cout<<"========================="<<endl;
        cout<<endl;
        cout<<endl;
        cout<<endl;
        cout<<endl;
   }
   */
   return 1;
}
}//networking
} //hdcs

/* hdcs::networking::client have four interface.
 *
 * interface 1 : constrction function.
 * interface 2 : connect 
 * interface 3 : communicate
 * interface 4 : aio_communicate
 */
class test_class{
public:
    test_class( int session_num, int thd_num){
        echo_client = new Connection( 
                [this](void* s, string p){handle_request(s,p);},
                session_num, 
                thd_num);
    }

    ~test_class(){
    }
   
    void sync_send_request(uint64_t send_times, string& send_buffer){
        cout<<" ======sync communicate ======="<<endl;
        for(uint64_t i=0; i<send_times; i++ ){
            send_lock.lock();
            send_index++;
            send_lock.unlock();
            if(i%100000==0){
                cout<<"Now, sync sending the "<<i<<"-th msg"<<endl;
            }
            // interface 2
            echo_client->communicate( send_buffer );
        }
        sleep(1);
        cout<<" ======sync communicate over======="<<endl;
    } 

    void async_send_request(uint64_t send_times, int qd, string& send_buffer){
        cout<<" ======async communicate ======="<<endl;
        for(uint64_t i=0; i<send_times; i++ ){
            send_lock.lock();
            send_index++;
            send_lock.unlock();
            // simple queue depth 
            while( (send_index-receive_index) > qd ){
                //cout<<"waiting....... "<<qd<<endl;
            }
            echo_client->aio_communicate( std::move(send_buffer) );
        }
        sleep(1);
        cout<<" ======async communicate over======="<<endl;
    }

    void run(uint64_t send_times, uint64_t qd){
        string ip_address("127.0.0.1");
        string port("6666");
        cout<<"begin sync connection "<<endl;
        echo_client->connect(ip_address, port);
        std::cout<<"main_client: connection sucess.........."<<std::endl;

        string send_buffer("this is a test program : i'm client"); // need to live out async send.
        //the size of message is 4096 bytes.
        while(send_buffer.size()<4096){
	   send_buffer.push_back('a');
        }

        async_send_request(1,32, send_buffer);

        std::cout<<"main_client: async_send success"<<std::endl;

        /*
        sync_send_request(500000, send_buffer);
        async_send_request(5000000,64,send_buffer);
        sync_send_request(500000, send_buffer);
        async_send_request(5000000,64,send_buffer);
        sync_send_request(500000,send_buffer);
        async_send_request(5000000,64, send_buffer);
        sync_send_request(500000,send_buffer);
        async_send_request(5000000,64, send_buffer);
        sync_send_request(500000,send_buffer);
        async_send_request(5000000,64, send_buffer);
        */
        sleep(2);
        cout<<endl;
        cout<<endl;
        cout<<endl;
        cout<<endl;
        cout<<"request operation have been sent out...."<<endl;
        cout<<endl;
        cout<<endl;
        cout<<endl;
    }
    void close(){
       echo_client->close();
    }

private:
    Connection* echo_client;
};

int main(){
    //uint64_t send_times = 10000000;
    uint64_t send_times = 100000;
    int session_num = 10;
    int thread_num_of_one_ios = 10; 
    uint64_t qd = 128;

    test_class test(session_num, thread_num_of_one_ios);
    test.run(send_times, qd);

    sleep(10);
    cout<<"close echo_server "<<endl;
    test.close();
    sleep(2);
    cout<<endl;
    cout<<endl;
    cout<<endl;
    cout<<endl;
    cout<<endl;
    cout<<endl;
    cout<<endl;

    cout<<"send request number =============> "<<receive_index<<endl;
    cout<<"receive ack number ==============> "<<send_index<<endl;

    cout<<endl;
    cout<<endl;
    cout<<endl;

    cout<<"====test over===="<<endl;
    cout<<endl;
    cout<<endl;
    cout<<endl;
    cout<<endl;



    return 1;
}
