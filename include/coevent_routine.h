// file encoding: UTF-8

#ifndef __CO_EVENT_ROUTINE_H__
#define __CO_EVENT_ROUTINE_H__

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


namespace andrewmc {
namespace libcoevent {

// ====================
// pure event, no network server supported
class SubRoutine : public Server {
protected:
    void            *_event_arg;
public:
    SubRoutine();
    virtual ~SubRoutine();
    struct Error init(Base *base, WorkerFunc func, void *user_arg = NULL, BOOL auto_free = TRUE);

    struct Error sleep(double seconds);     // can ONLY be incoked inside coroutine
    struct Error sleep(const struct timeval &sleep_time);
    struct Error sleep_milisecs(unsigned mili_secs);
private:
    void _init();
    void _clear();
protected:
    struct stCoRoutine_t *_coroutine();
};


}   // end of namespace libcoevent
}   // end of namespace andrewmc

#endif  // EOF
