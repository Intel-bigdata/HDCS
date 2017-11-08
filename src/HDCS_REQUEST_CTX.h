// Copyright [2017] <Intel>
#ifndef HDCS_REQUEST_CTX_H
#define HDCS_REQUEST_CTX_H

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

namespace hdcs {

#define HDCS_CONNECT          0X11
#define HDCS_READ             0X12
#define HDCS_WRITE            0X13
#define HDCS_FLUSH            0X14
#define HDCS_PROMOTE          0X15
#define HDCS_SET_CONFIG       0X16
#define HDCS_GET_STATUS       0X17
#define HDCS_CONNECT_REPLY    0X21
#define HDCS_READ_REPLY       0X22
#define HDCS_WRITE_REPLY      0X23
#define HDCS_FLUSH_REPLY      0X24
#define HDCS_PROMOTE_REPLY    0X25
#define HDCS_SET_CONFIG_REPLY 0X26
#define HDCS_GET_STATUS_REPLY 0X27

typedef uint8_t HDCS_REQUEST_TYPE;
struct HDCS_REQUEST_CTX_T {
  HDCS_REQUEST_TYPE type;
  void* hdcs_inst;
  void* comp;
  uint64_t offset;
  uint64_t length;
  void* ret_data_ptr;
  uint64_t size() {
    return length + sizeof(HDCS_REQUEST_CTX_T);
  }
};

class HDCS_REQUEST_CTX {
public:
  HDCS_REQUEST_CTX (uint8_t type = 0, void* hdcs_inst = nullptr,
                    void* comp = nullptr, uint64_t offset = 0,
                    uint64_t length = 0, void* data = nullptr) {
    data_ = new char[sizeof(HDCS_REQUEST_CTX_T) + length];
    ((HDCS_REQUEST_CTX_T*)data_)->type = type;
    ((HDCS_REQUEST_CTX_T*)data_)->hdcs_inst = hdcs_inst;
    ((HDCS_REQUEST_CTX_T*)data_)->comp = comp;
    ((HDCS_REQUEST_CTX_T*)data_)->offset = offset;
    ((HDCS_REQUEST_CTX_T*)data_)->length = length;
    ((HDCS_REQUEST_CTX_T*)data_)->ret_data_ptr = data;
    memcpy(&data_[sizeof(HDCS_REQUEST_CTX_T)], data, length);
  }

  ~HDCS_REQUEST_CTX() {
    delete[] data_;
  }

  char* data() {
    return data_;
  }

  uint64_t size() {
    return sizeof(HDCS_REQUEST_CTX_T) + ((HDCS_REQUEST_CTX_T*)data_)->length;
  }

  void set_ret_data_ptr(void* ptr) {
    ((HDCS_REQUEST_CTX_T*)data_)->ret_data_ptr = ptr;
  }
private:
  char* data_;
};

}// hdcs

#endif
