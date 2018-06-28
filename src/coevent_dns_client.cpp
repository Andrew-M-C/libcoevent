
#include "coevent.h"
#include "coevent_itnl.h"
#include "cpp_tools.h"
#include <string.h>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdint.h>

using namespace andrewmc::libcoevent;

#define _DNS_PORT       (53)
#define _DNS_FILE_PATH  "/etc/resolv.conf"

// ==========
#define __STATIC_PARAMETERS
#ifdef __STATIC_PARAMETERS

// DNS config file stat. If the file is updated, we should reload
// reference: [IPv6/IPv4 Address Embedding](http://www.tcpipguide.com/free/t_IPv6IPv4AddressEmbedding.htm)
// reference: [Setting Up the resolv.conf File](https://docs.oracle.com/cd/E19683-01/817-4843/dnsintro-ex-32/index.html)
static struct timespec g_DNS_file_modifed_time = {0, 0};
std::vector<std::string> g_DNS_IPs;
std::vector<NetType_t> g_DNS_network_type;

#endif  // end of __STATIC_PARAMETERS

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

    for (std::map<std::string, DNSResult *>::iterator each_dns = _dns_result.begin();
        each_dns != _dns_result.end();
        each_dns ++)
    {
        delete *each_dns;
    }
    _dns_result.clear();

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
            // OK
            break;

        case NetLocal:
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
    else    // NetIPv6
    {
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = 0;
        addr6.sin6_addr = in6addr_any;

        _fd_ipv6 = socket(AF_INET6, SOCK_DGRAM, 0);
        fd = _fd_ipv6;
        addr = (struct sockaddr *)(&addr6);
        addr_len = sizeof(addr6);
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

const DNSResult *DNSItnlClient::dns_result(const std::string &domain_name)
{
    std::map<std::string, DNSResult *>::iterator dns_item = _dns_result.find(domain_name);
    if (dns_item == _dns_result.end()) {
        return NULL;
    }

    // check if the object is valid
    if (0 == dns_item->time_to_live())  {
        _dns_result.erase(dns_item);
        return NULL;
    }
    else {
        return &(*dns_item);
    }
}


Server *DNSItnlClient::owner_server()
{
    return _owner_server;
}


#endif


// ==========
#define __PRIVATE_FUNCTIONS
#ifdef __PRIVATE_FUNCTIONS

struct Error DNSItnlClient::_send_dns_request_for(const char *c_domain_name, const struct sockaddr *addr, socklen_t addr_len)
{
    _status.clear_err();

    if (NULL == c_domain_name) {
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }
    if (NULL == addr) {
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

    ssize_t send_ret = sendto(_fd, query_data.c_data(), query_data.length(), 0, addr, addr_len);
    if (send_ret < 0) {
        _status.set_sys_errno();
    }

    return _status;
}


struct Error _recv_dns_reply(uint8_t *data_buff, size_t buff_len, size_t *recv_len_out, const struct timeval *timeout)
{
    if (!(data_buff && recv_len_out && timeout)) {
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }

    
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
    std::string dns_ip = std::string(dns_server_ip);

    // get default DNS ip address
    if (0 == dns_ip.length())
    {
        // read forst default DNS server
        std::vector<std::string>::iterator each_ip = g_DNS_IPs.begin();
        std::vector<NetType_t>::iterator each_type = g_DNS_network_type.begin();

        do {
            if (network_type() == *each_type) {
                dns_ip.assign(*each_ip);
                break;
            }
            each_ip ++;
            each_type ++;
        }
        while(each_ip != g_DNS_IPs.end());
        
        // failed to read default DNS
        if (0 == dns_ip.length()) {
            _status.set_app_errno(ERR_DNS_SERVER_IP_NOT_FOUND);
            return _status;
        }
    }

    // resolve DNS IP address
    int convert_stat = 0;
    struct sockaddr_storage addr;
    socklen_t addr_len;

    if (NetIPv4 == network_type())
    {
        struct sockaddr_in *p_addr = (struct sockaddr_in *)(&addr);
        addr_len = sizeof(*p_addr);
        p_addr->sin_family = AF_INET;
        p_addr->sin_port = htons(_DNS_PORT);
        convert_stat = inet_pton(AF_INET, dns_ip.c_str(), &(p_addr->sin_addr));
    }
    else {
        struct sockaddr_in6 *p_addr = (struct sockaddr_in6 *)(&addr);
        addr_len = sizeof(*p_addr);
        p_addr->sin6_family = AF_INET6;
        p_addr->sin6_port = htons(_DNS_PORT);
        convert_stat = inet_pton(AF_INET6, dns_ip.c_str(), &(p_addr->sin6_addr));
    }

    if (1 == convert_stat) {
        // OK and continue
    }
    else if (0 == convert_stat) {
        _status.set_app_errno(ERR_DNS_SERVER_IP_NOT_FOUND);
        return _status;
    }
    else {
        _status.set_sys_errno();
        return _status;
    }

    // send DNS request
    _send_dns_request_for(dns_ip.c_str(), (struct sockaddr *)(&addr), addr_len);
    if (FALSE == _status.is_ok()) {
        return _status;
    }

    // recv and resolve
    // TODO:

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


std::string DNSItnlClient::default_dns_server(size_t index, NetType_t *network_type_out)
{
    BOOL should_update_DNS = FALSE;
    struct stat dns_file_stat;

    // determine if we should update default DNS config
    if (0 == g_DNS_IPs.size()) {
        should_update_DNS = TRUE;
    }
    else {
        // check if DNS config file is updated
        int call_stat = stat(_DNS_FILE_PATH, &dns_file_stat);
        if (call_stat < 0) {
            ERROR("Failed to read DNS file stat: %s", strerror(errno));
        }
        else {
            if (dns_file_stat.st_mtim.tv_sec != g_DNS_file_modifed_time.tv_sec
                || dns_file_stat.st_mtim.tv_nsec != g_DNS_file_modifed_time.tv_nsec)
            {
                // DNS config file modified
                should_update_DNS = TRUE;
            }
        }
    }

    // update default DNS config
    if (should_update_DNS)
    {
        FILE *dns_file = fopen(_DNS_FILE_PATH, "f");
        if (NULL == dns_file) {
            ERROR("Failed to open DNS file: %s", strerror(errno));
        }
        else {
            char line_buff[516];

            g_DNS_IPs.clear();
            g_DNS_network_type.clear();

            while (fgets(line_buff, sizeof(line_buff), dns_file) != NULL)
            {
                const char *name_srv_leading = "nameserver";
                size_t ip_start = 0;
                BOOL is_ip_start_found = FALSE;
                NetType_t network_type = NetUnknown;

                // is this a name server configuration?
                if (NULL == strstr(line_buff, name_srv_leading)) {
                    continue;
                }

                // check network type
                if (NULL == strstr(line_buff, ":")) {
                    network_type = NetIPv6;
                } else {
                    network_type = NetIPv4;
                }

                // find DNS IP configuration
                ip_start += sizeof(name_srv_leading);
                do {
                    switch(name_srv_leading[ip_start])
                    {
                        case ':':
                        case '0':
                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7':
                        case '8':
                        case '9':
                        case 'a':
                        case 'b':
                        case 'c':
                        case 'e':
                        case 'f':
                        case 'A':
                        case 'B':
                        case 'C':
                        case 'D':
                        case 'E':
                        case 'F':
                            is_ip_start_found = TRUE;
                        case '\0':
                            continue;   // skip this line
                            break;
                        default:
                            ip_start ++;
                            break;
                    }
                } while (FALSE == is_ip_start_found);

                // resolve
                std::string ip = std::string(line_buff + ip_start);
                DEBUG("DNS - %u: %s", g_DNS_IPs.size(), ip.c_str());
                g_DNS_IPs.push_back(ip);
                g_DNS_network_type.push_back(network_type);

            }   // end of while()

            fclose(dns_file);
            dns_file = NULL;
        }

    }

    // return IP address
    if (index <= g_DNS_IPs.size()) {
        if (network_type_out) {
            *network_type_out = g_DNS_network_type[index];
        }
        return g_DNS_IPs[index];
    }
    else {
        if (network_type_out) {
            *network_type_out = NetUnknown;
        }
        return "";
    }
}


#endif

// end of file
