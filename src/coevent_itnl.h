// file encoding: UTF-8

#ifndef __CO_EVENT_ITNL_H__
#define __CO_EVENT_ITNL_H__

#include <sys/time.h>

namespace andrewmc {
namespace libcoevent {

// print()
#define CFG_PRINT_BUFF_SIZE     (1024)
ssize_t print(int fd, const char *format, ...);

#if DEBUG_FLAG
#define DEBUG(fmt, args...)   print(1, "DEBUG: %s", fmt, ##args)
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


}   // end of namespace libcoevent
}   // end of namespace andrewmc
#endif  // EOF
