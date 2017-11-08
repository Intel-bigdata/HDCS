#ifndef CONNECT
#define CONNECT
#include "./common/asio_session.h"
#include "./common/io_pool.h"
namespace hdcs{
namespace networking{

class Connect{
public:
    Connect(int _ios_num, int _thd_num_of_one_ios)
        : m_io_service_pool(_ios_num, _thd_num_of_one_ios){
        m_io_service_pool.run(false);
    }

    ~Connect(){
    }

    int async_connect( std::string ip_address, std::string port ){
        return 1;
    }

    SessionPtr sync_connect(std::string ip_address, std::string port){
       if(true){
           asio_session* new_session;
           new_session = new asio_session(m_io_service_pool.get_io_service(), 0);
           int ret;
           ret = new_session->sync_connection(ip_address, port);
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
