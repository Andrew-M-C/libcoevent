
#include "coevent.h"
#include "coevent_itnl.h"
#include "cpp_tools.h"
#include <string>
#include <stdio.h>
#include <set>

using namespace andrewmc::libcoevent;

struct _DNSPayloadHeader {
    uint16_t    transacton_ID;
    uint16_t    flags;
    uint16_t    questions;
    uint16_t    answer_rrs;
    uint16_t    authority_rrs;
    uint16_t    additional_rrs;
};

// ==========
// class DNSResult
#define __CLASS_DNS_RESULT
#ifdef __CLASS_DNS_RESULT

static ssize_t _search_next_zero_byte(const uint8_t *data, size_t offset, size_t length)
{
    while(0 != data[offset])
    {
        ++ offset;
        if (offset >= length) {
            return -1;
        }
    }
    return offset;
}


static BOOL _read_RR_name_data(::andrewmc::cpptools::Data &buff_out, const uint8_t *data, size_t offset, const size_t length, size_t *end_pos_out)
{
    const char DOT = '.';
    BOOL should_end_searching = FALSE;

    do {
        // read length
        uint8_t part_len = data[offset];
        if (0 == part_len)
        {
            // ends
            offset += 1;
            should_end_searching = TRUE;
        }
        else if (0 == (part_len & 0xC0))
        {
            // normal str part
            offset += 1;

            if (buff_out.length()) {
                buff_out.append((void*)(&DOT), sizeof(DOT));
            }
            buff_out.append(data + offset, part_len);
            offset += part_len;
        }
        else
        {
            uint16_t name_pointer = ((uint16_t)(part_len & ~(0xC0))) << 8;
            name_pointer += data[offset + 1];
            offset += 2;
            if (FALSE == _read_RR_name_data(buff_out, data, name_pointer, length, NULL))
            {
                return FALSE;
            }
            should_end_searching = TRUE;
        }
    }
    while(offset < length && FALSE == should_end_searching);

    if (end_pos_out) {
        *end_pos_out = offset;
    }
    return (offset < length);
}


static std::string _parse_query_RR(const uint8_t *data, size_t offset, const size_t length, size_t *next_pos_out)
{
    ::andrewmc::cpptools::Data data_buff;
    size_t next_pos = offset;
    const uint8_t STR_END_BYTE = 0;
    BOOL read_RR_status = _read_RR_name_data(data_buff, data, offset, length, &next_pos);

    if (FALSE == read_RR_status) {
        return "";
    }

    next_pos += 2 * sizeof(uint16_t);       // skip type and class sectors
    if (next_pos_out) {
        *next_pos_out = next_pos;
    }

    data_buff.append(&STR_END_BYTE, sizeof(STR_END_BYTE));
    return std::string((char *)(data_buff.c_data()));
}


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


BOOL DNSResult::parse_from_udp_payload(const void *bytes, const size_t length)
{
    const uint8_t *data = (const uint8_t *)bytes;
    if (NULL == bytes) {
        return FALSE;
    }
    if (length < sizeof(struct _DNSPayloadHeader)) {
        return FALSE;
    }

    // read header
    size_t offset = 0;
    struct _DNSPayloadHeader header;
    memcpy(&header, bytes, sizeof(header));

    for (uint16_t *part = (uint16_t *)(&header), offset = 0;
        offset < (sizeof(header) / sizeof(uint16_t));
        offset ++)
    {
        part[offset] = ntohs(part[offset]);
    }
    offset = 0;

    if (0 != (header.flags & 0x0F)) {
        ERROR("DNS response error code: %u", (unsigned)(header.flags & 0xF));
        return FALSE;
    }
    if (0 == header.answer_rrs) {
        ERROR("No DNS RR respond");
        return FALSE;
    }

    // read query
    offset += sizeof(header);
    _domain_name = _parse_query_RR(data, offset, length, &offset);
    if (0 == _domain_name.length()) {
        ERROR("Failed to parse DNS RR name from data: %s", ::andrewmc::cpptools::dump_data_to_string(data, length).c_str());
        return FALSE;
    }
    else {
        DEBUG("Got domain name: %s", _domain_name.c_str());
    }

    // TODO:
    return TRUE;
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
