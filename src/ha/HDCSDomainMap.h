#ifndef HDCSDOMAINMAP_H
#define HDCSDOMAINMAP_H

#include "ha/CrushMapGenerator.h"
#include "ha/HDCSDomainMapRequestHandler.h"
#include <iostream>
#include <sstream>

namespace hdcs {
namespace ha {

class HDCSDomainMap {
public:
  HDCSDomainMap(std::vector<std::string> host_list, uint8_t replication_count):
    replication_count(replication_count),
    cmap(replication_count - 1) {
    for (auto &it : host_list) {
      // only insert slave
      if (it.compare(it.size()-5, 5, "slave") == 0) {
        domain_map[it.substr(0, it.size() - 6)] = {};
      } else {
        online_node_list.push_back(it);
      }
    }
  }

  ~HDCSDomainMap() {

  }

  void generate_domain_map () {
    HDCS_DOMAIN_MAP_TYPE tmp_domain_map;
    for (auto &item : domain_map) {
      tmp_domain_map[item.first + "_slave"] = {};
    }
    cmap.build(tmp_domain_map);
    refresh_domain_map();
  } 

  void offline_host (std::string host) {
    if (host.compare(host.size()-5, 5, "slave") == 0) {
      cmap.offline_host(host);
    } else {
      auto it = std::find(online_node_list.begin(), online_node_list.end(), host);
      if (it != online_node_list.end()) {
        online_node_list.erase(it);
      }
    }
  }

  void online_host (std::string host) {
    if (host.compare(host.size()-5, 5, "slave") == 0) {
      cmap.online_host(host);
    } else {
      auto it = std::find(online_node_list.begin(), online_node_list.end(), host);
      if (it == online_node_list.end()) {
        online_node_list.push_back(host);
      }
    }
  }

  void refresh_domain_map () {
    HDCS_DOMAIN_MAP_TYPE tmp_domain_map = cmap.get();
    for (auto &item : tmp_domain_map) {
      // check tailed with 'slave'
      std::string host = item.first;
      assert (host.compare(host.size()-5, 5, "slave") == 0);
      host = host.substr(0, host.size() - 6);
      domain_map[host].clear();
      if (std::find(online_node_list.begin(), online_node_list.end(), host) != online_node_list.end()) {
        domain_map[host].push_back(host);
        domain_map[host].insert(domain_map[host].end(), item.second.begin(), item.second.end());
      }
    }
  }

  HDCS_DOMAIN_ITEM_TYPE get_host_domain_item (std::string host_id) {
    // only serve for master
    HDCS_DOMAIN_ITEM_TYPE new_item;
    if (host_id.compare(host_id.size()-5, 5, "slave") == 0) {
      return new_item;
    }
    auto it = domain_map.find(host_id);
    if (it == domain_map.end()) {
      new_item.resize(replication_count);
      auto new_it = domain_map.insert(std::pair<std::string, HDCS_DOMAIN_ITEM_TYPE>(host_id, std::move(new_item))); 
      return new_it.first->second;
    } else {
      for (auto &host_name : it->second) {
        new_item.push_back(host_name);
      }
      return new_item;
    }
  }

  uint8_t get_replication_count () {
    return replication_count;
  }

  std::string printToString () {
    std::stringstream ss;
    for (auto &it : domain_map) {
      ss << "Host: " << (it.first) << " -> {";
      ss << printToString(it.second);
      ss << "}" << std::endl;
    }
    return ss.str();
  }

  std::string printToString (HDCS_DOMAIN_ITEM_TYPE domain_item) {
    std::stringstream ss;
    bool first = true;
    for (auto &it : domain_item) {
      if (!first) {
        ss << ", ";
      } else {
        first = false;
      }
      ss << it;
    }
    return ss.str();
  }

  std::string printToString_host_weights () {
    return cmap.printToString_weight();
  }

private:
  HDCS_DOMAIN_MAP_TYPE domain_map;
  uint8_t replication_count;
  CrushMapGenerator cmap;
  std::vector<std::string> online_node_list;
}; 

}// ha
}// hdcs

#endif
