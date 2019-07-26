/* SPDX-License-Identifier: BSD-2 */
/*******************************************************************************
 * Copyright 2017-2018, Fraunhofer SIT sponsored by Infineon Technologies AG
 * All rights reserved.
 *******************************************************************************/

#include <stdlib.h>

#include "tss2_esys.h"

#include "esys_iutil.h"
#include "test-esapi.h"
#define LOGMODULE test
#include "util/log.h"
#include "util/aux_util.h"

/** This test is intended to test the ESAPI command Esys_NV_UndefineSpaceSpecial,
 *  The NV space attributes TPMA_NV_PLATFORMCREATE and TPMA_NV_POLICY_DELETE
 *  have to be set.
 *
 * A policy has to be defined for the command UndefineSpaceSpecial.
 * The special handling whether the auth value is not used in the HMAC
 * response verification will be checked.
 *
 *\b Note: platform authorization needed.
 *
 * Tested ESAPI commands:
 *  - Esys_FlushContext() (M)
 *  - Esys_NV_DefineSpace() (M)
 *  - Esys_NV_UndefineSpaceSpecial() (M)
 *  - Esys_PolicyAuthValue() (M)
 *  - Esys_PolicyCommandCode() (M)
 *  - Esys_PolicyGetDigest() (M)
 *  - Esys_StartAuthSession() (M)
 *
 * @param[in,out] esys_context The ESYS_CONTEXT.
 * @retval EXIT_FAILURE
 * @retval EXIT_SKIP
 * @retval EXIT_SUCCESS
 */
