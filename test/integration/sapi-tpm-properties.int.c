/* SPDX-License-Identifier: BSD-2 */
/***********************************************************************
 * Copyright (c) 2017-2018, Intel Corporation
 *
 * All rights reserved.
 ***********************************************************************/
#include <stdio.h>
#include <stdlib.h>

#include "tss2_sys.h"

#define LOGMODULE test
#include "util/log.h"
#include "test.h"
#include "sysapi_util.h"

int
test_invoke (TSS2_SYS_CONTEXT *sapi_context)
{
    TSS2_RC rc;
    TPMS_CAPABILITY_DATA capability_data;

    LOG_INFO("Get TPM Properties Test started.");
    rc = Tss2_Sys_GetCapability(sapi_context, 0, TPM2_CAP_TPM_PROPERTIES,
                                TPM2_PT_MANUFACTURER, 1, 0, &capability_data, 0);
    if (rc != TSS2_RC_SUCCESS ||
        capability_data.data.tpmProperties.tpmProperty[0].property != TPM2_PT_MANUFACTURER) {
        LOG_ERROR("Get TPM Properties TPM2_PT_MANUFACTURER FAILED! Response Code : 0x%x", rc);
        exit(1);
    }
    LOG_INFO("TPM Manufacturer 0x%x", capability_data.data.tpmProperties.tpmProperty[0].value);

    rc = Tss2_Sys_GetCapability(sapi_context, 0, TPM2_CAP_TPM_PROPERTIES,
                                TPM2_PT_REVISION, 1, 0, &capability_data, 0);
    if (rc != TSS2_RC_SUCCESS ||
        capability_data.data.tpmProperties.tpmProperty[0].property != TPM2_PT_REVISION) {
        LOG_ERROR("Get TPM Properties TPM2_PT_REVISION FAILED! Response Code : 0x%x", rc);
        exit(1);
    }
    LOG_INFO("TPM revision 0x%X", capability_data.data.tpmProperties.tpmProperty[0].value);

    LOG_INFO("Get TPM Properties Test Passed!");
    return 0;
}
