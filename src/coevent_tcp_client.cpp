
#include "coevent.h"
#include "coevent_itnl.h"
#include <string.h>
#include <string>
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
    struct _EventArg *arg = (struct _EventArg *)arg;

    // switch into the coroutine
    if (arg->libevent_what_ptr) {
        *(arg->libevent_what_ptr) = (uint32_t)what;
        DEBUG("libevent what: 0x%08x - %s%s", (unsigned)what, event_is_timeout(what) ? "timeout " : "", event_readable(what) ? "read" : "");
    }
    co_resume(arg->coroutine);

    // is coroutine end?
    if (is_coroutine_end(arg->coroutine)) {
        // delete the event if this is under control of the base
        TCPItnlClient *client = arg->client;
        Server *server = client->owner_server();
        Base *base = client->owner();

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
    _server = NULL;

    _libevent_what_storage = (uint32_t *)malloc(*_libevent_what_storage);
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
        _fd = NULL;
    }

    _event_arg = NULL;
    _fd = 0;
    _self_addr.ss_family = 0;
    _remote_addr.ss_family = 0;
    _addr_len = 0;
    _server = NULL;
    return;
}

#endif


// ==========
#define __INIT_FUNCTIONS
#ifdef __INIT_FUNCTIONS

struct Error TCPItnlClient::init(Server *server, struct stCoRoutine_t *coroutine, NetType_t network_type, void *user_arg)
{
    if (!(server && coroutine)) {
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }

    switch(network_type)
    {
        case NetIPv4:
            _addr_len = sizeof(struct sockaddr_in);
            _self_addr.ss_family = AF_INET;
            _remote_addr.ss_family = AF_INET;
            break;
        case NetIPv6:
            _addr_len = sizeof(struct sockaddr_in6);
            _self_addr.ss_family = AF_INET6;
            _remote_addr.ss_family = AF_INET6;
            break;
        case NetLocal:
            _addr_len = sizeof(struct sockaddr_un);
            _self_addr.ss_family = AF_UNIX;
            _remote_addr.ss_family = AF_UNIX;
            break;

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

    _clear();
    _status.clear_err();

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

    // non-block
    set_fd_nonblock(_fd);

    // create event
    _owner_server = server;
    _owner_base = server->owner();
    _owner_base->put_event_under_control(this);
    _event = event_new(_owner_base->event_base(), fd, EV_TIMEOUT | EV_READ, _libevent_callback, arg);
    if (NULL == _event) {
        ERROR("Failed to new a UDP event");
        _clear();
        _status.set_app_errno(ERR_EVENT_EVENT_NEW);
        return _status;
    }


    // TODO:
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
        struct sockaddr_in *addr = (struct sockaddr_in *)_remote_addr;
        char c_addr_str[INET_ADDRSTRLEN + 1];
        c_addr_str[INET_ADDRSTRLEN] = '\0';
        inet_ntop(AF_INET, &(addr->sin_addr), c_addr_str, sizeof(c_addr_str));
        return std::string(c_addr_str);
    }
    else if (AF_INET6 == _remote_addr.ss_family)
    {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)_remote_addr;
        char c_addr_str[INET6_ADDRSTRLEN + 1];
        c_addr_str[INET6_ADDRSTRLEN] = '\0';
        inet_ntop(AF_INET6, &(addr6->sin6_addr), c_addr_str, sizeof(c_addr_str));
        return std::string(c_addr_str);
    }
    else if (AF_UNIX == _remote_addr.ss_family)
    {
        struct sockaddr_un *addr = (struct sockaddr_un *)_remote_addr;
        return std::string(addr->sun_path);
    }
    else {
        return "";
    }
}


unsigned UDPItnlClient::remote_port()
{
    if (AF_INET == _remote_addr.ss_family)
    {
        struct sockaddr_in *addr = (struct sockaddr_in *)_remote_addr;
        return (unsigned)ntohs(addr->sin_port);
    }
    else if (AF_INET6 == _remote_addr.ss_family)
    {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)_remote_addr;
        return (unsigned)ntohs(addr6->sin6_port);
    }
    else {
        return 0;
    }
}


void UDPItnlClient::copy_remote_addr(struct sockaddr *addr_out, socklen_t addr_len)
{
    if (NULL == addr_out || addr_len < _addr_len) {
        return;
    }
    memcpy(addr_out, &_remote_addr, _addr_len);
    return;
}