int
test_esys_policy_nv_undefine_special(ESYS_CONTEXT * esys_context)
{
    TSS2_RC r;
    ESYS_TR nvHandle = ESYS_TR_NONE;
    ESYS_TR policySession = ESYS_TR_NONE;
    int failure_return = EXIT_FAILURE;
    /*
     * First the policy value for NV_UndefineSpaceSpecial has to be
     * determined with a policy trial session.
     */
    ESYS_TR sessionTrial = ESYS_TR_NONE;
    TPMT_SYM_DEF symmetricTrial = {.algorithm = TPM2_ALG_AES,
                                   .keyBits = {.aes = 128},
                                   .mode = {.aes = TPM2_ALG_CFB}
    };
    TPM2B_NONCE nonceCallerTrial = {
        .size = 20,
        .buffer = {11, 12, 13, 14, 15, 16, 17, 18, 19, 11,
                   21, 22, 23, 24, 25, 26, 27, 28, 29, 30}
    };

    r = Esys_StartAuthSession(esys_context, ESYS_TR_NONE, ESYS_TR_NONE,
                              ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
                              &nonceCallerTrial,
                              TPM2_SE_TRIAL, &symmetricTrial, TPM2_ALG_SHA1,
                              &sessionTrial);
    goto_if_error(r, "Error: During initialization of policy trial session", error);

    r = Esys_PolicyAuthValue(esys_context,
                             sessionTrial,
                             ESYS_TR_NONE,
                             ESYS_TR_NONE,
                             ESYS_TR_NONE
                             );
    goto_if_error(r, "Error: PolicyAuthValue", error);

    r = Esys_PolicyCommandCode(esys_context,
                               sessionTrial,
                               ESYS_TR_NONE,
                               ESYS_TR_NONE,
                               ESYS_TR_NONE,
                               TPM2_CC_NV_UndefineSpaceSpecial
                               );
    goto_if_error(r, "Error: PolicyCommandCode", error);

    TPM2B_DIGEST *policyDigestTrial;
    r = Esys_PolicyGetDigest(esys_context,
                             sessionTrial,
                             ESYS_TR_NONE,
                             ESYS_TR_NONE,
                             ESYS_TR_NONE,
                             &policyDigestTrial
                             );
    goto_if_error(r, "Error: PolicyGetDigest", error);

    TPM2B_AUTH auth = {.size = 20,
                       .buffer={10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
                                20, 21, 22, 23, 24, 25, 26, 27, 28, 29}};

    TPM2B_NV_PUBLIC publicInfo = {
        .size = 0,
        .nvPublic = {
            .nvIndex =TPM2_NV_INDEX_FIRST,
            .nameAlg = TPM2_ALG_SHA1,
            .attributes = (
                TPMA_NV_PLATFORMCREATE |
                TPMA_NV_PPWRITE |
                TPMA_NV_AUTHWRITE |
                TPMA_NV_WRITE_STCLEAR |
                TPMA_NV_READ_STCLEAR |
                TPMA_NV_AUTHREAD |
                TPMA_NV_PPREAD |
                TPMA_NV_POLICY_DELETE  /**< Undefine will only possible with policy */
                ),
            .authPolicy = *policyDigestTrial,
            .dataSize = 32,
        }
    };

    r = Esys_NV_DefineSpace(esys_context,
                            ESYS_TR_RH_PLATFORM,
                            ESYS_TR_PASSWORD,
                            ESYS_TR_NONE,
                            ESYS_TR_NONE,
                            &auth,
                            &publicInfo,
                            &nvHandle);

    if ((r & ~TPM2_RC_N_MASK) == TPM2_RC_BAD_AUTH) {
        /* Platform authorization not possible test will be skipped */
        LOG_WARNING("Platform authorization not possible.");
        failure_return = EXIT_SKIP;
        goto error;
    }

    goto_if_error(r, "Error esys define nv space", error);

    TPMT_SYM_DEF policySymmetric = {.algorithm = TPM2_ALG_AES,
                                    .keyBits = {.aes = 128},
                                    .mode = {.aes = TPM2_ALG_CFB}
    };
    TPM2B_NONCE policyNonceCaller = {
        .size = 20,
        .buffer = {11, 12, 13, 14, 15, 16, 17, 18, 19, 11,
                   21, 22, 23, 24, 25, 26, 27, 28, 29, 30}
    };

    r = Esys_StartAuthSession(esys_context, ESYS_TR_NONE, ESYS_TR_NONE,
                              ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
                              &policyNonceCaller,
                              TPM2_SE_POLICY, &policySymmetric, TPM2_ALG_SHA1,
                              &policySession);
    goto_if_error(r, "Error: During initialization of policy trial session", error);

    r = Esys_PolicyAuthValue(esys_context,
                             policySession,
                             ESYS_TR_NONE,
                             ESYS_TR_NONE,
                             ESYS_TR_NONE
                             );
    goto_if_error(r, "Error: PolicyAuthValue", error);

    r = Esys_PolicyCommandCode(esys_context,
                               policySession,
                               ESYS_TR_NONE,
                               ESYS_TR_NONE,
                               ESYS_TR_NONE,
                               TPM2_CC_NV_UndefineSpaceSpecial
                               );
    goto_if_error(r, "Error: PolicyCommandCode", error);

    r = Esys_NV_UndefineSpaceSpecial(esys_context,
                                     nvHandle,
                                     ESYS_TR_RH_PLATFORM,
                                     policySession,
                                     ESYS_TR_PASSWORD,
                                     ESYS_TR_NONE
                                     );

    if ((r & ~TPM2_RC_N_MASK) == TPM2_RC_BAD_AUTH) {
        /* Platform authorization not possible test will be skipped */
        LOG_WARNING("Platform authorization not possible.");
        failure_return = EXIT_SKIP;
        goto error;
    }

    goto_if_error(r, "Error: NV_UndefineSpace", error);

    r = Esys_FlushContext(esys_context, sessionTrial);
    goto_if_error(r, "Flushing context", error);

    r = Esys_FlushContext(esys_context, policySession);
    goto_if_error(r, "Flushing context", error);

    return EXIT_SUCCESS;

 error:

    if (sessionTrial != ESYS_TR_NONE) {
        if (Esys_FlushContext(esys_context, sessionTrial) != TSS2_RC_SUCCESS) {
            LOG_ERROR("Cleanup policySession failed.");
        }
    }

    if (policySession != ESYS_TR_NONE) {
        if (Esys_FlushContext(esys_context, policySession) != TSS2_RC_SUCCESS) {
            LOG_ERROR("Cleanup policySession failed.");
        }
    }

    return failure_return;
}

int
test_invoke_esapi(ESYS_CONTEXT * esys_context) {
    return test_esys_policy_nv_undefine_special(esys_context);
}
