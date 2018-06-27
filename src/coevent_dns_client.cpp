
#include "coevent.h"
#include "coevent_itnl.h"
#include "cpp_tools.h"
#include <string.h>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdint.h>

using namespace andrewmc::libcoevent;

#define _DNS_PORT       (53)

// ==========
// necessary definitions
#define __CO_EVENT_DNS_CLIENT_DEFINITIONS
#ifdef __CO_EVENT_DNS_CLIENT_DEFINITIONS

struct _EventArg {
    DNSItnlClient       *event;
    int                 fd;
    uint32_t            *libevent_what_ptr;

    struct stCoRoutine_t *coroutine;
    void                *user_arg;

    _EventArg(): event(NULL), fd(0), libevent_what_ptr(NULL), coroutine(NULL)
    {}
};


struct _DNSQueryText
{
    uint16_t    transacton_ID;
    uint16_t    flags;
    uint16_t    questions;
    uint16_t    answer_rrs;
    uint16_t    authority_rrs;
    uint16_t    additional_rrs;
    std::string query;

    _DNSQueryText() {
        transacton_ID = 0;
        flags = 0;
        questions = 0;
        answer_rrs = 0;
        authority_rrs = 0;
        additional_rrs = 0;
        return;
    }

    void set_transaction_ID(uint16_t id) {
        transacton_ID = htons(id);
        return;
    }

    void set_query_name(const char *c_domain_name)
    {
        if (NULL == c_domain_name) {
            c_domain_name = "";
            questions = 0;
        } else {
            questions = htons(1);
        }
        query = c_domain_name;
        DEBUG("Query domain name: %s", query.c_str());
        return;
    }

    void set_as_query_type() {
        int16_t host_flags = (1 << 8);       // Recursion desired
        flags = htons(host_flags);
        return;
    }

    void serialize_to_data(::andrewmc::cpptools::Data &data)
    {
        char header[sizeof(struct _DNSQueryText) - sizeof(query)];
        data.clear();

        memcpy(header, this, sizeof(header));
        data.append((void *)header, sizeof(header));

        char rr_name[query.length() + 1];
        std::vector<std::string> name_parts = ::andrewmc::cpptools::split_string(query, ".");
        size_t rr_index = 0;

        for (std::vector<std::string>::iterator iter = name_parts.begin();
            iter != name_parts.end();
            iter ++)
        {
            size_t rr_len = iter->length();
            rr_name[rr_index ++] = (char)rr_len;
            memcpy(rr_name + rr_index, iter->c_str(), rr_len);
            rr_index += rr_len;
        }

        data.append((void *)rr_name, sizeof(rr_name));
        return;
    }

};

#endif


// ==========
// libevent style callback, this callback is second part of the coroutine adapter
#define __DNS_CLIENT_SERVER_CALLBACK
#ifdef __DNS_CLIENT_SERVER_CALLBACK

static void _libevent_callback(evutil_socket_t fd, short what, void *libevent_arg)
{
    struct _EventArg *arg = (struct _EventArg *)libevent_arg;

    // switch into the coroutine
    if (arg->libevent_what_ptr) {
        *(arg->libevent_what_ptr) = (uint32_t)what;
        DEBUG("libevent what: 0x%08x - %s%s", (unsigned)what, event_is_timeout(what) ? "timeout " : "", event_readable(what) ? "read" : "");
    }
    co_resume(arg->coroutine);

    // is coroutine end?
    if (is_coroutine_end(arg->coroutine)) {
        // delete the event if this is under control of the base
        DNSItnlClient *client = arg->event;
        Server *server = client->owner_server();
        Base *base = client->owner();

        DEBUG("Server %s ends", server->identifier().c_str());
        base->delete_event_under_control(server);
    }

    // done
    return;
}

#endif  // end of __DNS_CLIENT_SERVER_CALLBACK


// ==========
#define __CONSTRUCT_AND_DESTRUCTION
#ifdef __CONSTRUCT_AND_DESTRUCTION

DNSItnlClient::DNSItnlClient()
{
    _init();
    return;
}


DNSItnlClient::~DNSItnlClient()
{
    _clear();

    if (_libevent_what_storage) {
        free(_libevent_what_storage);
        _libevent_what_storage = NULL;
    }
    return;
}


void DNSItnlClient::_init()
{
    char identifier[64];
    sprintf(identifier, "DNS client %p", this);
    _identifier = identifier;

    _fd_ipv4 = 0;
    _fd_ipv6 = 0;
    _fd_unix = 0;
    _fd = 0;
    _event_arg = NULL;
    _owner_server = NULL;
    _libevent_what_storage = NULL;
    _transaction_ID = 0;
    _status.clear_err();

    _clear();

    _libevent_what_storage = (uint32_t *)malloc(sizeof(*_libevent_what_storage));
    if (NULL == _libevent_what_storage) {
        ERROR("Failed to init storage");
        throw std::bad_alloc();
    }
    else {
        *_libevent_what_storage = 0;
    }

    return;
}


void DNSItnlClient::_clear()
{
    if (_event) {
        DEBUG("Delete DNS client event");
        event_del(_event);
        _event = NULL;
    }

    if (_event_arg) {
        struct _EventArg *arg = (struct _EventArg *)_event_arg;
        _event_arg = NULL;

        // do not remove coroutine because client does not own it

        DEBUG("Delete _event_arg");
        free(arg);
    }

    if (_fd_ipv4) {
        close(_fd_ipv4);
    }
    else if (_fd_ipv6) {
        close(_fd_ipv6);
    }
    else if (_fd_unix) {
        close(_fd_unix);
    }

    _fd = 0;
    _event_arg = NULL;
    _fd_ipv4 = 0;
    _fd_ipv6 = 0;
    _fd_unix = 0;
    return;
}


