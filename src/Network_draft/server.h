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
        //async_send_1(session_id, send_buffer, _callback); 
        async_send_2(session_id, send_buffer);   
    }
    // implement 1
    void async_send_1 (void* session_id, std::string send_buffer, OnSentServer _callback=NULL){
        session_id = (Session*)session_id;
        if(session_set.find((Session*)session_id)==session_set.end()){
            std::cout<<"fails : find session_id. Maybe need to re-connction "<<std::endl;
        }
        std::shared_ptr<aio_complete> onfinish;
        if( _callback==NULL){
            onfinish.reset(new aio_server_send_complete(
                        this, &server::on_sent_default, send_buffer.size()));
        }else{
            onfinish.reset(new aio_server_send_complete(_callback, send_buffer.size()));
        }
        ((Session*)session_id)->async_send(send_buffer, onfinish);
    }
    // implement 2 
    void async_send_2(void* session_id, std::string send_buffer ){
        if(session_set.find((Session*)session_id)==session_set.end()){
            std::cout<<"Networking::server: finding session_id failed. Maybe need to re-connction "<<std::endl;
            assert(0);
        }
        ((Session*)session_id)->async_send(send_buffer);
    }

    
private:
    // callback of async_send_1
    void on_sent_default(int error_code, uint64_t byte_num ,void* arg){
        if(!error_code){
        }else{
            std::cout<<"server::on_sent_default: send failed. "<<std::endl;
        }
    }

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
