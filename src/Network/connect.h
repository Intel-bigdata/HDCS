#ifndef CONNECT
#define CONNECT

#include "asio_common/asio_connect.h"
#include "rdma_common/rdma_connect.h"
#include "common/option.h"

namespace hdcs{
namespace networking{

class Connect{
private:
    std::shared_ptr<AsioConnect> asio_connect_ptr;
    std::shared_ptr<RDMAConnect> rdma_connect_ptr;
    const ClientOptions& client_options;

public:
    // TODO rmda and tcp use the same thread pool 
    Connect(const ClientOptions& _co)
       : asio_connect_ptr(NULL)
       , rdma_connect_ptr(NULL)
       , client_options(_co) 
    {}

    ~Connect()
    {
        close();
        asio_connect_ptr.reset();
        rdma_connect_ptr.reset();
    }

    void close()
    {
        if(asio_connect_ptr != NULL)
        {
            asio_connect_ptr->close();
        }
        if(rdma_connect_ptr != NULL)
        {
            rdma_connect_ptr->close();
        }
    }

    int async_connect( std::string ip_address, std::string port , std::vector<Session*>& _session_vec, int type /* connection callback */)
    {
        if(false)
        {
            if(asio_connect_ptr == NULL)
            {
                asio_connect_ptr.reset(new AsioConnect(client_options, _session_vec));
            }
            // TODO
            //asio_connect_ptr->async_connect(ip_address, port, connection_callback);
        }
        if(true)
        {
            if(rdma_connect_ptr == NULL)
            {
                //rdma_connect_ptr.reset(new RDMAConnect(client_options));
            }
            // TODO
            //rdma_connect_ptr->async_connect(ip_address, port, connection_callback);
        }
        return 1;
    }

    int sync_connect(std::string ip_address, std::string port, std::vector<Session*>& _session_vec, int _type)
    {
        if(_type == 0)
        {
            if(asio_connect_ptr == NULL)
            {
                asio_connect_ptr.reset(new AsioConnect(client_options, _session_vec));
            }
            return asio_connect_ptr->sync_connect(ip_address, port);
        }
        if(_type == 1)
        {
            if(rdma_connect_ptr == NULL)
            {
                rdma_connect_ptr.reset(new RDMAConnect(client_options, _session_vec));
            }
            return rdma_connect_ptr->sync_connect(ip_address, port);
        }
        if(_type == 2)
        {
            // TODO local communication
        }
    }

}; //connect

}//namespace networking
}//namespace hdcs
#endif
