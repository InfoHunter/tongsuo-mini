/*
 * Copyright 2023 The Tongsuo Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://github.com/Tongsuo-Project/tongsuo-mini/blob/main/LICENSE
 */

#include "internal/log.h"
#include <tongsuo/oscore_cbor.h>
#include <tongsuo/mem.h>
#include <stdlib.h>
#include <string.h>

static inline void util_write_byte(uint8_t **buffer, size_t *buf_size, uint8_t value)
{
    assert(*buf_size >= 1);
    (*buf_size)--;
    **buffer = value;
    (*buffer)++;
}

size_t tsm_oscore_cbor_put_nil(uint8_t **buffer, size_t *buf_size)
{
    util_write_byte(buffer, buf_size, 0xF6);
    return 1;
}

size_t tsm_oscore_cbor_put_true(uint8_t **buffer, size_t *buf_size)
{
    util_write_byte(buffer, buf_size, 0xF5);
    return 1;
}

size_t tsm_oscore_cbor_put_false(uint8_t **buffer, size_t *buf_size)
{
    util_write_byte(buffer, buf_size, 0xF4);
    return 1;
}

size_t tsm_oscore_cbor_put_text(uint8_t **buffer, size_t *buf_size, const char *text,
                                size_t text_len)
{
    uint8_t *pt = *buffer;
    size_t nb = tsm_oscore_cbor_put_unsigned(buffer, buf_size, text_len);
    assert(*buf_size >= text_len);
    (*buf_size) -= text_len;
    *pt = (*pt | 0x60);
    memcpy(*buffer, text, text_len);
    (*buffer) += text_len;
    return nb + text_len;
}

size_t tsm_oscore_cbor_put_array(uint8_t **buffer, size_t *buf_size, size_t elements)
{
    uint8_t *pt = *buffer;
    size_t nb = tsm_oscore_cbor_put_unsigned(buffer, buf_size, elements);
    *pt = (*pt | 0x80);
    return nb;
}

size_t tsm_oscore_cbor_put_bytes(uint8_t **buffer, size_t *buf_size, const uint8_t *bytes,
                                 size_t bytes_len)
{
    uint8_t *pt = *buffer;
    size_t nb = tsm_oscore_cbor_put_unsigned(buffer, buf_size, bytes_len);
    assert(*buf_size >= bytes_len);
    (*buf_size) -= bytes_len;
    *pt = (*pt | 0x40);
    memcpy(*buffer, bytes, bytes_len);
    (*buffer) += bytes_len;
    return nb + bytes_len;
}

size_t tsm_oscore_cbor_put_map(uint8_t **buffer, size_t *buf_size, size_t elements)
{
    uint8_t *pt = *buffer;
    size_t nb = tsm_oscore_cbor_put_unsigned(buffer, buf_size, elements);
    *pt = (*pt | 0xa0);
    return nb;
}

size_t tsm_oscore_cbor_put_number(uint8_t **buffer, size_t *buf_size, int64_t value)
{
    if (value < 0)
        return tsm_oscore_cbor_put_negative(buffer, buf_size, -value);
    else
        return tsm_oscore_cbor_put_unsigned(buffer, buf_size, value);
}

size_t tsm_oscore_cbor_put_simple_value(uint8_t **buffer, size_t *buf_size, uint8_t value)
{
    uint8_t *pt = *buffer;
    size_t nb = tsm_oscore_cbor_put_unsigned(buffer, buf_size, value);
    *pt = (*pt | 0xe0);
    return nb;
}

size_t tsm_oscore_cbor_put_tag(uint8_t **buffer, size_t *buf_size, uint64_t value)
{
    uint8_t *pt = *buffer;
    size_t nb = tsm_oscore_cbor_put_unsigned(buffer, buf_size, value);
    *pt = (*pt | 0xc0);
    return nb;
}

size_t tsm_oscore_cbor_put_negative(uint8_t **buffer, size_t *buf_size, int64_t value)
{
    value--;
    uint8_t *pt = *buffer;
    size_t nb = tsm_oscore_cbor_put_unsigned(buffer, buf_size, value);
    *pt = (*pt | 0x20);
    return nb;
}

static void put_b_f(uint8_t **buffer, uint64_t value, uint8_t nr)
{
    uint8_t *pt = *buffer - 1;
    uint64_t vv = value;
    for (int q = nr; q > -1; q--) {
        (*pt--) = (uint8_t)(vv & 0xff);
        vv = (vv >> 8);
    }
}

