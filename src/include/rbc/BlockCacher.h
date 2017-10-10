#ifndef RBC_BLOCK_CACHER_H_
#define RBC_BLOCK_CACHER_H_

#include <string>
#include "rbc/common/Config.h"


namespace rbc {

class BlockCacher{
public:
    static BlockCacher* create_cacher(const std::string cacher_type, Config* config);

    BlockCacher() {
    }
    virtual ~BlockCacher() {
    }
    virtual int _write(uint64_t cache_id, const char *buf, uint64_t offset, uint64_t length, std::time_t ts) = 0;
    virtual ssize_t _read(uint64_t cache_id, char *buf, uint64_t offset, uint64_t length) = 0;
    virtual int _remove(uint64_t cache_id) = 0;
};


}

#endif //RBC_BLOCK_CACHER_H_