Server *UDPItnlClient::owner_server()
{
    return _server;
}


#endif  // __MISC_FUNCTIONS


#ifdef __MACRO_NOT_EXIST


// ==========
#define __MISC_FUNCTIONS
#ifdef __MISC_FUNCTIONS

NetType_t UDPItnlClient::network_type()
{
    if (_fd_ipv4) {
        return NetIPv4;
    } else if (_fd_ipv6) {
        return NetIPv6;
    } else if (_fd_unix) {
        return NetLocal;
    } else {
        return NetUnknown;
    }
}


std::string UDPItnlClient::remote_addr()
{
    if (_fd_ipv4)
    {
        char c_addr_str[INET_ADDRSTRLEN + 1];
        c_addr_str[INET_ADDRSTRLEN] = '\0';
        inet_ntop(AF_INET, &(_remote_addr_ipv4.sin_addr), c_addr_str, sizeof(c_addr_str));
        return std::string(c_addr_str);
    }
    else if (_fd_ipv6)
    {
        char c_addr_str[INET6_ADDRSTRLEN + 1];
        c_addr_str[INET6_ADDRSTRLEN] = '\0';
        inet_ntop(AF_INET6, &(_remote_addr_ipv6.sin6_addr), c_addr_str, sizeof(c_addr_str));
        return std::string(c_addr_str);
    }
    else if (_fd_unix)
    {
        return std::string(_remote_addr_unix.sun_path);
    }
    else {
        return "";
    }
}


unsigned UDPItnlClient::remote_port()
{
    if (_fd_ipv4) {
        return (unsigned)ntohs(_remote_addr_ipv4.sin_port);
    } else if (_fd_ipv6) {
        return (unsigned)ntohs(_remote_addr_ipv6.sin6_port);
    } else {
        return 0;
    }
}


void UDPItnlClient::copy_remote_addr(struct sockaddr *addr_out, socklen_t addr_len)
{
    struct sockaddr *addr = NULL;
    socklen_t local_addr_len = 0;

    if (!(addr_out && addr_len)) {
        return;
    }

    if (_fd_ipv4) {
        addr = (struct sockaddr *)&_remote_addr_ipv4;
        local_addr_len = sizeof(_remote_addr_ipv4);
    }
    else if (_fd_ipv6) {
        addr = (struct sockaddr *)&_remote_addr_ipv6;
        local_addr_len = sizeof(_remote_addr_ipv6);
    }
    else if (_fd_unix) {
        addr = (struct sockaddr *)&_remote_addr_unix;
        local_addr_len = sizeof(_remote_addr_unix);
    }
    else {
        return;
    }

    memcpy(addr_out, addr, local_addr_len <= addr_len ? local_addr_len : addr_len);
    return;
}


Server *UDPItnlClient::owner_server()
{
    return _owner_server;
}


struct sockaddr *UDPItnlClient::_remote_addr()
{
    if (_fd_ipv4) {
        return (struct sockaddr *)&_remote_addr_ipv4;
    } else if (_fd_ipv6) {
        return (struct sockaddr *)&_remote_addr_ipv6;
    } else if (_fd_unix) {
        return (struct sockaddr *)&_remote_addr_unix;
    } else {
        return NULL;
    }
}


#endif  // end of __MISC_FUNCTIONS





// ==========
#define __SEND_FUNCTION
#ifdef __SEND_FUNCTION

struct Error UDPItnlClient::send(const void *data, const size_t data_len, size_t *send_len_out, const struct sockaddr *addr, socklen_t addr_len)
{
    _status.clear_err();
    if (!(data && data_len && addr)) {
        return _status;
    }

    if (_fd <= 0) {
        _status.set_app_errno(ERR_NOT_INITIALIZED);
        return _status;
    }
    
    ssize_t send_ret = 0;

    send_ret = sendto(_fd, data, data_len, 0, addr, addr_len);
    if (send_ret < 0) {
        _status.set_sys_errno();
    }

    if (send_len_out) {
        *send_len_out = (send_ret > 0) ? send_ret : 0;
    }
    return _status;
}


