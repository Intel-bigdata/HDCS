#ifndef NETWORKING_COMMON
#define NETWORKING_COMMON

#include <memory>
#include <string>
#include <iostream>
#include <boost/asio.hpp>
#include <set>

namespace hdcs{
namespace networking{

#define IO_SERVICE_NUM 15 
#define THREAD_NUM_OF_ONE_IOS 10

#ifndef ASIO_ACCEPTOR
class asio_acceptor;
#endif

#ifndef SERVER
class server;
#endif

#ifndef CLIENT 
class Client;
#endif

class Session;

#ifndef ASIO_SESSION
class asio_session;
#endif

#ifndef IO_POOL 
class io_service_pool;
#endif

//typedef std::shared_ptr<Session> SessionPtr;
typedef Session* SessionPtr;
typedef std::set<Session*> SessionSet;

// callback function at server layer.
typedef void(server::*OnSentServerDefault)(int, uint64_t, void*);
typedef void(*OnSentServer)(int, uint64_t, void*); // callback function of send.


// callback function at client layer
typedef void(Client::*OnSentClient)(int, Session*, void*);
typedef void(Client::*OnReceivedClient)(int, Session*, char*, uint64_t);


typedef std::function<void(void*, std::string)> ProcessMsg;
//typedef void(*ProcessMsg)(void*, std::string);
//typedef ssize_t(*ProcessMsgClient)(char*);
typedef std::function<void(void*, std::string)> ProcessMsgClient;


typedef boost::asio::io_service IOService;
typedef std::shared_ptr<IOService> IOServicePtr;

typedef boost::asio::io_service::work IOServiceWork;
typedef std::shared_ptr<IOServiceWork> IOServiceWorkPtr;

typedef std::unique_ptr<asio_acceptor> AsioAcceptorPtr;
typedef std::shared_ptr<io_service_pool> IOServicePoolPtr;

}
}


#ifndef CHECK_SESSION_INTERVAL
#define CHECK_SESSION_INTERVAL 60
#endif

#endif
