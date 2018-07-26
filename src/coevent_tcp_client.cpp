
#include "coevent.h"
#include "coevent_itnl.h"
#include <string.h>
#include <string>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>

using namespace andrewmc::libcoevent;

// ==========
#define __EVENT_ARG_DEFINITION
#ifdef __EVENT_ARG_DEFINITION

struct _EventArg {
    TCPItnlClient       *client;
    int                 fd;
    uint32_t            *libevent_what_ptr;

    struct stCoRoutine_t *coroutine;
    void                *user_arg;

};

#endif


// ==========
#define __LIBEVENT_CALLBACK
#ifdef __LIBEVENT_CALLBACK

static void _libevent_callback(evutil_socket_t fd, short what, void *libevent_arg)
{
    struct _EventArg *arg = (struct _EventArg *)libevent_arg;
    TCPItnlClient *client = arg->client;
    Procedure *server = client->owner_server();
    Base *base = client->owner();

    // switch into the coroutine
    if (arg->libevent_what_ptr) {
        *(arg->libevent_what_ptr) = (uint32_t)what;
        DEBUG("libevent what: 0x%08x - %s %s %s", (unsigned)what, event_is_timeout(what) ? "timeout " : "", event_readable(what) ? "read" : "", event_writable(what) ? "write" : "");
    }
    co_resume(arg->coroutine);

    // is coroutine end?
    if (is_coroutine_end(arg->coroutine)) {
        // delete the event if this is under control of the base
        DEBUG("Server %s ends", server->identifier().c_str());
        base->delete_event_under_control(server);
    }

    // done
    return;
}

#endif


// ==========
#define __CONSTRUCT_AND_DESTRUCT
#ifdef __CONSTRUCT_AND_DESTRUCT

TCPItnlClient::TCPItnlClient()
{
    char identifier[64];
    sprintf(identifier, "TCP client %p", this);
    _identifier = identifier;

    _event_arg = NULL;
    _fd = 0;
    _self_addr.ss_family = 0;
    _remote_addr.ss_family = 0;
    _addr_len = 0;
    _is_connected = FALSE;
    _owner_server = NULL;

    _libevent_what_storage = (uint32_t *)malloc(sizeof(*_libevent_what_storage));
    if (NULL == _libevent_what_storage) {
        _clear();
        _status.set_sys_errno();
        throw std::bad_alloc();
        return;
    }

    return;
}


TCPItnlClient::~TCPItnlClient()
{
    _clear();

    if (_libevent_what_storage) {
        free(_libevent_what_storage);
        _libevent_what_storage = NULL;
    }
    return;
}


void TCPItnlClient::_clear()
{
    if (_event) {
        DEBUG("Delete TCP event");
        event_del(_event);
        _event = NULL;
    }

    if (_event_arg) {
        free(_event_arg);
        _event_arg = NULL;
    }

    if (_fd > 0) {
        close(_fd);
        _fd = 0;
    }

    _fd = 0;
    _self_addr.ss_family = 0;
    _remote_addr.ss_family = 0;
    _addr_len = 0;
    _is_connected = FALSE;
    DEBUG("%s cleared", identifier().c_str());
    return;
}

#endif


// ==========
#define __INIT_FUNCTIONS
#ifdef __INIT_FUNCTIONS

struct Error TCPItnlClient::init(Procedure *server, struct stCoRoutine_t *coroutine, NetType_t network_type, void *user_arg)
{
    if (!(server && coroutine)) {
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }

    _clear();
    _status.clear_err();
    _owner_server = server;
    _owner_base = server->owner();

    switch(network_type)
    {
        case NetIPv4: {
            struct sockaddr_in *addr4 = (struct sockaddr_in *)&_self_addr;
            _addr_len = sizeof(struct sockaddr_in);
            _self_addr.ss_family = AF_INET;
            _remote_addr.ss_family = AF_INET;

            addr4->sin_port = 0;
            addr4->sin_addr.s_addr = htonl(INADDR_ANY);
            }break;

        case NetIPv6: {
            struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&_self_addr;
            _addr_len = sizeof(struct sockaddr_in6);
            _self_addr.ss_family = AF_INET6;
            _remote_addr.ss_family = AF_INET6;

            addr6->sin6_port = 0;
            addr6->sin6_addr = in6addr_any;
            }break;

        case NetLocal: {
            _addr_len = sizeof(struct sockaddr_un);
            _self_addr.ss_family = AF_UNIX;
            _remote_addr.ss_family = AF_UNIX;
            }break;

        case NetUnknown:
        default:
            _status.set_app_errno(ERR_NETWORK_TYPE_ILLEGAL);
            return _status;
            break;
    }

