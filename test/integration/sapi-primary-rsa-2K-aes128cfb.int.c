/* SPDX-License-Identifier: BSD-2 */
/***********************************************************************
 * Copyright (c) 2017-2018, Intel Corporation
 *
 * All rights reserved.
 ***********************************************************************/
#include <inttypes.h>

#define LOGMODULE test
#include "util/log.h"
#include "sapi-util.h"
#include "test.h"

int
test_invoke (TSS2_SYS_CONTEXT *sapi_context)
{
    TSS2_RC rc;
    TPM2_HANDLE handle;

    rc = create_primary_rsa_2048_aes_128_cfb (sapi_context, &handle);
    if (rc != TSS2_RC_SUCCESS) {
        LOG_ERROR("CreatePrimary failed with 0x%"PRIx32, rc);
        return 1;
    }

    rc = Tss2_Sys_FlushContext(sapi_context, handle);
    if (rc != TSS2_RC_SUCCESS) {
        LOG_ERROR("Tss2_Sys_FlushContext failed with 0x%"PRIx32, rc);
        return 99; /* fatal error */
    }

    return 0;
}
