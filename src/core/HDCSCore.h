// Copyright [2017] <Intel>
#ifndef HDCS_CORE_H
#define HDCS_CORE_H

#include "common/WorkQueue.h"
#include "common/Request.h"
#include "common/Config.h"
#include "common/ThreadPool.h"
#include "common/tq.h"
#include "core/BlockRequest.h"
#include "core/BlockGuard.h"
#include "core/policy/Policy.h"
#include <mutex>

namespace hdcs {
namespace core {
  class HDCSCore {
  public:
    WorkQueue<void*> request_queue;
    HDCSCore(std::string name);
    ~HDCSCore();
    void close();
    void promote_all();
    void flush_all();
    void queue_io (Request *req);
    void aio_read (char* data, uint64_t offset, uint64_t length, void* c);
    void aio_write (const char* data, uint64_t offset, uint64_t length, void* c);
  private:
    TWorkQueue *hdcs_op_threads;
    std::thread *main_thread;
    bool go;
    Config *config;
    Policy* policy;
    BlockGuard* block_guard;
    std::mutex block_request_list_lock;
    BlockRequestList block_request_list;

    void process();
    void process_request(Request *req);
    void map_block(BlockRequest &&block_request);

  };
}// core
}// hdcs

#endif
