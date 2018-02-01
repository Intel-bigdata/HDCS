#ifndef SESSION_ARG
#define SESSION_ARG

namespace hdcs{
namespace networking{

// At HDCS, SessionArg will be passed to HDCS,
// then re-passed back to networking layer. 
class SessionArg{
public:
    SessionArg(const void* s_id, uint64_t _seq)
        :session_id(s_id),seq_id(_seq)
    {}

    SessionArg(const void* s_id)
        :session_id(s_id)
    {}

    ~SessionArg(){}

    void set_seq_id(uint64_t _seq_id){
        seq_id = _seq_id;
    }

    uint64_t get_seq_id(){
        return seq_id;
    }

    const void* get_session_id(){
        return session_id;
    }        

private:
    const void* session_id;
    uint64_t seq_id;
};

}
}
#endif
