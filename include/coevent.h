// file encoding: UTF-8

#ifndef __CO_EVENT_H__
#define __CO_EVENT_H__

#include "coevent_const.h"
#include "event2/event.h"

#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <string>
#include <set>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>

namespace andrewmc {
namespace libcoevent {

class Base;
class Event;


// network type
typedef enum {
    NetUnknown = 0,
    NetIPv4,
    NetIPv6,
    NetLocal,
} NetType_t;


// coroutine function
typedef void (*WorkerFunc)(evutil_socket_t, Event *, void *);


// libcoevent use this structure to return error information
struct Error {
private:
    uint16_t _sys_errno;    // error defined in "errno.h"
    uint16_t _lib_errno;    // error defined by libcoevent in coevent_const.h
    ssize_t  _ssize_ret;
    std::string _err_msg;

public:
    Error():
        _sys_errno(0),_lib_errno(0),_ssize_ret(0)
    {}

    BOOL is_error();
    BOOL is_ok();
    BOOL is_timeout();
    void set_ssize_t(ssize_t val);
    ssize_t ssize();
    void clear_err();
    void set_sys_errno();
    void set_sys_errno(int sys_errno);
    void set_app_errno(ErrCode_t lib_errno);
    void set_app_errno(ErrCode_t lib_errno, const char *c_err_msg);
    void set_app_errno(ErrCode_t lib_errno, const std::string &err_msg);
    const char *c_err_msg();
    uint32_t err_code();
};


// event base of libcoevent
class Base {
    // private implementations
private:
    struct event_base   *_event_base;
    std::string         _identifier;
    std::set<Event *>   _events_under_control;  // User may put an event into a base, it will be deallocated automatically when this is not needed anymore.

    // constructor and destructors
public:
    Base();
    ~Base();
    struct event_base *event_base();
    struct Error run();
    void set_identifier(std::string &identifier);
    const std::string &identifier();
    void put_event_under_control(Event *event);
    void delete_event_under_control(Event *event);
};


// event of libcoevent
class Event {
protected:
    std::string     _identifier;
    Base            *_owner_base;
    struct event    *_event;
    struct Error    _status;
    void            *_custom_storage;
    size_t          _custom_storage_size;
public:
    Event();
    virtual ~Event();

    void set_identifier(std::string &identifier);
    const std::string &identifier();

    struct Error create_custom_storage(size_t size);      // allocate customized storage to store user data. This storage will automatically freed when the object is destructed.
    void *custom_storage();
    size_t custom_storage_size();

    struct Error status();
    Base *owner();
};


// pure event, no network interfaces supported
class TimerEvent : public Event {
protected:
    BOOL            _is_initialized;
    void            *_event_arg;
public:
    TimerEvent();
    ~TimerEvent();
    struct Error init(Base *base, WorkerFunc func, void *user_arg, BOOL auto_free = TRUE);

    struct Error sleep(double seconds);     // can ONLY be incoked inside coroutine
    struct Error sleep(const struct timeval &sleep_time);
    struct Error sleep_milisecs(unsigned mili_secs);
private:
    void _init();
    void _clear();
};


// UDP event
class UDPServer : public Event {
protected:
    void            *_event_arg;
    int             _fd_ipv4;
    int             _fd_ipv6;
    int             _fd_unix;
    struct sockaddr_in  _sockaddr_ipv4;
    struct sockaddr_in6 _sockaddr_ipv6;
    struct sockaddr_un  _sockaddr_unix;
    uint32_t        _action_mask;
    uint32_t        *_libevent_what_storage;    // ensure that this is assigned in heap instead of stack

    struct sockaddr_in  _remote_addr_ipv4;
    socklen_t           _remote_addr_ipv4_len;
    struct sockaddr_in6 _remote_addr_ipv6;
    socklen_t           _remote_addr_ipv6_len;
    struct sockaddr_un  _remote_addr_unix;
    socklen_t           _remote_addr_unix_len;

public:
    UDPServer();
    ~UDPServer();

    struct Error init(Base *base, WorkerFunc func, const struct sockaddr *addr, socklen_t addr_len, void *user_arg = NULL, BOOL auto_free = TRUE);
    struct Error init(Base *base, WorkerFunc func, const struct sockaddr &addr, socklen_t addr_len, void *user_arg = NULL, BOOL auto_free = TRUE);
    struct Error init(Base *base, WorkerFunc func, NetType_t network_type, int bind_port = 0, void *user_arg = NULL, BOOL auto_free = TRUE);
    struct Error init(Base *base, WorkerFunc func, const char *bind_path, void *user_arg = NULL, BOOL auto_free = TRUE);
    struct Error init(Base *base, WorkerFunc func, std::string &bind_path, void *user_arg = NULL, BOOL auto_free = TRUE);

    NetType_t network_type();
    const char *c_socket_path();    // valid in local type
    int port();                     // valid in IPv4 or IPv6 type

    struct Error sleep(double seconds);
    struct Error sleep(struct timeval &sleep_time);
    struct Error sleep_milisecs(unsigned mili_secs);

    struct Error recv(void *data_out, const size_t len_limit, size_t *len_out = NULL, double timeout_seconds = 0);
    struct Error recv_in_timeval(void *data_out, const size_t len_limit, size_t *len_out, const struct timeval &timeout);
    struct Error recv_in_mimlisecs(void *data_out, const size_t len_limit, size_t *len_out, unsigned timeout_milisecs);
    struct Error send(const void *data, const size_t data_len, size_t *send_len_out_nullable, const struct sockaddr *addr_nullable);
    struct Error send(const void *data, const size_t data_len, size_t *send_len_out_nullable = NULL, const std::string &target_address = "", unsigned port = 80);
    struct Error reply(const void *data, const size_t data_len, size_t *reply_len_out = NULL);
    // TODO: send()

    std::string client_addr();      // valid in IPv4 or IPv6 type
    unsigned client_port();         // valid in IPv4 or IPv6 type

private:
    void _init();
    void _clear();

    uint32_t _libevent_what();
    int _fd();
    struct sockaddr *_remote_sock_addr();
    socklen_t *_remote_sock_addr_len();
};

}   // end of namespace libcoevent
}   // end of namespace andrewmc

#endif  // EOF
