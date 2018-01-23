#ifndef ASIO_SESSION
#define ASIO_SESSION
#include <cstdlib>
#include <iostream>
#include <memory>
#include "../common/networking_common.h"
#include "../common/session.h"
#include "asio_messenger.h"
#define SESSION_TIMEOUT 30

namespace hdcs{
namespace networking{

class asio_session: public Session{
private:
    std::shared_ptr<asio_messenger> m_messenger;
    ProcessMsg process_msg;
    std::atomic<bool> session_work;
    boost::posix_time::ptime last_active;

public:
    asio_session( IOService& _io_service, int _role) 
        : m_messenger(new asio_messenger(_io_service, _role))
    {}

    ~asio_session()
    {
        stop();
    }

    void stop()
    {
        m_messenger->close();
    }

    void cancel()
    {
        m_messenger->cancel();
    }

    void set_process_msg_client(ProcessMsgClient _p_m)
    {
        m_messenger->set_process_msg_client(_p_m);
    }

    void set_process_msg_arg_client(void* _arg)
    {
        m_messenger->set_process_msg_arg_client(_arg);
    }

    void set_process_msg_server(ProcessMsg _p_m)
    {
        m_messenger->set_process_msg_server(_p_m);
    }

    void set_process_msg_arg_server(void* _arg)
    {
        m_messenger->set_process_msg_arg_server(_arg);
    }

    int startup_post_process_msg(int _x)
    {
        return m_messenger->startup_post_process_msg(_x);
    }

    bool start()
    {
	m_messenger->aio_receive_server();
        return true;
    }

    int async_send(std::string send_buffer, uint64_t _seq_id)
    {
       return m_messenger->async_send(send_buffer, _seq_id);
    }

    void update_time()
    {
        last_active = boost::posix_time::microsec_clock::local_time();
    }

    bool if_timeout()
    {
        boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
        return ((now - last_active).total_milliseconds() > SESSION_TIMEOUT) ? true : false;
    }

    // called by client
    ssize_t communicate(std::string send_buffer, uint64_t _seq_id)
    {
        return m_messenger->communicate(send_buffer, _seq_id);
    }

    // called by client
    void aio_communicate(std::string& send_buffer, uint64_t _seq_id)
    {
        m_messenger->aio_communicate(send_buffer, _seq_id);
    }

    boost::asio::ip::tcp::socket& get_stream()
    {
        return m_messenger->get_socket();
    }

    COMMUNICATION_TYPE communication_type()
    {
        return COMMUNICATION_TYPE(0);
    }
    
};
}
}
#endif

