#ifndef HDCS_CONFIG_H
#define HDCS_CONFIG_H

#include <map>
#include <string>
#include <vector>
#include <iostream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#define HDCS_OK        0
#define HDCS_ERR      -1
#define HDCS_EAGIN    -2
#define HDCS_ENOMEM   -3


namespace hdcs {

class Config{

public:
    typedef std::map<std::string, std::string> ConfigInfo;
    ConfigInfo configValues{
        {"log_to_file","false"},
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
        {"local_port","9000"},
        {"role","hdcs_replica"},
        {"replication_nodes",  "192.168.3.100:9090, 192.168.3.100:9091"},
    };
    Config(std::string name, std::string config_name="general.conf"){

      std::cout << "config_file path: " << config_name << std::endl;
        const std::string cfg_file = config_name;
        boost::property_tree::ptree pt;
        try {
            boost::property_tree::ini_parser::read_ini(cfg_file, pt);
            std::cout<<"success"<<std::endl;
        } catch(...) {
            std::cout << "error when reading: " << cfg_file
                      << ", config file for missing?" << std::endl;
            // assume general.conf should be created by admin manually
            assert(0);
        }

        std::string s;
        for (ConfigInfo::const_iterator it = configValues.begin(); it!=configValues.end(); it++) {
            try {
                s = pt.get<std::string>(name + "." + it->first);
            } catch(...) {
                try {
                    s = pt.get<std::string>("global." + it->first);
                } catch(...) {
                    continue;
                }
                if ((it->first == "log_to_file")&&(s != "false")) {
                    pt.put(name + "." + it->first, s + "_" + name + ".log");
                    s = s + "_" + name + ".log";
                } else {
                    pt.put(name + "." + it->first, s);
                }
            }
            if (s == "") {
                s = pt.get<std::string>("global." + it->first);
                pt.put(name + "." + it->first, s);
            }
            configValues[it->first] = s;
            s = "";
        }
        boost::property_tree::ini_parser::write_ini(cfg_file, pt);

        configValues["cache_dir_dev"] = configValues["cache_dir"] + "/" + name + "_cache.data";
        configValues["cache_dir_meta"] = configValues["cache_dir"] + "/" + name + "_meta";
        configValues["cache_dir_run"] = configValues["cache_dir"] + "/" + name + "_run";

        for (auto &it : configValues) {
            std::cout << it.first << " : " << it.second << std::endl;

        }
    }
    ~Config(){
    }
};

}

#endif
