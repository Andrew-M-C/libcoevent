
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
#define _TCP_PORT       (6666)

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

    // check if this is a quit command
    const char QUIT_COMMAND[] = "quit";
    if (0 == strcmp(QUIT_COMMAND, (char *)data_buff.c_data()))
    {
        UDPServer *server = (UDPServer *)arg;
        LOG("Got quit request");
        server->quit_session_mode_server();
        return;
    }

    // send DNS request
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

    // wait for client's goodbye
    data_buff.set_length(0);
    status = session->recv(data_buff.mutable_raw_data(), data_buff.buff_capacity(), &recv_len, 2.0);
    if (FALSE == status.is_ok()) {
        LOG("sesssion recv error: %s", status.c_err_msg());
        return;
    }
    data_buff.set_raw_data_length(recv_len ++);
    data_buff.append_nul();
    LOG("Get message, length %u: %s", (unsigned)recv_len, (char *)(data_buff.c_data()));

    // send goodbye back
    const char goodbye[] = "Goodbye!";
    session->reply(goodbye, sizeof(goodbye));

    // end
    LOG("Session ends");
    return;
}


#endif


// ==========
#define __TCP_SERVER
#ifdef __TCP_SERVER

static void _tcp_session_routine(evutil_socket_t fd, Event *abs_server, void *arg)
{
    TCPSession *session = (TCPSession *)abs_server;
    struct Error status;
    size_t recv_len = 0;

    ::andrewmc::cpptools::Data data_buff;

    status = session->recv(data_buff.mutable_raw_data(), data_buff.buff_capacity(), &recv_len, 2.0);
    if (FALSE == status.is_ok()) {
        LOG("TCP session error: %s", status.c_err_msg());
        return;
    }

    data_buff.set_raw_data_length(recv_len);
    data_buff.append_nul();

    // read data
    LOG("Got TCP session data: %s", ::andrewmc::cpptools::dump_data_to_string(data_buff).c_str());
    if (0 == strcmp((char *)(data_buff.c_data()), "quit")) {
        LOG("Now quit server");
        TCPServer *server = (TCPServer *)arg;
        server->quit_session_mode_server();
    }

    // reply
    LOG("Now reply");
    const char reply_msg[] = "Thank you for your TCP stream.";
    session->reply(reply_msg, sizeof(reply_msg), &recv_len);
    LOG("%u byte(s) sent", (unsigned)recv_len);

    // end
    LOG("TCP session ends");
    return;
}

#endif


// ==========
#define __SIMPLE_TEST_ROUTINE
#ifdef __SIMPLE_TEST_ROUTINE

static void _simple_test_routine(evutil_socket_t fd, Event *abs_server, void *arg)
{
    NoServer *routine = (NoServer *)abs_server;
    std::string server_address;

    LOG("Routine starts");
    routine->sleep(1.0);
    LOG("Routine awake");

    // do DNS request
    {
        DNSClient *dns_client = routine->new_DNS_client(NetIPv4);
        if (NULL == dns_client) {
            LOG("Cannot get DNS client");
            return;
        }

        const char *domain_name = "www.ccb.com";
        server_address = dns_client->quick_resolve(domain_name, 5.0);
        if (0 == server_address.length()) {
            LOG("Failed to resolve domain address");
            return;
        }

        LOG("IP address for %s: %s", domain_name, server_address.c_str());
        routine->delete_client(dns_client);
    }

    routine->sleep(1.0);
    LOG("Test routine ends");
    return;
}


#endif  // end of __SIMPLE_TEST_ROUTINE

// ==========
#define __MAIN
#ifdef __MAIN

int main(int argc, char *argv[])
{
    Base *base = new Base;
    Error status;
    UDPServer *server_A = new UDPServer;
    TCPServer *server_B = new TCPServer;
    NoServer *dummy_server = new NoServer;
    LOG("Hello, libcoevent! Base: %s", base->identifier().c_str());

    status = server_A->init_session_mode(base, _udp_session_routine, NetIPv4, _UDP_PORT, server_A);
    if (FALSE == status.is_ok()) {
        LOG("Init UDP server failed: %s", status.c_err_msg());
        goto END;
    }

    status = server_B->init_session_mode(base, _tcp_session_routine, NetIPv4, _TCP_PORT, server_B);
    if (FALSE == status.is_ok()) {
        LOG("Init TCP server failed: %s", status.c_err_msg());
        goto END;
    }

    status = dummy_server->init(base, _simple_test_routine);
    if (FALSE == status.is_ok()) {
        LOG("Init test routine failed: %s", status.c_err_msg());
        goto END;
    }

    base->run();

END:
    LOG("libcoevent base ends");
    delete base;
    base = NULL;
    server_A = NULL;
    server_B = NULL;

    return 0;
}

#endif  // end of libcoevent::Base

