// Copyright [2017] <Intel>
#ifndef HDCS_POLICY_H
#define HDCS_POLICY_H
#include "core/BlockOp.h"

namespace hdcs {

namespace core {

struct Entry {
  Entry () : status(UNPROMOTED), entry_id(0), timeout_comp(nullptr) {
  }
  Entry (uint32_t entry_id) : entry_id(entry_id), status(UNPROMOTED), timeout_comp(nullptr) {
  }
  const uint32_t entry_id;
  store::BLOCK_STATUS_TYPE status;
  AioCompletion* timeout_comp;
};  

class Policy {
public:
  virtual BlockOp* map(BlockRequest &&block_request, BlockOp** block_op_end) = 0;
  virtual void flush_all() = 0;

};

}// core

}// hdcs
#endif
