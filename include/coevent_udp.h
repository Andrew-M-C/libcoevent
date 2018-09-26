// file encoding: UTF-8

#ifndef __CO_EVENT_UDP_H__
#define __CO_EVENT_UDP_H__

#include "coevent_error.h"
#include "coevent_const.h"
#include "coevent_virtual.h"

#include "event2/event.h"
#include "co_routine.h"

#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <string>
#include <set>
#include <map>
#include <vector>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>

namespace andrewmc {
namespace libcoevent {

// ====================
// UDP server
class UDPServer : public Server {
protected:
    void            *_event_arg;
    int             _fd_ipv4;
    int             _fd_ipv6;
    int             _fd_unix;
    struct sockaddr_in  _sockaddr_ipv4;
    struct sockaddr_in6 _sockaddr_ipv6;
    struct sockaddr_un  _sockaddr_unix;
    uint32_t        *_libevent_what_storage;    // ensure that this is assigned in heap instead of stack

    struct sockaddr_in  _remote_addr_ipv4;
    socklen_t           _remote_addr_ipv4_len;
    struct sockaddr_in6 _remote_addr_ipv6;
    socklen_t           _remote_addr_ipv6_len;
    struct sockaddr_un  _remote_addr_unix;
    socklen_t           _remote_addr_unix_len;

    std::map<std::string, UDPSession *> _session_collection;

public:
    UDPServer();
    virtual ~UDPServer();

    struct Error init(Base *base, WorkerFunc func, const struct sockaddr *addr, socklen_t addr_len, void *user_arg = NULL, BOOL auto_free = TRUE);
    struct Error init(Base *base, WorkerFunc func, NetType_t network_type, int bind_port = 0, void *user_arg = NULL, BOOL auto_free = TRUE);
    struct Error init(Base *base, WorkerFunc func, const char *bind_path, void *user_arg = NULL, BOOL auto_free = TRUE);
    struct Error init(Base *base, WorkerFunc func, std::string &bind_path, void *user_arg = NULL, BOOL auto_free = TRUE);

    // session mode does not support AF_UNIX
    struct Error init_session_mode(Base *base, WorkerFunc session_func, const struct sockaddr *addr, socklen_t addr_len, void *user_arg = NULL, BOOL auto_free = TRUE);
    struct Error init_session_mode(Base *base, WorkerFunc session_func, NetType_t network_type, int bind_port = 0, void *user_arg = NULL, BOOL auto_free = TRUE);
    struct Error quit_session_mode_server();
    struct Error notify_session_ends(UDPSession *session);      // actually protected

    NetType_t network_type();
    const char *c_socket_path();    // valid in local type
    int port();                     // valid in IPv4 or IPv6 type

    struct Error sleep(double seconds);
    struct Error sleep(struct timeval &sleep_time);
    struct Error sleep_milisecs(unsigned mili_secs);

    struct Error recv(void *data_out, const size_t len_limit, size_t *len_out_nullable = NULL, double timeout_seconds = 0);
    struct Error recv_in_timeval(void *data_out, const size_t len_limit, size_t *len_out_nullable, const struct timeval &timeout);
    struct Error recv_in_mimlisecs(void *data_out, const size_t len_limit, size_t *len_out_nullable, unsigned timeout_milisecs);

    struct Error send(const void *data, const size_t data_len, size_t *send_len_out_nullable, const struct sockaddr *addr_nullable);
    struct Error send(const void *data, const size_t data_len, size_t *send_len_out_nullable = NULL, const std::string &target_address = "", unsigned port = 80);
    struct Error reply(const void *data, const size_t data_len, size_t *reply_len_out = NULL);

    std::string client_addr();      // valid in IPv4 or IPv6 type
    unsigned client_port();         // valid in IPv4 or IPv6 type
    void copy_client_addr(struct sockaddr *addr_out, socklen_t addr_len);

private:
    void _init();
    void _clear();

    uint32_t _libevent_what();
    int _fd();
    struct sockaddr *_remote_sock_addr();
    socklen_t *_remote_sock_addr_len();
protected:
    struct stCoRoutine_t *_coroutine();
};


// UDP Session
class UDPSession : public Session {
public:
    UDPSession(){};
    virtual ~UDPSession(){};

    virtual NetType_t network_type() = 0;

    virtual struct Error reply(const void *data, const size_t data_len, size_t *send_len_out_nullable = NULL) = 0;
    virtual struct Error recv(void *data_out, const size_t len_limit, size_t *len_out_nullable, double timeout_seconds = 0) = 0;
    virtual struct Error recv_in_timeval(void *data_out, const size_t len_limit, size_t *len_out_nullable, const struct timeval &timeout) = 0;
    virtual struct Error recv_in_mimlisecs(void *data_out, const size_t len_limit, size_t *len_out_nullable, unsigned timeout_milisecs) = 0;

    virtual struct Error sleep(double seconds) = 0;
    virtual struct Error sleep(struct timeval &sleep_time) = 0;
    virtual struct Error sleep_milisecs(unsigned mili_secs) = 0;

    virtual std::string remote_addr() = 0;      // valid in IPv4 or IPv6 type
    virtual unsigned remote_port() = 0;         // valid in IPv4 or IPv6 type
    virtual void copy_remote_addr(struct sockaddr *addr_out, socklen_t addr_len) = 0;
};


// UDP Client
class UDPClient : public Client {
public:
    UDPClient(){};
    virtual ~UDPClient(){};

    virtual NetType_t network_type() = 0;

    virtual struct Error send(const void *data, const size_t data_len, size_t *send_len_out_nullable, const struct sockaddr *addr, socklen_t addr_len) = 0;
    virtual struct Error send(const void *data, const size_t data_len, size_t *send_len_out_nullable, const std::string &target_address, unsigned target_port = 80) = 0;
    virtual struct Error send(const void *data, const size_t data_len, size_t *send_len_out_nullable, const char *target_address = "", unsigned target_port = 80) = 0;
    virtual struct Error reply(const void *data, const size_t data_len, size_t *send_len_out_nullable = NULL) = 0;

    virtual struct Error recv(void *data_out, const size_t len_limit, size_t *len_out_nullable, double timeout_seconds) = 0;
    virtual struct Error recv_in_timeval(void *data_out, const size_t len_limit, size_t *len_out_nullable, const struct timeval &timeout) = 0;
    virtual struct Error recv_in_mimlisecs(void *data_out, const size_t len_limit, size_t *len_out_nullable, unsigned timeout_milisecs) = 0;

    virtual std::string remote_addr() = 0;    // valid in IPv4 or IPv6 type
    virtual unsigned remote_port() = 0;       // valid in IPv4 or IPv6 type
    virtual void copy_remote_addr(struct sockaddr *addr_out, socklen_t addr_len) = 0;

    virtual Procedure *owner_server() = 0;
};


}   // end of namespace libcoevent
}   // end of namespace andrewmc

#endif  // EOF
