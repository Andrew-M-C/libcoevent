// file encoding: UTF-8

#ifndef __CO_EVENT_ITNL_H__
#define __CO_EVENT_ITNL_H__

#include "co_routine_inner.h"
#include "co_routine.h"
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>

namespace andrewmc {
namespace libcoevent {

// forever
#define FOREVER_SECONDS         (3600*24*365*10)

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
struct timeval to_timeval_from_milisecs(unsigned milisecs);

// to_double()
double to_double(struct timeval &time);

// libco ext
BOOL is_coroutine_end(const struct stCoRoutine_t *routine);
BOOL is_coroutine_started(const struct stCoRoutine_t *routine);

// fd settings
int set_fd_nonblock(int fd);

// recvfrom()
ssize_t recv_from(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);

// libevent flag check
BOOL event_is_timeout(uint32_t libevent_what);
BOOL event_readable(uint32_t libevent_what);

// struct sockaddr conversion
void convert_str_to_sockaddr_in(const std::string &str, unsigned port, struct sockaddr_in *addr_out);
void convert_str_to_sockaddr_in6(const std::string &str, unsigned port, struct sockaddr_in6 *addr_out);

}   // end of namespace libcoevent
}   // end of namespace andrewmc
#endif  // EOF
