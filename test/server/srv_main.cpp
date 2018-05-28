
#include "coevent.h"
#include <stdio.h>
#include <string>

#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>

#define LOG(fmt, args...)   _print("SERVER: %s, %d: "fmt, __FILE__, __LINE__, ##args)
static ssize_t _print(const char *format, ...)
{
    char buff[1024];
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
        dateLen = sprintf(buff, "%04d-%02d-%02d,%02d:%02d:%02d.%06ld ", 
                            currTime.tm_year + 1900, currTime.tm_mon + 1, currTime.tm_mday,
                            currTime.tm_hour, currTime.tm_min, currTime.tm_sec, currDayTime.tv_usec);
    }

    va_start(vaList, format);
    vsnprintf((char *)(buff + dateLen), sizeof(buff) - dateLen - 1, format, vaList);
    va_end(vaList);

    dateLen = strlen(buff);
    buff[dateLen + 0] = '\n';
    buff[dateLen + 1] = '\0';

    return (write(1, buff, dateLen + 1));
}

using namespace andrewmc::libcoevent;


// ==========
#define __EVENTS
#ifdef __EVENTS

static void _time_routine(evutil_socket_t fd, Event *abs_event, void *arg)
{
    TimerEvent *event = (TimerEvent *)abs_event;

    LOG("Start");
    event->sleep(1);
    LOG("1");
    event->sleep(0.5);
    LOG("2");
    event->sleep(2);
    LOG("END");

    return;
}


#endif

// ==========
#define __MAIN
#ifdef __MAIN

int main(int argc, char *argv[])
{
    Base *base = new Base;
    TimerEvent *event = new TimerEvent;
    LOG("Hello, libcoevent! Identifier: %s", base->identifier().c_str());

    event->add_to_base(base, _time_routine, NULL, TRUE);
    base->run();

    LOG("libcoevent base ends");
    delete base;
    base = NULL;
    event = NULL;

    return 0;
}

#endif  // end of libcoevent::Base

