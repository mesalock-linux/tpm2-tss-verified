/* SPDX-License-Identifier: BSD-2 */
/*******************************************************************************
 * Copyright 2017-2018, Fraunhofer SIT sponsored by Infineon Technologies AG
 * All rights reserved.
 ******************************************************************************/

#include "tss2_mu.h"
#include "tss2_sys.h"
#include "tss2_esys.h"

#include "esys_types.h"
#include "esys_iutil.h"
#include "esys_mu.h"
#define LOGMODULE esys
#include "util/log.h"
#include "util/aux_util.h"

/** Store command parameters inside the ESYS_CONTEXT for use during _Finish */
static void store_input_parameters (
    ESYS_CONTEXT *esysContext,
    const TPM2B_SENSITIVE *inPrivate,
    const TPM2B_PUBLIC *inPublic,
    TPMI_RH_HIERARCHY hierarchy)
{
    esysContext->in.LoadExternal.hierarchy = hierarchy;
    if (inPrivate == NULL) {
        esysContext->in.LoadExternal.inPrivate = NULL;
    } else {
        esysContext->in.LoadExternal.inPrivateData = *inPrivate;
        esysContext->in.LoadExternal.inPrivate =
            &esysContext->in.LoadExternal.inPrivateData;
    }
    if (inPublic == NULL) {
        esysContext->in.LoadExternal.inPublic = NULL;
    } else {
        esysContext->in.LoadExternal.inPublicData = *inPublic;
        esysContext->in.LoadExternal.inPublic =
            &esysContext->in.LoadExternal.inPublicData;
    }
}

/** One-Call function for TPM2_LoadExternal
 *
 * This function invokes the TPM2_LoadExternal command in a one-call
 * variant. This means the function will block until the TPM response is
 * available. All input parameters are const. The memory for non-simple output
 * parameters is allocated by the function implementation.
 *
 * @param[in,out] esysContext The ESYS_CONTEXT.
 * @param[in]  shandle1 First session handle.
 * @param[in]  shandle2 Second session handle.
 * @param[in]  shandle3 Third session handle.
 * @param[in]  inPrivate The sensitive portion of the object (optional).
 * @param[in]  inPublic The public portion of the object.
 * @param[in]  hierarchy Hierarchy with which the object area is associated.
 * @param[out] objectHandle  ESYS_TR handle of ESYS resource for TPM2_HANDLE.
 * @retval TSS2_RC_SUCCESS if the function call was a success.
 * @retval TSS2_ESYS_RC_BAD_REFERENCE if the esysContext or required input
 *         pointers or required output handle references are NULL.
 * @retval TSS2_ESYS_RC_BAD_CONTEXT: if esysContext corruption is detected.
 * @retval TSS2_ESYS_RC_MEMORY: if the ESAPI cannot allocate enough memory for
 *         internal operations or return parameters.
 * @retval TSS2_ESYS_RC_BAD_SEQUENCE: if the context has an asynchronous
 *         operation already pending.
 * @retval TSS2_ESYS_RC_INSUFFICIENT_RESPONSE: if the TPM's response does not
 *          at least contain the tag, response length, and response code.
 * @retval TSS2_ESYS_RC_MALFORMED_RESPONSE: if the TPM's response is corrupted.
 * @retval TSS2_ESYS_RC_RSP_AUTH_FAILED: if the response HMAC from the TPM
           did not verify.
 * @retval TSS2_ESYS_RC_MULTIPLE_DECRYPT_SESSIONS: if more than one session has
 *         the 'decrypt' attribute bit set.
 * @retval TSS2_ESYS_RC_MULTIPLE_ENCRYPT_SESSIONS: if more than one session has
 *         the 'encrypt' attribute bit set.
 * @retval TSS2_RCs produced by lower layers of the software stack may be
 *         returned to the caller unaltered unless handled internally.
 */
