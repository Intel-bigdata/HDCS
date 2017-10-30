// Copyright [2017] <Intel>
#include "include/libhdcs.h"
#include "common/C_AioRequestCompletion.h"
#include "Network/client.h"
#include "HDCS_REQUEST_CTX.h"

struct hdcs_ioctx_t{
  Connection* conn;
  void* hdcs_inst; 
};

ssize_t request_handler(char msg_content[]) {
  hdcs::HDCS_REQUEST_CTX_T *io_ctx = (hdcs::HDCS_REQUEST_CTX_T*) msg_content;
    char* data = &msg_content[sizeof(hdcs::HDCS_REQUEST_CTX_T)];
    switch (io_ctx->type) {
      case HDCS_CONNECT_REPLY:
        return (ssize_t)(io_ctx->hdcs_inst);
        break;
      case HDCS_READ_REPLY:
        if((ssize_t)(io_ctx->ret_data_ptr) != -1) {
          memcpy(io_ctx->ret_data_ptr, data, io_ctx->length);
          ((hdcs::AioCompletion*)io_ctx->comp)->complete(0);
        } else {
          ((hdcs::AioCompletion*)io_ctx->comp)->complete(-1);
        }
        break;
      case HDCS_WRITE_REPLY:
        ((hdcs::AioCompletion*)io_ctx->comp)->complete((ssize_t)io_ctx->ret_data_ptr);
        break;
      case HDCS_FLUSH_REPLY:
        break;
      case HDCS_PROMOTE_REPLY:
        break;
      case HDCS_SET_CONFIG_REPLY:
        break;
      case HDCS_GET_STATUS_REPLY:
        break;
      default:
        break;
    }
    return 0;
}

void hdcs_aio_release(hdcs_completion_t c){
  hdcs::AioCompletion *comp = (hdcs::C_AioRequestCompletion*) c;
  delete comp;
}

void hdcs_aio_wait_for_complete(hdcs_completion_t c){
  hdcs::AioCompletion *comp = (hdcs::C_AioRequestCompletion*) c;
  comp->wait_for_complete();
}

int hdcs_aio_create_completion(void *cb_arg, callback_t complete_cb, hdcs_completion_t *c){
  hdcs::AioCompletion *comp = new hdcs::C_AioRequestCompletion(cb_arg, complete_cb);
  *c = (hdcs_completion_t) comp;
  return 0;
}

ssize_t hdcs_aio_get_return_value(hdcs_completion_t c) {
  hdcs::AioCompletion *comp = (hdcs::C_AioRequestCompletion*) c;
  return comp->get_return_value();
}

int hdcs_open(void** io, char* name) {
  *io = malloc(sizeof(hdcs_ioctx_t));
  hdcs_ioctx_t* io_ctx = (hdcs_ioctx_t*)*io;
  io_ctx->conn = new Connection();
  hdcs::HDCS_REQUEST_CTX msg_content(HDCS_CONNECT, nullptr, nullptr, 0, strlen(name), name);
  io_ctx->conn->connect("127.0.0.1", "9000");
  ssize_t ret = io_ctx->conn->communicate(std::move(std::string(msg_content.data(), msg_content.size())));
  io_ctx->hdcs_inst = (void*)ret;
  return 0;
}

int hdcs_close(void* io) {
  ((hdcs_ioctx_t*)io)->conn->close();
  free(io);
  return 0;
}

int hdcs_aio_read(void* io, char* data, uint64_t offset, uint64_t length, hdcs_completion_t c){
  void* comp = (void*)c;
  hdcs::HDCS_REQUEST_CTX msg_content(HDCS_READ, ((hdcs_ioctx_t*)io)->hdcs_inst, comp, offset, length, data);
  ((hdcs_ioctx_t*)io)->conn->aio_communicate(std::move(std::string(msg_content.data(), msg_content.size())));
  return 0;
}

int hdcs_aio_write(void* io, const char* data, uint64_t offset, uint64_t length, hdcs_completion_t c){
  void* comp = (void*)c;
  hdcs::HDCS_REQUEST_CTX msg_content(HDCS_WRITE, ((hdcs_ioctx_t*)io)->hdcs_inst, comp, offset, length, const_cast<char*>(data));
  ((hdcs_ioctx_t*)io)->conn->aio_communicate(std::move(std::string(msg_content.data(), msg_content.size())));
  return 0;
}

int hdcs_promote_all(void* io) {
  hdcs::HDCS_REQUEST_CTX msg_content(HDCS_PROMOTE, ((hdcs_ioctx_t*)io)->hdcs_inst, nullptr);
  ((hdcs_ioctx_t*)io)->conn->communicate(std::move(std::string(msg_content.data(), msg_content.size())));
  return 0;
}

int hdcs_flush_all(void* io) {
  hdcs::HDCS_REQUEST_CTX msg_content(HDCS_FLUSH, ((hdcs_ioctx_t*)io)->hdcs_inst, nullptr);
  ((hdcs_ioctx_t*)io)->conn->communicate(std::move(std::string(msg_content.data(), msg_content.size())));
  return 0;
}
