
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

uint16_t DNSItnlClient::_transaction_ID = 0;

// DNS config file stat. If the file is updated, we should reload
// reference: [IPv6/IPv4 Address Embedding](http://www.tcpipguide.com/free/t_IPv6IPv4AddressEmbedding.htm)
// reference: [Setting Up the resolv.conf File](https://docs.oracle.com/cd/E19683-01/817-4843/dnsintro-ex-32/index.html)
// reference: [DNS 报文结构和个人 DNS 解析代码实现——解决 getaddrinfo() 阻塞问题](https://segmentfault.com/a/1190000009369381)
static struct timespec g_DNS_file_modifed_time = {0, 0};
std::vector<std::string> g_DNS_IPs;
std::vector<NetType_t> g_DNS_network_type;

#endif  // end of __STATIC_PARAMETERS

// ==========
// necessary definitions
#define __CO_EVENT_DNS_CLIENT_DEFINITIONS
#ifdef __CO_EVENT_DNS_CLIENT_DEFINITIONS

#define _DNS_HEADER_LENGTH      (12)

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

    void serialize_to_data(::andrewmc::cpptools::Data &data, NetType_t network_type)
    {
        char header[_DNS_HEADER_LENGTH];
        data.clear();

        // header
        memcpy(header, this, sizeof(header));
        data.append((void *)header, sizeof(header));

        // RR name
        const uint8_t null_char = '\0';
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
        data.append((void *)(&null_char), sizeof(null_char));

        // RR type
        uint16_t rr_type = 0;
        if (NetIPv4 == network_type) {
            rr_type = htons(1);     // "A"
        } else if (NetIPv6 == network_type) {
            rr_type = htons(28);    // "AAAA"
        }
        data.append((void *)(&rr_type), sizeof(rr_type));

        // RR class
        uint16_t rr_class = htons(0x0001);
        data.append((void *)(&rr_class), sizeof(rr_class));
        return;
    }

    unsigned error_code()
    {
        uint16_t host_flags = ntohs(flags);
        host_flags &= 0x000F;
        return (unsigned)host_flags;
    }

    size_t answer_RR_count()
    {
        return ntohs(answer_rrs);
    }

};

#endif


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
    if (_udp_client)
    {
        delete _udp_client;
        _udp_client = NULL;
    }

    return;
}


void DNSItnlClient::_init()
{
    char identifier[64];
    sprintf(identifier, "DNS client %p", this);
    _identifier = identifier;

    _udp_client = NULL;
    return;
}


struct Error DNSItnlClient::init(Server *server, struct stCoRoutine_t *coroutine, NetType_t network_type, void *user_arg)
{
    if (!(server && coroutine)) {
        _status.set_app_errno(ERR_PARA_NULL);
        return _status;
    }

    _init();
    _udp_client = new UDPItnlClient;
    _status = _udp_client->init(server, coroutine, network_type, user_arg);
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
        DEBUG("DNS recode for %s not found", domain_name.c_str());
        return NULL;
    }

    // check if the object is valid
    if (0 == dns_item->second->time_to_live())  {
        DEBUG("DNS record for %s timeout", domain_name.c_str());
        _dns_result.erase(dns_item);
        return NULL;
    }
    else {
        return dns_item->second;
    }
}


Server *DNSItnlClient::owner_server()
{
    if (_udp_client) {
        return _udp_client->owner_server();
    } else {
        return NULL;
    }
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
    query.set_transaction_ID(++ _transaction_ID);
    query.set_query_name(c_domain_name);
    query.serialize_to_data(query_data, network_type());

    DEBUG("Netowrk type: %u", network_type());

    DEBUG("Get request data: %s", dump_data_to_string(query_data).c_str());

    _status = _udp_client->send(query_data.c_data(), query_data.length(), NULL, addr, addr_len);
    return _status;
}


void DNSItnlClient::_parse_dns_response(const uint8_t *data_buff, size_t data_len)
{
    DNSResult *result = new DNSResult;
    if (result->parse_from_udp_payload(data_buff, data_len)) {
        DEBUG("%s DNS record found", result->domain_name().c_str());
        _dns_result[result->domain_name()] = result;
    }
    else {
        delete result;
        result = NULL;
    }
    return;
}


#endif  // end of __PRIVATE_FUNCTIONS


// ========
#define __DNS_PUBLIC_FUNCTIONS
#ifdef __DNS_PUBLIC_FUNCTIONS

NetType_t DNSItnlClient::network_type()
{
    if (_udp_client) {
        return _udp_client->network_type();
    } else {
        return NetUnknown;
    }
}


struct Error DNSItnlClient::resolve(const std::string &domain_name, double timeout_seconds, const std::string &dns_server_ip)
{
    struct timeval timeout = to_timeval(timeout_seconds);
    return resolve_in_timeval(domain_name, timeout, dns_server_ip);
}


struct Error DNSItnlClient::resolve_in_milisecs(const std::string &domain_name, unsigned timeout_milisecs, const std::string &dns_server_ip)
{
    struct timeval timeout = to_timeval_from_milisecs(timeout_milisecs);
    return resolve_in_timeval(domain_name, timeout, dns_server_ip);
}


