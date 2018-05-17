
#include "coevent.h"
#include <stdio.h>
#include <string>

#define LOG(fmt, args...)   printf("SERVER: %s, %d: "fmt"\n", __FILE__, __LINE__, ##args)

using namespace andrewmc::libcoevent;

// ==========
#define __MAIN
#ifdef __MAIN

int main(int argc, char *argv[])
{
    Base *base = new Base;
    LOG("Hello, libcoevent! Identifier: %s", base->identifier().c_str());
    return 0;
}

#endif  // end of libcoevent::Base

