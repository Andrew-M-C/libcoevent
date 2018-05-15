
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
    LOG("Hello, libcoevent!");
    return 0;
}

#endif  // end of libcoevent::Base

