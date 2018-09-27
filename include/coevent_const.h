// file encoding: UTF-8

#ifndef __CO_EVENT_CONST_H__
#define __CO_EVENT_CONST_H__

#include "event2/event.h"

#ifndef NULL
#define NULL    ((void*)0)
#endif


#ifndef BOOL
#define BOOL    int
#endif


#ifndef FALSE
#define FALSE   (0)
#define TRUE    (!FALSE)
#endif


namespace andrewmc {
namespace libcoevent {

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

}   // end of namespace libcoevent
}   // end of namespace andrewmc
#endif  // EOF
