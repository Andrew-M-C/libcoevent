// file encoding: UTF-8

#ifndef __CO_EVENT_CONST_H__
#define __CO_EVENT_CONST_H__

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

typedef enum {
    ERR_SUCCESS = 0,
    ERR_EVENT_BASE_NEW,
    ERR_EVENT_BASE_DISPATCH,
    ERR_EVENT_EVENT_NEW,
    ERR_EVENT_BASE_NO_EVENT_PANDING,

    ERR_LIBCO_CREATE,
    ERR_PARA_NULL,
    ERR_PARA_ILLEGAL,

    ERR_NETWORK_TYPE_ILLEGAL,
    ERR_BIND_PATH_ILLEGAL,

    ERR_INTERRUPTED_SLEEP,
    ERR_TIMEOUT,

    ERR_NOT_INITIALIZED,
    ERR_OBJ_NOT_FOUND,

    ERR_DNS_SERVER_IP_NOT_FOUND,

    ERR_UNKNOWN     // should place at last
} ErrCode_t;

}   // end of namespace libcoevent
}   // end of namespace andrewmc
#endif  // EOF