TSS2_RC
Esys_LoadExternal(
    ESYS_CONTEXT *esysContext,
    ESYS_TR shandle1,
    ESYS_TR shandle2,
    ESYS_TR shandle3,
    const TPM2B_SENSITIVE *inPrivate,
    const TPM2B_PUBLIC *inPublic,
    TPMI_RH_HIERARCHY hierarchy, ESYS_TR *objectHandle)
{
    TSS2_RC r;

    r = Esys_LoadExternal_Async(esysContext, shandle1, shandle2, shandle3,
                                inPrivate, inPublic, hierarchy);
    return_if_error(r, "Error in async function");

    /* Set the timeout to indefinite for now, since we want _Finish to block */
    int32_t timeouttmp = esysContext->timeout;
    esysContext->timeout = -1;
    /*
     * Now we call the finish function, until return code is not equal to
     * from TSS2_BASE_RC_TRY_AGAIN.
     * Note that the finish function may return TSS2_RC_TRY_AGAIN, even if we
     * have set the timeout to -1. This occurs for example if the TPM requests
     * a retransmission of the command via TPM2_RC_YIELDED.
     */
    do {
        r = Esys_LoadExternal_Finish(esysContext, objectHandle);
        /* This is just debug information about the reattempt to finish the
           command */
        if ((r & ~TSS2_RC_LAYER_MASK) == TSS2_BASE_RC_TRY_AGAIN)
            LOG_DEBUG("A layer below returned TRY_AGAIN: %" PRIx32
                      " => resubmitting command", r);
    } while ((r & ~TSS2_RC_LAYER_MASK) == TSS2_BASE_RC_TRY_AGAIN);

    /* Restore the timeout value to the original value */
    esysContext->timeout = timeouttmp;
    return_if_error(r, "Esys Finish");

    return TSS2_RC_SUCCESS;
}

/** Asynchronous function for TPM2_LoadExternal
 *
 * This function invokes the TPM2_LoadExternal command in a asynchronous
 * variant. This means the function will return as soon as the command has been
 * sent downwards the stack to the TPM. All input parameters are const.
 * In order to retrieve the TPM's response call Esys_LoadExternal_Finish.
 *
 * @param[in,out] esysContext The ESYS_CONTEXT.
 * @param[in]  shandle1 First session handle.
 * @param[in]  shandle2 Second session handle.
 * @param[in]  shandle3 Third session handle.
 * @param[in]  inPrivate The sensitive portion of the object (optional).
 * @param[in]  inPublic The public portion of the object.
 * @param[in]  hierarchy Hierarchy with which the object area is associated.
 * @retval ESYS_RC_SUCCESS if the function call was a success.
 * @retval TSS2_ESYS_RC_BAD_REFERENCE if the esysContext or required input
 *         pointers or required output handle references are NULL.
 * @retval TSS2_ESYS_RC_BAD_CONTEXT: if esysContext corruption is detected.
 * @retval TSS2_ESYS_RC_MEMORY: if the ESAPI cannot allocate enough memory for
 *         internal operations or return parameters.
 * @retval TSS2_RCs produced by lower layers of the software stack may be
           returned to the caller unaltered unless handled internally.
 * @retval TSS2_ESYS_RC_MULTIPLE_DECRYPT_SESSIONS: if more than one session has
 *         the 'decrypt' attribute bit set.
 * @retval TSS2_ESYS_RC_MULTIPLE_ENCRYPT_SESSIONS: if more than one session has
 *         the 'encrypt' attribute bit set.
 */