    struct _EventArg *arg = (struct _EventArg *)malloc(sizeof(*arg));
    if (NULL == arg) {
        _clear();
        _status.set_sys_errno();
        return _status;
    }

    _event_arg = arg;
    arg->client = this;
    arg->libevent_what_ptr = _libevent_what_storage;
    arg->coroutine = coroutine;
    arg->user_arg = user_arg;

    // create socket
    _fd = socket(_self_addr.ss_family, SOCK_STREAM, 0);
    if (_fd < 0) {
        _status.set_sys_errno();
        _clear();
        return _status;
    }

    // try binding
    int status = bind(_fd, (struct sockaddr *)&_self_addr, _addr_len);
    if (status < 0) {
        _clear();
        _status.set_sys_errno(errno);
        return _status;
    }

    // non-block
    set_fd_nonblock(_fd);

    // create event
    _owner_base->put_event_under_control(this);
    _event = event_new(_owner_base->event_base(), _fd, EV_TIMEOUT | EV_READ, _libevent_callback, arg);
    if (NULL == _event) {
        ERROR("Failed to new a UDP event");
        _clear();
        _status.set_app_errno(ERR_EVENT_EVENT_NEW);
        return _status;
    }

    return _status;
}


#endif  // end of __INIT_FUNCTIONS


// ==========
#define __MISC_FUNCTIONS
#ifdef __MISC_FUNCTIONS

NetType_t TCPItnlClient::network_type()
{
    switch(_self_addr.ss_family)
    {
        case AF_INET:
            return NetIPv4;
            break;
        case AF_INET6:
            return NetIPv6;
            break;
        case AF_UNIX:
            return NetLocal;
            break;
        default:
            return NetUnknown;
            break;
    }
    return NetUnknown;
}


std::string TCPItnlClient::remote_addr()
{
    if (AF_INET == _remote_addr.ss_family)
    {
        struct sockaddr_in *addr = (struct sockaddr_in *)&_remote_addr;
        char c_addr_str[INET_ADDRSTRLEN + 1];
        c_addr_str[INET_ADDRSTRLEN] = '\0';
        inet_ntop(AF_INET, &(addr->sin_addr), c_addr_str, sizeof(c_addr_str));
        return std::string(c_addr_str);
    }
    else if (AF_INET6 == _remote_addr.ss_family)
    {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&_remote_addr;
        char c_addr_str[INET6_ADDRSTRLEN + 1];
        c_addr_str[INET6_ADDRSTRLEN] = '\0';
        inet_ntop(AF_INET6, &(addr6->sin6_addr), c_addr_str, sizeof(c_addr_str));
        return std::string(c_addr_str);
    }
    else if (AF_UNIX == _remote_addr.ss_family)
    {
        struct sockaddr_un *addr = (struct sockaddr_un *)&_remote_addr;
        return std::string(addr->sun_path);
    }
    else {
        return std::string("");
    }
}


unsigned TCPItnlClient::remote_port()
{
    if (AF_INET == _remote_addr.ss_family)
    {
        struct sockaddr_in *addr = (struct sockaddr_in *)&_remote_addr;
        return (unsigned)ntohs(addr->sin_port);
    }
    else if (AF_INET6 == _remote_addr.ss_family)
    {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&_remote_addr;
        return (unsigned)ntohs(addr6->sin6_port);
    }
    else {
        return 0;
    }
}


void TCPItnlClient::copy_remote_addr(struct sockaddr *addr_out, socklen_t addr_len)
{
    if (NULL == addr_out || addr_len < _addr_len) {
        return;
    }
    memcpy(addr_out, &_remote_addr, _addr_len);
    return;
}


Procedure *TCPItnlClient::owner_server()
{
    return _owner_server;
}


