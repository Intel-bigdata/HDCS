#ifndef HDCS_HA_CONFIG
#define HDCS_HA_CONFIG

#include <vector>
#include <string>
#include <boost/algorithm/string.hpp>

namespace hdcs {
namespace ha {
typedef std::map<std::string, std::string> ConfigInfo;
class HAConfig {

private:
  ConfigInfo config_data;

public:
  HAConfig (ConfigInfo config):
    config_data(config) {
  }

  ~HAConfig () {
  }

  std::string get(std::string key) {
    return config_data.find(key)->second;
  }

  std::vector<std::string> get_host_list () {
    std::string host_list_iss(get("hdcs_host_list"));
    std::vector<std::string> nodes;
    boost::erase_all(host_list_iss, " ");
    boost::split(nodes, host_list_iss, boost::is_any_of(","));
    return nodes;
  }

  std::string get_host_addr (std::string host_name) {
    auto it = config_data.find(host_name);
    assert (it != config_data.end());
    return it->second;
  }

  std::string get_host_port (std::string host_name) {
    auto it = config_data.find(host_name);
    assert (it != config_data.end());

    std::string host_addr_iss(it->second);
    std::cout << host_addr_iss << std::endl;
    std::vector<std::string> addr;
    boost::erase_all(host_addr_iss, " ");
    boost::split(addr, host_addr_iss, boost::is_any_of(":"));
    return addr[1];
  }

};
}// ha
}// hdcs

#endif
