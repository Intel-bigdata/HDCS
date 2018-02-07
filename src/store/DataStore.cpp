#include "store/DataStore.h"
#include "store/SimpleStore/SimpleBlockStore.h"
#include "store/KVStore/KVStore.h"

namespace hdcs {
namespace store {
  DataStore* DataStore::create_engine(const std::string engine_type,
                                      const std::string store_path,
                                      const uint64_t total_size,
                                      const uint64_t store_size,
                                      const uint64_t block_size) {
    if (engine_type == "kv") {
      return new KVStore(store_path, total_size, store_size, block_size);
    }
    return new SimpleBlockStore(store_path, total_size, store_size, block_size);
  }
}
}
