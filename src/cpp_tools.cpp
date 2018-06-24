
#include "coevent.h"
#include "cpp_tools.h"

#include <stdint.h>
#include <stdio.h>

using namespace andrewmc::cpptools;

// ==========
#define __PUBLIC_FUNCTIONS
#ifdef __PUBLIC_FUNCTIONS

static const char _UINT8_TO_CHAR[128] = {
    '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.',     // 0x00~0x0F
    '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.',     // 0x10~0x1F
    ' ', '!', '\"','#', '$', '%', '&', '\'','(', ')', '*', '+', ',', '-', '.', '/',     // 0x20~0x2F
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',     // 0x30~0x3F
    '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',     // 0x40~0x4F
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\',']', '^', '_',     // 0x50~0x5F
    '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l' ,'m', 'n', 'o',     // 0x60~0x6F
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|' ,'}', '~', '.',     // 0x70~0x7F
};


static size_t _strcpy_without_end(char *dst, const char *src)
{
    if (!(dst && src)) {
        return 0;
    }
    size_t ret = 0;
    while (*src)
    {
        *dst = *src;
        dst ++;
        src ++;
        ret ++;
    }
    return ret;
}


static inline void _byte_to_str_without_end(char *dst, uint8_t byte)
{
    uint8_t higher = (byte & 0xF0) >> 4;
    uint8_t lower  = (byte & 0x0F);

    if (higher >= 0xA) {
        dst[0] = (char)(higher - 0x0A + 'A');
    } else {
        dst[0] = (char)(higher + '0');
    }

    if (lower >= 0xA) {
        dst[1] = (char)(lower - 0x0A + 'A');
    } else {
        dst[1] = (char)(lower + '0');
    }

    return;
}


std::string andrewmc::cpptools::dump_data_to_string(const void *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    std::string ret;

    // leading message
    {
        char line[128];
        sprintf(line, "Data in address 0x%p, length %u", data, (unsigned)size);
        ret.append(line);
    }

    // header
    {
        const char *line = "\n------      0  1  2  3  4  5  6  7  | 8  9  A  B  C  D  E  F      012345678 9ABCDEF";
        ret.append(line);
    }

    // each line
    {
        // line indexes:    0123456789 123456789 123456789 123456789 123456789 123456789 12345
        //char line_A[] = "\n0x0000      00 00 00 00 00 00 00 00 | 00 00 00 00 00 00 00 00    "
        //char line_B[] = "01234567 89ABCDEF";
        char buff[128] = "";

        for (size_t line_start = 0; line_start < size; line_start += 16)
        {
            char line_A[] = "\n                                                                  ";
            char line_B[] = "                 ";
            size_t offset_A = 0;
            size_t offset_B = 0;

            sprintf(buff, "\n0x%04X", (unsigned)line_start);
            _strcpy_without_end(line_A, buff);
            offset_A = 13;
            offset_B = 0;

            for (size_t index = line_start;
                index < (line_start + 16) && index < size;
                index ++)
            {
                uint8_t byte = (uint8_t)(bytes[index]);

                // line_A
                _byte_to_str_without_end(line_A + offset_A, byte);

                // line_B
                if (byte > 0x7F) {
                    line_B[offset_B ++] = '.';
                } else {
                    line_B[offset_B ++] = _UINT8_TO_CHAR[byte];
                }

                // offsets
                if (line_start + 7 == index) {
                    offset_A += 3;
                    line_A[offset_A] = '|';
                    offset_A += 2;
                    offset_B ++;
                }
                else {
                    offset_A += 3;
                }
            }

            ret.append(line_A);
            ret.append(line_B);
        }   // end of for (...)
    }

    // return
    return ret;
}

#endif  // end of __PUBLIC_FUNCTIONS



