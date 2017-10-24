// Copyright [2017] <Intel>
#include "include/libhdcs.hpp"
#include "common/C_AioRequestCompletion.h"
#include "common/Request.h"
#include "core/HDCSCore.h"

using namespace hdcs;

void hdcs_aio_release(hdcs_completion_t c){
  AioCompletion *comp = (C_AioRequestCompletion*) c;
  delete comp;
}

int hdcs_aio_create_completion(void *cb_arg, callback_t complete_cb, hdcs_completion_t *c){
  AioCompletion *comp = new C_AioRequestCompletion(cb_arg, complete_cb);
  *c = (hdcs_completion_t) comp;
  return 0;
}

libhdcs::libhdcs(char* name) {
  hdcs_inst = new core::HDCSCore(name);
}

libhdcs::~libhdcs() {
  delete hdcs_inst;
}

int libhdcs::hdcs_aio_read(char* data, uint64_t offset, uint64_t length, hdcs_completion_t c){
  void* arg = (void*)c;
  Request *req = new Request(IO_TYPE_READ, data, offset, length, arg);
  hdcs_inst->queue_io(req);
  return 0;
}

int libhdcs::hdcs_aio_write( const char* data, uint64_t offset, uint64_t length, hdcs_completion_t c ){
  void* arg = (void*)c;
  Request *req = new Request(IO_TYPE_WRITE, const_cast<char*>(data), offset, length, arg);
  hdcs_inst->queue_io(req);
  return 0;
}
