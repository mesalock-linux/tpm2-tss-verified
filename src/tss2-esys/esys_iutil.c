/* SPDX-License-Identifier: BSD-2 */
/*******************************************************************************
 * Copyright 2017-2018, Fraunhofer SIT sponsored by Infineon Technologies AG
 * All rights reserved.
 ******************************************************************************/
#include <inttypes.h>

#include "tss2_esys.h"
#include "esys_mu.h"

#include "esys_iutil.h"
#include "esys_int.h"
#define LOGMODULE esys
#include "util/log.h"
#include "util/aux_util.h"
#include <limits.h>

/**
 * Compare variables of type UINT16.
 * @param[in] in1 Variable to be compared with:
 * @param[in] in2
 */
bool
cmp_UINT16(const UINT16 * in1, const UINT16 * in2)
{
    if (!in1 || !in2) return false;

    LOG_TRACE("call");
    if (*in1 == *in2)
        return true;
    else {
        LOG_TRACE("cmp false");
        return false;
    }
}

/**
 * Compare variables of type BYTE.
 * @param[in] in1 Variable to be compared with:
 * @param[in] in2
 */
bool
cmp_BYTE(const BYTE * in1, const BYTE * in2)
{
    if (!in1 || !in2) return false;

    LOG_TRACE("call");
    if (*in1 == *in2)
        return true;
    else {
        LOG_TRACE("cmp false");
        return false;
    }
}

/**
 * Compare two arrays of type BYTE.
 * @param[in] in1 array to be compared with:.
 * @param[in] in2
 */

bool
cmp_BYTE_array(const BYTE * in1, size_t count1, const BYTE * in2, size_t count2)
{
    if (count1 != count2) {
        LOG_TRACE("cmp false");
        return false;
    }

    if (!in1 || !in2) return false;

    for (size_t i = 0; i < count1; i++) {
        if (!cmp_BYTE(&in1[i], &in2[i])) {
            LOG_TRACE("cmp false");
            return false;
        }
    }
    return true;
}

/**
 * Compare two variables of type TPM2B_DIGEST.
 * @param[in] in1 variable to be compared with:
 * @param[in] in2
 */
bool
cmp_TPM2B_DIGEST(const TPM2B_DIGEST * in1, const TPM2B_DIGEST * in2)
{
    LOG_TRACE("call");

    if (!in1 || !in2) return false;

    if (!cmp_UINT16(&in1->size, &in2->size)) {
        LOG_TRACE("cmp false");
        return false;
    }

    return cmp_BYTE_array((BYTE *) & in1->buffer, in1->size,
                          (BYTE *) & in2->buffer, in2->size);

    return true;
}

/**
 * Compare two variables of type TPM2B_NAME.
 * @param[in] in1 variable to be compared with:
 * @param[in] in2
 */
bool
cmp_TPM2B_NAME(const TPM2B_NAME * in1, const TPM2B_NAME * in2)
{
    LOG_TRACE("call");

    if (!in1 || !in2) return false;

    if (!cmp_UINT16(&in1->size, &in2->size)) {
        LOG_TRACE("cmp false");
        return false;
    }

    return cmp_BYTE_array((BYTE *) & in1->name, in1->size, (BYTE *) & in2->name,
                          in2->size);

    return true;
}

/**
 * Compare two structures of type TPM2B_AUTH.
 * @param[in] in1 Structure to be compared with:
 * @param[in] in1
 */
bool
cmp_TPM2B_AUTH(const TPM2B_AUTH * in1, const TPM2B_AUTH * in2)
{
    LOG_TRACE("call");
    return cmp_TPM2B_DIGEST(in1, in2);
}

TSS2_RC
init_session_tab(ESYS_CONTEXT *esys_context,
                 ESYS_TR shandle1, ESYS_TR shandle2, ESYS_TR shandle3)
{
    TSS2_RC r = TPM2_RC_SUCCESS;
    ESYS_TR handle_tab[3] = { shandle1, shandle2, shandle3 };
    for (int i = 0; i < 3; i++) {
        esys_context->session_type[i] = handle_tab[i];
        if (handle_tab[i] == ESYS_TR_NONE || handle_tab[i] == ESYS_TR_PASSWORD) {
            esys_context->session_tab[i] = NULL;
        } else {
            r = esys_GetResourceObject(esys_context, handle_tab[i],
                                       &esys_context->session_tab[i]);
            return_if_error(r, "Unknown resource.");
        }
    }
    return r;
}

/** Delete all resource objects stored in the esys context.
 *
 * All resource objects stored in a linked list of the esys context are deleted.
 * @param[in,out] esys_context The ESYS_CONTEXT
 */
void
iesys_DeleteAllResourceObjects(ESYS_CONTEXT * esys_context)
{
    if (!esys_context) return;

    RSRC_NODE_T *node_rsrc;
    RSRC_NODE_T *next_node_rsrc;
    for (node_rsrc = esys_context->rsrc_list; node_rsrc != NULL;
         node_rsrc = next_node_rsrc) {
        next_node_rsrc = node_rsrc->next;
        SAFE_FREE(node_rsrc);
    }
}
/**  Compute the TPM nonce of the session used for parameter encryption.
 *
 * Since only encryption session can be used an error is signaled if
 * more encryption sessions are used.
 * @param[in] esys_context The ESYS_CONTEXT
 * @param[out] encryptNonceIndex The number of the session used for encryption.
 * @param[out] encryptNonce The nonce used for encryption by TPM.
 * @retval TSS2_RC_SUCCESS on Success.
 * @retval TSS2_ESYS_RC_MULTIPLE_ENCRYPT_SESSIONS if more than one encrypt
 *         session is used.
 */
TSS2_RC
iesys_compute_encrypt_nonce(ESYS_CONTEXT * esys_context,
                            int *encryptNonceIdx, TPM2B_NONCE ** encryptNonce)
{
    _ESYS_ASSERT_NON_NULL(esys_context);
    _ESYS_ASSERT_NON_NULL(encryptNonceIdx);
    _ESYS_ASSERT_NON_NULL(encryptNonce);

    for (int i = 0; i < 3; i++) {
        RSRC_NODE_T *session = esys_context->session_tab[i];
        if (session != NULL) {
            if (session->rsrc.misc.rsrc_session.
                sessionAttributes & TPMA_SESSION_ENCRYPT) {
                if (*encryptNonce != NULL) {
                    /* Encrypt nonce already found */
                    return_error(TSS2_ESYS_RC_MULTIPLE_ENCRYPT_SESSIONS,
                                 "More than one encrypt session");
                }
                *encryptNonceIdx = i;
                *encryptNonce = &session->rsrc.misc.rsrc_session.nonceTPM;
            }
        }
    }
    return TSS2_RC_SUCCESS;
}

/** Computation of the command parameter(cp) hashes.
 *
 * The command parameter(cp) hash of the command is computed for every
 * session.  If the sessions use different hash algorithms then different cp
 * hashes must be calculated.
 * The names of objects with an auth index and the command buffer are used
 * to compute the cp hash with the hash algorithm of the corresponding session.
 * The result is stored in table together with the used hash algorithm.
 * @param[in] esys_context The ESYS_CONTEXT
 * @param[in] name1 The name of the first object with an auth index.
 * @param[in] name2 The name of the second object with an auth index.
 * @param[in] name3 The name of the third object with an auth index.
 * @param[3] [out] cp_hash_tab An array with all cp hashes.
 *        The used hash algorithm is stored in this table to find the
 *        appropriate values for a session.
 * @param[out] cpHashNum Number of computed cp hash values. This value
 *        corresponds to the number of used hash algorithms.
 * @retval TSS2_RC_SUCCESS on success,
 * @retval TSS2_ESYS_RC_BAD_REFERENCE for invalid parameters.
 * @retval TSS2_ESYS_RC_NOT_IMPLEMENTED if a hash algorithm is not implemented.
 * @retval TSS2_SYS_RC_* for SAPI errors.
 */
