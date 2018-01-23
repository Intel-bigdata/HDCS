#ifndef OPTION_H
#define OPTION_H

#include <vector>
#include <string>

namespace hdcs{
namespace networking{

enum COMMUNICATION_TYPE{
    TCP_COMMUNICATION = 0,
    RDMA_COMMUNICATION = 1,
    LOCAL_COMMUNICATION = 2, // TODO for domain socket or share memory
};

struct ClientOptions{
    // default: _io_service num is equal to _session_num
    int _io_service_num;
    int _session_num; 
    int _thd_num_on_one_session;
    // for example: 
    //     server 1: ip: 192.168.3.1 port 6666 TCP
    //     server 2: ip: 192.168.3.2 port 7777 RDMA
    //
    // usage: 
    //     _ip_address_vec.push_back("192.168.3.1"); 
    //     _port_num_vec.push_back("6666"); 
    //     _communication_type_vec.push_back("TCP_COMMUNICATION");
    //
    //     _ip_address_vec.push_back("192.168.3.2"); 
    //     _port_num_vec.push_back("7777"); 
    //     _communication_type_vec.push_back("RDMA_COMMUNICATION");
    //
    std::vector<std::string> _ip_address_vec;
    std::vector<std::string> _port_num_vec;
    std::vector<COMMUNICATION_TYPE> _communication_type_vec;
    // namely, client's hdcs_handle_request
    ProcessMsgClient _process_msg; 
    void* _process_msg_arg;

    //if tcp_no_delay == true, means disable the Nagle algorithm. 
    bool _tcp_no_delay = true; 

    // if send_buffer_size == 0, using asio's default size.
    uint64_t _send_buffer_size = 0; 
    uint64_t _receive_buffer_size = 0;

    // when _post_process_msg > 0, so execute hdcs_handle_request in new threadpool.
    // asio callback in one threadpool 
    // hdcs_handle_request in other threadpool 
    int _post_process_msg = 0;


};

struct ServerOptions{
    int _io_service_num;
    int _session_num; // no meaning.
    int _thd_num_on_one_session; 

    std::vector<std::string> _port_num_vec;
    std::vector<COMMUNICATION_TYPE> _communication_type_vec;
    // namely, server's hdcs_handle_request
    ProcessMsg _process_msg; 
    void* _process_msg_arg;

    //if tcp_no_delay == true, means disable the Nagle algorithm. 
    bool _tcp_no_delay = true; 

    // if send_buffer_size == 0, using asio's default size.
    uint64_t _send_buffer_size = 0; 
    uint64_t _receive_buffer_size = 0;

    // when _post_process_msg > 0, so execute hdcs_handle_request in new threadpool.
    // asio callback in one threadpool 
    // hdcs_handle_request in other threadpool 
    int _post_process_msg = 0;

};

}// namespace networking 
}// namespace hdcs
#endif
