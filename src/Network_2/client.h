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
class Connection{   
private:

    std::vector<SessionPtr> session_vec;
    std::atomic<int> session_index;
    std::atomic<bool> is_closed;
    Connect m_connect;
    ProcessMsgClient process_msg;
    void* process_msg_arg;
    int session_num;
    std::mutex receive_lock;
    bool is_begin_aio;
public:

    Connection( ProcessMsgClient _process_msg , int _s_num, int _thd_num)
        : session_index(0)
        , session_num(_s_num)
        , process_msg(_process_msg)
        , is_begin_aio(false)
        , m_connect(_s_num, _thd_num)
        , is_closed(true)
    {}

    ~Connection(){
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

    int connect( std::string ip_address, std::string port){
        SessionPtr new_session;
        for(int i=0; i < session_num; i++){
            new_session = m_connect.sync_connect( ip_address, port );
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

    ssize_t communicate( std::string send_buffer){
        if(is_begin_aio){
            cancel();
            is_begin_aio = false;
        }
        ssize_t ret = session_vec[session_index]->communicate(send_buffer, process_msg);
        if(++session_index >= session_vec.size()){
            session_index = 0;
        }
        return ret;
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
} //hdcs
}
#endif