struct Error DNSItnlClient::resolve_in_timeval(const std::string &domain_name, const struct timeval &timeout_orig, const std::string &dns_server_ip)
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
    _send_dns_request_for(domain_name.c_str(), (struct sockaddr *)(&addr), addr_len);
    if (FALSE == _status.is_ok()) {
        return _status;
    }

    // recv and resolve
    size_t recv_size = 0;
    uint8_t data_buff[2048];
    struct timeval now_time = ::andrewmc::cpptools::sys_up_timeval();
    struct timeval end_time;
    struct timeval remain_time;
    BOOL should_recv_forever = FALSE;
    BOOL should_continue_recv = TRUE;

    if (0 == timeout_orig.tv_sec && 0 == timeout_orig.tv_usec)
    {
        should_recv_forever = TRUE;
        end_time.tv_sec = FOREVER_SECONDS;
        end_time.tv_usec = 0;
        remain_time.tv_sec = FOREVER_SECONDS;
        remain_time.tv_usec = 0;
    }
    else {
        should_recv_forever = FALSE;
        timeradd(&now_time, &timeout_orig, &end_time);
        remain_time.tv_sec = timeout_orig.tv_sec;
        remain_time.tv_usec = timeout_orig.tv_usec;
    }

    // recv()
    do {
        recv_size = 0;
        _status = _udp_client->recv_in_timeval(data_buff, sizeof(data_buff), &recv_size, remain_time);

        // check recv status
        if (FALSE == _status.is_ok()) {
            should_continue_recv = FALSE;
        }
        else if (0 == recv_size) {
            DEBUG("No data to go");
            should_continue_recv = TRUE;
        }
        else {
            DEBUG("Get DNS reply: %s", ::andrewmc::cpptools::dump_data_to_string(data_buff, recv_size).c_str());
            _parse_dns_response(data_buff, recv_size);

            if (dns_result(domain_name)) {
                // OK, searched
                DEBUG("DNS resuest found");
                should_continue_recv = FALSE;
            }
            else {
                DEBUG("Not the DNS request we are loking for");
            }
        }   // end of if (FALSE == _status.is_ok())

        // check timeout and continue recv
        if (should_continue_recv)
        {
            if (FALSE == should_recv_forever)
            {
                now_time = ::andrewmc::cpptools::sys_up_timeval();
                if (timercmp(&now_time, &end_time, <)) {
                    // should continue waiting
                    timersub(&end_time, &now_time, &remain_time);
                }
                else {
                    _status.set_app_errno(ERR_TIMEOUT);
                    should_continue_recv = FALSE;
                }
            }
        }
    } while (should_continue_recv);

    // return
    return _status;
}
#endif  // end of __DNS_PUBLIC_FUNCTIONS


// ========
#define __REMOTE_ADDR
#ifdef __REMOTE_ADDR

std::string DNSItnlClient::remote_addr()
{
    return _udp_client->remote_addr();
}


unsigned DNSItnlClient::remote_port()
{
    return _udp_client->remote_port();
}


void DNSItnlClient::copy_remote_addr(struct sockaddr *addr_out, socklen_t addr_len)
{
    _udp_client->copy_remote_addr(addr_out, addr_len);
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
            should_update_DNS = TRUE;
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
        FILE *dns_file = fopen(_DNS_FILE_PATH, "r");
        if (NULL == dns_file)
        {
            ERROR("Failed to open DNS file: %s", strerror(errno));

            switch (network_type())
            {
                default:
                case NetIPv4:
                    return std::string("8.8.8.8");
                    break;
                case NetIPv6:
                    return std::string("2001:4860:4860::8888");
                    break;
            }
        }
        else {
            char line_buff[516];

            g_DNS_IPs.clear();
            g_DNS_network_type.clear();

            while (fgets(line_buff, sizeof(line_buff), dns_file) != NULL)
            {
                const char name_srv_leading[] = "nameserver";
                size_t line_len = strlen(line_buff);
                size_t ip_start = 0;
                BOOL is_ip_start_found = FALSE;
                NetType_t network_type = NetUnknown;

                // is this a name server configuration?
                if (NULL == strstr(line_buff, name_srv_leading)) {
                    continue;
                }

                // remove return char
                while((line_len > 0) && 
                    ('\n' == line_buff[line_len - 1] || '\r' == line_buff[line_len - 1]))
                {
                    line_buff[line_len - 1] = '\0';
                    line_len -= 1;
                }

                // check network type
                ip_start += sizeof(name_srv_leading);
                if (strstr(line_buff + ip_start, ":")) {
                    network_type = NetIPv6;
                } else {
                    network_type = NetIPv4;
                }

                // find DNS IP configuration
                do {
                    switch(line_buff[ip_start])
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
                //DEBUG("DNS - %u: %s", g_DNS_IPs.size(), ip.c_str());
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
        switch (network_type())
        {
            default:
            case NetIPv4:
                return std::string("8.8.8.8");
                break;
            case NetIPv6:
                return std::string("2001:4860:4860::8888");
                break;
        }
    }
}


#endif

// end of file
