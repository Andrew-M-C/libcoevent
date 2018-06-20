
#include "coevent.h"
#include "coevent_itnl.h"
#include "event2/event.h"
#include <string>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <arpa/inet.h>

using namespace andrewmc::libcoevent;

// ==========
#define __CO_EVENT_ITNL
#ifdef __CO_EVENT_ITNL

// reference: https://github.com/Andrew-M-C/Linux-Linux_Programming_Template/blob/master/AMCCommonLib.c
ssize_t andrewmc::libcoevent::print(int fd, const char *format, ...)
{
    char buff[CFG_PRINT_BUFF_SIZE];
    va_list vaList;
    size_t dateLen = 0;

    tzset();
    time_t currSec = time(0);
    struct tm currTime;
    struct tm *pTime = localtime(&currSec);
    struct timeval currDayTime;

    gettimeofday(&currDayTime, NULL);
    if (pTime)
    {
        memcpy(&currTime, pTime, sizeof(currTime));
        dateLen = sprintf(buff, "%04d-%02d-%02d, %02d:%02d:%02d.%06ld - ", 
                            currTime.tm_year + 1900, currTime.tm_mon + 1, currTime.tm_mday,
                            currTime.tm_hour, currTime.tm_min, currTime.tm_sec, currDayTime.tv_usec);
    }

    va_start(vaList, format);
    vsnprintf((char *)(buff + dateLen), sizeof(buff) - dateLen - 1, format, vaList);
    va_end(vaList);

    dateLen = strlen(buff);
    buff[dateLen + 0] = '\n';
    buff[dateLen + 1] = '\0';

    return (write(fd, buff, dateLen + 1));
}


struct timeval andrewmc::libcoevent::to_timeval(double seconds)
{
    struct timeval ret;
    if (seconds > 0) {
        ret.tv_sec = (time_t)seconds;
        ret.tv_usec = (suseconds_t)((seconds - ret.tv_sec) * 1000000);
    }
    return ret;
}


struct timeval andrewmc::libcoevent::to_timeval(float seconds)
{
    struct timeval ret;
    if (seconds > 0) {
        ret.tv_sec = (time_t)seconds;
        ret.tv_usec = (suseconds_t)((seconds - ret.tv_sec) * 1000000);
    }
    return ret;
}


struct timeval andrewmc::libcoevent::to_timeval(int seconds)
{
    struct timeval ret;
    if (seconds > 0) {
        ret.tv_sec = (time_t)seconds;
        ret.tv_usec = 0;
    }
    return ret;
}


struct timeval andrewmc::libcoevent::to_timeval(long seconds)
{
    struct timeval ret;
    if (seconds > 0) {
        ret.tv_sec = (time_t)seconds;
        ret.tv_usec = 0;
    }
    return ret;
}


struct timeval andrewmc::libcoevent::to_timeval_from_milisecs(unsigned milisecs)
{
    unsigned secs = milisecs / 1000;
    milisecs = milisecs - secs * 1000;

    struct timeval sleep_time;
    sleep_time.tv_sec = secs;
    sleep_time.tv_usec = milisecs * 1000;

    return sleep_time;
}


double andrewmc::libcoevent::to_double(struct timeval &time)
{
    double ret;
    ret = (double)(time.tv_sec);
    ret += ((double)(time.tv_usec)) / 1000000.0;
    return ret;
}


BOOL andrewmc::libcoevent::is_coroutine_end(const struct stCoRoutine_t *routine)
{
    if (routine) {
        return (routine->cEnd) ? TRUE : FALSE;
    }
    return TRUE;
}


BOOL andrewmc::libcoevent::is_coroutine_started(const struct stCoRoutine_t *routine)
{
    if (routine) {
        return (routine->cStart) ? TRUE : FALSE;
    }
    return FALSE;
}


int andrewmc::libcoevent::set_fd_nonblock(int fd)
{
    int flag = fcntl(fd, F_GETFL, 0);
    if (flag >= 0)
    {
        return fcntl(fd, F_SETFL, flag | O_NONBLOCK);
    }
    else
    {
        return flag;
    }
}


ssize_t andrewmc::libcoevent::recv_from(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
    ssize_t ret = recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
    if (ret < 0) {
        if (EAGAIN == errno) {
            DEBUG("EAGAIN");
            return 0;
        }
    }
    return ret;
}


BOOL andrewmc::libcoevent::event_is_timeout(uint32_t libevent_what)
{
    return (libevent_what & EV_TIMEOUT) ? TRUE : FALSE;
}


BOOL andrewmc::libcoevent::event_readable(uint32_t libevent_what)
{
    return (libevent_what & EV_READ) ? TRUE : FALSE;
}


void andrewmc::libcoevent::convert_str_to_sockaddr_in(const std::string &str, unsigned port, struct sockaddr_in *addr)
{
    if (NULL == addr) {
        return;
    }
    addr->sin_family = AF_INET;
    addr->sin_port = htons((unsigned short)port);
    inet_pton(AF_INET, str.c_str(), &(addr->sin_addr));
    return;
}


void andrewmc::libcoevent::convert_str_to_sockaddr_in6(const std::string &str, unsigned port, struct sockaddr_in6 *addr)
{
    if (NULL == addr) {
        return;
    }
    addr->sin6_family = AF_INET6;
    addr->sin6_port = htons((unsigned short)port);
    inet_pton(AF_INET6, str.c_str(), &(addr->sin6_addr));
    return;
}


void andrewmc::libcoevent::convert_str_to_sockaddr_un(const std::string &str, struct sockaddr_un *addr)
{
    if (NULL == addr) {
        return;
    }
    addr->sun_family = AF_UNIX;
    strncpy(addr->sun_path, str.c_str(), sizeof(addr->sun_path));
    return;
}


#endif  // end of libcoevent::print



