#ifndef ASIO_ACCEPTOR
#define ASIO_ACCEPTOR
#include <atomic>
#include "./common/networking_common.h"
#include "./common/asio_session.h"
#include "./common/io_pool.h"

namespace hdcs {
namespace networking{

using boost::asio::ip::tcp;

class asio_acceptor{
public:
    asio_acceptor(std::string ip_address, std::string port_num, SessionSet& _set, int s_num, int thd_num)
        : _io_service_pool( s_num, thd_num )
        , m_endpoint( boost::asio::ip::address::from_string(ip_address), stoi(port_num) )
        , m_acceptor(_io_service_pool.get_io_service()) 
        , is_closed(true)
        , session_set(_set){
    }

    ~asio_acceptor(){
        close();
    }

    void close() {
        if (is_closed.load()){
            return;
        }
        is_closed.store(true);
        boost::system::error_code ec;
        m_acceptor.cancel(ec);
        m_acceptor.close(ec);
    }

    bool start( ProcessMsg _process_msg ){
        process_msg = _process_msg; 
        boost::system::error_code ec;
        m_acceptor.open(m_endpoint.protocol(), ec);
        if (ec){
            std::cout<< "start_listen(): open acceptor failed: "<<ec.message()<<std::endl;
            return false;
        }

        int ret = fcntl(m_acceptor.native(), F_SETFD, fcntl(m_acceptor.native(), F_GETFD) | FD_CLOEXEC);
        if (ret < 0){
            std::cout<< "start_listen(): make fd close_on_exec failed: "<<ec.message()<<std::endl;
            return false;
        }

        m_acceptor.set_option(tcp::acceptor::reuse_address(true), ec);
        if (ec){
            std::cout<< "start_listen(): set acceptor option failed: "<<ec.message()<<std::endl;
            return false;
        }

        m_acceptor.bind(m_endpoint, ec);
        if (ec){
            std::cout<< "start_listen(): bind acceptor failed: "<<ec.message()<<std::endl;
            return false;
        }

        m_acceptor.listen(boost::asio::socket_base::max_connections, ec);
        if (ec){
            std::cout<< "start_listen(): listen acceptor failed: "<<ec.message()<<std::endl;
            return false;
        }
        is_closed.store(false);
        async_accept(); 
        return true;
    }

    void run(){
        _io_service_pool.run();
    }

private:

    void async_accept() {
        SessionPtr new_session_ptr = new asio_session(_io_service_pool.get_io_service(), 1);
        m_acceptor.async_accept(new_session_ptr->get_stream(), boost::bind(
                    &asio_acceptor::on_accept, this, new_session_ptr, _1));
    }

    void on_accept(const SessionPtr new_session_ptr, const boost::system::error_code& ec){
        if (is_closed.load()){
            return;
        }
        std::cout<<"Networking: New session have been created, and session ID is: "<<new_session_ptr<<std::endl;

        if (ec){
            std::cout<< "start_listen(): async_acceptor failed: "<<ec.message()<<std::endl;
            close();
        }else{
            session_set.insert(new_session_ptr);
            new_session_ptr->set_option();
            new_session_ptr->start( process_msg ); // a session start to run.
            async_accept(); 
        }
    }

private:
    SessionSet& session_set;
    io_service_pool _io_service_pool;
    tcp::endpoint m_endpoint;
    tcp::acceptor m_acceptor;
    std::atomic<bool> is_closed;
    ProcessMsg process_msg;
}; 
} 
}
#endif 