TSS2_RC
Esys_LoadExternal_Async(
    ESYS_CONTEXT *esysContext,
    ESYS_TR shandle1,
    ESYS_TR shandle2,
    ESYS_TR shandle3,
    const TPM2B_SENSITIVE *inPrivate,
    const TPM2B_PUBLIC *inPublic,
    TPMI_RH_HIERARCHY hierarchy)
{
    TSS2_RC r;
    LOG_TRACE("context=%p, inPrivate=%p, inPublic=%p,"
              "hierarchy=%"PRIx32 "",
              esysContext, inPrivate, inPublic, hierarchy);
    TSS2L_SYS_AUTH_COMMAND auths;

    /* Check context, sequence correctness and set state to error for now */
    if (esysContext == NULL) {
        LOG_ERROR("esyscontext is NULL.");
        return TSS2_ESYS_RC_BAD_REFERENCE;
    }
    r = iesys_check_sequence_async(esysContext);
    if (r != TSS2_RC_SUCCESS)
        return r;
    esysContext->state = _ESYS_STATE_INTERNALERROR;

    /* Check and store input parameters */
    r = check_session_feasibility(shandle1, shandle2, shandle3, 0);
    return_state_if_error(r, _ESYS_STATE_INIT, "Check session usage");
    store_input_parameters(esysContext, inPrivate, inPublic, hierarchy);

    /* Initial invocation of SAPI to prepare the command buffer with parameters */
    r = Tss2_Sys_LoadExternal_Prepare(esysContext->sys, inPrivate, inPublic,
                                      hierarchy);
    return_state_if_error(r, _ESYS_STATE_INIT, "SAPI Prepare returned error.");

    /* Calculate the cpHash Values */
    r = init_session_tab(esysContext, shandle1, shandle2, shandle3);
    return_state_if_error(r, _ESYS_STATE_INIT, "Initialize session resources");
    iesys_compute_session_value(esysContext->session_tab[0], NULL, NULL);
    iesys_compute_session_value(esysContext->session_tab[1], NULL, NULL);
    iesys_compute_session_value(esysContext->session_tab[2], NULL, NULL);

    /* Generate the auth values and set them in the SAPI command buffer */
    r = iesys_gen_auths(esysContext, NULL, NULL, NULL, &auths);
    return_state_if_error(r, _ESYS_STATE_INIT,
                          "Error in computation of auth values");

    esysContext->authsCount = auths.count;
    r = Tss2_Sys_SetCmdAuths(esysContext->sys, &auths);
    return_state_if_error(r, _ESYS_STATE_INIT, "SAPI error on SetCmdAuths");

    /* Trigger execution and finish the async invocation */
    r = Tss2_Sys_ExecuteAsync(esysContext->sys);
    return_state_if_error(r, _ESYS_STATE_INTERNALERROR,
                          "Finish (Execute Async)");

    esysContext->state = _ESYS_STATE_SENT;

    return r;
}

/** Asynchronous finish function for TPM2_LoadExternal
 *
 * This function returns the results of a TPM2_LoadExternal command
 * invoked via Esys_LoadExternal_Finish. All non-simple output parameters
 * are allocated by the function's implementation. NULL can be passed for every
 * output parameter if the value is not required.
 *
 * @param[in,out] esysContext The ESYS_CONTEXT.
 * @param[out] objectHandle  ESYS_TR handle of ESYS resource for TPM2_HANDLE.
 * @retval TSS2_RC_SUCCESS on success
 * @retval ESYS_RC_SUCCESS if the function call was a success.
 * @retval TSS2_ESYS_RC_BAD_REFERENCE if the esysContext or required input
 *         pointers or required output handle references are NULL.
 * @retval TSS2_ESYS_RC_BAD_CONTEXT: if esysContext corruption is detected.
 * @retval TSS2_ESYS_RC_MEMORY: if the ESAPI cannot allocate enough memory for
 *         internal operations or return parameters.
 * @retval TSS2_ESYS_RC_BAD_SEQUENCE: if the context has an asynchronous
 *         operation already pending.
 * @retval TSS2_ESYS_RC_TRY_AGAIN: if the timeout counter expires before the
 *         TPM response is received.
 * @retval TSS2_ESYS_RC_INSUFFICIENT_RESPONSE: if the TPM's response does not
 *         at least contain the tag, response length, and response code.
 * @retval TSS2_ESYS_RC_RSP_AUTH_FAILED: if the response HMAC from the TPM did
 *         not verify.
 * @retval TSS2_ESYS_RC_MALFORMED_RESPONSE: if the TPM's response is corrupted.
 * @retval TSS2_RCs produced by lower layers of the software stack may be
 *         returned to the caller unaltered unless handled internally.
 */
