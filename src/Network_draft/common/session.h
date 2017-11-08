#ifndef SESSION
#define SESSION

#include "networking_common.h"

#include "aio_complete.h"


namespace hdcs{
namespace networking{

class Session{
public:
    Session(){}
    virtual ~Session(){}

    virtual bool start( ProcessMsg )=0;
    virtual void close()=0;
    virtual void cancel()=0;
    virtual void set_session_arg(void*)=0;
    
    virtual int async_send( std::string, std::shared_ptr<aio_complete> )=0;
    virtual int async_send( std::string )=0;
    virtual int async_receive(char*, std::shared_ptr<aio_complete> )=0;

    virtual ssize_t sync_send(std::string)=0;
    virtual ssize_t sync_receive( char*& )=0;

    virtual bool if_session_work()=0;
    virtual bool if_timeout()=0;
    virtual boost::asio::ip::tcp::socket&  get_stream()=0;

    virtual void set_busy()=0;
    virtual void set_idle()=0;
    virtual bool if_busy()=0;

    virtual void set_option()=0;
    
//
    virtual ssize_t communicate(std::string)=0;
    virtual void aio_communicate(std::string)=0;
    virtual void aio_receive(ProcessMsgClient)=0; 

};//Session
}
}//dslab

#endif
