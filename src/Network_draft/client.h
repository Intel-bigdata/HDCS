#ifndef CLIENT 
#define CLIENT 
#include <mutex>
#include <string>
#include <thread>
#include <map>
#include <vector>
#include <atomic>
#include <memory>
#include <atomic>
#include "connect.h"

namespace hdcs{
namespace networking{
class Client{   
private:

    std::vector<SessionPtr> session_vec;
    std::atomic<int> session_index;
    std::atomic<bool> is_closed;
    Connect connect;
    ProcessMsgClient process_msg;
    void* process_msg_arg;
    int session_num;
    std::mutex receive_lock;
    bool is_begin_aio;
public:

    Client( ProcessMsgClient _process_msg , int _s_num, int _thd_num)
        : session_index(0)
        , session_num(_s_num)
        , process_msg(_process_msg)
        , is_begin_aio(false)
        , connect(_s_num, _thd_num)
        , is_closed(true)
    {}

    ~Client(){
        if(!is_closed.load()){
            close();
        }
    }

    void close() {
        for(int i = 0 ; i < session_vec.size(); ++i){
            session_vec[i]->close();
            delete session_vec[i];
        }
        is_closed.store(true);
    }

    void cancel(){
        for(int i = 0; i < session_vec.size(); ++i){
            session_vec[i]->cancel();
        }
        sleep(1);
    }
    
    void set_session_arg(void* arg){
        process_msg_arg = arg; 
        for(int i = 0; i < session_vec.size(); ++i){
            session_vec[i]->set_session_arg(arg);
        }
    }

    int sync_connect( std::string ip_address, std::string port){
        SessionPtr new_session;
        for(int i=0; i < session_num; i++){
            new_session = connect.sync_connect( ip_address, port );
            if(new_session != NULL){
                //std::cout<<"Networking::Client::sync_connect successed. Session ID is : "<<
                 //   new_session<<std::endl;
                session_vec.push_back( new_session );
            }else{
                std::cout<<"Client::sync_connect failed.."<<std::endl;
            }
        }
        if( session_vec.size() == session_num ){
            std::cout<<"Networking: "<<session_vec.size()<<" sessions have been created..."<<std::endl;
            is_closed.store(true);
        }else{
            assert(0);
        }
        return 0;
    }

    // namely, Communicate of HDCS
    ssize_t sync_process( std::string send_buffer){
        ssize_t ret;
        ret = session_vec[session_index]->sync_send(send_buffer);
        if( ret < 0 ){
            std::cout<<"Client::sync_process: sync_send failed. "<<std::endl;
            assert(0);
            return -1;
        }
        // use "response" to return received msg.
        char* response; 
        ret = session_vec[session_index]->sync_receive( response );
        if(ret<0){
            std::cout<<"Client::sync_process: sync_receive failed. "<<std::endl;
            assert(0);
            return -1;
        }
        // namely, execute handle_request of HDCS.
        process_msg( process_msg_arg, response); ////!!!

        delete[] response;

        if(++session_index >= session_vec.size()){
            session_index = 0;
        }
        return ret;
    }

    // namely, aio_communicate of HDCS
    int async_process( std::string send_buffer ){

        std::shared_ptr<aio_complete> onfinish(new aio_client_send_complete(
                    this, session_vec[session_index], &Client::on_send_handle));

        //select_idle_session()->async_send( send_buffer, onfinish );
        session_vec[session_index]->async_send( send_buffer, onfinish );
        if(++session_index==session_vec.size()){
            session_index = 0;
        }
        return 0;
    }

    ssize_t communicate(std::string send_buffer){
        if(is_begin_aio){
           cancel();
           is_begin_aio = false;
        }
        return sync_process(send_buffer);
    }

   void aio_receive( ProcessMsgClient _process_msg ){
       for( int i=0; i<session_vec.size(); i++){
	   session_vec[i]->aio_receive(_process_msg);
       }
   }

   // just call async_send of session.
    void aio_communicate(std::string send_buffer){
        if(!is_begin_aio){
           aio_receive( process_msg );
           is_begin_aio=true;
	}
        session_vec[session_index]->aio_communicate( send_buffer );
        if(++session_index==session_vec.size()){
            session_index = 0;
        }
    }

private:

    // callback function of session->async_send().
    void on_send_handle(int error_code, SessionPtr session_id, void* arg){
        if( !error_code ){
            std::shared_ptr<aio_complete> onfinish(new aio_client_receive_complete(
                    this, session_id, &Client::on_receive_handle));
            receive_lock.lock();
            char* msg;
            // receive ack/answer from server.
            session_id->async_receive(msg, onfinish);
        }else{
            std::cout<<"client::on_send_handle, async_send failed.."<<std::endl;
            assert(0);
        }
    }

    // callback function of session->async_receive().
    void on_receive_handle(int error_code, Session* session_id, char* msg, uint64_t _byte_num){
        receive_lock.unlock();
        if(!error_code){
            // namely, handle_request of HDCS.
            process_msg( process_msg_arg, msg );
            delete[] msg;
        }else{
            std::cout<<"client::on_receive_handle, async_receive failed.."<<std::endl;
            assert(0);
        }
    }

    Session* select_idle_session(){
        while(true){
            for( int i = 0; i<session_vec.size(); i++){
                if(!(session_vec[i]->if_busy())){
                    std::cout<<"select session id is "<<i<<std::endl;
                    session_vec[i]->set_busy();
                    return session_vec[i];
                }
            }
        }
    }

};



class Connection {
public:
    Connection( ProcessMsgClient task, int s_num, int thd_num )
        :connection_impl(task, s_num, thd_num){
    }

    ~Connection(){
    }

    void close(){
       connection_impl.close();
    }

    void start(){
    }

    void wait(){
    }

    void connect(std::string ip, std::string port){
        connection_impl.sync_connect(ip, port);
    }

    void set_session_arg(void* arg ){
        connection_impl.set_session_arg(arg);
    }

    int aio_communicate( std::string send_buffer ){
        //connection_impl.async_process( send_buffer ); // implement 1
        connection_impl.aio_communicate(send_buffer);   // implement 2
        return 1;
    }

    ssize_t communicate( std::string send_buffer ){
        //connection_impl.async_process( send_buffer );  // implement 1
        return connection_impl.communicate(send_buffer); // implement 2
    }

private:
    Client connection_impl;
    
}; // connection

} //hdcs
}
#endif
