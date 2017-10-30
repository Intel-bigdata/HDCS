//Copyright [2017] <Intel>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*callback_t)(void* comp, void *arg);
typedef void* hdcs_completion_t;

void hdcs_aio_release(hdcs_completion_t c);
void hdcs_aio_wait_for_complete(hdcs_completion_t c);
ssize_t hdcs_aio_get_return_value(hdcs_completion_t c);
int hdcs_aio_create_completion(void *cb_arg, callback_t complete_cb, hdcs_completion_t *c);

int hdcs_open(void* *io, char* name);
int hdcs_close(void* io);
int hdcs_aio_read(void* io, char* data, uint64_t offset, uint64_t length, hdcs_completion_t c);
int hdcs_aio_write(void* io, const char* data, uint64_t offset, uint64_t length, hdcs_completion_t c );
int hdcs_promote_all(void* io);
int hdcs_flush_all(void* io);

#ifdef __cplusplus
} //end extern "C"
#endif