TSS2_RC
iesys_compute_cp_hashtab(ESYS_CONTEXT * esys_context,
                         const TPM2B_NAME * name1,
                         const TPM2B_NAME * name2,
                         const TPM2B_NAME * name3,
                         HASH_TAB_ITEM cp_hash_tab[3], uint8_t * cpHashNum)
{
    _ESYS_ASSERT_NON_NULL(esys_context);
    _ESYS_ASSERT_NON_NULL(cpHashNum);
    _ESYS_ASSERT_NON_NULL(cp_hash_tab);

    uint8_t ccBuffer[4];
    TSS2_RC r = Tss2_Sys_GetCommandCode(esys_context->sys, &ccBuffer[0]);
    return_if_error(r, "Error: get command code");
    const uint8_t *cpBuffer;
    size_t cpBuffer_size;
    r = Tss2_Sys_GetCpBuffer(esys_context->sys, &cpBuffer_size, &cpBuffer);
    return_if_error(r, "Error: get cp buffer");
    *cpHashNum = 0;
    for (int i = 0; i < 3; i++) {
        RSRC_NODE_T *session = esys_context->session_tab[i];
        bool cpHashFound = false;
        if (session != NULL) {
            /* We do not want to compute cpHashes multiple times for the same
               algorithm to save time and space */
            for (int j = 0; j < *cpHashNum; j++)
                /* Check if cpHash for this algorithm was already computed */
                if (cp_hash_tab[j].alg ==
                    session->rsrc.misc.rsrc_session.authHash) {
                    cpHashFound = true;
                    break;
                }
            /* If not, we compute it and append it to the list */
            if (!cpHashFound) {
                cp_hash_tab[*cpHashNum].size = sizeof(TPMU_HA);
                r = iesys_crypto_cpHash(session->rsrc.misc.rsrc_session.
                                        authHash, ccBuffer, name1, name2, name3,
                                        cpBuffer, cpBuffer_size,
                                        &cp_hash_tab[*cpHashNum].digest[0],
                                        &cp_hash_tab[*cpHashNum].size);
                return_if_error(r, "crypto cpHash");

                cp_hash_tab[*cpHashNum].alg =
                    session->rsrc.misc.rsrc_session.authHash;
                *cpHashNum += 1;
            }
        }
    }
    return r;
}

/** Computation of the response parameter (rp) hashes.
 * The response parameter (rp) hash of the response is computed for every
 * session.  If the sessions use different hash algorithms then different rp
 * hashes must be calculated.
 * The names of objects with an auth index and the command buffer are used
 * to compute the cp hash with the hash algorithm of the corresponding session.
 * The result is stored in table together with the used hash algorithm.
 * @param[in] esys_context The ESYS_CONTEXT
 * @param[in] rspAuths List of response
 * @param[in] const uint8_t * rpBuffer The pointer to the response buffer
 * @param[in] size_t rpBuffer_size The size of the response.
 * @param[out] HASH_TAB_ITEM rp_hash_tab[3] An array with all rp hashes.
 *        The used hash algorithm is stored in this table to find the
 *        appropriate values for a session.
 * @param[out] uint8_t Number of computed rp hash values. This value
 *        corresponds to the number of used hash algorithms.
 * @retval TSS2_RC_SUCCESS on success.
 * @retval TSS2_ESYS_RC_BAD_REFERENCE for invalid parameters.
 * @retval TSS2_ESYS_RC_NOT_IMPLEMENTED if a hash algorithm is not implemented.
 * @retval TSS2_SYS_RC_* for SAPI errors.
 */
TSS2_RC
iesys_compute_rp_hashtab(ESYS_CONTEXT * esys_context,
                         const uint8_t * rpBuffer,
                         size_t rpBuffer_size,
                         HASH_TAB_ITEM rp_hash_tab[3], uint8_t * rpHashNum)
{
    _ESYS_ASSERT_NON_NULL(esys_context);
    _ESYS_ASSERT_NON_NULL(rpHashNum);
    _ESYS_ASSERT_NON_NULL(rp_hash_tab);

    uint8_t rcBuffer[4] = { 0 };
    uint8_t ccBuffer[4];
    TSS2_RC r = Tss2_Sys_GetCommandCode(esys_context->sys, &ccBuffer[0]);
    return_if_error(r, "Error: get command code");

    for (int i = 0; i < esys_context->authsCount; i++) {
        RSRC_NODE_T *session = esys_context->session_tab[i];
        if (session == NULL)
            continue;
        bool rpHashFound = false;
        /* We do not want to compute cpHashes multiple times for the same
           algorithm to save time and space */
        for (int j = 0; j < *rpHashNum; j++)
            if (rp_hash_tab[j].alg == session->rsrc.misc.rsrc_session.authHash) {
                rpHashFound = true;
                break;
            }
        /* If not, we compute it and append it to the list */
        if (!rpHashFound) {
            rp_hash_tab[*rpHashNum].size = sizeof(TPMU_HA);
            r = iesys_crypto_rpHash(session->rsrc.misc.rsrc_session.authHash,
                                    rcBuffer, ccBuffer, rpBuffer, rpBuffer_size,
                                    &rp_hash_tab[*rpHashNum].digest[0],
                                    &rp_hash_tab[*rpHashNum].size);
            return_if_error(r, "crypto rpHash");
            rp_hash_tab[*rpHashNum].alg =
                session->rsrc.misc.rsrc_session.authHash;
            *rpHashNum += 1;
        }
    }
    return TPM2_RC_SUCCESS;
}
/** Create an esys resource object corresponding to a TPM object.
 *
 * The esys object is appended to the resource list stored in the esys context
 * (rsrc_list).
 * @param[in] esys_context The ESYS_CONTEXT
 * @param[in] esys_handle The esys handle which will be used for this object.
 * @param[out] esys_object The new resource object.
 * @retval TSS2_RC_SUCCESS on success.
 * @retval TSS2_ESYS_RC_MEMORY if the object can not be allocated.
 */
TSS2_RC
esys_CreateResourceObject(ESYS_CONTEXT * esys_context,
                          ESYS_TR esys_handle, RSRC_NODE_T ** esys_object)
{
    _ESYS_ASSERT_NON_NULL(esys_context);
    _ESYS_ASSERT_NON_NULL(esys_object);

    RSRC_NODE_T *new_esys_object = calloc(1, sizeof(RSRC_NODE_T));
    if (new_esys_object == NULL)
        return_error(TSS2_ESYS_RC_MEMORY, "Out of memory.");
    if (esys_context->rsrc_list == NULL) {
        /* The first object of the list will be added */
        esys_context->rsrc_list = new_esys_object;
        new_esys_object->next = NULL;
    } else {
        /* The new object will become the first element of the list */
        new_esys_object->next = esys_context->rsrc_list;
        esys_context->rsrc_list = new_esys_object;
    }
    *esys_object = new_esys_object;
    new_esys_object->esys_handle = esys_handle;
    return TSS2_RC_SUCCESS;
}

/** Compute tpm handle for standard esys handles.
 *
 * The tpm handle ist computed for esys handles representing pcr registers and
 * hierarchies.
 * @parm esys_handle [in] The esys handle.
 * @parm tpm_handle [out] The corresponding tpm handle.
 * @retval TSS2_RC_SUCCESS on success.
 * @retval TSS2_ESYS_RC_BAD_VALUE if no standard handle is passed.
 */
TSS2_RC
iesys_handle_to_tpm_handle(ESYS_TR esys_handle, TPM2_HANDLE * tpm_handle)
{
    _ESYS_ASSERT_NON_NULL(tpm_handle);

    /* Since ESYS_TR_PCR0 is equal zero only <= ESYS_TR_PCR31 has to be checked */
    if (esys_handle <= ESYS_TR_PCR31) {
        *tpm_handle = (TPM2_HANDLE) esys_handle;
        return TPM2_RC_SUCCESS;
    }
    if (esys_handle == ESYS_TR_RH_OWNER) {
        *tpm_handle = TPM2_RH_OWNER;
        return TPM2_RC_SUCCESS;
    }
    if (esys_handle == ESYS_TR_RH_NULL) {
        *tpm_handle = TPM2_RH_NULL;
        return TPM2_RC_SUCCESS;
    }
    if (esys_handle == ESYS_TR_RH_LOCKOUT) {
        *tpm_handle = TPM2_RH_LOCKOUT;
        return TPM2_RC_SUCCESS;
    }
    if (esys_handle == ESYS_TR_RH_ENDORSEMENT) {
        *tpm_handle = TPM2_RH_ENDORSEMENT;
        return TPM2_RC_SUCCESS;
    }
    if (esys_handle == ESYS_TR_RH_PLATFORM) {
        *tpm_handle = TPM2_RH_PLATFORM;
        return TPM2_RC_SUCCESS;
    }
    if (esys_handle == ESYS_TR_RH_PLATFORM_NV) {
        *tpm_handle = TPM2_RH_PLATFORM_NV;
        return TPM2_RC_SUCCESS;
    }
    LOG_ERROR("Error: Esys invalid ESAPI handle (%x).", esys_handle);
    return TSS2_ESYS_RC_BAD_VALUE;
}
/** Get the type of a tpm handle.
 *
 * @parm handle[in] The tpm handle.
 * @retval The part of the handle which represents the handle type.
 */
