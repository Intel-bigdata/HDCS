#ifndef ASIO_SESSION
#define ASIO_SESSION
#include <cstdlib>
#include <iostream>
#include <memory>
#include "networking_common.h"
#include "session.h"
#include "asio_messenger.h"
#include "aio_complete_impl.h"
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
    std::atomic<bool> is_busy;

public:
    asio_session( IOService& _io_service , int _role) 
        : m_messenger(new asio_messenger( _io_service, _role ))
        , role(_role) 
    {  
        is_busy.store(false);
    }

    ~asio_session(){
        close();
    }

    void close(){
        m_messenger->close();
    }

    void cancel(){
        m_messenger->cancel();
    }

    void set_option(){
        m_messenger->set_socket_option();
    }       

    void set_session_arg(void* _arg){
        m_messenger->set_callback_arg(_arg);
    }

    /********** will be called by server. There have two methods to implemnet.**********/
/*
    bool start( ProcessMsg _process_msg ){
        std::shared_ptr<aio_complete> onfinish(new aio_session_start_complete(
                    this, _process_msg));
        char* receive_buffer;
        async_receive(receive_buffer, onfinish);
    }
*/


    bool start( ProcessMsg _process_msg ){
	m_messenger->aio_receive((void*)this, _process_msg);
        return true;
    }

   /**************connection***************/ 

    int sync_connection( std::string ip_address, std::string port){
        return m_messenger->sync_connection(ip_address, port);
    }
   
    int async_connection( std::string ip_address, short port, std::shared_ptr<aio_complete> _onfinish){
        return 1;
    }

   /**************Send*******************/ 
    
    ssize_t sync_send(std::string send_buffer){
        return m_messenger->sync_send(send_buffer);
    }

    int async_send(std::string send_buffer, std::shared_ptr<aio_complete> _onfinish){
        std::shared_ptr<aio_complete> onfinish( new aio_session_send_complete( _onfinish ) );
        m_messenger->async_send(send_buffer, _onfinish);
        return 0;
    }
   
    int async_send(std::string send_buffer){
       return m_messenger->async_send(send_buffer);
    }

   /*************Recieve********************/ 

    ssize_t sync_receive( char* &receive_buffer ){
        return m_messenger->sync_receive( receive_buffer );
    }

    int async_receive( char* receive_buffer, std::shared_ptr<aio_complete> _onfinish ){ 
        //std::shared_ptr<aio_complete> onfinish(new aio_session_receive_complete(_onfinish));
        std::shared_ptr<aio_complete> onfinish(new aio_session_receive_complete(
                    _onfinish ));
        m_messenger->async_receive( receive_buffer, onfinish);
        return 0;
    }

   /*********************************/ 

    void update_time(){
        last_active = boost::posix_time::microsec_clock::local_time();
    }

    bool if_timeout(){
        boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
        return ((now - last_active).total_milliseconds() > SESSION_TIMEOUT)?true:false;
    }

    bool if_session_work(){
        return session_work;
    }
	
    bool set_session_work(bool temp){
        session_work=temp;
    }

    void set_busy(){
        is_busy.store(true);
    }

    void set_idle(){
        is_busy.store(false);
    }

    bool if_busy(){
        return is_busy.load();
    }

    boost::asio::ip::tcp::socket& get_stream(){
        return m_messenger->get_socket();
    }

    /***********************************************************/

    ssize_t communicate(std::string send_buffer){
        return m_messenger->communicate(send_buffer);
    }

    void aio_communicate(std::string send_buffer){
        m_messenger->aio_communicate(send_buffer);
    }
    
    void aio_receive( ProcessMsgClient _process_msg){
       m_messenger->aio_receive( _process_msg );
    }


};
}
}
#endif

