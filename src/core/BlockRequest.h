// Copyright [2017] <Intel>
#ifndef HDCS_BLOCK_REQUEST_H
#define HDCS_BLOCK_REQUEST_H
#include "common/Log.h"
#include "common/Request.h"
#include "core/BlockMap.h"
#include "store/DataStoreRequest.h"

namespace hdcs {

namespace core {

class BlockRequest {
public:
  BlockRequest(char* data_ptr, uint64_t offset,
               uint64_t size, std::shared_ptr<Request> req,
               Block* block, std::shared_ptr<store::DataStoreRequest> data_store_req,
               uint32_t tid) :
               data_ptr(data_ptr), offset(offset), 
               size(size), req(req), block(block),
               data_store_req(data_store_req), should_delete(false),
               tid(tid) {
    if (req != nullptr) {
      req->add_request();  
    }           
  }
  ~BlockRequest() {
  }
  void complete(int r) {
    if (req != nullptr) {
      req->complete(r);
    }
  }
  Block* block;
  uint64_t offset;
  uint64_t size;
  char* data_ptr;
  bool should_delete;
  uint32_t tid;
  std::shared_ptr<Request> req;
  std::shared_ptr<store::DataStoreRequest> data_store_req;
};
typedef std::list<BlockRequest> BlockRequestList;

} //namespace core
} //namespace hdcs
#endif