TPM2_HT
iesys_get_handle_type(TPM2_HANDLE handle)
{
    /* upper bytes of input data */
    TPM2_HT ht = (TPM2_HT) ((handle & TPM2_HR_RANGE_MASK) >> TPM2_HR_SHIFT);
    return ht;
}

/** Compute name derived from public info with a tpm name.
 *
 * A tpm name is computed from a public info structure and compared with a
 * second tpm name.
 * @param[in]  publicInfo The public info for name computation.
 * @param[in] name The name used for comparison.
 * @retval bool indicates whether the names are equal.
 */
bool
iesys_compare_name(TPM2B_PUBLIC * publicInfo, TPM2B_NAME * name)
{
    TSS2_RC r = TSS2_RC_SUCCESS;
    TPM2B_NAME public_info_name = {0};
    if (publicInfo == NULL || name == NULL)
        return false;
    r = iesys_get_name(publicInfo, &public_info_name);
    if (r != TSS2_RC_SUCCESS) {
        LOG_DEBUG("name could not be computed.");
        return false;
    }
    return cmp_TPM2B_NAME(&public_info_name, name);
}

/** Compute a random salt which will be used for parameter encryption.
 *
 * Depending in the type of TPM key used for key exchange a salt will be computed.
 * For an ECC key an ephemeral key will be computed. This key together with the
 * public point of the TPMs key will be used to compute a shared secret which will
 * be used for the key derivation of the key for parameter encryption.
 * For an RSA key a random number will be computed to derive this key. The random
 * number will be encrypted with the TPM key.
 * @param[in,out]  esys_context The ESYS_CONTEXT. The generated salt will be
 *                 stored in this context.
 * @param[in] tpmKeyNode The esys resource object of the TPM key which will be
 *            used for key exchange.
 * @param[out] encryptedSalt In the case of an ECC the public point of the
 *             ephemeral key will be marshaled into this buffer.
 *             In the case of a TPM key the encrypted salt will be stored.
 * @retval TSS2_RC_SUCCESS on success.
 * @retval TSS2_ESYS_RC_MEMORY Memory can not be allocated.
 * @retval TSS2_ESYS_RC_BAD_VALUE for invalid parameters.
 * @retval TSS2_ESYS_RC_BAD_REFERENCE for unexpected NULL pointer parameters.
 * @retval TSS2_ESYS_RC_GENERAL_FAILURE for errors of the crypto library.
 * @retval TSS2_SYS_RC_* for SAPI errors.
 */
TSS2_RC
iesys_compute_encrypted_salt(ESYS_CONTEXT * esys_context,
                             RSRC_NODE_T * tpmKeyNode,
                             TPM2B_ENCRYPTED_SECRET * encryptedSalt)
{
    TSS2_RC r = TSS2_RC_SUCCESS;
    size_t keyHash_size = 0;
    size_t cSize = 0;
    TPM2B_ECC_PARAMETER Z; /* X coordinate of privKey*publicKey */
    TPMS_ECC_POINT Q; /* Public point of ephemeral key */

    if (tpmKeyNode == 0) {
        _ESYS_ASSERT_NON_NULL(encryptedSalt);
        encryptedSalt->size = 0;
        return TSS2_RC_SUCCESS;
    }

    TPM2B_PUBLIC pub = tpmKeyNode->rsrc.misc.rsrc_key_pub;
    if (tpmKeyNode->rsrc.rsrcType != IESYSC_KEY_RSRC) {
        LOG_TRACE("Public info needed.");
        return TSS2_ESYS_RC_BAD_VALUE;
    }
    r = iesys_crypto_hash_get_digest_size(tpmKeyNode->rsrc.misc.
                                          rsrc_key_pub.publicArea.nameAlg,
                                          &keyHash_size);
    return_if_error(r, "Hash algorithm not supported.");

    _ESYS_ASSERT_NON_NULL(esys_context);
    _ESYS_ASSERT_NON_NULL(encryptedSalt);

    switch (pub.publicArea.type) {
    case TPM2_ALG_RSA:

        iesys_crypto_random2b((TPM2B_NONCE *) & esys_context->salt,
                              keyHash_size);

        /* When encrypting salts, the encryption scheme of a key is ignored and
           TPM2_ALG_OAEP is always used. */
        pub.publicArea.parameters.rsaDetail.scheme.scheme = TPM2_ALG_OAEP;
        r = iesys_crypto_pk_encrypt(&pub,
                                    keyHash_size, &esys_context->salt.buffer[0],
                                    sizeof(TPMU_ENCRYPTED_SECRET),
                                    (BYTE *) &encryptedSalt->secret[0], &cSize,
                                    "SECRET");
        return_if_error(r, "During encryption.");
        LOGBLOB_DEBUG(&encryptedSalt->secret[0], cSize, "IESYS encrypted salt");
        encryptedSalt->size = cSize;
        break;
    case TPM2_ALG_ECC:
        r = iesys_crypto_get_ecdh_point(&pub, sizeof(TPMU_ENCRYPTED_SECRET),
                                        &Z, &Q,
                                        (BYTE *) &encryptedSalt->secret[0],
                                        &cSize);
        return_if_error(r, "During computation of ECC public key.");
        encryptedSalt->size = cSize;

        /* Compute salt from Z with KDFe */
        r = iesys_crypto_KDFe(tpmKeyNode->rsrc.misc.
                              rsrc_key_pub.publicArea.nameAlg,
                              &Z, "SECRET", &Q.x,
                              &pub.publicArea.unique.ecc.x,
                              keyHash_size*8,
                              &esys_context->salt.buffer[0]);
        return_if_error(r, "During KDFe computation.");
        esys_context->salt.size = keyHash_size;
        break;
    default:
        LOG_ERROR("Not implemented");
        return TSS2_ESYS_RC_GENERAL_FAILURE;
        break;
    }
    return r;
}

/** Generate caller nonces for all sessions.
 *
 * For every uses session stored in context random nonce is computed.
 * @param[in,out]  esys_context The ESYS_CONTEXT. The generated nonces will be
 *                 stored in this context.
 * @retval TPM2_RC_SUCCESS on success. An possible error is:
 * @retval TSS2_ESYS_RC_BAD_VALUE if an illegal hash algorithm value is stored
 *         in a session.
 */
TSS2_RC
iesys_gen_caller_nonces(ESYS_CONTEXT * esys_context)
{
    _ESYS_ASSERT_NON_NULL(esys_context);

    TSS2_RC r;
    size_t authHash_size = 0;

    for (int i = 0; i < 3; i++) {
        RSRC_NODE_T *session = esys_context->session_tab[i];
        if (session == NULL)
            continue;
        r = iesys_crypto_hash_get_digest_size(session->rsrc.misc.rsrc_session.
                                              authHash, &authHash_size);
        return_if_error(r, "Error: initialize auth session.");

        r = iesys_crypto_random2b(&session->rsrc.misc.rsrc_session.nonceCaller,
                                authHash_size);
        return_if_error(r, "Error: computing caller nonce (%x).");
    }
    return TSS2_RC_SUCCESS;
}
/** Parameter encryption with AES or XOR obfuscation.
 *
 * One parameter of a TPM command will be encrypted with the selected method.
 * The buffer to encrypted is determined with the SAPI function:
 * Tss2_Sys_GetCpBuffer. If more than one encryption session es used an error
 * will be returned. The decryption nonce of the session used for encryption
 * will be returned and used for HMAC computation. The encryption key is
 * derived with KDFa.
 * @retval TSS2_RC_SUCCESS on success.
 * @retval TSS2_ESYS_RC_MEMORY Memory can not be allocated.
 * @retval TSS2_ESYS_RC_BAD_VALUE for invalid parameters.
 * @retval TSS2_ESYS_RC_BAD_REFERENCE for unexpected NULL pointer parameters.
 * @retval TSS2_ESYS_RC_GENERAL_FAILURE for errors of the crypto library.
 * @retval TSS2_SYS_RC_* for SAPI errors.
 */
