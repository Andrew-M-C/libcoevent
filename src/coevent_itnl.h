// file encoding: UTF-8

#ifndef __CO_EVENT_ITNL_H__
#define __CO_EVENT_ITNL_H__

#include "co_routine_inner.h"
#include "co_routine.h"
#include <sys/time.h>

namespace andrewmc {
namespace libcoevent {

// print()
#define CFG_PRINT_BUFF_SIZE     (1024)
ssize_t print(int fd, const char *format, ...);

#if DEBUG_FLAG
#define DEBUG(fmt, args...)   print(1, "%s, %s(), %d: "fmt, __FILE__, __func__, __LINE__, ##args)
#else
#define DEBUG(fmt, args...)
#endif

#define INFO(fmt, args...)    print(1, "%s", fmt, ##args)
#define ERROR(fmt, args...)   print(1, "%s", fmt, ##args)

// to_timeval()
struct timeval to_timeval(double seconds);
struct timeval to_timeval(float seconds);
struct timeval to_timeval(int seconds);
struct timeval to_timeval(long seconds);

// to_double()
double to_double(struct timeval &time);

// libco ext
BOOL is_coroutine_end(const struct stCoRoutine_t *routine);
BOOL is_coroutine_started(const struct stCoRoutine_t *routine);


}   // end of namespace libcoevent
}   // end of namespace andrewmc
#endif  // EOF
