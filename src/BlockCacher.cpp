#include "rbc/BlockCacher.h"
#include "rbc/KVBlockCacher.h"
#include "rbc/SimpleBlockCacher.h"


namespace rbc {
BlockCacher* BlockCacher::create_cacher(const std::string cacher_type, Config* config) {
  if (cacher_type == "kv")
    return new KVBlockCacher(config->configValues["cache_dir_dev"],
                             config->configValues["cache_dir_run"],
                             std::stoi(config->configValues["cache_total_size"]),
                             std::stoi(config->configValues["object_size"]), NULL);

  return new SimpleBlockCacher(config->configValues["cache_dir_dev"],
                               config->configValues["cache_dir_run"],
                               std::stoi(config->configValues["cache_total_size"]),
                               std::stoi(config->configValues["object_size"]), NULL);

}
}
