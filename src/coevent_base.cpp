
#include "coevent.h"
#include <string>

using namespace andrewmc::libcoevent;

// ==========
#define __CO_EVENT_BASE
#ifdef __CO_EVENT_BASE

struct Error Base::run()
{
    struct Error ret_code;

    if (NULL == _event_base) {
        ret_code.set_app_errno(ERR_EVENT_BASE_NEW);
        return ret_code;
    }

    int err = event_base_dispatch(_event_base);
    if (0 == err) {
        // nothing more to do
    }
    else if (1 == err) {
        ret_code.set_app_errno(ERR_EVENT_BASE_NO_EVENT_PANDING);
    }
    else {
        ret_code.set_app_errno(ERR_EVENT_BASE_DISPATCH);
    }
    return ret_code;
}

#endif  // end of libcoevent::Base

