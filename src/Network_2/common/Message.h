#ifndef DSLAB_HDCS_MESSAGE_H
#define DSLAB_HDCS_MESSAGE_H

#include <string>
#include <sstream>
namespace hdcs{
namespace networking{
struct MsgHeader {
  const uint64_t msg_flag;
  uint64_t size;
  MsgHeader(uint64_t size) : msg_flag(0xFFFFFFFFFFFFFFFF), size(size) {}
  uint64_t get_data_size() {
    return size;
  }
};

struct Message {
  MsgHeader header;
  std::string data;
  Message(char* data, uint64_t size) : header(size), data(data) {}
  Message(std::string data) : header(data.size()), data(data) {}
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
}
}
#endif
