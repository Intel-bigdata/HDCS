#ifndef ASIO_SESSION
#define ASIO_SESSION
#include <cstdlib>
#include <iostream>
#include <memory>
#include "../common/networking_common.h"
#include "../common/session.h"
#include "asio_messenger.h"
#define SESSION_TIMEOUT 30

namespace hdcs{
namespace networking{

class asio_session: public Session{
private:
    std::shared_ptr<asio_messenger> m_messenger;
    ProcessMsg process_msg;
    std::atomic<bool> session_work;
    boost::posix_time::ptime last_active;
    int role;
    SessionArg* session_arg_ptr;

public:
    asio_session( IOService& _io_service , int _role) 
        : m_messenger(new asio_messenger( _io_service, _role ))
        , role(_role) 
        , session_arg_ptr(NULL) 
    {}

    ~asio_session(){
        stop();
        if(role==1 && session_arg_ptr!=NULL){
            delete session_arg_ptr;
        }
    }

    void stop(){
        m_messenger->close();
    }

    void cancel(){
        m_messenger->cancel();
    }

     // called by server.
    void set_option(){
        m_messenger->set_socket_option();
    }       

    void set_session_arg(void* _arg){
        m_messenger->set_callback_arg(_arg);
    }

    bool start( ProcessMsg _process_msg ){
        m_messenger->set_server_process_msg(_process_msg);
        session_arg_ptr = new SessionArg((void*)this);
	m_messenger->aio_receive(session_arg_ptr);
        return true;
    }

    int sync_connection( std::string ip_address, std::string port, ProcessMsgClient _process_msg){
        return m_messenger->sync_connection(ip_address, port, _process_msg);
    }
   
    int async_connection(){
        // TODO
        return 1;
    }

    int async_send(std::string send_buffer, uint64_t _seq_id){
       return m_messenger->async_send(send_buffer, _seq_id);
    }

    void update_time(){
        last_active = boost::posix_time::microsec_clock::local_time();
    }

    bool if_timeout(){
        boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
        return ((now - last_active).total_milliseconds() > SESSION_TIMEOUT)?true:false;
    }

    // called by client
    ssize_t communicate(std::string send_buffer, uint64_t _seq_id){
        return m_messenger->communicate(send_buffer, _seq_id);
    }

    // called by client
    void aio_communicate(std::string& send_buffer, uint64_t _seq_id){
        m_messenger->aio_communicate(send_buffer, _seq_id);
    }

    boost::asio::ip::tcp::socket& get_stream(){
        return m_messenger->get_socket();
    }

    COMMUNICATION_TYPE communication_type(){
        return COMMUNICATION_TYPE(0);
    }
    
};
}
}
#endif

