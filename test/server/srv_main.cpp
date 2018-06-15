
#include "coserver.h"
#include <stdio.h>
#include <string>

#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>

using namespace andrewmc::libcoserver;
#define _UDP_PORT       (2333)
#define _UDP_PORT_2     (6666)

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
        dateLen = sprintf(buff, "%04d-%02d-%02d, %02d:%02d:%02d.%06ld ", 
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


// ==========
#define __serverS
#ifdef __serverS

static void _main_server_routine(evutil_socket_t fd, server *abs_server, void *arg)
{
    UDPServer *server = (UDPServer *)abs_server;
    const size_t BUFF_LEN = 2048;
    struct Error status;
    size_t read_len = 0;
    uint8_t data_buff[BUFF_LEN + 1];
    data_buff[BUFF_LEN] = (uint8_t)0;
    BOOL should_quit = FALSE;
    std::string client_addr;

    LOG("Start server, binded at Port %d", server->port());
    LOG("Now sleep(1.5)");
    server->sleep(1.5);
    
    LOG("Now recv");
    do {
        should_quit = FALSE;
        status = server->recv(data_buff, BUFF_LEN, &read_len, 10);
        if (status.is_timeout()) {
            LOG("Timeout, wait again");
        }
        else if (status.is_error()) {
            LOG("server error: %s", status.c_err_msg());
            should_quit = TRUE;
        }
        else {
            data_buff[read_len] = '\0';
            server->copy_client_addr(client_addr);
            LOG("Got message from '%s', length %u, msg: '%s'", client_addr.c_str(), (unsigned)read_len, (char*)data_buff);
            if (0 == strcmp((char *)data_buff, "quit")) {
                should_quit = TRUE;
            }
        }
    } while(FALSE == should_quit);
    
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
    UDPServer *server = new UDPServer;
    LOG("Hello, libcoserver! Base: %s", base->identifier().c_str());

    server->init(base, _main_server_routine, NetIPv4, _UDP_PORT);
    base->run();

    LOG("libcoserver base ends");
    delete base;
    base = NULL;
    server = NULL;

    return 0;
}

#endif  // end of libcoserver::Base

