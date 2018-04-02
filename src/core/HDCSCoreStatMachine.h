#ifndef HDCSCORESTATMACHINE_H
#define HDCSCORESTATMACHINE_H

#include <boost/statechart/event.hpp>
#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/simple_state.hpp>
#include <boost/statechart/transition.hpp>
#include <boost/statechart/custom_reaction.hpp>
#include <ctime>
#include <iostream>
#include "core/HDCSCore.h"

namespace hdcs {
namespace core {

struct EvStatFail : boost::statechart::event< EvStatFail > {};
struct EvStatNext : boost::statechart::event< EvStatNext > {};

struct StatInfo {
  virtual std::string get_stat_name() const=0;
  virtual HDCS_CORE_STAT_TYPE get_stat_code() const=0;
};

class HDCSCoreStatGuard;
struct NotWorking;
struct HDCSCoreStatMachine :
  boost::statechart::state_machine<HDCSCoreStatMachine, NotWorking> {
  private:
    HDCSCore* core;
    std::string host_name;
    std::string volume_name;
    std::string cfg_file_path;
    hdcs_repl_options replication_options;
    HDCSCoreStatGuard* core_stat_guard_ptr;
  public:
    HDCSCoreStatMachine (std::string host,
                         std::string name,
                         std::string cfg_file,
                         hdcs_repl_options replication_options_param,
                         HDCSCoreStatGuard* core_stat_guard_ptr):
                         host_name(host),
                         volume_name(name),
                         cfg_file_path(cfg_file),
                         replication_options(replication_options_param),
                         core_stat_guard_ptr(core_stat_guard_ptr),
                         core(nullptr) {
    }

    std::string get_stat_name () const {
      return state_cast< const StatInfo & >().get_stat_name();
    }

    HDCS_CORE_STAT_TYPE get_stat_code () const {
      return state_cast< const StatInfo & >().get_stat_code();
    }

    bool IsHDCSCoreExists () {
      if (core != nullptr) {
        return true;
      } else {
        core = new HDCSCore(host_name, volume_name, cfg_file_path, core_stat_guard_ptr, std::move(replication_options));
        if (core) {
          return true;
        } else {
          return false;
        }
      }
    }

    bool IsHDCSCoreReady () {
      if (core->check_data_consistency()) {
        return true;
      } else {
        return false;
      }
    }

    bool IsHDCSCorePeered () {
      if (core->get_peered_core_num() >= core->get_min_replica_size()) {
        return true;
      } else {
        return false;
      }
    }

    bool IsHDCSCoreHealthy () {
      if (core->get_peered_core_num() == core->get_replica_size()) {
        return true;
      } else {
        return false;
      }
    }

    HDCSCore* get_hdcs_core () {
      if (IsHDCSCoreExists()) {
        return core;
      } else {
        return nullptr;
      }
    }

};

class HDCSCoreStatGuard {
private:
  HDCSCoreStatMachine stat_machine;
public:
  HDCSCoreStatGuard (
      std::string host,
      std::string name,
      std::string cfg_file,
      hdcs_repl_options &&replication_options_param
      ) : stat_machine(host, name, cfg_file, replication_options_param, this) {
    stat_machine.initiate(); 
  }

  HDCSCore* get_hdcs_core () {
    return stat_machine.get_hdcs_core();
  }

  HDCS_CORE_STAT_TYPE get_hdcs_core_state () {
    HDCS_CORE_STAT_TYPE cur_stat_code;
    do {
      cur_stat_code = stat_machine.get_stat_code();
      stat_machine.process_event(EvStatNext());
    } while(stat_machine.get_stat_code() != cur_stat_code);
    std::cout << "current hdcs core status: " << stat_machine.get_stat_name() << std::endl;
    return cur_stat_code;
  }
};

struct Error;
struct Readonly;

struct NotWorking :
  boost::statechart::simple_state<NotWorking, HDCSCoreStatMachine, Error> {
public:
  typedef boost::statechart::transition< EvStatFail, NotWorking > reactions;

  NotWorking (): statename_("NotWorking"), statecode_(HDCS_CORE_STAT_ERROR) {
  
  }
  virtual std::string get_stat_name() const {
    return statename_;
  }

  virtual HDCS_CORE_STAT_TYPE get_stat_code() const {
    return statecode_;
  }

private:
  std::string statename_;
  HDCS_CORE_STAT_TYPE statecode_;
};

struct Working :
  boost::statechart::simple_state<Working, HDCSCoreStatMachine, Readonly> {
public:
  typedef boost::statechart::transition< EvStatFail, NotWorking > reactions;

  Working (): statename_("Working"), statecode_(HDCS_CORE_STAT_READONLY) {
  
  }
  virtual std::string get_stat_name() const {
    return statename_;
  }

  virtual HDCS_CORE_STAT_TYPE get_stat_code() const {
    return statecode_;
  }

private:
  std::string statename_;
  HDCS_CORE_STAT_TYPE statecode_;
};

struct Healthy : StatInfo, boost::statechart::simple_state< Healthy, Working > {
public:
  Healthy() : statename_("Healthy"), statecode_(HDCS_CORE_STAT_OK) {}

