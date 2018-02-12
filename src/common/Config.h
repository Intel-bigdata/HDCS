#ifndef HDCS_CONFIG_H
#define HDCS_CONFIG_H

#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

namespace hdcs {
#define HDCS_OK        0
#define HDCS_ERR      -1
#define HDCS_EAGIN    -2
#define HDCS_ENOMEM   -3

typedef std::map<std::string, std::string> ConfigInfo;
class Config{

public:
  Config(std::string name, std::string config_name="/etc/hdcs/general.conf"): name(name) {
      const std::string cfg_file = config_name;
      try {
          boost::property_tree::ini_parser::read_ini(cfg_file, pt);
      } catch(const std::exception &exc) {
          std::cout << "error when reading: " << cfg_file
                    << ", error: " << exc.what() << std::endl;
      }
  }

  ~Config(){
  }

  std::string indent(int level) {
    std::string s; 
    for (int i=0; i<level; i++) s += "  ";
    return s; 
  } 
  
  void printTree (boost::property_tree::ptree &pt, int level) {
    if (pt.empty()) {
      std::cout << "\""<< pt.data()<< "\"";
    }
  
    else {
      if (level) std::cout << std::endl; 
  
      std::cout << indent(level) << "{" << std::endl;     
  
      for (boost::property_tree::ptree::iterator pos = pt.begin(); pos != pt.end();) {
        std::cout << indent(level+1) << "\"" << pos->first << "\": "; 
  
        printTree(pos->second, level + 1); 
        ++pos; 
        if (pos != pt.end()) {
          std::cout << ","; 
        }
        std::cout << std::endl;
      } 
  
     std::cout << indent(level) << " }";     
    }
  
    return; 
  }

  ConfigInfo get_config (std::string type = "HDCSCore") {

    if (type.compare("HDCSManager") == 0) {
      ConfigInfo configValues{
        {"hdcs_HAManager_name", "HDCSManager"},
        {"ha_heartbeat_listen_port", "10000"},
        {"hdcs_replication_count", "3"},
        {"status_check_timeout", "30000000000"},
        {"layback_domain_distribute_timeout", "10000000000"},
      };
      std::string s;
      for (ConfigInfo::const_iterator it = configValues.begin(); it!=configValues.end(); it++) {
          try {
              s = pt.get<std::string>("global." + it->first);
          } catch(...) {
              continue;
          }
          if (s != "") {
              configValues[it->first] = s;
          }
          s = "";
      }
      //generate host list
      std::string host_list = "";
      for (boost::property_tree::ptree::iterator pos = pt.begin(); pos != pt.end(); pos++) {
        if (pos->first != "global") {
          if (host_list.size() == 0)
            host_list = pos->first;
          else
            host_list += "," + pos->first;
          try {
          configValues[pos->first] = pt.get<std::string>(pos->first + ".adm_addr");
          } catch (...) {
            continue;
          }
        }
      }
      configValues["hdcs_host_list"] = host_list;
      for (auto &it : configValues) {
          std::cout << it.first << " : " << it.second << std::endl;
      }
      return configValues;
    } else if (type.compare("HDCSController") == 0) {
      ConfigInfo configValues{
        {"cfg_file_path","/etc/hdcs/general.conf"},
        {"log_to_file","false"},
      };
      std::string s;
      for (ConfigInfo::const_iterator it = configValues.begin(); it!=configValues.end(); it++) {
          try {
              s = pt.get<std::string>("global." + it->first);
          } catch(...) {
              continue;
          }
          if (s != "") {
              configValues[it->first] = s;
          }
          s = "";
      }
      for (boost::property_tree::ptree::iterator pos = pt.begin(); pos != pt.end(); pos++) {
        if (pos->first != "global") {
          try {
            configValues[pos->first] = pt.get<std::string>(pos->first + ".data_addr");
          } catch (...) {
            continue;
          }
        }
      }
      for (auto &it : configValues) {
          std::cout << it.first << " : " << it.second << std::endl;
      }
      return configValues;
    } else if (type.compare("HDCSCore") == 0) {
      ConfigInfo configValues{
        {"rbd_pool_name","rbd"},
        {"rbd_volume_name","volume_1"},
        {"cache_dir","/tmp/"},
        {"cache_mode","write_back"},
        {"policy_mode","tier"},
        {"total_size","10737418240"},
        {"cache_total_size","10737418240"},
        {"cache_ratio_health","0.85"},
        {"cache_dirty_timeout_nanoseconds", "10000000000"},
        {"cache_min_alloc_size","4096"},
        {"op_threads_num","64"},
        {"engine_type","simple"},
      };
      //scan global section, overwrite default config
      std::string s;
      for (ConfigInfo::const_iterator it = configValues.begin(); it!=configValues.end(); it++) {
          try {
              s = pt.get<std::string>("global." + it->first);
          } catch(...) {
              continue;
          }
          if (s != "") {
              configValues[it->first] = s;
          }
          s = "";
      }

      configValues["rbd_volume_name"] = name;
      configValues["cache_dir_dev"] = configValues["cache_dir"] + "/" + name + "_cache.data";
      configValues["cache_dir_meta"] = configValues["cache_dir"] + "/" + name + "_meta";
      configValues["cache_dir_run"] = configValues["cache_dir"] + "/" + name + "_run";

      /*for (auto &it : configValues) {
          std::cout << it.first << " : " << it.second << std::endl;
      }*/
      return configValues;
    } else if (type.compare("HDCSClient") == 0) {
      ConfigInfo configValues{
        {"addr","127.0.0.1:9000"},
      };
      std::string s;
      for (boost::property_tree::ptree::iterator pos = pt.begin(); pos != pt.end(); pos ++) {
        if (pos->first != name) {
          continue;
        }
        try {
          configValues["addr"] = pt.get<std::string>(pos->first + ".addr");
        } catch (...) {
          continue;
        }
      }
      for (auto &it : configValues) {
          std::cout << it.first << " : " << it.second << std::endl;
      }
      return configValues;
    }
    ConfigInfo tmp;
    return tmp; 
  }
private:
  std::string name;
  boost::property_tree::ptree pt;
};


}

#endif
