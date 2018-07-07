
#include "coevent.h"
#include "cpp_tools.h"
#include <stdio.h>
#include <string>

#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <arpa/inet.h>

using namespace andrewmc::libcoevent;
using namespace andrewmc::cpptools;

#define _UDP_PORT       (2333)

#define LOG(fmt, args...)   _print("SERVER: %s, %s, %d: "fmt, __FILE__, __func__, __LINE__, ##args)
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
#define __UDP_SERVER
#ifdef __UDP_SERVER


static void _udp_session_routine(evutil_socket_t fd, Event *abs_server, void *arg)
{
    UDPSession *session = (UDPSession *)abs_server;
    struct Error status;

    ::andrewmc::cpptools::Data data_buff;
    size_t recv_len = 0;

    // first of all, read client request
    status = session->recv(data_buff.mutable_raw_data(), data_buff.buff_capacity(), &recv_len, 2.0);
    if (FALSE == status.is_ok()) {
        LOG("sesssion recv error: %s", status.c_err_msg());
        return;
    }

    // read and handle DNS request
    data_buff.set_raw_data_length(recv_len ++);
    data_buff.append_nul();
    LOG("Get client request: %s", ::andrewmc::cpptools::dump_data_to_string(data_buff).c_str());

    DNSClient *dns = session->new_DNS_client(session->network_type());
    if (NULL == dns) {
        LOG("Failed to create DNS client");
        return;
    }

    status = dns->resolve((char *)(data_buff.c_data()), 2.0);
    if (FALSE == status.is_ok()) {
        LOG("Failed to resolve DNS: %s", status.c_err_msg());
        session->delete_client(dns);
        return;
    }

    // read DNS response
    std::string dns_result_str = "resource not found";
    const DNSResult *dns_result = dns->dns_result((char *)(data_buff.c_data()));

    for (size_t index = 0; index < dns_result->resource_record_count(); index ++)
    {
        const DNSResourceRecord *rr = dns_result->resource_record(index);
        if (DnsRRType_IPv4Addr == rr->record_type())
        {
            dns_result_str.assign(rr->record_address());
            break;
        }
    }

    // send DNS result back
    status = session->reply(dns_result_str.c_str(), dns_result_str.length());
    if (status.is_error()) {
        LOG("Failed to reply DNS result: %s", status.c_err_msg());
        return;
    }

    // DNS resolve OK
    session->delete_client(dns);
    dns = NULL;

    // end
    LOG("Session ends");
    return;
}


#endif

// ==========
#define __MAIN
#ifdef __MAIN

int main(int argc, char *argv[])
{
    Base *base = new Base;
    UDPServer *server_A = new UDPServer;
    LOG("Hello, libcoevent! Base: %s", base->identifier().c_str());

    server_A->init_session_mode(base, _udp_session_routine, NetIPv4, _UDP_PORT, base);
    base->run();

    LOG("libcoevent base ends");
    delete base;
    base = NULL;
    server_A = NULL;

    return 0;
}

#endif  // end of libcoevent::Base

