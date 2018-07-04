
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
#define _UDP_PORT_2     (6666)

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

static void _sub_udp_routine(evutil_socket_t fd, Event *abs_server, void *arg);


// ==========
#define __CHILD_CLASS_OF_NO_SERVER
#ifdef __CHILD_CLASS_OF_NO_SERVER

class DNSServer : public NoServer {
protected:
    struct sockaddr_in  _client_addr;
    std::string         _server_name;
public:
    struct sockaddr_in &client_addr() {
        return _client_addr;
    }
    std::string &server_name() {
        return _server_name;
    }
};


#endif

// ==========
#define __SUB_SESSION
#ifdef __SUB_SESSION



#endif


// ==========
#define __UDP_SERVER
#ifdef __UDP_SERVER

static void _main_server_routine(evutil_socket_t fd, Event *abs_server, void *arg)
{
    UDPServer *server = (UDPServer *)abs_server;
    Base *base = (Base *)arg;
    const size_t BUFF_LEN = 2048;
    struct Error status;
    size_t read_len = 0;
    uint8_t data_buff[BUFF_LEN + 1];
    data_buff[BUFF_LEN] = (uint8_t)0;
    BOOL should_quit = FALSE;

    //LOG("Start server, binded at Port %d", server->port());
    //LOG("Now sleep(1.5)");
    //server->sleep(1.5);
    
    LOG("Now recv");
    do {
        should_quit = FALSE;
        status = server->recv(data_buff, BUFF_LEN, &read_len, 0);
        if (status.is_timeout()) {
            LOG("Timeout, wait again");
        }
        else if (status.is_error()) {
            LOG("server error: %s", status.c_err_msg());
            should_quit = TRUE;
        }
        else {
            data_buff[read_len] = '\0';
            LOG("Got message from '%s:%u', length %u, msg: '%s'", server->client_addr().c_str(), server->client_port(), (unsigned)read_len, (char*)data_buff);

            std::string msg_str = dump_data_to_string(data_buff, read_len);
            //LOG("Data detail:\n%s", msg_str.c_str());

            if (0 == strcmp((char *)data_buff, "quit")) {
                should_quit = TRUE;
            }
            else {
                DNSServer *sub_server = new DNSServer;

                server->copy_client_addr((struct sockaddr *)&(sub_server->client_addr()), sizeof(struct sockaddr_in));
                sub_server->server_name().assign((char *)data_buff);
                sub_server->init(base, _sub_udp_routine, NULL);
            }
        }
    } while(FALSE == should_quit);

    // tell second server to quit
    if (0) {
        const char str[] = "quit";
        UDPClient *client = server->new_UDP_client(NetIPv4);
        client->send(str, sizeof(str), NULL, "127.0.0.1", _UDP_PORT_2);
        server->delete_client(client);
        client = NULL;
    }
    else {
        const char str[] = "quit";
        server->send(str, sizeof(str), NULL, "127.0.0.1", _UDP_PORT_2);
    }
    
    LOG("END");
    return;
}


static void _sub_udp_routine(evutil_socket_t fd, Event *abs_server, void *arg)
{
    DNSServer *server = (DNSServer *)abs_server;
    struct Error err;

    DNSClient *dns = server->new_DNS_client(NetIPv4);
    NetType_t type;

    const char *request_name = server->server_name().c_str();
    LOG("parameters sent from client: '%s'", request_name);

    LOG("Default DNS server 1: %s(%d)", dns->default_dns_server(0, &type).c_str(), type);
    LOG("Default DNS server 2: %s(%d)", dns->default_dns_server(1, &type).c_str(), type);

    err = dns->resolve(request_name, 2.0);
    LOG("resolve result: %s", err.c_err_msg());
    LOG("DNS server address: %s:%u", dns->remote_addr().c_str(), dns->remote_port());

    if (err.is_ok())
    {
        std::string ret;
        ret += "resource \"";
        ret += request_name;
        ret += "\" not found.";

        const DNSResult *dns_result = dns->dns_result(request_name);
        for (size_t index = 0; index < dns_result->resource_record_count(); index ++)
        {
            const DNSResourceRecord *rr = dns_result->resource_record(index);
            if (DnsRRType_IPv4Addr == rr->record_type())
            {
                ret.assign(rr->record_address());
                break;
            }
        }

        LOG("Send back message: %s", ret.c_str());
        UDPClient *udp = server->new_UDP_client(NetIPv4);
        udp->send(ret.c_str(), ret.length() + 1, NULL, (const struct sockaddr *)&(server->client_addr()), sizeof(struct sockaddr_in));
    }
    else
    {
        const char *err_msg = err.c_err_msg();
        UDPClient *udp = server->new_UDP_client(NetIPv4);
        udp->send(err_msg, strlen(err_msg) + 1, NULL, (const struct sockaddr *)&(server->client_addr()), sizeof(struct sockaddr_in));
        server->delete_client(udp);
        udp = NULL;
    }

    LOG("Sub service ends");
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

    server_A->init(base, _main_server_routine, NetIPv4, _UDP_PORT, base);
    base->run();

    LOG("libcoevent base ends");
    delete base;
    base = NULL;
    server_A = NULL;

    return 0;
}

#endif  // end of libcoevent::Base

