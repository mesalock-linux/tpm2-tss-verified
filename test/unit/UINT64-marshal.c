/* SPDX-License-Identifier: BSD-2 */
/***********************************************************************
 * Copyright (c) 2017-2018, Intel Corporation
 *
 * All rights reserved.
 ***********************************************************************/
#include <stdlib.h>
#include <stdio.h>

#include <setjmp.h>
#include <cmocka.h>

#include "tss2_mu.h"
#include "util/tss2_endian.h"

/*
 * Test case for successful UINT64 marshaling with NULL offset.
 */
void
UINT64_marshal_success (void **state)
{
    UINT64   src = 0xdeadbeefdeadbeef, tmp;
    uint8_t buffer [8] = { 0 };
    size_t  buffer_size = sizeof (buffer);
    TSS2_RC rc;

    rc = Tss2_MU_UINT64_Marshal (src, buffer, buffer_size, NULL);
    tmp = HOST_TO_BE_64 (src);

    assert_int_equal (rc, TSS2_RC_SUCCESS);
    assert_memory_equal (&tmp, &buffer [0], sizeof (tmp));
}
/*
 * Test case for successful UINT64 marshaling with offset.
 */
void
UINT64_marshal_success_offset (void **state)
{
    UINT64 src = 0xdeadbeefdeadbeef, tmp = 0;
    uint8_t buffer [9] = { 0 };
    size_t  buffer_size = sizeof (buffer);
    size_t  offset = 1;
    TSS2_RC rc;

    rc = Tss2_MU_UINT64_Marshal (src, buffer, buffer_size, &offset);
    tmp = HOST_TO_BE_64 (src);

    assert_int_equal (rc, TSS2_RC_SUCCESS);
    assert_memory_equal (&tmp, &buffer [1], sizeof (tmp));
    assert_int_equal (offset, sizeof (buffer));
}
/*
 * Test case passing NULL buffer and non-NULL offset. Test to be sure offset
 * is updated to the size of the src parameter.
 */
void
UINT64_marshal_buffer_null_with_offset (void **state)
{
    UINT64 src = 0xdeadbeefdeadbeef;
    size_t offset = 100;
    TSS2_RC rc;

    rc = Tss2_MU_UINT64_Marshal (src, NULL, 2, &offset);

    assert_int_equal (rc, TSS2_RC_SUCCESS);
    assert_int_equal (offset, 100 + sizeof (src));
}
/*
 * Test case passing NULL buffer and NULL offset.
 */
void
UINT64_marshal_buffer_null_offset_null (void **state)
{
    UINT64 src = 0xdeadbeefdeadbeef;
    TSS2_RC rc;

    rc = Tss2_MU_UINT64_Marshal (src, NULL, sizeof (src), NULL);

    assert_int_equal (rc, TSS2_MU_RC_BAD_REFERENCE);
}
/*
 * Test failing case where buffer_size - offset (size of available space
 * in buffer) is less than sizeof (UINT64). Also check offset is unchanged.
 */
void
UINT64_marshal_buffer_size_lt_data (void **state)
{
    UINT64   src = 0xdeadbeefdeadbeef;
    uint8_t buffer [8] = { 0 };
    size_t  offset = 2;
    TSS2_RC rc;

    rc = Tss2_MU_UINT64_Marshal (src, buffer, sizeof (src), &offset);

    assert_int_equal (rc, TSS2_MU_RC_INSUFFICIENT_BUFFER);
    assert_int_equal (offset, 2);
}
/*
 * Test failing case where buffer_size is less than the offset value.
 * This should return INSUFFICIENT_BUFFER and the offset should be unchanged.
 */
void
UINT64_marshal_buffer_size_lt_offset (void **state)
{
    UINT64   src = 0xdeadbeefdeadbeef;
    uint8_t buffer [8] = { 0 };
    size_t  buffer_size = sizeof (buffer);
    size_t  offset = sizeof (buffer) + 1;
    TSS2_RC rc;

    rc = Tss2_MU_UINT64_Marshal (src, buffer, buffer_size, &offset);

    assert_int_equal (rc, TSS2_MU_RC_INSUFFICIENT_BUFFER);
    assert_int_equal (offset, sizeof (buffer) + 1);
}
/*
 * Test case for successful UINT64 unmarshaling.
 */
