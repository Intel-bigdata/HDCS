#ifndef RDMA_CLIENT_MESSENGER
#define RDMA_CLIENR_MESSENGER

#include <mutex>
#include <chrono>
#include <memory>
#include <iostream>
#include <boost/system/error_code.hpp>

#include "rdma_messenger/RDMAClient.h"

#include "rdma_session.h"
#include "rdma_internal_callback.h"
#include "../common/Message.h"
#include "../common/wait_event.h"
#include "../common/networking_common.h"
#include "../common/counter.h"


// wraper of rdma_messenger --sdh
//
namespace hdcs{
namespace networking{

class RDMAConnect{
public:
    RDMAConnect()
        : new_session(NULL)
    {

        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = 0;
        hints.ai_protocol = 0;   
        rdma_client_NULL = true;
        //std::cout<<"RDMAConnect construction success...."<<std::endl;
    }

    ~RDMAConnect(){}

    SessionPtr connect(std::string ip_address, std::string port_num, ProcessMsg _process_msg){
        if(rdma_client_NULL){
            std::cout<<"create RDMAClient...."<<std::endl;
            to_rdma_address(ip_address, port_num);
            rdma_client_ptr.reset(new RDMAClient(res->ai_addr, 10));
            internal_on_connection_ptr.reset(new InternalOnConnection(new_session, wait_event));
            internal_process_msg_ptr.reset(new InternalProcessMsg(_process_msg, 1));
            internal_on_connection_ptr->set_process_msg(internal_process_msg_ptr);
            rdma_client_NULL = false;
            std::cout<<"create RDMAClient over...."<<std::endl;
        }
        std::cout<<"rdmaconnect: connect begin...."<<std::endl;
        rdma_client_ptr->connect(internal_on_connection_ptr.get());
        std::cout<<"call wait_event.wait()"<<std::endl;
        wait_event.Wait();
        // default connection success
        return new_session;
    }  

    void close(){
       // TODO close RDMAClient
    }

    void set_hdcs_arg(void* arg){
        internal_process_msg_ptr->set_hdcs_arg(arg);
    }

private:
    int to_rdma_address(std::string ip_address, std::string port_num ){
        int n = getaddrinfo(ip_address.c_str(), port_num.c_str(), &hints, &res);
        return n<0 ? -1:0;
    }

private:
    std::shared_ptr<RDMAClient> rdma_client_ptr; // connector
    std::shared_ptr<InternalOnConnection> internal_on_connection_ptr;
    std::shared_ptr<InternalProcessMsg> internal_process_msg_ptr;
    struct addrinfo* res;
    struct addrinfo hints;
    WaitEvent wait_event;
    SessionPtr new_session;
    bool rdma_client_NULL;
  
};

}
}

#endif
