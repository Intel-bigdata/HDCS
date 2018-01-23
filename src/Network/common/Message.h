#ifndef MESSAGE
#define MESSAGE

#include <string>
#include <sstream>

/* 
 * 1: crc_byte_num need to be multiple of 8
 *    otherwise, sizeof(MsgHeader) will occur error.
 *
 * 2: msg_content size > crc_byte_num
 *
 */
#define CRC_BYTE_NUM 8

namespace hdcs{
namespace networking{

struct MsgHeader {
    const uint64_t seq_id;
    uint64_t size; // msg content size
    char crc_flag[CRC_BYTE_NUM];

    MsgHeader(uint64_t size, const char* _crc, uint64_t _seq_id)
        : seq_id(_seq_id)
        , size(size)
    {
        for(int i=0; i < CRC_BYTE_NUM; ++i){
            crc_flag[i]=_crc[i];
        }
    }

    uint64_t get_data_size() {
        return size;
    }

    uint64_t get_seq_id(){
        return seq_id;
    }
};

struct Message {
    MsgHeader header;
    std::string data;
    Message(char* data, uint64_t size, uint64_t _seq_id) 
        : header(size, data + size - CRC_BYTE_NUM, _seq_id)
        , data(data)
    {}

    Message(std::string data, uint64_t _seq_id) 
        : header(data.size(), data.c_str()+data.size()-CRC_BYTE_NUM, _seq_id)
        , data(data) 
    {}

    std::string to_buffer() {
        std::stringstream ss;
        std::string header_str((char*)&header, sizeof(MsgHeader));
        ss << header_str;
        ss << data;
        return ss.str();
    } 

    uint64_t size() {
        return sizeof(MsgHeader) + header.get_data_size();
    }
};

inline bool check_crc(const char* msg_header, const char* msg_content, const uint64_t content_size){
    MsgHeader* temp = (MsgHeader*)msg_header; 
    for(int i=0; i< CRC_BYTE_NUM; i++){
        if((temp->crc_flag)[i] != msg_content[content_size-CRC_BYTE_NUM+i]){
            std::cout<<"check_crc: failed.."<<std::endl;
            return false;
        }
    }
    return true;
}

}
}
#endif
