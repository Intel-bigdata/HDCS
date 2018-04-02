// Copyright [2017] <Intel>
#ifndef HDCS_CORE_H
#define HDCS_CORE_H

#include "common/WorkQueue.h"
#include "common/Request.h"
#include "common/Config.h"
#include "common/ThreadPool.h"
#include "common/tq.h"
#include "common/HDCS_REQUEST_HANDLER.h"
#include "core/BlockRequest.h"
#include "core/BlockGuard.h"
#include "core/policy/Policy.h"
#include <mutex>
#include <map>
#include <atomic>
#include <string>

namespace hdcs {
  struct hdcs_repl_options {
    hdcs_repl_options(std::string role, std::vector<std::string> &&replication_nodes):
      role(role),
      replication_nodes(replication_nodes){};
    std::string role;
    std::vector<std::string> replication_nodes;
  };

namespace core {
class HDCSCoreStatGuard;
  class HDCSCore {
  public:
    std::mutex core_lock;
    WorkQueue<std::shared_ptr<Request>> request_queue;
    HDCSCore(
      std::string host,
      std::string name,
      std::string cfg_file,
      HDCSCoreStatGuard* core_stat = nullptr,
      hdcs_repl_options &&replication_options = std::move(hdcs_repl_options("master", {}))
      );
    ~HDCSCore();
    void close();
    void promote_all();
    void flush_all();
    void queue_io (std::shared_ptr<Request> req);
    void aio_read (char* data, uint64_t offset, uint64_t length, void* c);
    void aio_write (char* data, uint64_t offset, uint64_t length, void* c);

    // for failover
    bool check_data_consistency();
    uint8_t get_peered_core_num();
    uint8_t get_min_replica_size();
    uint8_t get_replica_size();
  private:
    TWorkQueue *hdcs_op_threads;
    std::thread *main_thread;
    std::atomic<bool> go;
    Config *config;
    Policy* policy;
    BlockGuard* block_guard;
    std::mutex block_request_list_lock;
    BlockRequestList block_request_list;
    //slave hdcs core list
    template<char delimiter>
    class WordDelimitedBy : public std::string {};
    std::mutex replication_core_map_mutex;
    hdcs_replica_nodes_t replication_core_map;
    hdcs_repl_options replication_options;
    std::string name;
    std::string host;
    store::DataStore* datastore;
    uint8_t replica_size;
    uint8_t min_replica_size;
    HDCSCoreStatGuard* core_stat;

    void process();
    void process_request(std::shared_ptr<Request> req);
    void map_block(BlockRequest &&block_request);
    void connect_to_replica(std::vector<std::string> replication_nodes);
    void replica_send_out(AioCompletion* comp, uint64_t offset, uint64_t length, char* data);

  };
}// core
}// hdcs

#endif
