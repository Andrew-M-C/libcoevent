
#include "coevent.h"
#include "coevent_itnl.h"
#include "cpp_tools.h"
#include <string>
#include <stdio.h>
#include <set>

using namespace andrewmc::libcoevent;

// ==========
// class DNSResult
#define __CLASS_DNS_RESULT
#ifdef __CLASS_DNS_RESULT

DNSResult::DNSResult()
{
    _update_time = 0;
    _update_ttl = 0;
    return;
}


DNSResult::~DNSResult()
{
    for (std::vector<DNSResourceRecord *>::iterator each_rr = _rr_list.begin();
        each_rr != _rr_list.end();
        ++ each_rr)
    {
        delete *each_rr;
    }
    _rr_list.clear();
    return;
}


const std::string &DNSResult::domain_name() const
{
    return _domain_name;
}


std::vector<std::string> DNSResult::IP_addresses()
{
    std::vector<std::string> ret;

    for (std::vector<DNSResourceRecord *>::iterator each_rr = _rr_list.begin();
        each_rr != _rr_list.end();
        ++ each_rr)
    {
        ret.push_back((*each_rr)->record_address());
    }

    return ret;
}


time_t DNSResult::time_to_live()
{
    time_t sysup_time = ::andrewmc::cpptools::sys_up_time();
    time_t expire_time = _update_time + _update_ttl;
    return (expire_time < sysup_time) ? 0 : expire_time - sysup_time;
}


#endif  // end of __CLASS_DNS_RESULT


// ==========
// class DNSResourceRecord
#define __CLASS_DNS_RESOURCE_RECORD
#ifdef __CLASS_DNS_RESOURCE_RECORD

const std::string &DNSResourceRecord::record_name() const
{
    return _rr_name;
}


DNSRRType_t DNSResourceRecord::record_type() const
{
    return _rr_type;
}


DNSRRClass_t DNSResourceRecord::record_class() const
{
    return _rr_class;
}


uint32_t DNSResourceRecord::record_ttl() const
{
    return _rr_ttl;
}


const std::string &DNSResourceRecord::record_address() const
{
    return _rr_address;
}


#endif  // end of __CLASS_DNS_RESOURCE_RECORD

// end of file
