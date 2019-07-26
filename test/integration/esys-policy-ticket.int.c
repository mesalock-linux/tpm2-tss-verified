/* SPDX-License-Identifier: BSD-2 */
/*******************************************************************************
 * Copyright 2017-2018, Fraunhofer SIT sponsored by Infineon Technologies AG
 * All rights reserved.
 *******************************************************************************/

#include <stdlib.h>

#include "tss2_esys.h"
#include "tss2_mu.h"

#include "esys_iutil.h"
#include "test-esapi.h"
#define LOGMODULE test
#include "util/log.h"
#include "util/aux_util.h"

/** This test is intended to test the ESAPI policy commands related to
 *  signed authorization actions.
 *
 * Esys_PolicySigned, Esys_PolicyTicket, and Esys_PolicySecret.
 *
 * Tested ESAPI commands:
 *  - Esys_CreatePrimary() (M)
 *  - Esys_FlushContext() (M)
 *  - Esys_HashSequenceStart() (M)
 *  - Esys_PolicySecret() (M)
 *  - Esys_PolicySigned() (M)
 *  - Esys_PolicyTicket() (O)
 *  - Esys_ReadPublic() (M)
 *  - Esys_SequenceComplete() (M)
 *  - Esys_SequenceUpdate() (M)
 *  - Esys_Sign() (M)
 *  - Esys_StartAuthSession() (M)
 *
 * @param[in,out] esys_context The ESYS_CONTEXT.
 * @retval EXIT_FAILURE
 * @retval EXIT_SUCCESS
 */