#endif  // __MISC_FUNCTIONS


// ==========
#define __TCP_CONNECT_FUNCTION
#ifdef __TCP_CONNECT_FUNCTION

// reference: [Linux socket非阻塞connect方法（一）](https://blog.csdn.net/nphyez/article/details/10268723)
// reference: [非阻塞connect编写方法介绍](http://dongxicheng.org/network/non-block-connect-implemention/)
struct Error TCPItnlClient::connect_in_timeval(const struct sockaddr *addr, socklen_t addr_len, const struct timeval &timeout)
{
    // para and instance status check
    if (!(addr && addr_len)) {
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }

    if (addr_len < _addr_len) {
        _status.set_app_errno(ERR_PARA_ILLEGAL);
        return _status;
    }

    if (_fd <= 0) {
        _status.set_app_errno(ERR_NOT_INITIALIZED);
        return _status;
    }

    if (addr->sa_family != _self_addr.ss_family) {
        _status.set_app_errno(ERR_NETWORK_TYPE_ILLEGAL);
        return _status;
    }

    if (_is_connected) {
        _status.set_app_errno(ERR_ALREADY_CONNECTED);
        return _status;
    }

    // invoke connect()
    BOOL should_enter_libevent = FALSE;
    int conn_stat = connect(_fd, addr, addr_len);
    if (0 == conn_stat) {
        DEBUG("Connection succeed instantly");
        _status.clear_err();
    }
    else {
        if (EINPROGRESS == errno) {
            DEBUG("EINPROGRESS");
            should_enter_libevent = TRUE;
        } else {
            _status.set_sys_errno();
        }
    }

    // connect by libevent
    while (should_enter_libevent)
    {
        struct _EventArg *arg = (struct _EventArg *)_event_arg;
        should_enter_libevent = FALSE;      // exit the loop

        conn_stat = event_assign(_event, _owner_base->event_base(), _fd, EV_TIMEOUT | EV_READ | EV_WRITE, _libevent_callback, arg);
        if (conn_stat) {
            _status.set_app_errno(ERR_EVENT_UNEXPECTED_ERROR);
            break;
        }

        struct timeval timeout_copy;
        timeout_copy.tv_sec = timeout.tv_sec;
        timeout_copy.tv_usec = timeout.tv_usec;
        if ((0 == timeout_copy.tv_sec) && (0 == timeout_copy.tv_usec)) {
            timeout_copy.tv_sec = FOREVER_SECONDS;
        }
        event_add(_event, &timeout_copy);
        co_yield(arg->coroutine);       // hand coroutine control over

        // check libevent return
        uint32_t libevent_what = *_libevent_what_storage;
        if (event_is_timeout(libevent_what))
        {
            *_libevent_what_storage = 0;
            _status.set_app_errno(ERR_TIMEOUT);
        }
        else if (event_writable(libevent_what) || event_readable(libevent_what))
        {
#if 0
            // check status, valid in Linux ONLY
            connect(_fd, addr, addr_len);
            int err_copy = errno;
            if (EISCONN == err_copy) {
                // success
                DEBUG("Connect TCP via libevent successed");
                _status.clear_err();
            }
            else {
                DEBUG("Failed to connect via libevent: %s", strerror(err_copy));
                _status.set_sys_errno(err_copy);
            }
#else
            int err = 0;
            socklen_t errlen = sizeof(err);
            conn_stat = getsockopt(_fd, SOL_SOCKET, SO_ERROR, &err, &errlen);
            if (conn_stat < 0) {
                _status.set_sys_errno();
                ERROR("Failed in getsockopt: %s", strerror(errno));
            }
            else {
                if (0 == err) {
                    DEBUG("Connect TCP via libevent successed");
                    _status.clear_err();
                }
                else {
                    DEBUG("Failed to connect via libevent: %d", err);
                    _status.set_sys_errno(err);
                }
            }
#endif
        }
        else {
            ERROR("unrecognized event flag: 0x%04u", (unsigned)libevent_what);
            *_libevent_what_storage = 0;
            _status.set_app_errno(ERR_UNKNOWN);
        }
    }

