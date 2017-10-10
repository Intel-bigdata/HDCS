#ifndef KVBLOCKCACHER_H
#define KVBLOCKCACHER_H

#include <hyperds/Kvdb.h>
#include <hyperds/Options.h>

#include "rbc/BlockCacher.h"
#include "rbc/MetaStore.h"
#include "rbc/common/Log.h"
#include "rbc/common/Mempool.h"


namespace rbc {
class KVBlockCacher: public BlockCacher{
public:
    std::string device_name;

    KVBlockCacher( std::string device_name, std::string metastore_dir, uint64_t cache_total_size, uint64_t object_size, Mempool* mempool );
    ~KVBlockCacher();
    int _remove( uint64_t cache_id );
    int _write( uint64_t cache_id, const char *buf, uint64_t offset, uint64_t length, std::time_t ts );
    ssize_t _read(  uint64_t cache_id, char *buf, uint64_t offset, uint64_t length );

private:

    kvdb::DB* testkvdb=nullptr;
    kvdb::Options opts;

    Mempool* mempool;
    uint64_t cache_block_size;
    uint64_t object_size;
    uint64_t _total_size;

    int device_fd;

    int _open(std::string device_name);
    int _close(int block_fd);
};
}

#endif