int
test_esys_policy_ticket(ESYS_CONTEXT * esys_context)
{
    TSS2_RC r;
    ESYS_TR primaryHandle = ESYS_TR_NONE;
    ESYS_TR session = ESYS_TR_NONE;
    ESYS_TR sessionTrial = ESYS_TR_NONE;
    int failure_return = EXIT_FAILURE;

    /*
     * 1. Create Primary. This primary will be used as signing key.
     */

    TPM2B_AUTH authValuePrimary = {
        .size = 5,
        .buffer = {1, 2, 3, 4, 5}
    };

    TPM2B_SENSITIVE_CREATE inSensitivePrimary = {
        .size = 4,
        .sensitive = {
            .userAuth = authValuePrimary,
            .data = {
                 .size = 0,
                 .buffer = {0},
             },
        },
    };

    TPM2B_PUBLIC inPublic = {
        .size = 0,
        .publicArea = {
            .type = TPM2_ALG_RSA,
            .nameAlg = TPM2_ALG_SHA1,
            .objectAttributes = (TPMA_OBJECT_USERWITHAUTH |
                                 TPMA_OBJECT_SIGN_ENCRYPT  |
                                 TPMA_OBJECT_FIXEDTPM |
                                 TPMA_OBJECT_FIXEDPARENT |
                                 TPMA_OBJECT_SENSITIVEDATAORIGIN),
            .authPolicy = {
                 .size = 0,
             },
            .parameters.rsaDetail = {
                 .symmetric = {
                     .algorithm = TPM2_ALG_NULL,
                     .keyBits.aes = 128,
                     .mode.aes = TPM2_ALG_CFB},
                 .scheme = {
                      .scheme = TPM2_ALG_RSAPSS,
                      .details = {
                          .rsapss = { .hashAlg = TPM2_ALG_SHA1 }
                      }
                  },
                 .keyBits = 2048,
                 .exponent = 0,
             },
            .unique.rsa = {
                 .size = 0,
                 .buffer = {},
             },
        },
    };
    LOG_INFO("\nRSA key will be created.");

    TPM2B_DATA outsideInfo = {
        .size = 0,
        .buffer = {},
    };

    TPML_PCR_SELECTION creationPCR = {
        .count = 0,
    };

    TPM2B_AUTH authValue = {
        .size = 0,
        .buffer = {}
    };

    r = Esys_TR_SetAuth(esys_context, ESYS_TR_RH_OWNER, &authValue);
    goto_if_error(r, "Error: TR_SetAuth", error);

    TPM2B_PUBLIC *outPublic;
    TPM2B_CREATION_DATA *creationData;
    TPM2B_DIGEST *creationHash;
    TPMT_TK_CREATION *creationTicket;

    r = Esys_CreatePrimary(esys_context, ESYS_TR_RH_OWNER, ESYS_TR_PASSWORD,
                           ESYS_TR_NONE, ESYS_TR_NONE,
                           &inSensitivePrimary, &inPublic,
                           &outsideInfo, &creationPCR, &primaryHandle,
                           &outPublic, &creationData, &creationHash,
                           &creationTicket);
    goto_if_error(r, "Error esys create primary", error);

    TPM2B_NAME *nameKeySign;
    TPM2B_NAME *keyQualifiedName;

    r = Esys_ReadPublic(esys_context,
                        primaryHandle,
                        ESYS_TR_NONE,
                        ESYS_TR_NONE,
                        ESYS_TR_NONE,
                        &outPublic,
                        &nameKeySign,
                        &keyQualifiedName);
    goto_if_error(r, "Error: ReadPublic", error);

    /*
     * 2. A policy session will be created. Based on the signed policy the
     *    ticket policySignedTicket will be created.
     *    With this ticket the function Esys_PolicyTicket will be tested.
     */
    TPM2B_DIGEST *signed_digest;
    INT32 expiration = -(10*365*24*60*60); /* Expiration ten years */

    TPM2B_DIGEST expiration2b;
    TPMT_SYM_DEF symmetric = {.algorithm = TPM2_ALG_AES,
                              .keyBits = {.aes = 128},
                              .mode = {.aes = TPM2_ALG_CFB}
    };
    TPM2B_NONCE nonceCaller = {
        .size = 20,
        .buffer = {11, 12, 13, 14, 15, 16, 17, 18, 19, 11,
                   21, 22, 23, 24, 25, 26, 27, 28, 29, 30}
    };

    size_t offset = 0;

    r = Tss2_MU_INT32_Marshal(expiration, &expiration2b.buffer[0],
                              4, &offset);
    goto_if_error(r, "Marshaling name", error);
    expiration2b.size = offset;

    r = Esys_StartAuthSession(esys_context, ESYS_TR_NONE, ESYS_TR_NONE,
                              ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
                              &nonceCaller,
                              TPM2_SE_POLICY, &symmetric, TPM2_ALG_SHA1,
                              &session);
    goto_if_error(r, "Error: During initialization of policy trial session", error);

    TPM2B_NONCE policyRef = {0};
    TPM2B_NONCE *nonceTPM;
    TPM2B_DIGEST cpHashA = {0};
    TPM2B_TIMEOUT *timeout;
    TPMT_TK_AUTH *policySignedTicket;

    r = Esys_TRSess_GetNonceTPM(esys_context, session, &nonceTPM);
    goto_if_error(r, "Error: During initialization of policy trial session", error);

    /* Compute hash from nonceTPM||expiration */

    TPMI_ALG_HASH hashAlg = TPM2_ALG_SHA1;
    ESYS_TR sequenceHandle_handle;
    TPM2B_AUTH auth = {0};

    r = Esys_HashSequenceStart(esys_context,
                               ESYS_TR_NONE,
                               ESYS_TR_NONE,
                               ESYS_TR_NONE,
                               &auth,
                               hashAlg,
                               &sequenceHandle_handle
                               );
    goto_if_error(r, "Error: HashSequenceStart", error);

    r = Esys_TR_SetAuth(esys_context, sequenceHandle_handle, &auth);
    goto_if_error(r, "Error esys TR_SetAuth ", error);

    r = Esys_SequenceUpdate(esys_context,
                            sequenceHandle_handle,
                            ESYS_TR_PASSWORD,
                            ESYS_TR_NONE,
                            ESYS_TR_NONE,
                            (const TPM2B_MAX_BUFFER *)nonceTPM
                            );
    goto_if_error(r, "Error: SequenceUpdate", error);

    TPMT_TK_HASHCHECK *validation;

    r = Esys_SequenceComplete(esys_context,
                              sequenceHandle_handle,
                              ESYS_TR_PASSWORD,
                              ESYS_TR_NONE,
                              ESYS_TR_NONE,
                              (const TPM2B_MAX_BUFFER *)&expiration2b,
                              TPM2_RH_OWNER,
                              &signed_digest,
                              &validation
                              );
    goto_if_error(r, "Error: SequenceComplete", error);

    TPMT_SIG_SCHEME inScheme = { .scheme = TPM2_ALG_NULL };
    TPMT_TK_HASHCHECK hash_validation = {
        .tag = TPM2_ST_HASHCHECK,
        .hierarchy = TPM2_RH_OWNER,
        .digest = {0}
    };
    TPMT_SIGNATURE *signature;

    /* Policy expiration of ten years will be signed */

    r = Esys_Sign(
        esys_context,
        primaryHandle,
        ESYS_TR_PASSWORD,
        ESYS_TR_NONE,
        ESYS_TR_NONE,
        signed_digest,
        &inScheme,
        &hash_validation,
        &signature);
    goto_if_error(r, "Error: Sign", error);

    r = Esys_PolicySigned(
        esys_context,
        primaryHandle,
        session,
        ESYS_TR_NONE,
        ESYS_TR_NONE,
        ESYS_TR_NONE,
        nonceTPM,
        &cpHashA,
        &policyRef,
        expiration,
        signature,
        &timeout,
        &policySignedTicket);
    goto_if_error(r, "Error: PolicySigned", error);

    r = Esys_FlushContext(esys_context, session);
    goto_if_error(r, "Error: FlushContext", error);

    session = ESYS_TR_NONE;

    r = Esys_StartAuthSession(esys_context, ESYS_TR_NONE, ESYS_TR_NONE,
                              ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
                              &nonceCaller,
                              TPM2_SE_POLICY, &symmetric, TPM2_ALG_SHA1,
                              &session);
    goto_if_error(r, "Error: During initialization of policy session", error);

    r = Esys_PolicyTicket(
        esys_context,
        session,
        ESYS_TR_NONE,
        ESYS_TR_NONE,
        ESYS_TR_NONE,
        timeout,
        &cpHashA,
        &policyRef,
        nameKeySign,
        policySignedTicket);

    if ((r == TPM2_RC_COMMAND_CODE) ||
        (r == (TPM2_RC_COMMAND_CODE | TSS2_RESMGR_RC_LAYER)) ||
        (r == (TPM2_RC_COMMAND_CODE | TSS2_RESMGR_TPM_RC_LAYER))) {
        LOG_WARNING("Command TPM2_PolicyTicket not supported by TPM all other tests PASSED.");
        r = Esys_FlushContext(esys_context, session);
        goto_if_error(r, "Error: FlushContext", error);
    } else {
        goto_if_error(r, "Error: PolicyTicket", error);

        r = Esys_FlushContext(esys_context, session);
        goto_if_error(r, "Error: FlushContext", error);

        session = ESYS_TR_NONE;
    }

    /*
     * 3. A policy tial session will be created. With this trial policy the
     *    function Esys_PolicySecret will be tested.
     */

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

    TPMT_TK_AUTH *policySecretTicket;

    r = Esys_PolicySecret(
        esys_context,
        primaryHandle,
        sessionTrial,
        ESYS_TR_PASSWORD,
        ESYS_TR_NONE,
        ESYS_TR_NONE,
        nonceTPM,
        &cpHashA,
        &policyRef,
        expiration,
        &timeout,
        &policySecretTicket);
    goto_if_error(r, "Error: PolicySecret", error);

    r = Esys_FlushContext(esys_context, sessionTrial);
    goto_if_error(r, "Error: FlushContext", error);

    sessionTrial = ESYS_TR_NONE;

    r = Esys_FlushContext(esys_context, primaryHandle);
    goto_if_error(r, "Error: FlushContext", error);

    return EXIT_SUCCESS;

 error:

    if (session != ESYS_TR_NONE) {
        if (Esys_FlushContext(esys_context, session) != TSS2_RC_SUCCESS) {
            LOG_ERROR("Cleanup session failed.");
        }
    }

    if (sessionTrial != ESYS_TR_NONE) {
        if (Esys_FlushContext(esys_context, sessionTrial) != TSS2_RC_SUCCESS) {
            LOG_ERROR("Cleanup sessionTrial failed.");
        }
    }

    if (primaryHandle != ESYS_TR_NONE) {
        if (Esys_FlushContext(esys_context, primaryHandle) != TSS2_RC_SUCCESS) {
            LOG_ERROR("Cleanup primaryHandle failed.");
        }
    }

    return failure_return;
}

int
test_invoke_esapi(ESYS_CONTEXT * esys_context) {
    return test_esys_policy_ticket(esys_context);
}
