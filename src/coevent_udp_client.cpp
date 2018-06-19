
#include "coevent.h"
#include "coevent_itnl.h"
#include <string>
#include <stdio.h>
#include <set>

using namespace andrewmc::libcoevent;

// ----------
#define __INIT_FUNCTIONS
#ifdef __INIT_FUNCTIONS


UDPItnlClient::UDPItnlClient()
{
    _init();
    return;
}


UDPItnlClient::~UDPItnlClient()
{
    _clear();
    return;
}


void UDPItnlClient::_init()
{

}


#endif  // end of __INIT_FUNCTIONS

// end of file
