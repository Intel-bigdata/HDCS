#ifndef CRUSHMAP_GENERATOR_H
#define CRUSHMAP_GENERATOR_H

extern "C" {
#include "libcrush/crush/hash.h"
#include "libcrush/crush/builder.h"
#include "libcrush/crush/mapper.h"
}

#include <map>
#include <vector>
#include <list>
#include <mutex>
#include <sstream>

#define ASSERT_EQ(X, Y) assert(X == Y)
namespace hdcs {
namespace ha {

#define CRUSH_TYPE_ROOT 1
#define CRUSH_TYPE_HOST 2
#define CRUSH_WEIGHT_HIGH 0X10000
#define CRUSH_WEIGHT_LOW  0X00000

typedef std::vector<std::string> HDCS_DOMAIN_ITEM_TYPE;
typedef  std::map<std::string, HDCS_DOMAIN_ITEM_TYPE> HDCS_DOMAIN_MAP_TYPE; 

struct host_item_t {
  int host_index;
  int bucket_no;
  crush_bucket* bucket;
  host_item_t (int host_index) :
    host_index(host_index),
    bucket(nullptr) {
  }
};

class CrushMapGenerator {
public:
  CrushMapGenerator(uint8_t replication_count):
    host_count(0),
    replication_count(replication_count),
    weights(nullptr),
    cwin(nullptr) {
    // init crush map
    m = crush_create();
    root = crush_make_bucket(m, CRUSH_BUCKET_STRAW2, CRUSH_HASH_DEFAULT, CRUSH_TYPE_ROOT,
                                           0, NULL, NULL);
    ASSERT_EQ(0, crush_add_bucket(m, 0, root, &rootno));

    // init rule
    rule = crush_make_rule(3, 0, 0, 0, 0);
    crush_rule_set_step(rule, 0, CRUSH_RULE_TAKE, rootno, 0);
    crush_rule_set_step(rule, 1, CRUSH_RULE_CHOOSELEAF_FIRSTN, 0, CRUSH_TYPE_HOST);
    crush_rule_set_step(rule, 2, CRUSH_RULE_EMIT, 0, 0);

    ruleno = crush_add_rule(m, rule, -1);

  }

  ~CrushMapGenerator() {
    if (cwin) delete[] cwin;
    if (weights) delete[] weights;
    for (auto &it : host_map) {
      delete (it.second);
    }
    crush_destroy(m);
  }

  void build(HDCS_DOMAIN_MAP_TYPE domain_map) {
    for (auto &it : domain_map) {
      add_host(it.first);
    }
    finalize_host_tree ();
  }

  HDCS_DOMAIN_MAP_TYPE get () {
    HDCS_DOMAIN_MAP_TYPE domain_map;
    for (auto &it : host_map) {
      domain_map[it.first] = get_host_domain(it.first);
    }
    return domain_map;
  }

  void offline_host (std::string host_id) {
    auto it = host_map.find(host_id);
    if (it == host_map.end()) {
      return;
    }
    int host_index = it->second->host_index;
    weights[host_index] = 0x00000;
  }

  std::string printToString_weight () {
    std::stringstream ss;
    for (auto& it : host_map) {
      int weight = weights[it.second->host_index];
      ss << "host name: " << it.first << ", weight: 0X" << std::hex << weight << std::endl;
    }
    return ss.str();
  }

  void online_host (std::string host_id) {
    auto it = host_map.find(host_id);
    if (it == host_map.end()) {
      return;
    }
    int host_index = it->second->host_index;
    weights[host_index] = 0x10000;
  }

private:
  std::mutex crushmap_mutex;
  uint8_t replication_count;
  crush_map* m;
  crush_bucket* root;
  int rootno;

  uint32_t* weights;
  char* cwin;

  struct crush_rule *rule;
  int ruleno;

  std::map<std::string, host_item_t*> host_map;
  uint32_t host_count;

  void finalize_host_tree () {
    // init weights
    weights = new uint32_t[host_count];
    for (int i = 0; i < host_count; i++) {
      weights[i] = 0x10000;
    }

    // generate crush map
    crush_finalize(m);
    int cwin_size = crush_work_size(m, replication_count);
    cwin = new char[cwin_size];
    crush_init_workspace(m, cwin);
  }

  void add_host (std::string host_id) {
    int host_index = 0;
    auto it = host_map.find(host_id);
    if (it == host_map.end()) {
      host_index = host_count++;
      auto ret = host_map.insert(std::pair<std::string, host_item_t*>(host_id, new host_item_t(host_index)));
      it = ret.first;
    } else {
      host_index = it->second->host_index;
    }
    int* bucket_no_ptr = &(it->second->bucket_no);
    crush_bucket** bucket_ptr = &(it->second->bucket);

    int weights[1] = {CRUSH_WEIGHT_HIGH};
    int items[1] = {host_index};
    *bucket_ptr = crush_make_bucket(m,
                               CRUSH_BUCKET_STRAW2,
                               CRUSH_HASH_DEFAULT,
                               CRUSH_TYPE_HOST,
                               1,// bucket_size
                               items, //bucket_items_array
                               weights //bucket_weights_array
                              );
    ASSERT_EQ(0, crush_add_bucket(m, 0, *bucket_ptr, bucket_no_ptr));
    ASSERT_EQ(0, crush_bucket_add_item(m, root, *bucket_no_ptr, (*bucket_ptr)->weight));
  }

  HDCS_DOMAIN_ITEM_TYPE get_host_domain (std::string host_id) {
    HDCS_DOMAIN_ITEM_TYPE domain_item;
    std::lock_guard<std::mutex> lock(crushmap_mutex);  

    auto it = host_map.find(host_id);
    if (it == host_map.end()) {
      return domain_item;
    }
    
    int host_index = it->second->host_index;

    int result_len;
    bool do_reset = true;
    int result[replication_count] = {0};
    if (weights[host_index] == 0x00000) {
      /*domain_item.clear();
      return domain_item;*/
      do_reset = false;
    }
    weights[host_index] = 0x00000;

    result[0] = host_index;
    result_len = crush_do_rule(m, //crushmap
                               ruleno, //rule_number in crushmap
                               host_index, //hash_id
                               result, //result
                               replication_count, //replication_number
                               weights, // weights of each device
                               host_count, // device_count
                               cwin, //crush workspace
                               NULL);

    domain_item.resize(result_len);
    for (auto &it : host_map) {
      for (int i = 0; i < result_len; i++){
        if (it.second->host_index == result[i]) {
          domain_item.at(i) = it.first;
        }
      }
    }

    if (do_reset) {
      weights[host_index] = 0x10000;
    }
    return domain_item;
  }

  
};
}// namespace ha
}// namespace hdcs
#endif