struct Error UDPItnlClient::send(const void *data, const size_t data_len, size_t *send_len_out, const std::string &target_address, unsigned target_port)
{
    if (0 == target_address.length()) {
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }
    else {
        if (_fd_ipv4) {
            struct sockaddr_in addr;
            convert_str_to_sockaddr_in(target_address, target_port, &addr);
            return send(data, data_len, send_len_out, (struct sockaddr *)(&addr), sizeof(addr));
        }
        else if (_fd_ipv4) {
            struct sockaddr_in6 addr;
            convert_str_to_sockaddr_in6(target_address, target_port, &addr);
            return send(data, data_len, send_len_out, (struct sockaddr *)(&addr), sizeof(addr));
        }
        else if (_fd_unix) {
            struct sockaddr_un addr;
            convert_str_to_sockaddr_un(target_address, &addr);
            return send(data, data_len, send_len_out, (struct sockaddr *)(&addr), sizeof(addr));
        }
        else {
            _status.set_app_errno(ERR_NOT_INITIALIZED);
            return _status;
        }
    }
}


struct Error UDPItnlClient::send(const void *data, const size_t data_len, size_t *send_len_out, const char *target_address, unsigned target_port)
{
    if (NULL == target_address) {
        target_address = "";
    }
    std::string addr_str = target_address;
    return send(data, data_len, send_len_out, addr_str, target_port);
}


struct Error UDPItnlClient::reply(const void *data, const size_t data_len, size_t *send_len_out)
{
    struct sockaddr *addr = _remote_addr();
    if (NULL == addr) {
        _status.set_app_errno(ERR_NOT_INITIALIZED);
        return _status;
    }
    return send(data, data_len, send_len_out, addr, _remote_addr_len);
}

#endif  // end of __SEND_FUNCTION


// ==========
#define __RECV_FUNCTION
#ifdef __RECV_FUNCTION

struct Error UDPItnlClient::recv(void *data_out, const size_t len_limit, size_t *len_out, double timeout_seconds)
{
    struct timeval timeout;

    if (timeout_seconds <= 0) {
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
    }
    else {
        timeout = to_timeval(timeout_seconds);
    }

    return recv_in_timeval(data_out, len_limit, len_out, timeout);
}


struct Error UDPItnlClient::recv_in_timeval(void *data_out, const size_t len_limit, size_t *len_out, const struct timeval &timeout)
{
    ssize_t recv_len = 0;
    uint32_t libevent_what = *_libevent_what_storage;
    struct _EventArg *arg = (struct _EventArg *)_event_arg;

    if (!(data_out && len_limit)) {
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }
    _status.clear_err();

    if (event_readable(libevent_what))
    {
        recv_len = recv_from(_fd, data_out, len_limit, 0, _remote_addr(), &_remote_addr_len);
        if (recv_len < 0) {
            _status.set_sys_errno();
        }
        else if (0 == recv_len) {
            *_libevent_what_storage &=~ (EV_READ);
            return recv_in_timeval(data_out, len_limit, len_out, timeout);
        }
    }
    else {
        // no data readable
        struct timeval timeout_copy;
        timeout_copy.tv_sec = timeout.tv_sec;
        timeout_copy.tv_usec = timeout.tv_usec;

        DEBUG("UDP libevent what flag: 0x%04x, now wait", (unsigned)libevent_what);
        if ((0 == timeout_copy.tv_sec) && (0 == timeout_copy.tv_usec)) {
            timeout_copy.tv_sec = FOREVER_SECONDS;
        }
        event_add(_event, &timeout_copy);
        co_yield(arg->coroutine);

        // check if data readable
        libevent_what = *_libevent_what_storage;
        if (event_readable(libevent_what))
        {
            recv_len = recv_from(_fd, data_out, len_limit, 0, _remote_addr(), &_remote_addr_len);
            if (recv_len < 0) {
                _status.set_sys_errno();
            }
        }
        else if (event_is_timeout(libevent_what))
        {
            recv_len = 0;
            _status.set_app_errno(ERR_TIMEOUT);
        }
        else {
            ERROR("unrecognized event flag: 0x%04u", libevent_what);
            _status.set_app_errno(ERR_UNKNOWN);
        }
    }

    // write read data len and return
    if (len_out) {
        *len_out = (recv_len > 0) ? recv_len : 0;
    }
    return _status;
}


struct Error UDPItnlClient::recv_in_mimlisecs(void *data_out, const size_t len_limit, size_t *len_out, unsigned timeout_milisecs)
{
    struct timeval timeout = to_timeval_from_milisecs(timeout_milisecs);
    return recv_in_timeval(data_out, len_limit, len_out, timeout);
}

#endif
#endif

// end of file
