#ifndef AIO_COMPLETE
#define AIO_COMPLETE

namespace hdcs{
namespace networking{

class aio_complete{
public:
    aio_complete(){}
    virtual ~aio_complete(){}
    virtual void complete( int, uint64_t, char*, void* )=0;
};
}
}
#endif