size_t tsm_oscore_cbor_put_unsigned(uint8_t **buffer, size_t *buf_size, uint64_t value)
{
    if (value < 0x18) { /* small value half a byte */
        assert(*buf_size >= 1);
        (*buf_size)--;
        (**buffer) = (uint8_t)value;
        (*buffer)++;
        return 1;
    } else if ((value > 0x17) && (value < 0x100)) {
        /* one byte uint8_t  */
        assert(*buf_size >= 2);
        (*buf_size) -= 2;
        (**buffer) = (0x18);
        *buffer = (*buffer) + 2;
        put_b_f(buffer, value, 0);
        return 2;
    } else if ((value > 0xff) && (value < 0x10000)) {
        /* 2 bytes uint16_t     */
        assert(*buf_size >= 3);
        (*buf_size) -= 3;
        (**buffer) = (0x19);
        *buffer = (*buffer) + 3;
        put_b_f(buffer, value, 1);
        return 3;
    } else if ((value > 0xffff) && (value < 0x100000000)) {
        /* 4 bytes uint32_t   */
        assert(*buf_size >= 5);
        (*buf_size) -= 5;
        (**buffer) = (0x1a);
        *buffer = (*buffer) + 5;
        put_b_f(buffer, value, 3);
        return 5;
    } else { /*if(value > 0xffffffff)*/
        /* 8 bytes uint64_t  */
        assert(*buf_size >= 9);
        (*buf_size) -= 9;
        (**buffer) = (0x1b);
        *buffer = (*buffer) + 9;
        put_b_f(buffer, value, 7);
        return 9;
    }
}

static inline uint8_t get_byte(const uint8_t **buffer, size_t *buf_len)
{
#if NDEBUG
    (void)buf_len;
#endif /* NDEBUG */
    assert((*buf_len) > 0);
    return (*buffer)[0];
}

static inline uint8_t get_byte_inc(const uint8_t **buffer, size_t *buf_len)
{
    assert((*buf_len) > 0);
    (*buf_len)--;
    return ((*buffer)++)[0];
}

uint8_t tsm_oscore_cbor_get_next_element(const uint8_t **buffer, size_t *buf_len)
{
    uint8_t element = get_byte(buffer, buf_len);
    return element >> 5;
}

/* tsm_oscore_cbor_get_element_size returns
 *   - size of byte strings of character strings
 *   - size of array
 *   - size of map
 *   - value of unsigned integer
 */

size_t tsm_oscore_cbor_get_element_size(const uint8_t **buffer, size_t *buf_len)
{
    uint8_t control = get_byte(buffer, buf_len) & 0x1f;
    size_t size = get_byte_inc(buffer, buf_len);

    if (control < 0x18) {
        size = (uint64_t)control;
    } else {
        control = control & 0x3;
        int num = 1 << control;
        size = 0;
        size_t getal;
        for (int i = 0; i < num; i++) {
            getal = get_byte_inc(buffer, buf_len);
            size = (size << 8) + getal;
        }
    }
    return size;
}

uint8_t tsm_oscore_cbor_elem_contained(const uint8_t *data, size_t *buf_len, uint8_t *end)
{
    const uint8_t *buf = data;
    const uint8_t *last = data + tsm_oscore_cbor_get_element_size(&buf, buf_len);
    if (last > end) {
        LOGE("tsm_oscore_cbor_elem_contained returns 1 \n");
        return 1;
    } else
        return 0;
}

int64_t tsm_oscore_cbor_get_negative_integer(const uint8_t **buffer, size_t *buf_len)
{
    return -(int64_t)(tsm_oscore_cbor_get_element_size(buffer, buf_len) + 1);
}

uint64_t tsm_oscore_cbor_get_unsigned_integer(const uint8_t **buffer, size_t *buf_len)
{
    return tsm_oscore_cbor_get_element_size(buffer, buf_len);
}

int tsm_oscore_cbor_get_number(const uint8_t **data, size_t *buf_len, int64_t *value)
{
    uint8_t elem = tsm_oscore_cbor_get_next_element(data, buf_len);
    if (elem == CBOR_UNSIGNED_INTEGER) {
        *value = tsm_oscore_cbor_get_unsigned_integer(data, buf_len);
        return TSM_OK;
    } else if (elem == CBOR_NEGATIVE_INTEGER) {
        *value = tsm_oscore_cbor_get_negative_integer(data, buf_len);
        return TSM_OK;
    } else
        return TSM_FAILED;
}

