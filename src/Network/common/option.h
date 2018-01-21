#ifndef OPTION_H
#define OPTION_H

#include <vector>
#include <string>

namespace hdcs{
namespace networking{

enum COMMUNICATION_TYPE{
    TCP_COMMUNICAYION,
    RDMA_COMMUNICATION,
    TCP_AND_RDMA_COMMUNICATION,
};

struct ClientOptions{
    int _io_service_num;
    int _session_num; // default: _io_service_num is equal to _session_num.
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

    ProcessMsgClient _process_msg; // namely: client's hdcs_handle_request

};

struct ServerOptions{
    int _io_service_num;
    int _session_num;
    int _thd_num_on_one_session; 

    std::vector<std::string> _port_num_vec;
    std::vector<COMMUNICATION_TYPE> _communication_type_vec;

    ProcessMsg _process_msg; // namely: server's hdcs_handle_request

};

}// namespace networking 
}// namespace hdcs
#endif
