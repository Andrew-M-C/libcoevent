
#include "cpp_tools.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <bits/wordsize.h>

using namespace andrewmc::cpptools;

#define _DEFAULT_CAPACITY       (4*1024)        // 0x1000
#define _SIZE_DECREASING_THRESHOLD      (4*1024)
#define _COEFFICIENT_THRESHOLD  (10)

#define _DB(fmt, args...)   printf("%s, %s(), %d: "fmt"\n", __FILE__, __func__, __LINE__, ##args)

#ifndef FALSE
#define FALSE   (0)
#define TRUE    (!(FALSE))
#endif

// ==========
#define __PROTECTED_FUNCTIONS
#ifdef __PROTECTED_FUNCTIONS
// reference:
// https://blog.csdn.net/K346K346/article/details/80427245
// https://bbs.csdn.net/topics/330096991

size_t Data::_appropriate_buff_size(size_t length)
{
    if (0 == length) {
        length = _data_len;
    }

    if (length <= _SIZE_DECREASING_THRESHOLD) {
        return _SIZE_DECREASING_THRESHOLD;
    }
    size_t n = 1;

#if __WORDSIZE == 32
    size_t x = length;
    if (x >> 31) {
        throw std::bad_alloc();
        return 0;
    }
    if ((x>>16) == 0) {n = n+16; x = x<<16;}
    if ((x>>24) == 0) {n = n+8; x = x<<8;}
    if ((x>>28) == 0) {n = n+4; x = x<<4;}
    if ((x>>30) == 0) {n = n+2; x = x<<2;}
    return 1 << (n + 1);
#else
    size_t x = length;
    if (x >> 63) {
        throw std::bad_alloc();
        return 0;
    }
    if ((x>>32) == 0) {n = n+32; x = x<<32;}
    if ((x>>48) == 0) {n = n+16; x = x<<16;}
    if ((x>>56) == 0) {n = n+8; x = x<<8;}
    if ((x>>60) == 0) {n = n+4; x = x<<4;}
    if ((x>>62) == 0) {n = n+2; x = x<<2;}
    return 1 << (n + 1);
#endif
}


void Data::_check_coefficient()
{
    size_t expected_buff_size = _appropriate_buff_size();
    if (expected_buff_size < _buff_size) {
        _coefficient ++;
    }

    if (_coefficient > _COEFFICIENT_THRESHOLD)
    {
        // reduce capacity
        void *buff_backup = _data_buff;
        void *new_buff = realloc(_data_buff, expected_buff_size);
        if (new_buff) {
            _DB("realloc() from %u to %u", (unsigned)_buff_size, (unsigned)expected_buff_size);
            _data_buff = new_buff;
            _buff_size = expected_buff_size;
            _coefficient = 0;
        }
        else {
            // realloc failed
            _DB("realloc() from %u to %u failed, %s", (unsigned)_buff_size, (unsigned)expected_buff_size, strerror(errno));
            _data_buff = buff_backup;
            _coefficient = 0;   // do not realloc too frequently
        }
    }

    return;
}


BOOL Data::_resize(size_t length)
{
    size_t expected_size = _appropriate_buff_size(length);
    void *buff_backup = _data_buff;

    _data_buff = realloc(_data_buff, expected_size);
    if (NULL == _data_buff) {
        _DB("realloc() from %u to %u failed, %s", (unsigned)_buff_size, (unsigned)expected_size, strerror(errno));
        _data_buff = buff_backup;
        return FALSE;
    }
    else {
        _DB("realloc() from %u to %u", (unsigned)_buff_size, (unsigned)expected_size);
        _buff_size = expected_size;
        _coefficient = 0;
        return TRUE;
    }
}


#endif


// ==========
#define __ACCESS
#ifdef __ACCESS

size_t Data::size() const
{
    return _data_len;
}


size_t Data::length() const
{
    return _data_len;
}


const void *Data::c_data() const
{
    return _data_buff;
}


const void *Data::bytes() const
{
    return _data_buff;
}

#endif

// ==========
#define __INITIALIZATION
#ifdef __INITIALIZATION

Data::Data()
{
    _data_len = 0;
    _buff_size = 0;
    _coefficient = 0;

    _data_buff = malloc(_DEFAULT_CAPACITY);
    if (NULL == _data_buff) {
        throw std::bad_alloc();
        return;
    }

    _buff_size = _DEFAULT_CAPACITY;
    return;
}


Data::Data(const void *c_data, size_t length)
{
    _data_len = 0;
    _buff_size = 0;
    _coefficient = 0;
    _data_buff = NULL;

    if (NULL == c_data)
    {
        errno = EINVAL;
        throw std::invalid_argument("c_data");
        return;
    }

    size_t cap = _appropriate_buff_size(length);

    _data_buff = malloc(cap);
    if (NULL == _data_buff) {
        throw std::bad_alloc();
        return;
    }

    memcpy(_data_buff, c_data, length);
    _data_len = length;
    _buff_size = cap;

    return;
}