  ~Healthy() {}

  virtual std::string get_stat_name() const {
    return statename_;
  }

  virtual HDCS_CORE_STAT_TYPE get_stat_code() const {
    return statecode_;
  }

private:
  std::string statename_;
  HDCS_CORE_STAT_TYPE statecode_;
};

struct Degraded : StatInfo, boost::statechart::simple_state< Degraded, Working > {
public:
  //typedef boost::statechart::transition< EvStatNext, Healthy > reactions;
  typedef boost::statechart::custom_reaction< EvStatNext > reactions;

  Degraded() : statename_("Degraded"), statecode_(HDCS_CORE_STAT_DEGRADED) {}

  ~Degraded() {}

  virtual std::string get_stat_name() const {
    return statename_;
  }

  virtual HDCS_CORE_STAT_TYPE get_stat_code() const {
    return statecode_;
  }

  boost::statechart::result react(const EvStatNext &) {
    if (context<HDCSCoreStatMachine>().IsHDCSCoreHealthy()) {
      std::cout << "HDCSCore is Healthy, go next" << std::endl;
      return transit< Healthy >();
    } else {
      std::cout << "HDCSCore is expecting more peers" << std::endl;
      return discard_event();
    }
  }
private:
  std::string statename_;
  HDCS_CORE_STAT_TYPE statecode_;
};

struct Readonly : StatInfo, boost::statechart::simple_state< Readonly, Working > {
public:
  //typedef boost::statechart::transition< EvStatNext, Degraded > reactions;
  typedef boost::statechart::custom_reaction< EvStatNext > reactions;

  Readonly() : statename_("Readonly"), statecode_(HDCS_CORE_STAT_READONLY) {}

  ~Readonly() {}

  virtual std::string get_stat_name() const {
    return statename_;
  }
  
  virtual HDCS_CORE_STAT_TYPE get_stat_code() const {
    return statecode_;
  }

  boost::statechart::result react(const EvStatNext &) {
    if (context<HDCSCoreStatMachine>().IsHDCSCorePeered()) {
      std::cout << "HDCSCore has enough peers, go next" << std::endl;
      return transit< Degraded >();
    } else {
      std::cout << "HDCSCore is expecting more peers" << std::endl;
      return discard_event();
    }
  }
private:
  std::string statename_;
  HDCS_CORE_STAT_TYPE statecode_;
};

struct Inconsistent : StatInfo, boost::statechart::simple_state< Inconsistent, NotWorking > {
public:
  //typedef boost::statechart::transition< EvStatNext, Working > reactions;
  typedef boost::statechart::custom_reaction< EvStatNext > reactions;

  Inconsistent() : statename_("Inconsistent"), statecode_(HDCS_CORE_STAT_INCONSISTENT) {}

  ~Inconsistent() {}

  virtual std::string get_stat_name() const {
    return statename_;
  }

  virtual HDCS_CORE_STAT_TYPE get_stat_code() const {
    return statecode_;
  }

  boost::statechart::result react(const EvStatNext &) {
    if (context<HDCSCoreStatMachine>().IsHDCSCoreReady()) {
      std::cout << "HDCSCore is Ready, go next" << std::endl;
      return transit< Working >();
    } else {
      std::cout << "HDCSCore is not ready" << std::endl;
      return discard_event();
    }
  }

private:
  std::string statename_;
  HDCS_CORE_STAT_TYPE statecode_;
};

struct Error : StatInfo, boost::statechart::simple_state< Error, NotWorking > {
public:
  //typedef boost::statechart::transition< EvStatNext, Inconsistent > reactions;
  typedef boost::statechart::custom_reaction< EvStatNext > reactions;

  Error() : statename_("Error"), statecode_(HDCS_CORE_STAT_ERROR) {}

  ~Error() {}

  virtual std::string get_stat_name() const {
    return statename_;
  }

  virtual HDCS_CORE_STAT_TYPE get_stat_code() const {
    return statecode_;
  }

  boost::statechart::result react(const EvStatNext &) {
    if (context<HDCSCoreStatMachine>().IsHDCSCoreExists()) {
      std::cout << "HDCSCore Exists, go next" << std::endl;
      return transit< Inconsistent >();
    } else {
      std::cout << "HDCSCore unable to find" << std::endl;
      return discard_event();
    }
  }
private:
  std::string statename_;
  HDCS_CORE_STAT_TYPE statecode_;
};

}
}
#endif
