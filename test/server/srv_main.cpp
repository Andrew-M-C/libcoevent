
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

static void _sub_udp_routine(evutil_socket_t fd, Event *abs_server, void *arg);

// ==========
#define __SUB_SESSION
#ifdef __SUB_SESSION



#endif


// ========
#define __TEST
#ifdef __TEST

class BaseClass {
public:
    BaseClass() {
        LOG("Base");
    }
    virtual ~BaseClass() {
        LOG("~Base");
    }
};


class ChildClass : public BaseClass {
public:
    ChildClass() {
        LOG("Child");
    }
    ~ChildClass() {
        LOG("~Child");
    }
};


BaseClass *_create_obj()
{
    ChildClass *obj = new ChildClass;
    return obj;
}



static void _test()
{
    LOG("Firstly, let's see a inherance in C++");
    BaseClass *obj = _create_obj();
    delete obj;

    LOG("Now test size: sockaddr(%d), sockaddr_in(%d), sockaddr_in6(%d), sockaddr_un(%d)",
        sizeof(struct sockaddr), sizeof(struct sockaddr_in), sizeof(struct sockaddr_in6), sizeof(struct sockaddr_un));
    LOG("INET_ADDRSTRLEN = %u, INET6_ADDRSTRLEN = %u", INET_ADDRSTRLEN, INET6_ADDRSTRLEN);
    return;
}


#endif  // end of __TEST


// ==========
#define __SERVERS
#ifdef __SERVERS

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
            LOG("Data detail:\n%s", msg_str.c_str());

            if (0 == strcmp((char *)data_buff, "quit")) {
                should_quit = TRUE;
            }
            else {
                struct sockaddr_in *client_addr = NULL;
                struct Error err;
                NoServer *sub_server = new NoServer;

                err = sub_server->create_custom_storage(sizeof(*client_addr));
                if (err.is_OK())
                {
                    LOG("Ready to send back");
                    client_addr = (struct sockaddr_in *)sub_server->custom_storage();
                    server->copy_client_addr((struct sockaddr *)client_addr, sizeof(*client_addr));
                    sub_server->init(base, _sub_udp_routine, NULL);
                }
                else
                {
                    LOG("Now send back");
                    server->reply(data_buff, read_len + 1, NULL);
                }

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


static void _second_server_routine(evutil_socket_t fd, Event *abs_server, void *arg)
{
    UDPServer *server = (UDPServer *)abs_server;
    const size_t BUFF_LEN = 2048;
    struct Error status;
    size_t read_len = 0;
    BOOL should_quit = FALSE;
    uint8_t data_buff[BUFF_LEN + 1];

    unsigned count = 0;

    do {
        should_quit = FALSE;
        count ++;

        server->recv(data_buff, BUFF_LEN, &read_len);
        if (read_len > 0) {
            data_buff[read_len] = '\0';
            if (0 == strcmp((char *)data_buff, "quit")) {
                should_quit = TRUE;
                LOG("Quit second server");
            }
            else {
                LOG("Seconds server got message: '%s'", data_buff);
                server->sleep(1);

                char data_to_send[BUFF_LEN + 32];
                sprintf(data_to_send, "%s - %u", (char *)data_buff, count);
                server->reply(data_to_send, strlen(data_to_send) + 1, NULL);
            }
        }
    } while(FALSE == should_quit);

    return;
}


static void _sub_udp_routine(evutil_socket_t fd, Event *abs_server, void *arg)
{
    NoServer *server = (NoServer *)abs_server;
    void *storage = server->custom_storage();
    struct sockaddr_in *addr = (struct sockaddr_in *)storage;
    struct Error err;

    char data[1024] = "Thank you for your message!";
    size_t recv_len = 0;

    DNSClient *dns = server->new_DNS_client(NetIPv4);
    NetType_t type;

    LOG("Default DNS server 1: %s(%d)", dns->default_dns_server(0, &type).c_str(), type);
    LOG("Default DNS server 2: %s(%d)", dns->default_dns_server(1, &type).c_str(), type);

    err = dns->resolve("www.baidu.com", 1.0);
    LOG("resolve result: %s", err.c_err_msg());
    LOG("remote address: %s:%u", dns->remote_addr().c_str(), dns->remote_port());
    LOG("Test ends, now exit process");
    exit(-1);

    // TODO: 添加测试代码

    LOG("Sub service ends");
    return;
}


#endif

// ==========
#define __MAIN
#ifdef __MAIN

int main(int argc, char *argv[])
{
    _test();

    Base *base = new Base;
    UDPServer *server_A = new UDPServer;
    UDPServer *server_B = new UDPServer;
    LOG("Hello, libcoevent! Base: %s", base->identifier().c_str());

    server_A->init(base, _main_server_routine, NetIPv4, _UDP_PORT, base);
    server_B->init(base, _second_server_routine, NetIPv4, _UDP_PORT_2, base);
    base->run();

    LOG("libcoevent base ends");
    delete base;
    base = NULL;
    server_A = NULL;
    server_B = NULL;

    return 0;
}

#endif  // end of libcoevent::Base

