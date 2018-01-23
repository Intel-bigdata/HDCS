#ifndef CLIENT 
#define CLIENT 

#include <mutex>
#include <string>
#include <thread>
#include <map>
#include <vector>
#include <atomic>
#include <memory>
#include <atomic>

#include "connect.h"
#include "common/counter.h"
#include "common/option.h"

namespace hdcs{
namespace networking{

class Connection{   
private:
    std::vector<SessionPtr> session_vec;
    std::atomic<int> session_index;
    std::atomic<bool> is_closed;
    std::shared_ptr<Connect> connector_ptr;
    std::mutex session_index_lock;
    AtomicCounter64 _next_sequence_id; // free lock counter
    ClientOptions client_options;    

public:
    // default: just support TCP communication
    // old interface.
    Connection(ProcessMsgClient _process_msg , void* _p_m_param, int _s_num, int _thd_num)
        : session_index(0)
        , _next_sequence_id(0)
        , is_closed(false)
    {
        client_options._process_msg = _process_msg;
        client_options._process_msg_arg = _p_m_param;
        client_options._io_service_num =  _s_num;
        client_options._session_num = _s_num;
        client_options._thd_num_on_one_session = _thd_num;
        // create connector
        connector_ptr.reset(new Connect(client_options));
    }

    Connection(const ClientOptions& _co)
    {
        client_options._process_msg = _co._process_msg;
        client_options._process_msg_arg = _co._process_msg_arg;
        client_options._io_service_num = _co._io_service_num;
        client_options._session_num = _co._session_num;
        client_options._thd_num_on_one_session = _co._thd_num_on_one_session;
        //create connector
        connector_ptr.reset(new Connect(client_options));
    }

    ~Connection()
    {
        close();
    }

    void close() 
    {
        if(is_closed.load())
        {
            return;
        }
        for(int i = 0 ; i < session_vec.size(); ++i)
        {
            session_vec[i]->stop();
            delete session_vec[i];
        }
        connector_ptr->close();
        is_closed.store(true);
    }

    // high layer decide to use which communication type.
    int connect(std::string ip_address, std::string port, int type = 0)
    {
        if(client_options._session_num != (connector_ptr->sync_connect(ip_address, port, session_vec, type)))
        {
            std::cout<<"Networking::client: create session failed.."<<std::endl;
            assert(0);
            return -1;
        }
        return 0;
    }

    ssize_t communicate( std::string send_buffer)
    {
        int temp_index;
        session_index_lock.lock();
        temp_index = session_index;
        if(++session_index==session_vec.size())
        {
            session_index = 0;
        }
        session_index_lock.unlock();
        ssize_t ret = session_vec[temp_index]->communicate(send_buffer, generate_sequence_id());
        return ret;
    }

    void aio_communicate(std::string&& send_buffer)
    {
        int temp_index; 
        session_index_lock.lock();
        temp_index = session_index;
        if(++session_index==session_vec.size())
        {
            session_index = 0;
        }
        session_index_lock.unlock();
        session_vec[temp_index]->aio_communicate(send_buffer, generate_sequence_id());
    }

private:

    uint64_t generate_sequence_id(){
        return ++_next_sequence_id;
    }

}; // hdcs

} // namespace networking
} // namespace hdcs
#endif
