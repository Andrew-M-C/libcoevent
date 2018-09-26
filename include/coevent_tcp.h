// file encoding: UTF-8

#ifndef __CO_EVENT_TCP_H__
#define __CO_EVENT_TCP_H__

#include "coevent_error.h"
#include "coevent_const.h"
#include "coevent_virtual.h"
#include "coevent_routine.h"

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
// TCPServer
class TCPServer : public Server {
private:
    void                        *_event_arg;
    int                         _fd;
    struct sockaddr_storage     _sock_addr;
    socklen_t                   _sock_addr_len;
    std::map<int, TCPSession *> _sessions;
    unsigned                    _port;
public:
    TCPServer();
    virtual ~TCPServer();

    struct Error init_session_mode(Base *base, WorkerFunc session_func, const struct sockaddr *addr, socklen_t addr_len, void *user_arg = NULL, BOOL auto_free = TRUE);
    struct Error init_session_mode(Base *base, WorkerFunc session_func, NetType_t network_type, int bind_port = 0, void *user_arg = NULL, BOOL auto_free = TRUE);
    struct Error init_session_mode(Base *base, WorkerFunc session_func, const char *bind_path, void *user_arg = NULL, BOOL auto_free = TRUE);
    struct Error init_session_mode(Base *base, WorkerFunc session_func, std::string &bind_path, void *user_arg = NULL, BOOL auto_free = TRUE);
    struct Error quit_session_mode_server();
    struct Error notify_session_ends(TCPSession *session);      // actually protected

    NetType_t network_type();
    const char *c_socket_path();    // valid in local type
    int port();                     // valid in IPv4 or IPv6 type
private:
    void _clear();
};


// TCPSession
class TCPSession : public Session {
public:
    TCPSession(){};
    virtual ~TCPSession(){};

    virtual NetType_t network_type() = 0;

    virtual struct Error reply(const void *data, const size_t data_len, size_t *send_len_out_nullable = NULL) = 0;
    virtual struct Error recv(void *data_out, const size_t len_limit, size_t *len_out_nullable, double timeout_seconds = 0) = 0;
    virtual struct Error recv_in_timeval(void *data_out, const size_t len_limit, size_t *len_out_nullable, const struct timeval &timeout) = 0;
    virtual struct Error recv_in_mimlisecs(void *data_out, const size_t len_limit, size_t *len_out_nullable, unsigned timeout_milisecs) = 0;

    virtual struct Error disconnect(void) = 0;

    virtual struct Error sleep(double seconds) = 0;
    virtual struct Error sleep(struct timeval &sleep_time) = 0;
    virtual struct Error sleep_milisecs(unsigned mili_secs) = 0;

    virtual std::string remote_addr() = 0;      // valid in IPv4 or IPv6 type
    virtual unsigned remote_port() = 0;         // valid in IPv4 or IPv6 type
    virtual void copy_remote_addr(struct sockaddr *addr_out, socklen_t addr_len) = 0;
};


// TCPClient
class TCPClient : public Client {
public:
    TCPClient(){};
    virtual ~TCPClient(){};

    virtual NetType_t network_type() = 0;

    virtual struct Error connect_to_server(const struct sockaddr *addr, socklen_t addr_len, double timeout_seconds = 0) = 0;
    virtual struct Error connect_to_server(const std::string &target_address = "", unsigned target_port = 80, double timeout_seconds = 0) = 0;
    virtual struct Error connect_to_server(const char *target_address = "", unsigned target_port = 80, double timeout_seconds = 0) = 0;

    virtual struct Error connect_in_timeval(const struct sockaddr *addr, socklen_t addr_len, const struct timeval &timeout) = 0;
    virtual struct Error connect_in_timeval(const std::string &target_address, unsigned target_port, const struct timeval &timeout) = 0;
    virtual struct Error connect_in_timeval(const char *target_address, unsigned target_port, const struct timeval &timeout) = 0;

    virtual struct Error connect_in_mimlisecs(const struct sockaddr *addr, socklen_t addr_len, unsigned timeout_milisecs) = 0;
    virtual struct Error connect_in_mimlisecs(const std::string &target_address, unsigned target_port, unsigned timeout_milisecs) = 0;
    virtual struct Error connect_in_mimlisecs(const char *target_address, unsigned target_port, unsigned timeout_milisecs) = 0;

    virtual struct Error send(const void *data, const size_t data_len, size_t *send_len_out_nullable = NULL) = 0;

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