void
UINT64_unmarshal_success (void **state)
{
    uint8_t buffer [8] = { 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef };
    uint8_t buffer_size = sizeof (buffer);
    UINT64   dest = 0, tmp = 0;
    TSS2_RC rc;

    rc = Tss2_MU_UINT64_Unmarshal (buffer, buffer_size, NULL, &dest);
    tmp = HOST_TO_BE_64 (dest);

    assert_int_equal (rc, TSS2_RC_SUCCESS);
    assert_memory_equal (&tmp, buffer, sizeof (tmp));
}
/*
 * Test case for successful UINT64 unmarshaling with offset.
 */
void
UINT64_unmarshal_success_offset (void **state)
{
    UINT64   dest = 0, tmp = 0;
    uint8_t buffer [9] = { 0xff, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef };
    size_t  buffer_size = sizeof (buffer);
    size_t  offset = 1;
    TSS2_RC rc;

    rc = Tss2_MU_UINT64_Unmarshal (buffer, buffer_size, &offset, &dest);
    tmp = HOST_TO_BE_64 (dest);

    assert_int_equal (rc, TSS2_RC_SUCCESS);
    assert_memory_equal (&tmp, &buffer [1], sizeof (tmp));
    assert_int_equal (offset, 9);
}
/*
 * Test case ensures a NULL buffer parameter produces a BAD_REFERENCE RC.
 */
void
UINT64_unmarshal_buffer_null (void **state)
{
    TSS2_RC rc;

    rc = Tss2_MU_UINT64_Unmarshal (NULL, 1, NULL, NULL);

    assert_int_equal (rc, TSS2_MU_RC_BAD_REFERENCE);
}
/*
 * Test case ensures a NULL dest and offset parameters produce an
 * INSUFFICIENT_BUFFER RC.
 */
void
UINT64_unmarshal_dest_null (void **state)
{
    uint8_t buffer [1];
    TSS2_RC rc;

    rc = Tss2_MU_UINT64_Unmarshal (buffer, sizeof (buffer), NULL, NULL);

    assert_int_equal (rc, TSS2_MU_RC_BAD_REFERENCE);
}
/*
 * Test case ensures that INSUFFICIENT_BUFFER is returned when buffer_size
 * is less than the provided offset.
 */
void
UINT64_unmarshal_buffer_size_lt_offset (void **state)
{
    UINT64   dest = 0;
    uint8_t buffer [1];
    size_t  offset = sizeof (buffer) + 1;
    TSS2_RC rc;

    rc = Tss2_MU_UINT64_Unmarshal (buffer, sizeof (buffer), &offset, &dest);

    assert_int_equal (rc, TSS2_MU_RC_INSUFFICIENT_BUFFER);
    assert_int_equal (offset, sizeof (buffer) + 1);
    assert_int_equal (dest, 0);
}
/*
 * Test case ensures that INSUFFICIENT_BUFFER is returned when buffer_size -
 * local_offset is less than dest (the destination type).
 */
void
UINT64_unmarshal_buffer_size_lt_dest (void **state)
{
    UINT64   dest = 0;
    uint8_t buffer [3];
    size_t  offset = sizeof (buffer);
    TSS2_RC rc;

    rc = Tss2_MU_UINT64_Unmarshal (buffer, sizeof (buffer), &offset, &dest);

    assert_int_equal (rc, TSS2_MU_RC_INSUFFICIENT_BUFFER);
    assert_int_equal (offset, sizeof (buffer));
    assert_int_equal (dest, 0);
}
int
main (void)
{
    const struct CMUnitTest tests [] = {
        cmocka_unit_test (UINT64_marshal_success),
        cmocka_unit_test (UINT64_marshal_success_offset),
        cmocka_unit_test (UINT64_marshal_buffer_null_with_offset),
        cmocka_unit_test (UINT64_marshal_buffer_null_offset_null),
        cmocka_unit_test (UINT64_marshal_buffer_size_lt_data),
        cmocka_unit_test (UINT64_marshal_buffer_size_lt_offset),
        cmocka_unit_test (UINT64_unmarshal_success),
        cmocka_unit_test (UINT64_unmarshal_success_offset),
        cmocka_unit_test (UINT64_unmarshal_buffer_null),
        cmocka_unit_test (UINT64_unmarshal_dest_null),
        cmocka_unit_test (UINT64_unmarshal_buffer_size_lt_offset),
        cmocka_unit_test (UINT64_unmarshal_buffer_size_lt_dest),
    };
    return cmocka_run_group_tests (tests, NULL, NULL);
}