struct Error DNSItnlClient::init(Server *server, struct stCoRoutine_t *coroutine, NetType_t network_type, void *user_arg)
{
    if (!(server && coroutine)) {
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }

    switch(network_type)
    {
        case NetIPv4:
        case NetIPv6:
        case NetLocal:
            // OK
            break;

        case NetUnknown:
        default:
            // Illegal
            _status.set_app_errno(ERR_NETWORK_TYPE_ILLEGAL);
            return _status;
            break;
    }

    _init();
    _status.clear_err();

    // create arguments and assign coroutine
    int fd = 0;
    struct sockaddr_in addr4;
    struct sockaddr_in6 addr6;
    struct sockaddr_un addrun;
    struct sockaddr *addr = NULL;
    socklen_t addr_len = 0;

    struct _EventArg *arg = (struct _EventArg *)malloc(sizeof(*arg));
    if (NULL == arg) {
        _clear();
        _status.set_sys_errno();
        return _status;
    }

    _event_arg = arg;
    arg->event = this;
    arg->user_arg = user_arg;
    arg->libevent_what_ptr = _libevent_what_storage;
    arg->coroutine = coroutine;

    // create file descriptor
    if (NetIPv4 == network_type)
    {
        addr4.sin_family = AF_INET;
        addr4.sin_port = 0;
        addr4.sin_addr.s_addr = htonl(INADDR_ANY);

        _fd_ipv4 = socket(AF_INET, SOCK_DGRAM, 0);
        fd = _fd_ipv4;
        addr = (struct sockaddr *)(&addr4);
        addr_len = sizeof(addr4);
    }
    else if (NetIPv6 == network_type)
    {
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = 0;
        addr6.sin6_addr = in6addr_any;

        _fd_ipv6 = socket(AF_INET6, SOCK_DGRAM, 0);
        fd = _fd_ipv6;
        addr = (struct sockaddr *)(&addr6);
        addr_len = sizeof(addr6);
    }
    else    // NetLocal, AF_UNIX
    {
        addrun.sun_family = AF_UNIX;
        _fd_unix = socket(AF_UNIX, SOCK_DGRAM, 0);
        fd = _fd_unix;
        addr = (struct sockaddr *)(&addrun);
        addr_len = sizeof(addrun);
    }

    if (fd <= 0) {
        _clear();
        _status.set_sys_errno(errno);
        return _status;
    }
    else {
        _fd = fd;
    }

    // try binding
    int status = bind(fd, addr, addr_len);
    if (status < 0) {
        _clear();
        _status.set_sys_errno(errno);
        return _status;
    }

    // non-block
    set_fd_nonblock(fd);

    // create event
    _owner_server = server;
    _owner_base = server->owner();
    _event = event_new(_owner_base->event_base(), fd, EV_TIMEOUT | EV_READ, _libevent_callback, arg);
    if (NULL == _event) {
        ERROR("Failed to new a UDP event");
        _clear();
        _status.set_app_errno(ERR_EVENT_EVENT_NEW);
        return _status;
    }
    else {
        // Do NOT need to auto_free because auto_free is done in this class
        // base->put_event_under_control(this);
    }

    // This event will not be added to base now. It will be done later in recv operations

    // done
    return _status;
}


#endif  // end of __CONSTRUCT_AND_DESTRUCTION


// ==========
#define __MISC_FUNCTIONS
#ifdef __MISC_FUNCTIONS

const std::map<std::string, DNSResult *> &DNSItnlClient::result() const
{
    return _dns_result;
}


Server *DNSItnlClient::owner_server()
{
    return _owner_server;
}


#endif


// ==========
#define __PRIVATE_FUNCTIONS
#ifdef __PRIVATE_FUNCTIONS

struct Error DNSItnlClient::_send_dns_request_for(const char *c_domain_name)
{
    _status.clear_err();

    if (NULL == c_domain_name) {
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }

    // build DNS request
    ::andrewmc::cpptools::Data query_data;
    _DNSQueryText query;
    query.set_as_query_type();
    query.set_transaction_ID(_transaction_ID ++);
    query.set_query_name(c_domain_name);
    query.serialize_to_data(query_data);

    DEBUG("Get request data: %s", dump_data_to_string(query_data).c_str());
    // TODO:

    return _status;
}

#endif  // end of __PRIVATE_FUNCTIONS


// ========
#define __DNS_PUBLIC_FUNCTIONS
#ifdef __DNS_PUBLIC_FUNCTIONS

NetType_t DNSItnlClient::network_type()
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


struct Error DNSItnlClient::resolve(const std::string &domain_name, double timeout_seconds, const std::string &dns_server_ip)
{
    struct timeval timeout = to_timeval(timeout_seconds);
    return resolve_in_timeval(domain_name, timeout, dns_server_ip);
}


struct Error DNSItnlClient::resolve_in_timeval(const std::string &domain_name, const struct timeval &timeout, const std::string &dns_server_ip)
{
    _send_dns_request_for(domain_name.c_str());

    // TODO:
    return _status;
}
#endif  // end of __DNS_PUBLIC_FUNCTIONS


// ========
#define __REMOTE_ADDR
#ifdef __REMOTE_ADDR

std::string DNSItnlClient::remote_addr()
{
    // TODO: 
    return "";
}


unsigned DNSItnlClient::remote_port()
{
    // TODO:
    return 0;
}


void DNSItnlClient::copy_remote_addr(struct sockaddr *addr_out, socklen_t addr_len)
{
    // TODO:
    return;
}


#endif

// end of file
