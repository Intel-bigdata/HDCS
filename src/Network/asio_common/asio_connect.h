#ifndef ASIO_CONNECT
#define ASIO_CONNECT
#include "asio_session.h"
#include "../io_service/io_service_pool.h"
namespace hdcs{
namespace networking{

class AsioConnect{
public:
    AsioConnect(int _ios_num, int _thd_num_of_one_ios)
        : m_io_service_pool(_ios_num, _thd_num_of_one_ios)
    {
        m_io_service_pool.async_run();
    }

    ~AsioConnect(){
        close();
    }

    int async_connect( std::string ip_address, std::string port ){
        // TODO
        return 1;
    }

    void close(){
        m_io_service_pool.stop();
    }

    SessionPtr sync_connect(std::string ip_address, std::string port, ProcessMsgClient _process_msg){
       if(true){
           asio_session* new_session;
           new_session = new asio_session(m_io_service_pool.get_io_service(), 0);
           int ret;
           ret = new_session->sync_connection(ip_address, port, _process_msg);
           if(ret != 0){
               std::cout<<"connect:: new_session->sync_connection failed."<<std::endl;
               return NULL; // connect failed
           }
           return (Session*)new_session;
       }else{
           /*
            rdma_session new_session = new rdma_session();
            new_session->sync_connection();
            new_session->sync_connect(ip_address, port);
            return (Session*)new_session;
           */
       }
    }
private:

    io_service_pool m_io_service_pool;
};
}
}
#endif
