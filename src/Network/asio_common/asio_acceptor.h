#ifndef ASIO_ACCEPTOR
#define ASIO_ACCEPTOR
#include <atomic>
#include <boost/asio/error.hpp>
#include "../common/networking_common.h"
#include "asio_session.h"
#include "../io_service/io_service_pool.h"

namespace hdcs {
namespace networking{

using boost::asio::ip::tcp;

class asio_acceptor{
public:

    asio_acceptor(std::string ip_address, std::string port_num, SessionSet& _set, const ServerOptions& _so)
        : server_options(_so)
        , _io_service_pool(_so._io_service_num , _so._thd_num_on_one_session)
        , m_endpoint( boost::asio::ip::address::from_string(ip_address), stoi(port_num) )
        , m_acceptor(_io_service_pool.get_io_service()) 
        , is_closed(false)
        , session_set(_set)
    {}

    ~asio_acceptor()
    {
        stop();
    }

    void stop()
    {
        if (is_closed.load())
        {
            return;
        }
        _io_service_pool.stop();
        boost::system::error_code ec;
        m_acceptor.cancel(ec);
        if(ec)
        {
            error_handling("Network::asio_acceptor::stop: cancel failed ", ec);
        }
        m_acceptor.close(ec);
        if(ec)
        {
            error_handling("Network::asio_acceptor::stop: close failed ", ec);
        }
        is_closed.store(true);
    }

    bool start(ProcessMsg _process_msg)
    {
        process_msg = _process_msg; 
        boost::system::error_code ec;
        m_acceptor.open(m_endpoint.protocol(), ec);
        if (ec)
        {
            error_handling("Network::asio_acceptor::start: open acceptor failed: ", ec);
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
        if (ec)
        {
            error_handling("Network::asio_acceptor::start: set acceptor reuse_address failed: ", ec);
            return false;
        }

        m_acceptor.bind(m_endpoint, ec);
        if (ec)
        {
            error_handling("Network::asio_acceptor::start: bind acceptor failed: ", ec);
            return false;
        }

        //max_connections: the maximum length of the queue of pending incoming connections...asio doc
        m_acceptor.listen(boost::asio::socket_base::max_connections, ec);
        if (ec)
        {
            error_handling("Network::asio_acceptor::start: listen acceptor failed: ", ec);
            return false;
        }
        is_closed.store(false);
        async_accept(); 
        return true;
    }

    void sync_run()
    {
        _io_service_pool.sync_run();
    }

    void async_run()
    {
        _io_service_pool.async_run();
    }

private:

    void async_accept() 
    {
        asio_session* new_session_ptr = new asio_session(_io_service_pool.get_io_service(), 1); // create new session.
        m_acceptor.async_accept(new_session_ptr->get_stream(), boost::bind(
                    &asio_acceptor::on_accept, this, new_session_ptr, _1));
    }

    void on_accept(asio_session* new_session_ptr, const boost::system::error_code& ec)
    {
        // when having too many open file, should re-start listen...TODO ref baidu
        if (is_closed.load())
        {
            return;
        }
        if (ec)
        {
            error_handling("Network::on_accept: failed: ", ec);
            stop();
        }
        else
        {
            // Attention: sequence.
            // configure connection 
            set_connection_att(new_session_ptr);
            // put into session_set
            session_set.insert((Session*)new_session_ptr);
            // run session.
            new_session_ptr->start(); 
            // trigger to handle next connection request.
            async_accept(); 
        }
    }


    void set_connection_att(asio_session* new_session_ptr)
    {
        boost::system::error_code ec;
        // Nagle algorithm
        if(server_options._tcp_no_delay == true)
        {
            boost::asio::ip::tcp::no_delay no_delay(true);
            new_session_ptr->get_stream().set_option(no_delay, ec);
        }
        if(ec){
            error_handling("Networking::asio_acceptor: set socket NO delay failed ", ec);
            assert(0);
        }
        // send buffer
        if(server_options._send_buffer_size != 0)
        {
            boost::asio::socket_base::send_buffer_size s_b(server_options._send_buffer_size);
            new_session_ptr->get_stream().set_option(s_b, ec);
        }
        if(ec)
        {
            error_handling("Networking::asio_acceptor: set socket send_buffer_size failed ", ec);
            assert(0);
        }
        //receive buffer
        if(server_options._receive_buffer_size != 0)
        {
            boost::asio::socket_base::receive_buffer_size r_b(server_options._receive_buffer_size);
            new_session_ptr->get_stream().set_option(r_b, ec);
        }
        if(ec)
        {
            error_handling("Networking::asio_acceptor: set socket receive_buffer_size failed ", ec);
            assert(0);
        }
        // setting process msg 
        new_session_ptr->set_process_msg_server(server_options._process_msg);
        new_session_ptr->set_process_msg_arg_server((void*)new_session_ptr);
        // post threadpool
        if(server_options._post_process_msg != 0)
        {
            new_session_ptr->startup_post_process_msg(server_options._post_process_msg);
        }

    }

    void error_handling(const std::string& fail_position, const boost::system::error_code& ec)
    {
        if(boost::asio::error::operation_aborted == ec)
        {
            // namely: operation cancelled.
           return;  
        }
        if(boost::asio::error::fault == ec)
        {
            // bad address
        }
        if(boost::asio::error::connection_aborted == ec)
        {
            // connection have been absort, if don't actively stop, we need to re-try connection.
        }
        if(boost::asio::error::no_descriptors == ec)
        {
            // TODO TODO TODO TODO TODO TODO TODO TODO
            // too many open files, need to restart server.
        }

        if(true)
        {
            std::cout<<fail_position<<" "<<ec.message()<<std::endl;
        }
    }


private:
    SessionSet& session_set;
    io_service_pool _io_service_pool;
    tcp::endpoint m_endpoint;
    tcp::acceptor m_acceptor;
    std::atomic<bool> is_closed;
    ProcessMsg process_msg;
    const ServerOptions& server_options;

}; //asio_acceptor

} // neworking
} // hdcs
#endif 

