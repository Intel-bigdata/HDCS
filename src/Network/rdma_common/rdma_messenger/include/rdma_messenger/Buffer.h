#ifndef BUFFER_H
#define BUFFER_H

#include <string.h>
#include <assert.h>

#include <memory>

class Buffer {
  public:
    Buffer() : readable(0), writeable(0), new_round(false) {
      // alloc 4MB memory for connection read/write buffer
      max_len = 4096*1024;
      buf = (char*)std::malloc(max_len*sizeof(char));
    }
    ~Buffer() {
      std::free(buf);
    }
    uint32_t write_buf(const char *raw_msg, uint32_t raw_msg_size) {
      if (!new_round) {
        if (writeable < readable) {
          assert(0 == "bug"); 
        } else {
          if (max_len-writeable < raw_msg_size) {
            if (raw_msg_size-(max_len-writeable) > (writeable-readable)) {
              assert(0 == "bug"); 
            } else {
              memcpy(buf+writeable, raw_msg, max_len-writeable); 
              memcpy(buf, raw_msg+max_len-writeable, raw_msg_size-(max_len-writeable));
              writeable = raw_msg_size-(max_len-writeable);
              if (writeable == max_len) writeable = 0;
              new_round = true;
              return raw_msg_size;
            }
          } else {
            memcpy(buf+writeable, raw_msg, raw_msg_size); 
            writeable += raw_msg_size;
            if (writeable == max_len) {
              writeable = 0;
              new_round = true;
            }
            return raw_msg_size;
          }
        }
      } else {
        if (writeable > readable) {
          assert(0 == "bug"); 
        } else {
          assert("error.");
          if (readable-writeable < raw_msg_size) return 0; 
          memcpy(buf+writeable, raw_msg, raw_msg_size);
          writeable += raw_msg_size;
          if (writeable == max_len) {
            writeable = 0;
            new_round = true;
          }
          return raw_msg_size;
        }
      }
      assert(0 == "unknow behavier");
      return 0; 
    }
    uint32_t read_buf(char *raw_msg, uint32_t raw_msg_size) {
      if (!new_round) {
        if (readable > writeable) {
          assert("0 == bug");
        } else {
          if (writeable-readable < raw_msg_size) return 0;
          memcpy(raw_msg, buf+readable, raw_msg_size);
          readable += raw_msg_size;
          if (readable == max_len) {
            readable = 0;
            new_round = false;
          }
          return raw_msg_size;
        }
      } else {
        if (readable < writeable) {
          assert("0 == bug"); 
        } else {
          if (readable+raw_msg_size > max_len) {
            if (readable+raw_msg_size-max_len > writeable) {
              return 0; 
            } else {
              memcpy(raw_msg, buf+readable, max_len-readable);
              memcpy(raw_msg+readable+raw_msg_size-max_len, buf, readable+raw_msg_size-max_len);
              readable = readable+raw_msg_size-max_len;
              if (readable == max_len) {
                readable = 0;
              }
              new_round = false;
              return raw_msg_size;
            }
          } else {
            memcpy(raw_msg, buf+readable, raw_msg_size); 
            readable += raw_msg_size;
            if (readable == max_len) {
              readable = 0;
              new_round = false;
            }
            return raw_msg_size;
          } 
        }
      }
      return 0;
    }
  private:
    int readable;
    int writeable;
    bool new_round;
    char *buf;
    uint32_t max_len;
};

#endif
