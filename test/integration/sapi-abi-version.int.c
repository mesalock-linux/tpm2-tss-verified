/* SPDX-License-Identifier: BSD-2 */
/***********************************************************************
 * Copyright (c) 2017-2018, Intel Corporation
 *
 * All rights reserved.
 ***********************************************************************/
#include <stdlib.h>

#include "tss2_sys.h"

#include <stdio.h>
#define LOGMODULE test
#include "util/log.h"
#include "test.h"

#define TSSWG_INTEROP 1
#define TSS_SAPI_FIRST_FAMILY 2
#define TSS_SAPI_FIRST_LEVEL 1
#define TSS_SAPI_FIRST_VERSION 108

/**
 */
int
test_invoke (TSS2_SYS_CONTEXT *sapi_context)
{
    TSS2_RC rc;
    UINT32 contextSize;
    TSS2_TCTI_CONTEXT *tcti_context = NULL;
    TSS2_ABI_VERSION tstAbiVersion = { TSSWG_INTEROP, TSS_SAPI_FIRST_FAMILY, TSS_SAPI_FIRST_LEVEL, TSS_SAPI_FIRST_VERSION };

    LOG_INFO( "ABI NEGOTIATION TESTS" );

    /* Get the size needed for sapi context structure. */
    contextSize = Tss2_Sys_GetContextSize( 0 );

    rc = Tss2_Sys_GetTctiContext (sapi_context, &tcti_context);
    if( rc != TSS2_RC_SUCCESS )
    {
        LOG_ERROR("ABIVersion FAILED! Response Code : %x", rc);
        exit(1);
    }

    /* Initialize the system context structure. */
    tstAbiVersion.tssCreator = 0xF0000000;
    rc = Tss2_Sys_Initialize( sapi_context, contextSize, tcti_context, &tstAbiVersion );
    if( rc != TSS2_SYS_RC_ABI_MISMATCH )
    {
        LOG_ERROR("ABIVersion FAILED! Response Code : %x", rc);
        exit(1);
    }

    tstAbiVersion.tssCreator = TSSWG_INTEROP;
    tstAbiVersion.tssFamily = 0xF0000000;
    rc = Tss2_Sys_Initialize( sapi_context, contextSize, tcti_context, &tstAbiVersion );
    if( rc != TSS2_SYS_RC_ABI_MISMATCH )
    {
        LOG_ERROR("ABIVersion FAILED! Response Code : %x", rc);
        exit(1);
    }

    tstAbiVersion.tssFamily = TSS_SAPI_FIRST_FAMILY;
    tstAbiVersion.tssLevel = 0xF0000000;
    rc = Tss2_Sys_Initialize( sapi_context, contextSize, tcti_context, &tstAbiVersion );
    if( rc != TSS2_SYS_RC_ABI_MISMATCH )
    {
        LOG_ERROR("ABIVersion FAILED! Response Code : %x", rc);
        exit(1);
    }

    tstAbiVersion.tssLevel = TSS_SAPI_FIRST_LEVEL;
    tstAbiVersion.tssVersion = 0xF0000000;
    rc = Tss2_Sys_Initialize( sapi_context, contextSize, tcti_context, &tstAbiVersion );
    if( rc != TSS2_SYS_RC_ABI_MISMATCH )
    {
        LOG_ERROR("ABIVersion FAILED! Response Code : %x", rc);
    }


    LOG_INFO("ABIVersion Test Passed!");
    return 0;
}