int tsm_oscore_cbor_get_simple_value(const uint8_t **data, size_t *buf_len, uint8_t *value)
{
    uint8_t elem = tsm_oscore_cbor_get_next_element(data, buf_len);
    if (elem == CBOR_SIMPLE_VALUE) {
        *value = get_byte_inc(data, buf_len) & 0x1f;
        return TSM_OK;
    } else
        return TSM_FAILED;
}

void tsm_oscore_cbor_get_string(const uint8_t **buffer, size_t *buf_len, char *str, size_t size)
{
    (void)buf_len;
    for (size_t i = 0; i < size; i++) {
        *str++ = (char)get_byte_inc(buffer, buf_len);
    }
}

void tsm_oscore_cbor_get_array(const uint8_t **buffer, size_t *buf_len, uint8_t *arr, size_t size)
{
    (void)buf_len;
    for (size_t i = 0; i < size; i++) {
        *arr++ = get_byte_inc(buffer, buf_len);
    }
}

/* tsm_oscore_cbor_get_string_array
 * fills the the size and the array from the cbor element
 */
int tsm_oscore_cbor_get_string_array(const uint8_t **data, size_t *buf_len, uint8_t **result,
                                     size_t *len)
{
    uint8_t elem = tsm_oscore_cbor_get_next_element(data, buf_len);
    *len = tsm_oscore_cbor_get_element_size(data, buf_len);
    *result = NULL;
    void *rs = tsm_alloc(*len);
    *result = (uint8_t *)rs;
    if (elem == CBOR_TEXT_STRING) {
        tsm_oscore_cbor_get_string(data, buf_len, (char *)*result, *len);
        return TSM_OK;
    } else if (elem == CBOR_BYTE_STRING) {
        tsm_oscore_cbor_get_array(data, buf_len, *result, *len);
        return TSM_OK; /* all is well */
    } else {
        tsm_free(*result);
        *result = NULL;
        return TSM_FAILED; /* failure */
    }
}

/* oscore_cbor_skip value
 *  returns number of CBOR bytes
 */
static size_t oscore_cbor_skip_value(const uint8_t **data, size_t *buf_len)
{
    uint8_t elem = tsm_oscore_cbor_get_next_element(data, buf_len);
    uint8_t control = get_byte(data, buf_len) & 0x1f;
    size_t nb = 0; /* number of elements in array or map */
    size_t num = 0; /* number of bytes of length or number */
    size_t size = 0; /* size of value to be skipped */
    if (control < 0x18) {
        num = 1;
    } else {
        control = control & 0x3;
        num = 1 + (1 << control);
    }
    switch (elem) {
    case CBOR_UNSIGNED_INTEGER:
    case CBOR_NEGATIVE_INTEGER:
        assert((*buf_len) >= num);
        *buf_len -= num;
        *data = *data + num;
        size = num;
        break;
    case CBOR_BYTE_STRING:
    case CBOR_TEXT_STRING:
        size = num;
        size += tsm_oscore_cbor_get_element_size(data, buf_len);
        assert((*buf_len) >= (size - num));
        *buf_len -= (size - num);
        (*data) = (*data) + size - num;
        break;
    case CBOR_ARRAY:
        nb = tsm_oscore_cbor_get_element_size(data, buf_len);
        size = num;
        for (uint16_t qq = 0; qq < nb; qq++)
            size += oscore_cbor_skip_value(data, buf_len);
        break;
    case CBOR_MAP:
        nb = tsm_oscore_cbor_get_element_size(data, buf_len);
        size = num;
        for (uint16_t qq = 0; qq < nb; qq++) {
            size += oscore_cbor_skip_value(data, buf_len);
            size += oscore_cbor_skip_value(data, buf_len);
        }
        break;
    case CBOR_TAG:
        assert((*buf_len) >= 1);
        *buf_len -= 1;
        (*data)++;
        size = 1;
        break;
    default:
        return 0;
        break;
    } /* switch */
    return size;
}

/* oscore_cbor_strip value
 * strips the value of the cbor element into result
 *  and returns size
 */
int tsm_oscore_cbor_strip_value(const uint8_t **data, size_t *buf_len, uint8_t **result,
                                size_t *len)
{
    const uint8_t *st_data = *data;
    size_t size = oscore_cbor_skip_value(data, buf_len);

    *result = tsm_alloc(size);
    if (*result == NULL)
        return TSM_ERR_MALLOC_FAILED;

    for (uint16_t qq = 0; qq < size; qq++)
        (*result)[qq] = st_data[qq];

    *len = size;

    return TSM_OK;
}