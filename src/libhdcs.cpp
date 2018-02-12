// Copyright [2017] <Intel>
#include "include/libhdcs.hpp"
#include "common/C_AioRequestCompletion.h"
#include "common/Request.h"
#include "common/Config.h"
#include "core/HDCSCore.h"
#include "common/Config.h"
#include <boost/algorithm/string.hpp>
//#include "common/HDCS_REQUEST_HANDLER.h"

using namespace hdcs;


libhdcs::libhdcs(const char* name) {
  //TODO(): repl_opt should be ignored
  hdcs_inst = new core::HDCSCore("" , name, "/etc/hdcs/general.conf");
}

libhdcs::~libhdcs() {
  delete hdcs_inst;
}

int libhdcs::hdcs_aio_read(const char* volume_name, char* data, uint64_t offset, uint64_t length, hdcs_completion_t c){
  void* arg = (void*)c;
  //Request *req = new Request(IO_TYPE_READ, data, offset, length, arg);
  //hdcs_inst->queue_io(req);
  hdcs_inst->aio_read(data, offset, length, arg);
  return 0;
}

int libhdcs::hdcs_aio_write(const char* volume_name, const char* data, uint64_t offset, uint64_t length, hdcs_completion_t c){
   /* AioCompletion* comp = (AioCompletion*) c;
    comp->complete(0);
    return 0;*/
  //void* arg = (void*)c;
  //Request *req = new Request(IO_TYPE_WRITE, const_cast<char*>(data), offset, length, arg);
  //hdcs_inst->queue_io(req);
  void* arg = (void*)c;
  hdcs_inst->aio_write((char*)data, offset, length, arg);
  return 0;
}


extern "C" void hdcs_aio_release(hdcs_completion_t c){
  hdcs::AioCompletion *comp = (hdcs::C_AioRequestCompletion*) c;
  comp->release();
}

extern "C" void hdcs_aio_wait_for_complete(hdcs_completion_t c){
  hdcs::AioCompletion *comp = (hdcs::C_AioRequestCompletion*) c;
  comp->wait_for_complete();
}

extern "C" int hdcs_aio_create_completion(void *cb_arg, callback_t complete_cb, hdcs_completion_t *c){
  hdcs::AioCompletion *comp = new hdcs::C_AioRequestCompletion(cb_arg, complete_cb);
  *c = (hdcs_completion_t) comp;
  return 0;
}

extern "C" ssize_t hdcs_aio_get_return_value(hdcs_completion_t c) {
  hdcs::AioCompletion *comp = (hdcs::C_AioRequestCompletion*) c;
  return comp->get_return_value();
}

extern "C" int hdcs_open(void** io, char* name) {
  *io = malloc(sizeof(hdcs_ioctx_t));
  hdcs_ioctx_t* io_ctx = (hdcs_ioctx_t*)*io;

  io_ctx->conn = new hdcs::networking::Connection([](void* p, std::string s){request_handler(p, s);}, 16, 5);

  hdcs::HDCS_REQUEST_CTX msg_content(HDCS_CONNECT, nullptr, nullptr, 0, strlen(name), name);
  hdcs::Config *hdcs_config = new hdcs::Config(name);
  std::string addr = hdcs_config->get_config("HDCSClient")["addr"];
  std::vector<std::string> ip_port;
  boost::split(ip_port, addr, boost::is_any_of(":"));
  std::string ip = ip_port[0];
  std::string port = ip_port[1];

  io_ctx->conn->connect(ip, port);
  io_ctx->conn->set_session_arg(*io);

  io_ctx->conn->communicate(std::move(std::string(msg_content.data(), msg_content.size())));
  delete hdcs_config;
  return 0;
}

extern "C" int hdcs_close(void* io) {
  ((hdcs_ioctx_t*)io)->conn->close();
  free(io);
  return 0;
}

extern "C" int hdcs_aio_read(void* io, char* data, uint64_t offset, uint64_t length, hdcs_completion_t c){
  void* comp = (void*)c;
  hdcs::HDCS_REQUEST_CTX msg_content(HDCS_READ, ((hdcs_ioctx_t*)io)->hdcs_inst, comp, offset, length, data);
  ((hdcs_ioctx_t*)io)->conn->aio_communicate(std::move(std::string(msg_content.data(), msg_content.size())));
  return 0;
}

extern "C" int hdcs_aio_write(void* io, const char* data, uint64_t offset, uint64_t length, hdcs_completion_t c){
  void* comp = (void*)c;
  hdcs::HDCS_REQUEST_CTX msg_content(HDCS_WRITE, ((hdcs_ioctx_t*)io)->hdcs_inst, comp, offset, length, const_cast<char*>(data));
  ((hdcs_ioctx_t*)io)->conn->aio_communicate(std::move(std::string(msg_content.data(), msg_content.size())));
  return 0;
}

extern "C" int hdcs_promote_all(void* io) {
  hdcs::HDCS_REQUEST_CTX msg_content(HDCS_PROMOTE, ((hdcs_ioctx_t*)io)->hdcs_inst, nullptr);
  ((hdcs_ioctx_t*)io)->conn->communicate(std::move(std::string(msg_content.data(), msg_content.size())));
  return 0;
}

extern "C" int hdcs_flush_all(void* io) {
  hdcs::HDCS_REQUEST_CTX msg_content(HDCS_FLUSH, ((hdcs_ioctx_t*)io)->hdcs_inst, nullptr);
  ((hdcs_ioctx_t*)io)->conn->communicate(std::move(std::string(msg_content.data(), msg_content.size())));
  return 0;
}
