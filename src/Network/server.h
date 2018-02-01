#ifndef SERVER_H
#define SERVER_H

#include <sstream>
#include <map>
#include <vector>
#include <thread>
#include <atomic>

#include "common/session.h"
#include "common/networking_common.h"
#include "common/option.h"
#include "acceptor.h"

namespace hdcs{
namespace networking{

class server{
private:
    SessionSet session_set; 
    std::unique_ptr<Acceptor> acceptor_ptr;
    std::atomic<bool> is_stop;
    ServerOptions server_options;

public:

    // this construction: just support one communication type
    // old interface.
    server(ProcessMsg _process_msg, std::string ip_address, std::string port_num, int s_num=10, int thd_num=10, int _t = 0)
        : is_stop(false)
    {
        server_options._io_service_num = s_num;
        server_options._session_num = s_num;
        server_options._thd_num_on_one_session = thd_num;
        server_options._port_num_vec.push_back(port_num);
        if(_t == 1)
        {
            server_options._communication_type_vec.push_back(RDMA_COMMUNICATION);
        }
        else
        {
            server_options._communication_type_vec.push_back(TCP_COMMUNICATION);
        }
        server_options._process_msg = _process_msg;
        // create acceptor which just support TCP_COMMUNICATION
        acceptor_ptr.reset(new Acceptor(server_options, session_set));
    }

    // totally depend on server_options to configure server side.
    server(const ServerOptions& _server_options)
        : is_stop(false)
    {
        server_options._io_service_num =   _server_options._io_service_num;
        server_options._session_num = _server_options._session_num;
        server_options._thd_num_on_one_session = _server_options._thd_num_on_one_session;
        server_options._port_num_vec = _server_options._port_num_vec;
        server_options._communication_type_vec = _server_options._communication_type_vec;
        server_options._process_msg = _server_options._process_msg;
        // create acceptor which support muilti-communication accoding to ServerOptions.
        acceptor_ptr.reset(new Acceptor(server_options,session_set));
    }

    ~server()
    {
        stop();
    }

    void stop(){
        if(is_stop.load())
        {
            return;
        }
        for(auto it = session_set.begin(); it != session_set.end(); ++it)
        {
            (*it)->stop(); 
            session_set.erase(it);
        }
        acceptor_ptr->stop();
        is_stop.store(true);
    }

    void sync_run()
    {
        acceptor_ptr->sync_run();
    }

    void async_run()
    {
        acceptor_ptr->async_run();
    }

    // start listen 
    bool start()
    {
        acceptor_ptr->start();
    }

    //this is async send, but hdcs use this interface name.
    void send(void* session_arg, std::string send_buffer, OnSentServer _callback = NULL)
    {
        async_send(session_arg, send_buffer);   
    }

    void async_send(void* session_arg, std::string& send_buffer )
    {
        Session* temp_s_id = (Session*)(((SessionArg*)session_arg)->get_session_id());
        if(session_set.find(temp_s_id)==session_set.end()){
            std::cout<<"Networking::server: finding session_id failed. Maybe need to re-connction "<<std::endl;
            //assert(0);
        }
        temp_s_id->async_send(send_buffer, ((SessionArg*)session_arg)->get_seq_id());
        delete (SessionArg*)session_arg;
    }
}; 

}// networking
}// hdcs
#endif