TSS2_RC
iesys_encrypt_param(ESYS_CONTEXT * esys_context,
                    TPM2B_NONCE ** decryptNonce, int *decryptNonceIdx)
{
    _ESYS_ASSERT_NON_NULL(esys_context);
    _ESYS_ASSERT_NON_NULL(decryptNonce);
    _ESYS_ASSERT_NON_NULL(decryptNonceIdx);

    TPM2B_NONCE *encryptNonce = NULL;
    *decryptNonceIdx = 0;
    *decryptNonce = NULL;
    TSS2_RC r = TSS2_RC_SUCCESS;

    for (int i = 0; i < 3; i++) 
    {
        RSRC_NODE_T *session = esys_context->session_tab[i];
        if (session == NULL)
            continue;
        IESYS_SESSION *rsrc_session = &session->rsrc.misc.rsrc_session;
        TPMT_SYM_DEF *symDef = &rsrc_session->symmetric;
        if (rsrc_session->sessionAttributes & TPMA_SESSION_ENCRYPT) {
            return_if_notnull(encryptNonce, "More than one encrypt session",
                               TSS2_ESYS_RC_MULTIPLE_ENCRYPT_SESSIONS);
            esys_context->encryptNonceIdx = i;
            encryptNonce = &rsrc_session->nonceTPM;
            esys_context->encryptNonce = encryptNonce;
        }

        /* Session for encryption found */
        if (rsrc_session->sessionAttributes & TPMA_SESSION_DECRYPT) {
            return_if_notnull(*decryptNonce, "More than one decrypt session",
                               TSS2_ESYS_RC_MULTIPLE_DECRYPT_SESSIONS);
            *decryptNonceIdx = i;
            *decryptNonce = &rsrc_session->nonceTPM;
            size_t hlen;
            r = iesys_crypto_hash_get_digest_size(rsrc_session->authHash, &hlen);
            return_if_error(r, "get digest size");
            size_t key_len = TPM2_MAX_SYM_KEY_BYTES + TPM2_MAX_SYM_BLOCK_SIZE;
            if (key_len % hlen > 0)
                key_len = key_len + hlen - (key_len % hlen);
            uint8_t symKey[key_len];
            size_t paramSize = 0;
            const uint8_t *paramBuffer;
            r = Tss2_Sys_GetDecryptParam(esys_context->sys, &paramSize,
                                         &paramBuffer);
            return_if_error(r, "Encryption not possible");

            if (paramSize == 0)
                continue;

            BYTE encrypt_buffer[paramSize];
            memcpy(&encrypt_buffer[0], paramBuffer, paramSize);
            LOGBLOB_DEBUG(paramBuffer, paramSize, "param to encrypt");

            /* AES encryption with key derived with KDFa */
            if (symDef->algorithm == TPM2_ALG_AES) {
                if (symDef->mode.aes != TPM2_ALG_CFB) {
                    return_error(TSS2_ESYS_RC_BAD_VALUE,
                                 "Invalid symmetric mode (must be CFB)");
                }
                r = iesys_crypto_KDFa(rsrc_session->authHash,
                                      &rsrc_session->sessionValue[0],
                                      rsrc_session->sizeSessionValue, "CFB",
                                      &rsrc_session->nonceCaller,
                                      &rsrc_session->nonceTPM,
                                      symDef->keyBits.aes + AES_BLOCK_SIZE_IN_BYTES * 8,
                                      NULL, &symKey[0], FALSE);
                return_if_error(r, "while computing KDFa");

                size_t aes_off = ( symDef->keyBits.aes + 7) / 8;
                r = iesys_crypto_sym_aes_encrypt(&symKey[0],
                                                 symDef->algorithm,
                                                 symDef->keyBits.aes,
                                                 symDef->mode.aes,
                                                 AES_BLOCK_SIZE_IN_BYTES,
                                                 &encrypt_buffer[0], paramSize,
                                                 &symKey[aes_off]);
                return_if_error(r, "AES encryption not possible");
            }
            /* XOR obfuscation of parameter */
            else if (symDef->algorithm == TPM2_ALG_XOR) {
                r = iesys_xor_parameter_obfuscation(rsrc_session->authHash,
                                                    &rsrc_session->sessionValue[0],
                                                    rsrc_session->sizeSessionValue,
                                                    &rsrc_session->nonceCaller,
                                                    &rsrc_session->nonceTPM,
                                                    &encrypt_buffer[0],
                                                    paramSize);
                return_if_error(r, "XOR obfuscation not possible.");

            } else {
                return_error(TSS2_ESYS_RC_BAD_VALUE,
                             "Invalid symmetric algorithm (should be XOR or AES)");
            }
            r = Tss2_Sys_SetDecryptParam(esys_context->sys, paramSize,
                                         &encrypt_buffer[0]);
            return_if_error(r, "Set encrypt parameter not possible");

        }
    }
    return r;
}

/** Parameter decryption with AES or XOR obfuscation.
 *
 * One parameter of a TPM response will be decrypted with the selected method.
 * @param[in]  esys_context The ESYS_CONTEXT.
 * @param[in,out] rpBuffer The buffer to be encrypted. The ecrypted data will be
 *                overridden by the result.
 * @param[in] rpBuffer_size The size of the encrypted data buffer.
 * @retval TSS2_RC_SUCCESS on success.
 * @retval TSS2_ESYS_RC_MEMORY Memory can not be allocated.
 * @retval TSS2_ESYS_RC_BAD_VALUE for invalid parameters.
 * @retval TSS2_ESYS_RC_BAD_REFERENCE for unexpected NULL pointer parameters.
 * @retval TSS2_ESYS_RC_GENERAL_FAILURE for errors of the crypto library.
 * @retval TSS2_ESYS_RC_NOT_IMPLEMENTED if hash algorithm is not implemented.
 * @retval TSS2_SYS_RC_* for SAPI errors.
 */
 TSS2_RC
iesys_decrypt_param(ESYS_CONTEXT * esys_context,
                    const uint8_t * rpBuffer, size_t rpBuffer_size)
{
    _ESYS_ASSERT_NON_NULL(esys_context);
    _ESYS_ASSERT_NON_NULL(rpBuffer);

    size_t hlen;
    RSRC_NODE_T *session;
    session = esys_context->session_tab[esys_context->encryptNonceIdx];
    IESYS_SESSION *rsrc_session = &session->rsrc.misc.rsrc_session;
    TPMT_SYM_DEF *symDef = &rsrc_session->symmetric;
    TSS2_RC r = iesys_crypto_hash_get_digest_size(rsrc_session->authHash, &hlen);
    return_if_error(r, "Error");
    size_t key_len = TPM2_MAX_SYM_KEY_BYTES + TPM2_MAX_SYM_BLOCK_SIZE;

    if (key_len % hlen > 0)
        key_len = key_len + hlen - (key_len % hlen);
    uint8_t symKey[key_len];
    UINT16 p2BSize = 0;
    size_t offset = 0;
    r = Tss2_MU_UINT16_Unmarshal(rpBuffer, rpBuffer_size, &offset, &p2BSize);
    return_if_error(r, "Unmarshal error");
    if (p2BSize > rpBuffer_size) {
        return_error(TSS2_ESYS_RC_BAD_VALUE,
                     "Invalid length encrypted response.");
    }
    LOGBLOB_DEBUG(rpBuffer, p2BSize, "IESYS encrypt data");

    if (symDef->algorithm == TPM2_ALG_AES) {

        /* Parameter decryption with a symmetric AES key derived by KDFa */
        if (symDef->mode.aes != TPM2_ALG_CFB) {
            return_error(TSS2_ESYS_RC_BAD_VALUE,
                         "Invalid symmetric mode (must be CFB)");
        }
        LOGBLOB_DEBUG(&rsrc_session->sessionKey.buffer[0],
                      rsrc_session->sessionKey.size,
                      "IESYS encrypt session key");

        r = iesys_crypto_KDFa(rsrc_session->authHash,
                              &rsrc_session->sessionValue[0],
                              rsrc_session->sizeSessionValue,
                              "CFB", &rsrc_session->nonceTPM,
                              &rsrc_session->nonceCaller,
                              symDef->keyBits.aes
                              + AES_BLOCK_SIZE_IN_BYTES * 8, NULL,
                              &symKey[0], FALSE);
        return_if_error(r, "KDFa error");
        LOGBLOB_DEBUG(&symKey[0],
                      ((symDef->keyBits.aes +
                        AES_BLOCK_SIZE_IN_BYTES * 8) + 7) / 8,
                      "IESYS encrypt KDFa key");

        size_t aes_off = ( symDef->keyBits.aes + 7) / 8;
        r = iesys_crypto_sym_aes_decrypt(&symKey[0],
                                     symDef->algorithm,
                                     symDef->keyBits.aes,
                                     symDef->mode.aes,
                                     AES_BLOCK_SIZE_IN_BYTES,
                                     (uint8_t *) & rpBuffer[2], p2BSize,
                                     &symKey[aes_off]);
        return_if_error(r, "Decryption error");

    } else if (symDef->algorithm == TPM2_ALG_XOR) {

        /* Parameter decryption with XOR obfuscation */
        r = iesys_xor_parameter_obfuscation(rsrc_session->authHash,
                                            &rsrc_session->sessionValue[0],
                                            rsrc_session->sizeSessionValue,
                                            &rsrc_session->nonceTPM,
                                            &rsrc_session->nonceCaller,
                                            (uint8_t *) & rpBuffer[2],
                                            p2BSize);
        return_if_error(r, "XOR obfuscation not possible.");

    } else {
        return_error(TSS2_ESYS_RC_BAD_VALUE,
                     "Invalid symmetric algorithm (should be XOR or AES)");
    }
    return TSS2_RC_SUCCESS;
}

