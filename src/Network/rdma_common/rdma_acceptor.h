#ifndef RDMA_ACCEPTOR
#define RDMA_ACCEPTOR

#include "../common/networking_common.h"
#include "rdma_messenger/RDMAServer.h"
#include "rdma_internal_callback.h"

namespace hdcs{
namespace networking{

class rdma_acceptor{
private:
    std::shared_ptr<RDMAServer> rdma_server_ptr;
    std::shared_ptr<InternalOnAccept> internal_on_accept_ptr;
    std::shared_ptr<InternalProcessMsg> internal_process_msg_ptr;
    struct sockaddr_in sin; // rdma DS
    SessionSet& session_set;
    bool is_stop;

public:
    rdma_acceptor(std::string ip_address, std::string port_num, SessionSet& ss)
        : session_set(ss)
        , is_stop(true)
    {
        // for server, ip will be directly igored.
        sin.sin_family = AF_INET;
        sin.sin_port = htons(stoi(port_num));
        sin.sin_addr.s_addr = INADDR_ANY; 
    }

    ~rdma_acceptor()
    {
        stop();
        rdma_server_ptr.reset();
        internal_on_accept_ptr.reset();
        internal_process_msg_ptr.reset();
    }

    void stop()
    {
        if(is_stop){
            return;
        }
        is_stop = true;
    }

    bool start(ProcessMsg _process_msg)
    {
        rdma_server_ptr.reset(new RDMAServer((struct sockaddr*)&sin));
        internal_on_accept_ptr.reset(new InternalOnAccept(session_set)); 
        internal_process_msg_ptr.reset(new InternalProcessMsg(_process_msg, 0));
        internal_on_accept_ptr->set_process_msg(internal_process_msg_ptr);
        rdma_server_ptr->start(internal_on_accept_ptr.get());
        is_stop = false;
    }

    void async_run()
    {
        // TODO
    }

};//rdma_acceptor

}//networking
}// hdcs

#endif