Data::Data(const Data &data_to_copy)
{
    const void *c_data = data_to_copy.c_data();
    size_t length = data_to_copy.length();

    _data_len = 0;
    _buff_size = 0;
    _coefficient = 0;
    _data_buff = NULL;

    if (NULL == c_data)
    {
        errno = EINVAL;
        throw std::invalid_argument("c_data");
        return;
    }

    size_t cap = _appropriate_buff_size(length);

    _data_buff = malloc(cap);
    if (NULL == _data_buff) {
        throw std::bad_alloc();
        return;
    }

    memcpy(_data_buff, c_data, length);
    _data_len = length;
    _buff_size = cap;

    return;
}


Data::~Data()
{
    if (_data_buff) {
        free(_data_buff);
        _data_buff = NULL;
    }

    _data_len = 0;
    _buff_size = 0;
    _coefficient = 0;
    return;
}

#endif  // end of __INITIALIZATION


// ==========
#define __MODIFYING
#ifdef __MODIFYING

void Data::clear()
{
    _data_len = 0;
    if (_buff_size > _SIZE_DECREASING_THRESHOLD) {
        _check_coefficient();
    }
    return;
}


void Data::turncate(size_t length)
{
    if (length < _data_len)
    {
        _data_len = length;
        _check_coefficient();
    }
    return;
}


void Data::expand(size_t length)
{
    if (length <= _data_len) {
        return;
    }

    if (length < _buff_size) {
        uint8_t *data_buff = (uint8_t *)_data_buff;
        memset(data_buff + _data_len, 0, length - _data_len);
        _data_len = length;
        _check_coefficient();
        return;
    }

    // should invoke realloc
    BOOL realloc_ok = _resize(length);
    if (FALSE == realloc_ok) {
        throw std::bad_alloc();
        return;
    }

    uint8_t *data_buff = (uint8_t *)_data_buff;
    memset(data_buff + _data_len, 0, length - _data_len);
    _data_len = length;
    return;
}


void Data::set_length(size_t length)
{
    if (length < _data_len) {
        turncate(length);
    }
    else {
        expand(length);
    }
    return;
}


void Data::copy(const Data &data)
{
    const void *c_data = data.c_data();
    size_t length = data.length();
    copy(c_data, length);
    return;
}


void Data::copy(const void *c_data, size_t length)
{
    if (NULL == c_data) {
        errno = EINVAL;
        throw std::invalid_argument("c_data");
        return;
    }

    size_t new_length = _data_len + length;
    if (new_length <= _buff_size)
    {
        uint8_t *buff = (uint8_t *)_data_buff;
        memcpy(buff + _data_len, c_data, length);
        _data_len += new_length;
        _check_coefficient();
        return;
    }

    // should realloc
    BOOL realloc_ok = _resize(new_length);
    if (FALSE == realloc_ok) {
        throw std::bad_alloc();
        return;
    }
    else {
        uint8_t *buff = (uint8_t *)_data_buff;
        memcpy(buff + _data_len, c_data, length);
        _data_len += new_length;
        return;
    }
}


void Data::replace(size_t index, const Data &data)
{
    const void *c_data = data.c_data();
    size_t length = data.length();
    replace(index, c_data, length);
    return;
}


void Data::replace(size_t index, const void *c_data, size_t length)
{
    if (NULL == c_data) {
        errno = EINVAL;
        throw std::invalid_argument("c_data");
        return;
    }

    if (index >= _data_len)
    {
        append(c_data, length);
        return;
    }
    else
    {
        if (index + length <= _data_len)
        {
            uint8_t *buff = (uint8_t *)_data_buff;
            memcpy(buff + index, c_data, length);
            _check_coefficient();
        }
        else
        {
            size_t length_part_1 = _data_len - index;
            size_t length_part_2 = length - length_part_1;
            uint8_t *buff = (uint8_t *)_data_buff;
            const uint8_t *from_data = (const uint8_t *)c_data;

            memcpy(buff + index, c_data, length_part_1);
            append(from_data + length_part_1, length_part_2);
        }
        return;
    }
}


void Data::append(const Data &data)
{
    const void *c_data = data.c_data();
    size_t length = data.length();
    append(c_data, length);
    return;
}


void Data::append(const void *c_data, size_t length)
{
    if (NULL == c_data) {
        errno = EINVAL;
        throw std::invalid_argument("c_data");
        return;
    }

    size_t new_length = _data_len + length;
    size_t expected_buff_size = _appropriate_buff_size(new_length);
    BOOL should_check_coeffvient = TRUE;

    if (expected_buff_size > _buff_size)
    {
        BOOL realloc_ok = _resize(expected_buff_size);
        if (FALSE == realloc_ok) {
            throw std::bad_alloc();
            return;
        }
        should_check_coeffvient = FALSE;
    }

    uint8_t *buff = (uint8_t *)_data_buff;
    memcpy(buff + _data_len, c_data, length);
    _data_len = new_length;
    
    if (should_check_coeffvient) {
        _check_coefficient();
    }
    return;
}


#endif  // end of __MODIFYING