    // return
    if (_status.is_ok()) {
        _is_connected = TRUE;
        memcpy(&_remote_addr, addr, _addr_len);
    }
    return _status;
}


struct Error TCPItnlClient::connect_in_timeval(const std::string &target_address, unsigned target_port, const struct timeval &timeout)
{
    if (NetIPv4 == network_type()) {
        struct sockaddr_in addr;
        convert_str_to_sockaddr_in(target_address, target_port, &addr);
        return connect_in_timeval((struct sockaddr *)&addr, sizeof(addr), timeout);
    }
    else {
        struct sockaddr_in6 addr6;
        convert_str_to_sockaddr_in6(target_address, target_port, &addr6);
        return connect_in_timeval((struct sockaddr *)&addr6, sizeof(addr6), timeout);
    }
}


struct Error TCPItnlClient::connect_in_timeval(const char *target_address, unsigned target_port, const struct timeval &timeout)
{
    if (NULL == target_address) {
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }

    return connect_in_timeval(std::string(target_address), target_port, timeout);
}


struct Error TCPItnlClient::connect_to_server(const struct sockaddr *addr, socklen_t addr_len, double timeout_seconds)
{
    struct timeval timeout;
    timeout = to_timeval(timeout_seconds);
    return connect_in_timeval(addr, addr_len, timeout);
}


struct Error TCPItnlClient::connect_to_server(const std::string &target_address, unsigned target_port, double timeout_seconds)
{
    struct timeval timeout;
    timeout = to_timeval(timeout_seconds);
    return connect_in_timeval(target_address, target_port, timeout);
}


struct Error TCPItnlClient::connect_to_server(const char *target_address, unsigned target_port, double timeout_seconds)
{
    struct timeval timeout;
    timeout = to_timeval(timeout_seconds);
    return connect_in_timeval(target_address, target_port, timeout);
}


struct Error TCPItnlClient::connect_in_mimlisecs(const struct sockaddr *addr, socklen_t addr_len, unsigned timeout_milisecs)
{
    struct timeval timeout;
    timeout = to_timeval_from_milisecs(timeout_milisecs);
    return connect_in_timeval(addr, addr_len, timeout);
}


struct Error TCPItnlClient::connect_in_mimlisecs(const std::string &target_address, unsigned target_port, unsigned timeout_milisecs)
{
    struct timeval timeout;
    timeout = to_timeval_from_milisecs(timeout_milisecs);
    return connect_in_timeval(target_address, target_port, timeout);
}


struct Error TCPItnlClient::connect_in_mimlisecs(const char *target_address, unsigned target_port, unsigned timeout_milisecs)
{
    struct timeval timeout;
    timeout = to_timeval_from_milisecs(timeout_milisecs);
    return connect_in_timeval(target_address, target_port, timeout);
}


#endif      // end of __TCP_CONNECT_FUNCTION


// ==========
#define __SEND_FUNCTION
#ifdef __SEND_FUNCTION

struct Error TCPItnlClient::send(const void *data, const size_t data_len, size_t *send_len_out_nullable)
{
    // TODO:
    return _status;
}


#endif  // end of __SEND_FUNCTION


// ==========
#define __RECV_FUNCTIONS
#ifdef __RECV_FUNCTIONS

struct Error TCPItnlClient::recv_in_timeval(void *data_out, const size_t len_limit, size_t *len_out, const struct timeval &timeout)
{
    if (FALSE == _is_connected) {
        _status.set_app_errno(ERR_NOT_CONNECTED);
        return _status;
    }

    // TODO:
    return _status;
}


struct Error TCPItnlClient::recv(void *data_out, const size_t len_limit, size_t *len_out, double timeout_seconds)
{
    struct timeval timeout;
    timeout = to_timeval(timeout_seconds);
    return recv_in_timeval(data_out, len_limit, len_out, timeout);
}

struct Error TCPItnlClient::recv_in_mimlisecs(void *data_out, const size_t len_limit, size_t *len_out, unsigned timeout_milisecs)
{
    struct timeval timeout;
    timeout = to_timeval_from_milisecs(timeout_milisecs);
    return recv_in_timeval(data_out, len_limit, len_out, timeout);
}

#endif  // end of __RECV_FUNCTIONS

// end of file
