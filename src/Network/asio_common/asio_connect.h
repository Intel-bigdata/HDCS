#ifndef ASIO_CONNECT
#define ASIO_CONNECT
#include "asio_session.h"
#include "../io_service/io_service_pool.h"
namespace hdcs{
namespace networking{

class AsioConnect{
public:

    AsioConnect(const ClientOptions& _co, std::vector<Session*>& _session_vec)
        : client_options(_co)
        , session_vec(_session_vec)
        , io_service_pool_ptr(new io_service_pool(_co._io_service_num, _co._thd_num_on_one_session))
        , resolver_(io_service_pool_ptr->get_io_service())
    {
        io_service_pool_ptr->async_run();
        is_run = true;
    }

    ~AsioConnect()
    {
        close();
    }

    void close()
    {
        if(!is_run)
        {
            return;
        }
        io_service_pool_ptr->stop();
        io_service_pool_ptr.reset();
        is_run = false;
    }

    int async_connect( std::string ip_address, std::string port )
    {
        // TODO
        return 1;
    }

    int sync_connect(std::string ip_address, std::string port)
    {
        asio_session* new_session;
        int ret;
        for( int i = 0; i < client_options._session_num; ++i)
        {
            new_session = new asio_session(io_service_pool_ptr->get_io_service(), 0);

            // must set process_msg before calling sync_connect
            //new_session->set_process_msg_client(client_options._process_msg);
            //new_session->set_process_msg_arg_client(client_options._process_msg_arg);

            ret = sync_connect_impl(ip_address, port, new_session->get_stream());
            if(ret != 0)
            {
                std::cout<<"Networking::AsioConnect : sync_connect_impl failed."<<std::endl;
                assert(0);
            }
            ret = set_connection_att(new_session, new_session->get_stream());
            if(ret != 0)
            {
                std::cout<<"Networking::AsioConnect : set_connection_att failed."<<std::endl;
                assert(0);
            }
            session_vec.push_back(new_session);
        }
        if(client_options._session_num == session_vec.size())
        {
            std::cout<<"Networking::TCP communication: "<<client_options._session_num<<" sessions have been created."<<std::endl;
        }
        else
        {
            std::cout<<"Networking::AsioConnect: creating session failed."<<std::endl;
            assert(0);
        }
        return client_options._session_num;
    }

private:

    int sync_connect_impl(std::string ip_address, std::string port, boost::asio::ip::tcp::socket& socket_){
        boost::system::error_code ec;
        boost::asio::ip::tcp::resolver::iterator iter =
            resolver_.resolve(boost::asio::ip::tcp::resolver::query(ip_address, port), ec);
        if(ec)
        {
            std::cout<<"Networking::asio_connect::sync_connect_impl, resolver failed: "<<ec.message()<<std::endl;
            assert(0);
        }
        boost::asio::ip::tcp::endpoint endpoint_ = *iter;
        // connecting...
        socket_.connect(endpoint_, ec);
        if(ec)
        {
            if(ec == boost::asio::error::operation_aborted)
            {
                return 0;
            }
            std::cout<<"Networking::asio_connect::sync_connect_impl, connect  failed: "<<ec.message()<<std::endl;
            assert(0);
        }
        return 0;
    }

    int set_connection_att(asio_session* new_session, boost::asio::ip::tcp::socket& socket_)
    {
        boost::system::error_code ec;
        // Naggle's algorithm.
        if(client_options._tcp_no_delay == true )
        {
            boost::asio::ip::tcp::no_delay no_delay(true);
            socket_.set_option(no_delay, ec);
        }
        if(ec)
        {
            std::cout<<"Networking::asio_connect::set_connection_att: setting tcp_no_delay failed: "<<ec.message()<<std::endl;
            assert(0);
        }
        // send buffer size 
        if(client_options._send_buffer_size != 0)
        {
            boost::asio::socket_base::send_buffer_size s_b(client_options._send_buffer_size);
            socket_.set_option(s_b, ec);
        }
        if(ec)
        {
            std::cout<<"Networking::asio_connect::set_connection_att: setting send_buffer_size failed "<<ec.message()<<std::endl;
            assert(0);
        }
        // receiving buffer size
        if(client_options._receive_buffer_size != 0)
        {
            boost::asio::socket_base::receive_buffer_size r_b(client_options._receive_buffer_size);
            socket_.set_option(r_b, ec);
        }
        if(ec)
        {
            std::cout<<"Networking::asio_connect::set_connection_att: setting receive_buffer_size failed "<<ec.message()<<std::endl;
            assert(0);
        }

        if(client_options._post_process_msg != 0)
        {
            new_session->startup_post_process_msg(client_options._post_process_msg);
        }

        // setting msg handing function.
        new_session->set_process_msg_client(client_options._process_msg);
        new_session->set_process_msg_arg_client(client_options._process_msg_arg);
        // other connection attributes setting..
        if(false)
        {
            // TODO
        }
        return 0;
    }


private:
    std::vector<Session*>& session_vec;
    const ClientOptions& client_options;
    std::shared_ptr<io_service_pool> io_service_pool_ptr;
    bool is_run;
    boost::asio::ip::tcp::resolver resolver_;

};// asioconnect

} //networking
} //hdcs
#endif
