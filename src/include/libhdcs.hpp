//Copyright [2017] <Intel>
#include <stdint.h>

#ifndef LIB_HDCS_H
#define LIB_HDCS_H

namespace hdcs {
  namespace core {
    class HDCSCore;
  }
}

typedef void (*callback_t)(int r, void *arg);
typedef void* hdcs_completion_t;

static void hdcs_aio_release(hdcs_completion_t c);
static int hdcs_aio_create_completion(void *cb_arg, callback_t complete_cb, hdcs_completion_t *c);

class libhdcs {
public:
  libhdcs();
  ~libhdcs();
  int hdcs_aio_read(char* data, uint64_t offset, uint64_t length, hdcs_completion_t c);
  int hdcs_aio_write( const char* data, uint64_t offset, uint64_t length, hdcs_completion_t c );
private:
  hdcs::core::HDCSCore* hdcs_inst;
}; 
#endif
