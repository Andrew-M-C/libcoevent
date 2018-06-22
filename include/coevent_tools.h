// file encoding: UTF-8

#include "coevent.h"
#include <string>
#include <stddef.h>

#ifndef __CO_EVENT_TOOLS_H__
#define __CO_EVENT_TOOLS_H__

namespace andrewmc {
namespace libcoevent {

std::string dump_data_to_string(const void *data, size_t size);

}   // end of namespace libcoevent
}   // end of namespace andrewmc
#endif  // EOF
