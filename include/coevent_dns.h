// file encoding: UTF-8

#ifndef __CO_EVENT_DNS_H__
#define __CO_EVENT_DNS_H__

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
// DNSClient
class DNSResult;
class DNSResourceRecord;

class DNSClient : public Client {
public:
    DNSClient(){};
    virtual ~DNSClient(){};

    virtual NetType_t network_type() = 0;
    virtual struct Error resolve(const std::string &domain_name, double timeout_seconds = 0, const std::string &dns_server_ip = "") = 0;
    virtual struct Error resolve_in_timeval(const std::string &domain_name, const struct timeval &timeout, const std::string &dns_server_ip = "") = 0;
    virtual struct Error resolve_in_milisecs(const std::string &domain_name, unsigned timeout_milisecs, const std::string &dns_server_ip = "") = 0;

    virtual std::string default_dns_server(size_t index = 0, NetType_t *network_type_out = NULL) = 0;

    virtual const DNSResult *dns_result(const std::string &domain_name) = 0;
    virtual std::string quick_resolve(const std::string &domain_name, double timeout_seconds = 0, const std::string &dns_server_ip = "") = 0;
};


// DNSResult for DNSClient
typedef enum {
    DnsRRType_Unknown   = 0,
    DnsRRType_IPv4Addr  = 1,
    DnsRRType_IPv6Addr  = 28,
    DnsRRType_ServerName = 2,
    DnsRRType_CName     = 5,
} DNSRRType_t;

typedef enum {
    DnsRRClass_Unknown = 0,
    DnsRRClass_Internet = 1,
} DNSRRClass_t;


class DNSResult {
public:
    time_t      _update_time;   // saved as sysup time
    time_t      _update_ttl;    // saved as ttl remains comparing _update_time
    std::string _domain_name;
    std::vector<DNSResourceRecord *> _rr_list;
public:
    DNSResult();
    virtual ~DNSResult();
    const std::string &domain_name() const;
    std::vector<std::string> IP_addresses();
    time_t time_to_live() const;
    BOOL parse_from_udp_payload(const void *data, const size_t length);

    size_t resource_record_count() const;
    const DNSResourceRecord *resource_record(size_t index) const;
};


class DNSResourceRecord {
public:
    std::string     _rr_name;
    DNSRRType_t     _rr_type;
    DNSRRClass_t    _rr_class;
    uint32_t        _rr_ttl;
    std::string     _rr_address;
public:
    DNSResourceRecord():
        _rr_type(DnsRRType_Unknown), _rr_class(DnsRRClass_Unknown), _rr_ttl(0)
    {}

    const std::string &record_name() const;
    DNSRRType_t record_type() const;
    DNSRRClass_t record_class() const;
    uint32_t record_ttl() const;
    const std::string &record_address() const;
};


}   // end of namespace libcoevent
}   // end of namespace andrewmc

#endif  // EOF
