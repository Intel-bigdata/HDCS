#ifndef HS_CONFIG_H
#define HS_CONFIG_H

#include <map>
#include <string>
#include <vector>
#include <sstream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "Mempool.h"

#define HS_OK        0
#define HS_ERR      -1
#define HS_EAGIN    -2
#define HS_ENOMEM   -3


namespace rbc {


class Config{

public:
    typedef std::map<std::string, std::string> ConfigInfo;
    ConfigInfo configValues{
        {"master_ip",  "127.0.0.1"},
        {"messenger_port",  "9090"},
        {"replication_num", "3"},  // default amount of replication.          
        {"enable_MemoryUsageTracker","false"},
        {"cache_dir","/mnt/hyperstash_0/"},
        {"datastore_type","simple"},
        {"object_size","4096"},
        {"cache_total_size","10737418240"},
        {"cache_dirty_ratio_min","0.85"},
        {"cache_ratio_max","0.9"},
        {"cache_ratio_health","0.85"},
        {"cache_flush_interval","1"},
        {"cache_evict_interval","1"},
        {"cache_flush_queue_depth","64"},
        {"agent_threads_num","64"},
        {"cacheservice_threads_num","64"},
        {"log_to_file","false"}
    };
    std::vector<std::string> slave_ip_vec;
    std::vector<std::string> slave_port_vec;

    Config(std::string rbd_name, bool if_master=false){

        const std::string cfg_file = "/etc/rbc/general.conf";
        boost::property_tree::ptree pt;
        try {
            boost::property_tree::ini_parser::read_ini(cfg_file, pt);
        } catch(...) {
            std::cout << "error when reading: " << cfg_file
                      << ", config file for missing?" << std::endl;
            // assume general.conf should be created by admin manually
            assert(0);
        }

        std::string s;
        std::string k;
        for (ConfigInfo::const_iterator it = configValues.begin(); it!=configValues.end(); it++) {
            try {
                s = pt.get<std::string>(rbd_name + "." + it->first);
            } catch(...) {
                try {
                    s = pt.get<std::string>("global." + it->first);
                } catch(...) {
                    continue;
                }
                if ((it->first == "log_to_file")&&(s != "false")) {
                    pt.put(rbd_name + "." + it->first, s + "_" + rbd_name + ".log");
                    s = s + "_" + rbd_name + ".log";
                } else {
                    pt.put(rbd_name + "." + it->first, s);
                }
            }
            if (s == "") {
                s = pt.get<std::string>("global." + it->first);
                pt.put(rbd_name + "." + it->first, s);
            }
            configValues[it->first] = s;
            s = "";
        }

        // accoding to replication_num, load ip and port of slave from config file.
        if(if_master){
            int replica_num=std::stoi(configValues["replication_num"]);
            for( int i=0; i<replica_num-1; i++ ){
                std::stringstream temp;
                temp<<i;
                try{
                    s = pt.get<std::string>( rbd_name + "." + "slave_ip[" + temp.str() + "]");
                    k = pt.get<std::string>( rbd_name + "." + "slave_messenger_port[" + temp.str() + "]");
                }catch(...){
                    std::cout<<"when reading ip and port of slave, fails occur."<<std::endl;
                    assert(0);
                }
                slave_ip_vec.push_back(s);
                slave_port_vec.push_back(k);
            }
        }
        boost::property_tree::ini_parser::write_ini(cfg_file, pt);

        configValues["cache_dir_dev"] = configValues["cache_dir"] + "/" + rbd_name + "_cache.data";
        configValues["cache_dir_meta"] = configValues["cache_dir"] + "/" + rbd_name + "_meta";
        configValues["cache_dir_run"] = configValues["cache_dir"] + "/" + rbd_name + "_run";
    }
    ~Config(){
    }
};

}

#endif
