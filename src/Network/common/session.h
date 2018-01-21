#ifndef SESSION
#define SESSION

#include "networking_common.h"

namespace hdcs{
namespace networking{

class Session{
public:
    enum COMMUNICATION_TYPR{
        TCP_CONN = 0,
        RDMA_CONN = 1,
        // LOCAL_CONN,
    };
    Session(){}
    virtual ~Session(){}
    virtual bool start( ProcessMsg )=0;
    virtual void stop()=0;
    virtual void cancel()=0;
    virtual void set_session_arg(void*)=0;
    virtual int async_send( std::string, uint64_t )=0;
    virtual bool if_timeout()=0;
    virtual void set_option()=0;
    virtual ssize_t communicate(std::string, uint64_t)=0;
    virtual void aio_communicate(std::string&, uint64_t)=0;
    virtual COMMUNICATION_TYPE communication_type()=0;
};

}
}
#endif
