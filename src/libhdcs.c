// Copyright [2017] <Intel>
#include "include/libhdcs.h"
#include "common/C_AioRequestCompletion.h"
#include "common/HDCS_REQUEST_HANDLER.h"

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
  // param 2 : session numer
  // param 3: threads number at one session
  io_ctx->conn = new hdcs::networking::Connection([](void* p, std::string s){client::request_handler(p, s);}, 5, 5);

  hdcs::HDCS_REQUEST_CTX msg_content(HDCS_CONNECT, nullptr, nullptr, 0, strlen(name), name);
  io_ctx->conn->connect("127.0.0.1", "9000");
  io_ctx->conn->set_session_arg(*io);

  io_ctx->conn->communicate(std::move(std::string(msg_content.data(), msg_content.size())));
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
