#ifndef ASIO_ACCEPTOR
#define ASIO_ACCEPTOR
#include <atomic>
#include "../common/networking_common.h"
#include "asio_session.h"
#include "../io_service/io_service_pool.h"

namespace hdcs {
namespace networking{

using boost::asio::ip::tcp;

class asio_acceptor{
public:
    asio_acceptor(std::string ip_address, std::string port_num, SessionSet& _set, int s_num, int thd_num)
        : _io_service_pool( s_num, thd_num )
        , m_endpoint( boost::asio::ip::address::from_string(ip_address), stoi(port_num) )
        , m_acceptor(_io_service_pool.get_io_service()) 
        , is_closed(false)
        , session_set(_set){
    }

    ~asio_acceptor(){
        stop();
    }

    void stop() {
        if (is_closed.load()){
            return;
        }
        is_closed.store(true);
        _io_service_pool.stop();
        boost::system::error_code ec;
        m_acceptor.cancel(ec);
        if(ec){
            std::cout<<"asio_acceptor::stop(): cancel failed "<<ec.message()<<std::endl;
        }
        m_acceptor.close(ec);
        if(ec){
            std::cout<<"asio_acceptor::stop(): close failed "<<ec.message()<<std::endl;
        }

    }

    bool start( ProcessMsg _process_msg ){
        process_msg = _process_msg; 
        boost::system::error_code ec;
        m_acceptor.open(m_endpoint.protocol(), ec);
        if (ec){
            std::cout<< "start_listen(): open acceptor failed: "<<ec.message()<<std::endl;
            return false;
        }
/*
        int ret = fcntl(m_acceptor.native(), F_SETFD, fcntl(m_acceptor.native(), F_GETFD) | FD_CLOEXEC);
        if (ret < 0){
            std::cout<< "start_listen(): make fd close_on_exec failed: "<<ec.message()<<std::endl;
            return false;
        }
*/
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

    void sync_run(){
        _io_service_pool.sync_run();
    }

    void async_run(){
        _io_service_pool.async_run();
    }

private:

    void async_accept() {
        //SessionPtr new_session_ptr = new asio_session(_io_service_pool.get_io_service(), 1);
        asio_session* new_session_ptr = new asio_session(_io_service_pool.get_io_service(), 1); // create new session.
        m_acceptor.async_accept(new_session_ptr->get_stream(), boost::bind(
                    &asio_acceptor::on_accept, this, new_session_ptr, _1));
    }

    void on_accept(const SessionPtr new_session_ptr, const boost::system::error_code& ec){
        // when having too many open file, should re-start listen...TODO ref baidu
        if (is_closed.load()){
            return;
        }
        if (ec){
            std::cout<< "start_listen(): async_acceptor failed: "<<ec.message()<<std::endl;
            stop();
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

