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
#include "../common/option.h"

// wraper of rdma_messenger --sdh
namespace hdcs{
namespace networking{

class RDMAConnect{
public:
    RDMAConnect(const ClientOptions& _co, std::vector<Session*>& _s_v )
        : new_session(NULL)
        , client_options(_co)
        , rdma_client_ptr(NULL)
        , internal_on_connection_ptr(NULL)
        , internal_process_msg_ptr(NULL)
        , session_vec(_s_v)
    {
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = 0;
        hints.ai_protocol = 0;   
        //std::cout<<"RDMAConnect construction success...."<<std::endl;
    }

    ~RDMAConnect()
    {
        close();
        rdma_client_ptr.reset();
        internal_on_connection_ptr.reset();
        internal_process_msg_ptr.reset();
    }

    void close(){
       // TODO close RDMAClient
    }

    int sync_connect(std::string ip_address, std::string port_num)
    {
        if(rdma_client_ptr == NULL)
        {
            to_rdma_address(ip_address, port_num);
            rdma_client_ptr.reset(new RDMAClient(res->ai_addr, client_options._session_num));
            internal_on_connection_ptr.reset(new InternalOnConnection(session_vec, client_options._session_num, wait_event));
            internal_process_msg_ptr.reset(new InternalProcessMsg(client_options._process_msg, 1));
            internal_process_msg_ptr->set_hdcs_arg(client_options._process_msg_arg);
            internal_on_connection_ptr->set_process_msg(internal_process_msg_ptr);
        }
        // Attention: !!!!!
        // Althought 'connect' just be called one time, but internal_on_connection will be repeatedly called. 
        // (namely, client_option._session_num times)
        rdma_client_ptr->connect(internal_on_connection_ptr.get());
        //sleep(4); // TODO TODO TODO
        wait_event.Wait();
        if(session_vec.size() == client_options._session_num)
        {
            std::cout<<"Networking: RDMA communication: "<<client_options._session_num<<" sessions have been created."<<std::endl;
        }
        else
        {
            std::cout<<"Networking: RDMA communication: connect opearion failed."<<std::endl;
            assert(0);
        }
        // default connection success
        return client_options._session_num;
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
    const ClientOptions& client_options;
    std::vector<Session*>& session_vec;
  
}; //RDMAConnect

} // namespace networking
} // namespace hdcs

#endif