/** Check the HMAC values of the response for all sessions.
 *
 * The HMAC values are computed based on the session secrets, the used nonces,
 * the session attributes, the response hash.
 * @param[in] esys_context The ESYS_CONTEXT.
 * @param[in] rspAuths The list of the session auth values.
 * @param[in] rp_hashtab  The list of response hashes.
 * @param[in] rpHashNum The number of response hashes.
 * @retval TSS2_RC_SUCCESS on success.
 * @retval TSS2_ESYS_RC_MEMORY Memory can not be allocated.
 * @retval TSS2_ESYS_RC_BAD_VALUE for invalid parameters.
 * @retval TSS2_ESYS_RC_GENERAL_FAILURE for errors of the crypto library.
 * @retval TSS2_ESYS_RC_NOT_IMPLEMENTED if hash algorithm is not implemented.
 */
TSS2_RC
iesys_check_rp_hmacs(ESYS_CONTEXT * esys_context,
                     TSS2L_SYS_AUTH_RESPONSE * rspAuths,
                     HASH_TAB_ITEM rp_hash_tab[3],
                     uint8_t rpHashNum)
{
    _ESYS_ASSERT_NON_NULL(esys_context);
    _ESYS_ASSERT_NON_NULL(rspAuths);
    _ESYS_ASSERT_NON_NULL(rp_hash_tab);

    TSS2_RC r;
    for (int i = 0; i < rspAuths->count; i++) {
        RSRC_NODE_T *session = esys_context->session_tab[i];
        if (session == NULL)
            continue;

        IESYS_SESSION *rsrc_session = &session->rsrc.misc.rsrc_session;
        if (rsrc_session->type_policy_session == POLICY_PASSWORD) {
            /* A policy password session has no auth value */
            if (rspAuths->auths[i].hmac.size != 0) {
                LOG_ERROR("PolicyPassword session's HMAC must be 0-length.");
                return TSS2_ESYS_RC_RSP_AUTH_FAILED;
            }
            continue;
        }

        /* Find the rpHash for the hash algorithm used by this session */
        int hi;
        for (hi = 0; hi < rpHashNum; hi++) {
            if (rsrc_session->authHash == rp_hash_tab[hi].alg) {
                break;
            }
        }
        if (hi == rpHashNum) {
            LOG_ERROR("rpHash for alg %"PRIx16 " not found.",
                      rsrc_session->authHash);
            return TSS2_ESYS_RC_GENERAL_FAILURE;
        }

        TPM2B_AUTH rp_hmac;
        rp_hmac.size = sizeof(TPMU_HA);
        rsrc_session->nonceTPM = rspAuths->auths[i].nonce;
        rsrc_session->sessionAttributes =
            rspAuths->auths[i].sessionAttributes;
        r = iesys_crypto_authHmac(rsrc_session->authHash,
                                  &rsrc_session->sessionValue[0],
                                  rsrc_session->sizeHmacValue,
                                  &rp_hash_tab[hi].digest[0],
                                  rp_hash_tab[hi].size,
                                  &rsrc_session->nonceTPM,
                                  &rsrc_session->nonceCaller, NULL, NULL,
                                  rspAuths->auths[i].sessionAttributes,
                                  &rp_hmac);
        return_if_error(r, "HMAC error");

        if (!cmp_TPM2B_AUTH(&rspAuths->auths[i].hmac, &rp_hmac)) {
            LOG_ERROR("TPM's response auth is invalid for session %i", i);
            return TSS2_ESYS_RC_RSP_AUTH_FAILED;
        }
    }
    return TSS2_RC_SUCCESS;
}
/** Compute the value for check of bind authorization.
 *
 * This value has to be computed from the bind object in the StartAuthSession
 * command and later checked in for corresponding object authorizations.
 * @param[in] name The name of the bind object.
 * @param[in] auth The authorization of the bind object.
 * @param[out] bound_entity The value used for checking the bind authorization.
 */
void
iesys_compute_bound_entity(const TPM2B_NAME * name,
                           const TPM2B_AUTH * auth, TPM2B_NAME * bound_entity)
{
    if (!name || !bound_entity || !auth) return;

    UINT16 i;
    UINT16 j = 0;
    *bound_entity = *name;

    //the max index to access bound_entity->name is bound_entity->size - 1.
    size_t consumed_size = bound_entity->size;
    size_t name_size = sizeof(bound_entity->name);
    if (consumed_size >= name_size) {
        return;
    }
    size_t left_size = name_size - consumed_size;
    memset(&bound_entity->name[consumed_size], 0, left_size);

    for (i = sizeof(bound_entity->name) - auth->size;
         i < sizeof(bound_entity->name); i++)
        bound_entity->name[i] ^= auth->buffer[j++];
    bound_entity->size = sizeof(bound_entity->name);
}

/** Predicate whether the authorization is for the object bound to the session.
 *
 * @param[in] name The name of the object.
 * @param[in] auth The auth value of the object.
 * @param[in] sesssion The session to be checked.
 * @retval true if object is bind object of session.
 * @retval false if not.
 */
bool
iesys_is_object_bound(const TPM2B_NAME * name,
                      const TPM2B_AUTH * auth, RSRC_NODE_T * session)
{
    if (!name || !session || ! auth) return false;

    TPM2B_NAME tmp = {0};
    if (session->rsrc.misc.rsrc_session.bound_entity.size == 0)
        /* No bind session */
        return false;
    iesys_compute_bound_entity(name, auth, &tmp);
    return cmp_TPM2B_NAME(&session->rsrc.misc.rsrc_session.bound_entity, &tmp);
}

/**
 * Compute the session value
 *
 * This function derives the session value from the session key
 * and the auth value. The auth value is appended to the session key.
 * The session value is used for key derivation for parameter encryption and
 * HMAC computation. There is one exception for HMAC key derivation: If the
 * session is bound to an object only the session key is used. The auth value
 * is appended only for the key used for parameter encryption.
 * The auth value is only used if an authorization is necessary and the name
 * of the object is not equal to the name of an used bound entity
 * @param[in,out] session for which the session value will be computed.
 *       The value will be stored in sessionValue of the session object.
 *       The length of the object will be stored in sizeHmacValue and
 *       sizeSessionValue respectively to the purpose of usage (HMAC computation
 *       or parameter encryption).
 * @param[in] name name of the object to be authorized (NULL if no authorization)
 * @param[in] auth_value auth value of the object to be authorized
 *             (NULL if no authorization)
 */