TSS2_RC
Esys_LoadExternal_Finish(
    ESYS_CONTEXT *esysContext, ESYS_TR *objectHandle)
{
    TSS2_RC r;
    LOG_TRACE("context=%p, objectHandle=%p",
              esysContext, objectHandle);

    if (esysContext == NULL) {
        LOG_ERROR("esyscontext is NULL.");
        return TSS2_ESYS_RC_BAD_REFERENCE;
    }

    /* Check for correct sequence and set sequence to irregular for now */
    if (esysContext->state != _ESYS_STATE_SENT) {
        LOG_ERROR("Esys called in bad sequence.");
        return TSS2_ESYS_RC_BAD_SEQUENCE;
    }
    esysContext->state = _ESYS_STATE_INTERNALERROR;
    TPM2B_NAME name;
    RSRC_NODE_T *objectHandleNode = NULL;

    /* Allocate memory for response parameters */
    if (objectHandle == NULL) {
        LOG_ERROR("Handle objectHandle may not be NULL");
        return TSS2_ESYS_RC_BAD_REFERENCE;
    }
    *objectHandle = esysContext->esys_handle_cnt++;
    r = esys_CreateResourceObject(esysContext, *objectHandle, &objectHandleNode);
    if (r != TSS2_RC_SUCCESS)
        return r;

    if (esysContext->in.LoadExternal.inPublic) {
        objectHandleNode->rsrc.rsrcType = IESYSC_KEY_RSRC;
        objectHandleNode->rsrc.misc.rsrc_key_pub = *esysContext->in.LoadExternal.inPublic;
    } else {
        objectHandleNode->rsrc.misc.rsrc_key_pub.size = 0;
    }

    /*Receive the TPM response and handle resubmissions if necessary. */
    r = Tss2_Sys_ExecuteFinish(esysContext->sys, esysContext->timeout);
    if ((r & ~TSS2_RC_LAYER_MASK) == TSS2_BASE_RC_TRY_AGAIN) {
        LOG_DEBUG("A layer below returned TRY_AGAIN: %" PRIx32, r);
        esysContext->state = _ESYS_STATE_SENT;
        goto error_cleanup;
    }
    /* This block handle the resubmission of TPM commands given a certain set of
     * TPM response codes. */
    if (r == TPM2_RC_RETRY || r == TPM2_RC_TESTING || r == TPM2_RC_YIELDED) {
        LOG_DEBUG("TPM returned RETRY, TESTING or YIELDED, which triggers a "
            "resubmission: %" PRIx32, r);
        if (esysContext->submissionCount >= _ESYS_MAX_SUBMISSIONS) {
            LOG_WARNING("Maximum number of (re)submissions has been reached.");
            esysContext->state = _ESYS_STATE_INIT;
            goto error_cleanup;
        }
        esysContext->state = _ESYS_STATE_RESUBMISSION;
        r = Esys_LoadExternal_Async(esysContext, esysContext->session_type[0],
                                    esysContext->session_type[1],
                                    esysContext->session_type[2],
                                    esysContext->in.LoadExternal.inPrivate,
                                    esysContext->in.LoadExternal.inPublic,
                                    esysContext->in.LoadExternal.hierarchy);
        if (r != TSS2_RC_SUCCESS) {
            LOG_WARNING("Error attempting to resubmit");
            /* We do not set esysContext->state here but inherit the most recent
             * state of the _async function. */
            goto error_cleanup;
        }
        r = TSS2_ESYS_RC_TRY_AGAIN;
        LOG_DEBUG("Resubmission initiated and returning RC_TRY_AGAIN.");
        goto error_cleanup;
    }
    /* The following is the "regular error" handling. */
    if (iesys_tpm_error(r)) {
        LOG_WARNING("Received TPM Error");
        esysContext->state = _ESYS_STATE_INIT;
        goto error_cleanup;
    } else if (r != TSS2_RC_SUCCESS) {
        LOG_ERROR("Received a non-TPM Error");
        esysContext->state = _ESYS_STATE_INTERNALERROR;
        goto error_cleanup;
    }

    /*
     * Now the verification of the response (hmac check) and if necessary the
     * parameter decryption have to be done.
     */
    r = iesys_check_response(esysContext);
    goto_state_if_error(r, _ESYS_STATE_INTERNALERROR, "Error: check response",
                        error_cleanup);

    /*
     * After the verification of the response we call the complete function
     * to deliver the result.
     */
    r = Tss2_Sys_LoadExternal_Complete(esysContext->sys,
                                       &objectHandleNode->rsrc.handle, &name);
    goto_state_if_error(r, _ESYS_STATE_INTERNALERROR,
                        "Received error from SAPI unmarshaling" ,
                        error_cleanup);


    /* check name against inPublic */
    if (!iesys_compare_name(esysContext->in.LoadExternal.inPublic, &name)) {
        goto_error(r, TSS2_ESYS_RC_MALFORMED_RESPONSE,
                      "in Public name not equal name in response", error_cleanup);
    }
    objectHandleNode->rsrc.name = name;
    esysContext->state = _ESYS_STATE_INIT;

    return TSS2_RC_SUCCESS;

error_cleanup:
    Esys_TR_Close(esysContext, objectHandle);

    return r;
}
