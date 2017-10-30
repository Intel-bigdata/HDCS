#ifndef HDCS_AIOCOMPLETION_H
#define HDCS_AIOCOMPLETION_H

#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>

namespace hdcs{

typedef void *completion_t;

class AioCompletion{
public:
  virtual void complete(ssize_t r) = 0;
  virtual ssize_t get_return_value() = 0;
  virtual void wait_for_complete() = 0;
  virtual void set_reserved_ptr(void* ptr) = 0;
};

}// hdcs
#endif