void
iesys_compute_session_value(RSRC_NODE_T * session,
                            const TPM2B_NAME * name,
                            const TPM2B_AUTH * auth_value)
{
    if (session == NULL)
        return;

    /* First the session Key is copied into the sessionValue */
    session->rsrc.misc.rsrc_session.sizeSessionValue
        = session->rsrc.misc.rsrc_session.sessionKey.size;
    memcpy(&session->rsrc.misc.rsrc_session.sessionValue[0],
           &session->rsrc.misc.rsrc_session.sessionKey.buffer[0],
           session->rsrc.misc.rsrc_session.sessionKey.size);

    if (name == NULL)
        return;
    /* This requires an HMAC Session and not a password session */
    if (session->rsrc.misc.rsrc_session.sessionType != TPM2_SE_HMAC &&
        session->rsrc.misc.rsrc_session.sessionType != TPM2_SE_POLICY)
        return;

    session->rsrc.misc.rsrc_session.sizeHmacValue = session->rsrc.misc.rsrc_session.sizeSessionValue;

    if (auth_value == NULL) {
        return;
    }

    if (auth_value->size > sizeof(session->rsrc.misc.rsrc_session.sessionValue) - session->rsrc.misc.rsrc_session.sessionKey.size) {
        // if auth_value->size is super large, it can cause overflow by memcpy and integer overflow by +=. 
        return;
    }

    /* The auth value is appended to the session key */
    memcpy(&session->rsrc.misc.rsrc_session.
           sessionValue[session->rsrc.misc.rsrc_session.sessionKey.size],
           &auth_value->buffer[0], auth_value->size); 
    session->rsrc.misc.rsrc_session.sizeSessionValue += auth_value->size;  

    /* Then if we are a bound session, the auth value is not appended to the end
       of the session value for HMAC computation. The size of the key will not be
       increased.*/
    if (iesys_is_object_bound(name, auth_value,
                              session) &&
        /* type_policy_session set to POLICY_AUTH by command PolicyAuthValue */
        (session->rsrc.misc.rsrc_session.type_policy_session != POLICY_AUTH))
        return;
    session->rsrc.misc.rsrc_session.sizeHmacValue += auth_value->size; 
}

/**
 * Lookup the object to a handle from inside the context.
 *
 * This function searches the esapi context for an object that corresponds to a
 * provided esys_handle. These objects contain information such as the
 * appropriate tpm handle, the public name or the stored auth values.
 * These esys handles refer either to an object previously initialized on the
 * same context, in which case this will be returned. Or they refer to a
 * "global", in which case the corresponding object will be created if it does
 * not exist yet.
 * @param[in,out] esys_context The esys context to issue the command on.
 * @param[in] esys_handle The handle to find the corresponding object for.
 * @param[out] esys_object The object containing the name, tpm handle and auth value
 * @retval TSS2_RC_SUCCESS on success.
 * @retval TSS2_ESYS_RC_BAD_TR if the handle is invalid.
 * @retval TSS2_ESYS_RC_BAD_VALUE if an unknown handle < ESYS_TR_MIN_OBJECT is
 *         passed.
 */
TSS2_RC
esys_GetResourceObject(ESYS_CONTEXT * esys_context,
                       ESYS_TR esys_handle, RSRC_NODE_T ** esys_object)
{
    RSRC_NODE_T *esys_object_aux = NULL;
    TPM2_HANDLE tpm_handle;
    size_t offset = 0;
    TSS2_RC r;

    _ESYS_ASSERT_NON_NULL(esys_context);
    _ESYS_ASSERT_NON_NULL(esys_object);

    /* Sometimes the TPM API allows for optional objects. In those cases we map
       the object node to NULL. This will be handled accordingly by following
       code */
    if (esys_handle == ESYS_TR_NONE) {
        *esys_object = NULL;
        return TSS2_RC_SUCCESS;
    }

    /* The typical case is that we have a resource object already within the
       esys context's linked list. We iterate through the list and search
       for the corresponding object and return it if found.
       If no object is found, this can be an erroneous handle number or it
       can be because of a reference "global" object that does not require
       previous initialization. */
    for (esys_object_aux = esys_context->rsrc_list; esys_object_aux != NULL;
         esys_object_aux = esys_object_aux->next) {
        if (esys_object_aux->esys_handle == esys_handle) {
            *esys_object = esys_object_aux;
            return TPM2_RC_SUCCESS;
        }
    }

    /* All objects with a TR-handle larger than ESYS_TR_MIN_OBJECT must have
       been initialized previously. Therefore the TR handle was erroneous. */
    if (esys_handle >= ESYS_TR_MIN_OBJECT) {
        LOG_ERROR("Error: Esys handle does not exist (%x).",
                  TSS2_ESYS_RC_BAD_TR);
        return TSS2_ESYS_RC_BAD_TR;
    }

    /* There are special "global" object for the TPM, such as PCRs or
       hierarchies. If they do not exist yet inside the Esys context we create
       them here and return the newly created object. */
    r = iesys_handle_to_tpm_handle(esys_handle, &tpm_handle);
    return_if_error(r, "Unknown ESYS handle.");

    r = esys_CreateResourceObject(esys_context, esys_handle, &esys_object_aux);
    return_if_error(r, "Creating Resource Object.");

    esys_object_aux->rsrc.handle = tpm_handle;
    esys_object_aux->rsrc.rsrcType = IESYSC_WITHOUT_MISC_RSRC;

    r = Tss2_MU_TPM2_HANDLE_Marshal(tpm_handle,
                                &esys_object_aux->rsrc.name.name[0],
                                sizeof(esys_object_aux->rsrc.name.name),
                                &offset);
    return_if_error(r, "Marshaling TPM handle.");

    esys_object_aux->rsrc.name.size = offset;
    *esys_object = esys_object_aux;
    return TSS2_RC_SUCCESS;
}

/**
 * Check that the esys context is ready for an _async call.
 *
 * This function will check that the sequence of invocations to the esys context
 * was such that an _async function can be called. This means that the internal
 * @state field is either @_ESYS_STATE_INIT, @_ESYS_STATE_ERRORRESPONSE,
 * @_ESYS_STATE_FINISHED.
 * @param[in,out] esys_context The esys context to issue the command on.
 * @retval TSS2_RC_SUCCESS on success.
 * @retval TSS2_RC_BAD_SEQUENCE if context is not ready for this function.
 */
TSS2_RC
iesys_check_sequence_async(ESYS_CONTEXT * esys_context)
{
    if (esys_context == NULL) {
        LOG_ERROR("esyscontext is NULL.");
        return TSS2_ESYS_RC_BAD_REFERENCE;
    }

    if (esys_context->state != _ESYS_STATE_INIT &&
        esys_context->state != _ESYS_STATE_RESUBMISSION) {
        LOG_ERROR("Esys called in bad sequence.");
        return TSS2_ESYS_RC_BAD_SEQUENCE;
    }
    /* TODO: Check if RESUBMISSION BELONGS HERE OR RATHER INTO THE FINISH METHOD. */
    if (esys_context->state == _ESYS_STATE_RESUBMISSION) {
        esys_context->submissionCount++;
        LOG_DEBUG("The command will be resubmitted for the %i time.",
                  esys_context->submissionCount);
    } else {
        esys_context->submissionCount = 1;
    }
    return TSS2_RC_SUCCESS;
}

/** Check whether session without authorization occurs before one with.
 *
 * @param[in] session1-3 The three sessions.
 * @retval TPM2_RC_SUCCESS if the order is ok.
 * @retval TSS2_ESYS_RC_BAD_VALUE if not.
 */
TSS2_RC
check_session_feasibility(ESYS_TR shandle1, ESYS_TR shandle2, ESYS_TR shandle3,
                          int mandatory)
{
    ESYS_TR handle_tab[3] = { shandle1, shandle2, shandle3 };
    bool check_none = false;
    for (int i = 2; i >= 0; i--) {
        if (handle_tab[i] != ESYS_TR_NONE) {
            if (mandatory == INT_MIN) 
                return TSS2_ESYS_RC_BAD_VALUE;
            mandatory--;
        }
        if (handle_tab[i] != ESYS_TR_NONE && handle_tab[i] != ESYS_TR_PASSWORD)
            check_none = true;
        else {
            if (check_none) {
                if (handle_tab[i] == ESYS_TR_NONE) {
                    LOG_ERROR("Error: ESYS_TR_NONE used before other handle.");
                    return TSS2_ESYS_RC_BAD_VALUE;
                }
            }
        }
    }
    if (mandatory > 0) {
        LOG_ERROR("Not enough sessions provided for the command.");
        return TSS2_ESYS_RC_BAD_VALUE;
    }
    return TPM2_RC_SUCCESS;
}

/** Compute HMAC for a session.
 *
 * The HMAC is computed from the appropriate cp hash, the caller nonce, the TPM
 * nonce and the session attributes. If an encrypt session is not the first
 * session also the encrypt and the decrypt nonce have to be included.
 * @param[in] session The session for which the HMAC has to be computed.
 * @param[in] cp_hash_tab The table of computed cp hash values.
 * @param[in] cpHashNum The number of computed cp hash values which depens on
 *            the number of used hash algorithms.
 * @param[in] encryptNonce The encrypt Nonce of an encryption session. Has to
 *            be NULL if encryption session is first session.
 * @param[in] decryptNonce The decrypt Nonce of an encryption session. Has to
 *            be NULL if encryption session is first session.
 * @param[out] auth The computed HMAC value.
 * @retval TSS2_RC_SUCCESS on success.
 * @retval TSS2_ESYS_RC_MEMORY Memory can not be allocated.
 * @retval TSS2_ESYS_RC_BAD_VALUE for invalid parameters.
 * @retval TSS2_ESYS_RC_BAD_REFERENCE for unexpected NULL pointer parameters.
 * @retval TSS2_ESYS_RC_GENERAL_FAILURE for errors of the crypto library.
 * @retval TSS2_ESYS_RC_NOT_IMPLEMENTED if hash algorithm is not implemented.
 * @retval TSS2_SYS_RC_* for SAPI errors.
 */
