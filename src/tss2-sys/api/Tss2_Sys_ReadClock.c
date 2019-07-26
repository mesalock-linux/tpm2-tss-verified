/* SPDX-License-Identifier: BSD-2 */
/***********************************************************************;
 * Copyright (c) 2015 - 2017, Intel Corporation
 * All rights reserved.
 ***********************************************************************/

#include "tss2_tpm2_types.h"
#include "tss2_mu.h"
#include "sysapi_util.h"

TSS2_RC Tss2_Sys_ReadClock_Prepare(
    TSS2_SYS_CONTEXT *sysContext)
{
    _TSS2_SYS_CONTEXT_BLOB *ctx = syscontext_cast(sysContext);
    TSS2_RC rval;

    if (!ctx)
        return TSS2_SYS_RC_BAD_REFERENCE;

    rval = CommonPreparePrologue(ctx, TPM2_CC_ReadClock);
    if (rval)
        return rval;

    ctx->decryptAllowed = 0;
    ctx->encryptAllowed = 0;
    ctx->authAllowed = 0;

    return CommonPrepareEpilogue(ctx);
}

TSS2_RC Tss2_Sys_ReadClock_Complete(
    TSS2_SYS_CONTEXT *sysContext,
    TPMS_TIME_INFO *currentTime)
{
    _TSS2_SYS_CONTEXT_BLOB *ctx = syscontext_cast(sysContext);
    TSS2_RC rval;

    if (!ctx)
        return TSS2_SYS_RC_BAD_REFERENCE;

    rval = CommonComplete(ctx);
    if (rval)
        return rval;

    return Tss2_MU_TPMS_TIME_INFO_Unmarshal(ctx->cmdBuffer,
                                            ctx->maxCmdSize,
                                            &ctx->nextData,
                                            currentTime);
}

TSS2_RC Tss2_Sys_ReadClock(
    TSS2_SYS_CONTEXT *sysContext,
    TPMS_TIME_INFO *currentTime)
{
    _TSS2_SYS_CONTEXT_BLOB *ctx = syscontext_cast(sysContext);
    TSS2_RC rval;

    rval = Tss2_Sys_ReadClock_Prepare(sysContext);
    if (rval)
        return rval;

    rval = CommonOneCall(ctx, 0, 0);
    if (rval)
        return rval;

    return Tss2_Sys_ReadClock_Complete(sysContext, currentTime);
}
