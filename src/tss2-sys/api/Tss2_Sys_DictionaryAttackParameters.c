/* SPDX-License-Identifier: BSD-2 */
/***********************************************************************;
 * Copyright (c) 2015 - 2017, Intel Corporation
 * All rights reserved.
 ***********************************************************************/

#include "tss2_tpm2_types.h"
#include "tss2_mu.h"
#include "sysapi_util.h"

TSS2_RC Tss2_Sys_DictionaryAttackParameters_Prepare(
    TSS2_SYS_CONTEXT *sysContext,
    TPMI_RH_LOCKOUT lockHandle,
    UINT32 newMaxTries,
    UINT32 newRecoveryTime,
    UINT32 lockoutRecovery)
{
    _TSS2_SYS_CONTEXT_BLOB *ctx = syscontext_cast(sysContext);
    TSS2_RC rval;

    if (!ctx)
        return TSS2_SYS_RC_BAD_REFERENCE;

    rval = CommonPreparePrologue(ctx, TPM2_CC_DictionaryAttackParameters);
    if (rval)
        return rval;

    rval = Tss2_MU_UINT32_Marshal(lockHandle, ctx->cmdBuffer,
                                  ctx->maxCmdSize,
                                  &ctx->nextData);
    if (rval)
        return rval;


    rval = Tss2_MU_UINT32_Marshal(newMaxTries, ctx->cmdBuffer,
                                  ctx->maxCmdSize,
                                  &ctx->nextData);
    if (rval)
        return rval;
    rval = Tss2_MU_UINT32_Marshal(newRecoveryTime, ctx->cmdBuffer,
                                  ctx->maxCmdSize,
                                  &ctx->nextData);
    if (rval)
        return rval;
    rval = Tss2_MU_UINT32_Marshal(lockoutRecovery, ctx->cmdBuffer,
                                  ctx->maxCmdSize,
                                  &ctx->nextData);
    if (rval)
        return rval;

    ctx->decryptAllowed = 0;
    ctx->encryptAllowed = 0;
    ctx->authAllowed = 1;

    return CommonPrepareEpilogue(ctx);
}

TSS2_RC Tss2_Sys_DictionaryAttackParameters_Complete (
    TSS2_SYS_CONTEXT *sysContext)
{
    _TSS2_SYS_CONTEXT_BLOB *ctx = syscontext_cast(sysContext);

    if (!ctx)
        return TSS2_SYS_RC_BAD_REFERENCE;

    return CommonComplete(ctx);
}

TSS2_RC Tss2_Sys_DictionaryAttackParameters(
    TSS2_SYS_CONTEXT *sysContext,
    TPMI_RH_LOCKOUT lockHandle,
    TSS2L_SYS_AUTH_COMMAND const *cmdAuthsArray,
    UINT32 newMaxTries,
    UINT32 newRecoveryTime,
    UINT32 lockoutRecovery,
    TSS2L_SYS_AUTH_RESPONSE *rspAuthsArray)
{
    _TSS2_SYS_CONTEXT_BLOB *ctx = syscontext_cast(sysContext);
    TSS2_RC rval;
    rval = Tss2_Sys_DictionaryAttackParameters_Prepare(sysContext, lockHandle,
                                                       newMaxTries, newRecoveryTime,
                                                       lockoutRecovery);
    if (rval)
        return rval;

    rval = CommonOneCall(ctx, cmdAuthsArray, rspAuthsArray);
    if (rval)
        return rval;

    return Tss2_Sys_DictionaryAttackParameters_Complete(sysContext);
}
