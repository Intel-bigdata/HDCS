// Copyright [2017] <Intel>
#ifndef HDCS_DATA_STORE_REQUEST_H
#define HDCS_DATA_STORE_REQUEST_H
#include <stdint.h>
#include <map>
#include <mutex>
#include "common/AioCompletionImp.h"
#include "store/DataStore.h"
#include "common/HDCS_REQUEST_CTX.h"
#include "common/Timer.h"

namespace hdcs {
namespace store {
struct DataStoreRequest_t {
  uint64_t offset;
  uint64_t size;
  char* data;
  uint32_t tid;
  uint32_t commit_pos;
  DataStoreRequest_t (uint64_t offset, uint64_t size, char* data, uint32_t tid, uint32_t commit_pos):
    offset(offset), size(size), data(data), tid(tid), commit_pos(commit_pos) {}
};
typedef std::map<std::string, void*> hdcs_replica_nodes_t;
typedef std::map<uint64_t, DataStoreRequest_t*> data_store_request_chain_t; 
//typedef void DataStore;

  class DataStoreRequest {
  public:
    DataStoreRequest(uint8_t shared_count, uint64_t block_size,
                     AioCompletion* replica_comp,
                     hdcs_replica_nodes_t* connection_v,
                     SafeTimer* timer, uint64_t request_timeout):
      shared_count(shared_count), data_store(nullptr), replica_comp(replica_comp),
      block_size(block_size), connection_v(connection_v), timer(timer),
      request_timeout(request_timeout){
        //create comp for prepare_data_store_req.
        data_store_comp = std::make_shared<AioCompletionImp>([&](ssize_t r){
          submit_request();
        }, shared_count, false);
    }

    ~DataStoreRequest() {
      for (data_store_request_chain_t::iterator it = data_store_req_chain.begin(); 
          it != data_store_req_chain.end(); it++) {
        delete it->second;
        data_store_req_chain.erase(it);
      }
    }

    int prepare_request(DataStore* data_store_tmp,
        uint64_t block_id,
        char* data,
        uint32_t tid,
        uint32_t commit_pos) {
      data_store_mutex.lock();
      data_store = data_store_tmp;

      //printf("offset: %lu, data: %p\n", block_id * block_size, data);
      data_store_request_chain_t::iterator cur_it;
      bool inserted = false;
      uint64_t block_size_tmp = block_size;
      uint8_t step = block_size_tmp / block_size;
      for (data_store_request_chain_t::iterator it = data_store_req_chain.begin(); 
          it != data_store_req_chain.end();) {
        cur_it = it++;

        // merge to the block before to this one
          if (cur_it->second->data + cur_it->second->size == data) {
            cur_it->second->size += block_size_tmp;
            block_id = cur_it->first;
            block_size_tmp = cur_it->second->size;
            data = cur_it->second->data;
            inserted = true;
          }

        // merge to the block next to this one
          if (data + block_size_tmp == cur_it->second->data) {
            uint64_t tmp_size = block_size_tmp + cur_it->second->size;
            //insert new one
            data_store_req_chain[block_id] = new DataStoreRequest_t(block_id, tmp_size, data, tid, commit_pos);
 
            //remove original one
            delete cur_it->second;
            data_store_req_chain.erase(cur_it);
            block_size_tmp = tmp_size;
            inserted = true;
          }

      }// end for
      if (!inserted) {
        data_store_req_chain[block_id] = new DataStoreRequest_t(block_id, block_size_tmp, data, tid, commit_pos);
      }

      data_store_mutex.unlock();
      data_store_comp->complete(0);
      data_store_comp->wait_for_complete();
    }

    int submit_request() {
      void* io_ctx;
      int ret = 0;
      uint64_t offset;
      uint64_t length;
      char* data_ptr;
      uint32_t tid;
      uint32_t commit_pos;

      for (data_store_request_chain_t::iterator it = data_store_req_chain.begin(); 
          it != data_store_req_chain.end(); it++) {
        //TODO:  should reset aio_completion size if chain size greater than 1.

        //todo: different at cache mode.
        offset = it->first * block_size;
        length = it->second->size;
        data_ptr = it->second->data;
        tid = it->second->tid;
        commit_pos = it->second->commit_pos;

        //submit request to replica
        for (const auto& replica_node : *connection_v) {
          io_ctx = replica_node.second;
          //TODO: add tid into replica msg
          hdcs::HDCS_REQUEST_CTX msg_content(HDCS_WRITE, ((hdcs_ioctx_t*)io_ctx)->hdcs_inst,
                                             replica_comp, offset, length, data_ptr);
          ((hdcs_ioctx_t*)io_ctx)->conn->aio_communicate(std::move(std::string(msg_content.data(), msg_content.size())));
        }
        if (replica_comp) {
          // add a timeout to timer
          timer->add_event_after(request_timeout, replica_comp);
        }

        //submit request to local
        ret = data_store->write(data_ptr, offset, length);
        if (ret < 0) {
          log_err("data_store->write failed\n");
          return ret;
        }
        ret = data_store->guard->commit(tid, commit_pos, offset, length);
      }

      return ret;
    }

  private:
    //DataStore
    data_store_request_chain_t data_store_req_chain;

    std::mutex data_store_mutex;
    std::shared_ptr<AioCompletion> data_store_comp;
    uint8_t shared_count;
    DataStore* data_store;
    IO_TYPE io_type;
    uint64_t block_size;
    hdcs_replica_nodes_t* connection_v;
    AioCompletion* replica_comp;
    SafeTimer* timer;
    uint64_t request_timeout;
  };
}// store
}// hdcs

#endif
