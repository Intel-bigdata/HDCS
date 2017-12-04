#ifndef DS_SERVER 
#define DS_SERVER

#include <sstream>
#include <map>
#include <vector>
#include <thread>
#include <atomic>
#include "./common/session.h"
#include "./common/networking_common.h"
#include "acceptor.h"

namespace hdcs{
namespace networking{

class server{
private:
    SessionSet session_set; 
    std::unique_ptr<Acceptor> acceptor_ptr;
    std::unique_ptr<std::thread> check_session_thread;
    std::atomic<bool> is_stop;

public:

    server(std::string _ip_address, std::string _port_num, int s_num=10, int thd_num=10): 
        is_stop(false){
        acceptor_ptr.reset(new Acceptor( _ip_address, _port_num, session_set , s_num, thd_num));
        //check_session_thread.reset(new std::thread([this](){loop_check_session();}));
        //check_session_thread->detach();
    }

    ~server(){
        stop();
    }

    void stop(){
        is_stop.store(true);
        acceptor_ptr->close();
        for(auto it=session_set.begin(); it!=session_set.end(); ++it){
            (*it)->close(); 
        }
    }

    void run(){
        acceptor_ptr->run();
    }

    // start listen 
    bool start( ProcessMsg process_msg ){
        acceptor_ptr->start( process_msg );
    }

    //this is async send, but hdcs use this interface name.
    void send(void* session_id, std::string send_buffer, OnSentServer _callback=NULL){
        async_send(session_id, send_buffer);   
    }

    void async_send(void* session_id, std::string& send_buffer ){
        if(session_set.find((Session*)session_id)==session_set.end()){
            std::cout<<"Networking::server: finding session_id failed. Maybe need to re-connction "<<std::endl;
            assert(0);
        }
        ((Session*)session_id)->async_send(send_buffer);
    }

    
private:

    void loop_check_session(){
        while(!is_stop.load()){
            for(auto it=session_set.begin(); it!=session_set.end(); ++it){
                if( (!((*it)->if_session_work())) || ( (*it)->if_timeout()) ){
                    std::cout<<"delete session, id is "<<(*it)<<std::endl;
                    (*it)->close();
                    session_set.erase(it);	
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(CHECK_SESSION_INTERVAL));
        }
    }

}; 
}
}
#endif
/*
    asio::error::operation_aborted  -1
    asio::error::connection_aborted -2
    asio::error::connection_reset -3
    asio::error::bad_descriptor -4
    asio::error::interrupted -5
    asio::error::network_down -6
    asio::error::not_connected -7
    asio::error::shut_down -8

*/