TSS2_RC
iesys_compute_hmac(RSRC_NODE_T * session,
                   HASH_TAB_ITEM cp_hash_tab[3],
                   uint8_t cpHashNum,
                   TPM2B_NONCE * decryptNonce,
                   TPM2B_NONCE * encryptNonce,
                   TPMS_AUTH_COMMAND * auth)
{
    TSS2_RC r;
    size_t authHash_size = 0;

    if (session != NULL) {
        _ESYS_ASSERT_NON_NULL(auth);
        _ESYS_ASSERT_NON_NULL(cp_hash_tab);

        IESYS_SESSION *rsrc_session = &session->rsrc.misc.rsrc_session;
        r = iesys_crypto_hash_get_digest_size(rsrc_session->
                                              authHash, &authHash_size);
        return_if_error(r, "Initializing auth session");

        int hi = 0;
        for (int j = 0; j < cpHashNum; j++) {
            if (rsrc_session->authHash == cp_hash_tab[j].alg) {
                hi = j;
                break;
            }
        }
        auth->hmac.size = sizeof(TPMU_HA);
        /* if other than first session is used for for parameter encryption
           the corresponding nonces have to be included into the hmac
           computation of the first session */
        r = iesys_crypto_authHmac(rsrc_session->authHash,
                                  &rsrc_session->sessionValue[0],
                                  rsrc_session->sizeHmacValue,
                                  &cp_hash_tab[hi].digest[0],
                                  cp_hash_tab[hi].size,
                                  &rsrc_session->nonceCaller,
                                  &rsrc_session->nonceTPM,
                                  decryptNonce, encryptNonce,
                                  rsrc_session->sessionAttributes, &auth->hmac);
        return_if_error(r, "HMAC error");
        auth->sessionHandle = session->rsrc.handle;
        auth->nonce = rsrc_session->nonceCaller;
        auth->sessionAttributes =
            rsrc_session->sessionAttributes;
    }
    return TSS2_RC_SUCCESS;
}

/** Compute the auth values (HMACs) for all sessions.
 *
 * The caller nonce, the encrypt nonces, the cp hashes, and the HMAC values for
 * the command authorization are computed.
 * @param[in] esys_context The esys context to issue the command on.
 * @param[in] h1-3 The esys session resource objects.
 * @param[out] The list if the authorizations with teh computed HMACs.
 * @param[out] auth The computed HMAC value.
 * @retval TSS2_RC_SUCCESS on success.
 * @retval TSS2_ESYS_RC_MEMORY Memory can not be allocated.
 * @retval TSS2_ESYS_RC_BAD_VALUE for invalid parameters.
 * @retval TSS2_ESYS_RC_BAD_REFERENCE for unexpected NULL pointer parameters.
 * @retval TSS2_ESYS_RC_GENERAL_FAILURE for errors of the crypto library.
 * @retval TSS2_ESYS_RC_NOT_IMPLEMENTED if hash algorithm is not implemented.
 * @retval TSS2_SYS_RC_* for SAPI errors.
 */
TSS2_RC
iesys_gen_auths(ESYS_CONTEXT * esys_context,
                RSRC_NODE_T * h1,
                RSRC_NODE_T * h2,
                RSRC_NODE_T * h3,
                TSS2L_SYS_AUTH_COMMAND * auths)
{
    TSS2_RC r;
    TPM2B_NONCE *decryptNonce = NULL;
    int decryptNonceIdx = 0;
    int encryptNonceIdx = 0;
    TPM2B_NONCE *encryptNonce = NULL;

    RSRC_NODE_T *objects[] = { h1, h2, h3 };

    HASH_TAB_ITEM cp_hash_tab[3];
    uint8_t cpHashNum = 0;

    _ESYS_ASSERT_NON_NULL(esys_context);
    _ESYS_ASSERT_NON_NULL(auths);
    auths->count = 0;
    memset(auths->auths, 0, 3*sizeof(TPMS_AUTH_COMMAND));


    r = iesys_gen_caller_nonces(esys_context);
    return_if_error(r, "Error nonce generation caller");
    r = iesys_encrypt_param(esys_context, &decryptNonce, &decryptNonceIdx);
    return_if_error(r, "Error parameter encryption");
    r = iesys_compute_encrypt_nonce(esys_context, &encryptNonceIdx,
                                    &encryptNonce);
    return_if_error(r, "More than one crypt session");

    /* Compute cp hash values for command buffer for all used algorithms */

    r = iesys_compute_cp_hashtab(esys_context,
                                 (h1 != NULL) ? &h1->rsrc.name : NULL,
                                 (h2 != NULL) ? &h2->rsrc.name : NULL,
                                 (h3 != NULL) ? &h3->rsrc.name : NULL,
                                 &cp_hash_tab[0], &cpHashNum);
    return_if_error(r, "Error while computing cp hashes");

    for (int session_idx = 0; session_idx < 3; session_idx++) {
        auths->auths[auths->count].nonce.size = 0;
        auths->auths[auths->count].sessionAttributes = 0;
        if (esys_context->session_type[session_idx] == ESYS_TR_PASSWORD) {
            if (objects[session_idx] == NULL) {
                auths->auths[auths->count].hmac.size = 0;
                auths->count += 1;
            } else {
                auths->auths[auths->count].sessionHandle = TPM2_RS_PW;
                auths->auths[auths->count].hmac = objects[session_idx]->auth;
                auths->count += 1;
            }
            continue;
        }
        RSRC_NODE_T *session = esys_context->session_tab[session_idx];
        if (session != NULL) {
            IESYS_SESSION *rsrc_session = &session->rsrc.misc.rsrc_session;
            if (rsrc_session->type_policy_session == POLICY_PASSWORD) {
                auths->auths[auths->count].sessionHandle = session->rsrc.handle;
                if (objects[session_idx] == NULL) {
                    auths->auths[auths->count].hmac.size = 0;
                    auths->count += 1;
                } else {
                    auths->auths[auths->count].hmac = objects[session_idx]->auth;
                    auths->count += 1;
                }
                continue;
            }
        }
        r = iesys_compute_hmac(esys_context->session_tab[session_idx],
                               &cp_hash_tab[0], cpHashNum,
                               (session_idx == 0
                                && decryptNonceIdx > 0) ? decryptNonce : NULL,
                               (session_idx == 0
                                && encryptNonceIdx > 0) ? encryptNonce : NULL,
                               &auths->auths[session_idx]);
        return_if_error(r, "Error while computing hmacs");
        if (esys_context->session_tab[session_idx] != NULL && session != NULL) {
            auths->auths[auths->count].sessionHandle = session->rsrc.handle;
            auths->count++;
        }
    }

    esys_context->encryptNonceIdx = encryptNonceIdx;
    esys_context->encryptNonce = encryptNonce;

    return TSS2_RC_SUCCESS;
}

/** Check the response HMACs for all sessions.
 *
 * The response HMAC values are computed. Based on these values the HMACs for
 * all sessions are computed and compared with the HMACs stored in the response
 * auth list which is determined with the SAPI function Tss2_Sys_GetRspAuths.
 * @param[in] esys_context The esys context which is used to get the response
 * auth values and the sessions.
 * @retval TSS2_RC_SUCCESS on success.
 * @retval TSS2_ESYS_RC_MEMORY Memory can not be allocated.
 * @retval TSS2_ESYS_RC_BAD_VALUE for invalid parameters.
 * @retval TSS2_ESYS_RC_BAD_REFERENCE for unexpected NULL pointer parameters.
 * @retval TSS2_ESYS_RC_GENERAL_FAILURE for errors of the crypto library.
 * @retval TSS2_ESYS_RC_NOT_IMPLEMENTED if hash algorithm is not implemented.
 * @retval TSS2_SYS_RC_* for SAPI errors.
 */
