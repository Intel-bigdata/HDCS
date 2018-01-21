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
    
    struct sockaddr_in sin;
    SessionSet& session_set;
    bool is_sync;
    bool is_stop;
    WaitEvent wait_event;


public:
    rdma_acceptor(std::string ip_address, std::string port_num, SessionSet& ss)
        : session_set(ss)
        , is_sync(true)
        , is_stop(true)
    {
        sin.sin_family = AF_INET;
        sin.sin_port = htons(stoi(port_num));
        //sin.sin_port = htons(20082);
        // for server, ip_address will be directly igored.
        sin.sin_addr.s_addr = INADDR_ANY; 
        //std::cout<<"rdma_acceptor construnction success..."<<std::endl;
    }

    ~rdma_acceptor(){
        stop();
    }

    void stop(){
        if(is_stop){
            return;
        }
        if(!is_sync){
            return;
        }
        wait_event.Signal();
    }

    bool start(ProcessMsg _process_msg){
    
        //std::cout<<"rdma_acceptor-->start..."<<std::endl;
        rdma_server_ptr.reset(new RDMAServer((struct sockaddr*)&sin));
        internal_on_accept_ptr.reset(new InternalOnAccept(session_set)); 
        internal_process_msg_ptr.reset(new InternalProcessMsg(_process_msg, 0));


        internal_on_accept_ptr->set_process_msg(internal_process_msg_ptr);

        std::cout<<"rdma_server-->start"<<std::endl;
        rdma_server_ptr->start(internal_on_accept_ptr.get());
        std::cout<<"rdma_acceptor start success..."<<std::endl;
    }


    void sync_run(){
        is_stop = false;
        is_sync = true;
        std::cout<<"rdma_acceptor wait...."<<std::endl;
        wait_event.Wait(); 
        std::cout<<"rdma_acceptor wait ending..."<<std::endl;
    }

    void async_run(){
        is_stop = false;
        is_sync = false;
        // TODO
    }

};//rdma_acceptor

}//networking
}// hdcs

#endif