TSS2_RC
iesys_check_response(ESYS_CONTEXT * esys_context)
{
    TSS2_RC r;
    const uint8_t *rpBuffer;
    size_t rpBuffer_size;
    TSS2L_SYS_AUTH_RESPONSE rspAuths;
    HASH_TAB_ITEM rp_hash_tab[3];
    uint8_t rpHashNum = 0;

    _ESYS_ASSERT_NON_NULL(esys_context);

    if (esys_context->authsCount == 0) {
        LOG_TRACE("No auths to verify");
        return TSS2_RC_SUCCESS;
    }

    r = Tss2_Sys_GetRspAuths(esys_context->sys, &rspAuths);
    return_if_error(r, "Error: GetRspAuths");

    if (rspAuths.count != esys_context->authsCount) {
        LOG_ERROR("Number of response auths differs: %i (expected %i)",
                  rspAuths.count, esys_context->authsCount);
        return TSS2_ESYS_RC_GENERAL_FAILURE;
    }
    /*
     * At least one session object is defined so the rp hashes must be computed
     * and the HMACs of the responses have to be checked.
     * Encrypted response parameters will be decrypted.
     */
    if (esys_context->session_type[0] >= ESYS_TR_MIN_OBJECT ||
        esys_context->session_type[1] >= ESYS_TR_MIN_OBJECT ||
        esys_context->session_type[2] >= ESYS_TR_MIN_OBJECT) {
        r = Tss2_Sys_GetRpBuffer(esys_context->sys, &rpBuffer_size, &rpBuffer);
        return_if_error(r, "Error: get rp buffer");

        r = iesys_compute_rp_hashtab(esys_context,
                                     rpBuffer, rpBuffer_size,
                                     &rp_hash_tab[0], &rpHashNum);
        return_if_error(r, "Error: while computing response hashes");

        r = iesys_check_rp_hmacs(esys_context, &rspAuths, &rp_hash_tab[0],
                                 rpHashNum);
        return_if_error(r, "Error: response hmac check");

        if (esys_context->encryptNonce != NULL) {
            r = iesys_decrypt_param(esys_context, rpBuffer, rpBuffer_size);
            return_if_error(r, "Error: while decrypting parameter.");
        }
    }
    return TSS2_RC_SUCCESS;
}

/** Compute the name from the public data of a NV index.
 *
 * The name of a NV index is computed as follows:
 *   name =  nameAlg||Hash(nameAlg,marshal(publicArea))
 * @param[in] publicInfo The public information of the NV index.
 * @param[out] name The computed name.
 * @retval TSS2_RC_SUCCESS on success.
 * @retval TSS2_ESYS_RC_MEMORY Memory can not be allocated.
 * @retval TSS2_ESYS_RC_BAD_VALUE for invalid parameters.
 * @retval TSS2_ESYS_RC_BAD_REFERENCE for unexpected NULL pointer parameters.
 * @retval TSS2_ESYS_RC_GENERAL_FAILURE for errors of the crypto library.
 * @retval TSS2_ESYS_RC_NOT_IMPLEMENTED if hash algorithm is not implemented.
 * @retval TSS2_SYS_RC_* for SAPI errors.
 */
TSS2_RC
iesys_nv_get_name(TPM2B_NV_PUBLIC * publicInfo, TPM2B_NAME * name)
{
    BYTE buffer[sizeof(TPMS_NV_PUBLIC)];
    size_t offset = 0;
    size_t size = sizeof(TPMU_NAME) - sizeof(TPMI_ALG_HASH);
    size_t len_alg_id = sizeof(TPMI_ALG_HASH);
    IESYS_CRYPTO_CONTEXT_BLOB *cryptoContext;
    
    _ESYS_ASSERT_NON_NULL(publicInfo);
    _ESYS_ASSERT_NON_NULL(name);

    if (publicInfo->nvPublic.nameAlg == TPM2_ALG_NULL) {
        name->size = 0;
        return TSS2_RC_SUCCESS;
    }
    TSS2_RC r;
    r = iesys_crypto_hash_start(&cryptoContext, publicInfo->nvPublic.nameAlg);
    return_if_error(r, "Crypto hash start");

    r = Tss2_MU_TPMS_NV_PUBLIC_Marshal(&publicInfo->nvPublic,
                                       &buffer[0], sizeof(TPMS_NV_PUBLIC),
                                       &offset);
    if (r != TSS2_RC_SUCCESS) {
        SAFE_FREE(cryptoContext);
    }

    return_if_error(r, "Marshaling TPMS_NV_PUBLIC");

    r = iesys_crypto_hash_update(cryptoContext, &buffer[0], offset);
    if (r != TSS2_RC_SUCCESS) {
        SAFE_FREE(cryptoContext);
    }

    return_if_error(r, "crypto hash update");

    r = iesys_crypto_hash_finish(&cryptoContext, &name->name[len_alg_id],
                                     &size);
    return_if_error(r, "crypto hash finish");

    offset = 0;
    r = Tss2_MU_TPMI_ALG_HASH_Marshal(publicInfo->nvPublic.nameAlg,
                                  &name->name[0], sizeof(TPMI_ALG_HASH),
                                  &offset);
    return_if_error(r, "Marshaling TPMI_ALG_HASH");

    name->size = size + len_alg_id;
    return TSS2_RC_SUCCESS;
}

/** Compute the name of a TPM transient or persistent object.
 *
 * The name of a NV index is computed as follows:
 *   name = Hash(nameAlg,marshal(publicArea))
 * @param[in] publicInfo The public information of the TPM object.
 * @param[out] name The computed name.
 * @retval TPM2_RC_SUCCESS  or one of the possible errors TSS2_ESYS_RC_BAD_VALUE,
 * TSS2_ESYS_RC_MEMORY, TSS2_ESYS_RC_GENERAL_FAILURE, TSS2_ESYS_RC_NOT_IMPLEMENTED,
 * or return codes of SAPI errors.
 */
TSS2_RC
iesys_get_name(TPM2B_PUBLIC * publicInfo, TPM2B_NAME * name)
{
    BYTE buffer[sizeof(TPMT_PUBLIC)];
    size_t offset = 0;
    size_t len_alg_id = sizeof(TPMI_ALG_HASH);
    size_t size = sizeof(TPMU_NAME) - sizeof(TPMI_ALG_HASH);
    IESYS_CRYPTO_CONTEXT_BLOB *cryptoContext;

    _ESYS_ASSERT_NON_NULL(publicInfo);
    _ESYS_ASSERT_NON_NULL(name);

    if (publicInfo->publicArea.nameAlg == TPM2_ALG_NULL) {
        name->size = 0;
        return TSS2_RC_SUCCESS;
    }
    TSS2_RC r;
    r = iesys_crypto_hash_start(&cryptoContext, publicInfo->publicArea.nameAlg);
    return_if_error(r, "crypto hash start");

    r = Tss2_MU_TPMT_PUBLIC_Marshal(&publicInfo->publicArea,
                                    &buffer[0], sizeof(TPMT_PUBLIC), &offset);
    if (r != TSS2_RC_SUCCESS) {
        SAFE_FREE(cryptoContext);
    }

    return_if_error(r, "Marshaling TPMT_PUBLIC");

    r = iesys_crypto_hash_update(cryptoContext, &buffer[0], offset);
    if (r != TSS2_RC_SUCCESS) {
        SAFE_FREE(cryptoContext);
    }

    return_if_error(r, "crypto hash update");

    r = iesys_crypto_hash_finish(&cryptoContext, &name->name[len_alg_id],
                                     &size);
    return_if_error(r, "crypto hash finish");

    offset = 0;
    r = Tss2_MU_TPMI_ALG_HASH_Marshal(publicInfo->publicArea.nameAlg,
                                  &name->name[0], sizeof(TPMI_ALG_HASH),
                                  &offset);
    return_if_error(r, "Marshaling TPMI_ALG_HASH");

    name->size = size + len_alg_id;
    return TSS2_RC_SUCCESS;
}

/** Check whether the return code corresponds to an TPM error.
 *
 * if no layer is part of the return code or a layer from the resource manager
 * is given the function will return true.
 * @param[in] r The return code to be checked.
 * @retval true if r corresponds to an TPM error.
 * @retval false in other cases.
 */
bool
iesys_tpm_error(TSS2_RC r)
{
    return (r != TSS2_RC_SUCCESS &&
            ((r & TSS2_RC_LAYER_MASK) == 0 ||
             (r & TSS2_RC_LAYER_MASK) == TSS2_RESMGR_TPM_RC_LAYER ||
             (r & TSS2_RC_LAYER_MASK) == TSS2_RESMGR_RC_LAYER));
}
