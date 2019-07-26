/* SPDX-License-Identifier: BSD-2 */
/***********************************************************************
 * Copyright (c) 2017-2018, Intel Corporation
 *
 * All rights reserved.
 ***********************************************************************/
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/opensslv.h>

#define LOGMODULE testintegration
#include "util/log.h"
#include "sapi-util.h"
#include "test.h"
/*
 * Use te provide SAPI context to create & load a primary key. The key will
 * be a 2048 bit (restricted decryption) RSA key. The associated symmetric
 * key is a 128 bit AES (CFB mode) key.
 */
TSS2_RC
create_primary_rsa_2048_aes_128_cfb (
    TSS2_SYS_CONTEXT *sapi_context,
    TPM2_HANDLE       *handle)
{
    TSS2_RC                 rc              = TSS2_RC_SUCCESS;
    TPM2B_SENSITIVE_CREATE  in_sensitive    = { 0 };
    TPM2B_PUBLIC            in_public       = { 0 };
    TPM2B_DATA              outside_info    = { 0 };
    TPML_PCR_SELECTION      creation_pcr    = { 0 };
    TPM2B_PUBLIC            out_public      = { 0 };
    TPM2B_CREATION_DATA     creation_data   = { 0 };
    TPM2B_DIGEST            creation_hash   = TPM2B_DIGEST_INIT;
    TPMT_TK_CREATION        creation_ticket = { 0 };
    TPM2B_NAME              name            = TPM2B_NAME_INIT;
    /* session parameters */
    /* command session info */
    TSS2L_SYS_AUTH_COMMAND  sessions_cmd = {
        .auths = {{ .sessionHandle = TPM2_RS_PW }},
        .count = 1
    };
    /* response session info */
    TSS2L_SYS_AUTH_RESPONSE  sessions_rsp     = {
        .auths = { 0 },
        .count = 0
    };

    if (sapi_context == NULL || handle == NULL) {
        return TSS2_RC_LAYER_MASK | TSS2_BASE_RC_BAD_REFERENCE;
    }
    in_public.publicArea.type = TPM2_ALG_RSA;
    in_public.publicArea.nameAlg = TPM2_ALG_SHA256;
    in_public.publicArea.objectAttributes |= TPMA_OBJECT_RESTRICTED;
    in_public.publicArea.objectAttributes |= TPMA_OBJECT_USERWITHAUTH;
    in_public.publicArea.objectAttributes |= TPMA_OBJECT_DECRYPT;
    in_public.publicArea.objectAttributes |= TPMA_OBJECT_FIXEDTPM;
    in_public.publicArea.objectAttributes |= TPMA_OBJECT_FIXEDPARENT;
    in_public.publicArea.objectAttributes |= TPMA_OBJECT_SENSITIVEDATAORIGIN;
    in_public.publicArea.parameters.rsaDetail.symmetric.algorithm = TPM2_ALG_AES;
    in_public.publicArea.parameters.rsaDetail.symmetric.keyBits.aes = 128;
    in_public.publicArea.parameters.rsaDetail.symmetric.mode.aes = TPM2_ALG_CFB;
    in_public.publicArea.parameters.rsaDetail.scheme.scheme = TPM2_ALG_NULL;
    in_public.publicArea.parameters.rsaDetail.keyBits = 2048;

    LOG_INFO("CreatePrimary RSA 2048, AES 128 CFB");
    rc = Tss2_Sys_CreatePrimary (sapi_context,
                                 TPM2_RH_OWNER,
                                 &sessions_cmd,
                                 &in_sensitive,
                                 &in_public,
                                 &outside_info,
                                 &creation_pcr,
                                 handle,
                                 &out_public,
                                 &creation_data,
                                 &creation_hash,
                                 &creation_ticket,
                                 &name,
                                 &sessions_rsp);
    if (rc == TPM2_RC_SUCCESS) {
        LOG_INFO("success");
    } else {
        LOG_ERROR("CreatePrimary FAILED! Response Code : 0x%x", rc);
        exit(1);
    }

    return rc;
}

TSS2_RC
create_aes_128_cfb (
    TSS2_SYS_CONTEXT *sapi_context,
    TPM2_HANDLE        handle_parent,
    TPM2_HANDLE       *handle)
{
    TSS2_RC                 rc              = TSS2_RC_SUCCESS;
    TPM2B_SENSITIVE_CREATE  in_sensitive    = { 0 };
    /* template defining key type */
    TPM2B_PUBLIC            in_public       = {
            .publicArea.type = TPM2_ALG_SYMCIPHER,
            .publicArea.nameAlg = TPM2_ALG_SHA256,
            .publicArea.objectAttributes = TPMA_OBJECT_DECRYPT |
                                           TPMA_OBJECT_FIXEDTPM |
                                           TPMA_OBJECT_FIXEDPARENT |
                                           TPMA_OBJECT_SENSITIVEDATAORIGIN |
                                           TPMA_OBJECT_SIGN_ENCRYPT |
                                           TPMA_OBJECT_USERWITHAUTH,
            .publicArea.parameters.symDetail.sym = {
                .algorithm = TPM2_ALG_AES,
                .keyBits.sym = 128,
                .mode.sym = TPM2_ALG_CFB,
            },
    };

    TPM2B_DATA              outside_info    = { 0 };
    TPML_PCR_SELECTION      creation_pcr    = { 0 };
    TPM2B_PRIVATE           out_private     = TPM2B_PRIVATE_INIT;
    TPM2B_PUBLIC            out_public      = { 0 };
    TPM2B_CREATION_DATA     creation_data   = { 0 };
    TPM2B_DIGEST            creation_hash   = TPM2B_DIGEST_INIT;
    TPMT_TK_CREATION        creation_ticket = { 0 };
    TPM2B_NAME              name            = TPM2B_NAME_INIT;
    /* session parameters */
    /* command session info */
    TSS2L_SYS_AUTH_COMMAND  sessions_cmd = {
        .auths = {{ .sessionHandle = TPM2_RS_PW }},
        .count = 1
    };
    /* response session info */
    TSS2L_SYS_AUTH_RESPONSE  sessions_rsp     = {
        .auths = { 0 },
        .count = 0
    };

    rc = TSS2_RETRY_EXP (Tss2_Sys_Create (sapi_context,
                                          handle_parent,
                                          &sessions_cmd,
                                          &in_sensitive,
                                          &in_public,
                                          &outside_info,
                                          &creation_pcr,
                                          &out_private,
                                          &out_public,
                                          &creation_data,
                                          &creation_hash,
                                          &creation_ticket,
                                          &sessions_rsp));
    if (rc != TPM2_RC_SUCCESS) {
        return rc;
    }

    return Tss2_Sys_Load (sapi_context,
                          handle_parent,
                          &sessions_cmd,
                          &out_private,
                          &out_public,
                          handle,
                          &name,
                          &sessions_rsp);
}

TSS2_RC
create_keyedhash_key (
    TSS2_SYS_CONTEXT *sapi_context,
    TPM2_HANDLE       handle_parent,
    TPM2_HANDLE      *handle)
{
    TSS2_RC                 rc              = TSS2_RC_SUCCESS;
    TPM2B_SENSITIVE_CREATE  in_sensitive    = { 0 };
    /* template defining key type */
    TPM2B_PUBLIC            in_public       = {
            .publicArea.type = TPM2_ALG_KEYEDHASH,
            .publicArea.nameAlg = TPM2_ALG_SHA256,
            .publicArea.objectAttributes = TPMA_OBJECT_RESTRICTED |
                                           TPMA_OBJECT_SIGN_ENCRYPT |
                                           TPMA_OBJECT_FIXEDTPM |
                                           TPMA_OBJECT_FIXEDPARENT |
                                           TPMA_OBJECT_SENSITIVEDATAORIGIN |
                                           TPMA_OBJECT_USERWITHAUTH,
            .publicArea.parameters.keyedHashDetail.scheme.scheme = TPM2_ALG_HMAC,
            .publicArea.parameters.keyedHashDetail.scheme.details.hmac.hashAlg = TPM2_ALG_SHA1,
            .publicArea.unique.keyedHash.size = 0,
    };

    TPM2B_DATA              outside_info    = { 0 };
    TPML_PCR_SELECTION      creation_pcr    = { 0 };
    TPM2B_PRIVATE           out_private     = TPM2B_PRIVATE_INIT;
    TPM2B_PUBLIC            out_public      = { 0 };
    TPM2B_CREATION_DATA     creation_data   = { 0 };
    TPM2B_DIGEST            creation_hash   = TPM2B_DIGEST_INIT;
    TPMT_TK_CREATION        creation_ticket = { 0 };
    TPM2B_NAME              name            = TPM2B_NAME_INIT;
    /* session parameters */
    /* command session info */
    TSS2L_SYS_AUTH_COMMAND  sessions_cmd = {
        .auths = {{ .sessionHandle = TPM2_RS_PW }},
        .count = 1
    };
    /* response session info */
    TSS2L_SYS_AUTH_RESPONSE  sessions_rsp     = {
        .auths = { 0 },
        .count = 0
    };

    rc = TSS2_RETRY_EXP (Tss2_Sys_Create (sapi_context,
                                          handle_parent,
                                          &sessions_cmd,
                                          &in_sensitive,
                                          &in_public,
                                          &outside_info,
                                          &creation_pcr,
                                          &out_private,
                                          &out_public,
                                          &creation_data,
                                          &creation_hash,
                                          &creation_ticket,
                                          &sessions_rsp));
    if (rc != TPM2_RC_SUCCESS) {
        return rc;
    }

    return Tss2_Sys_Load (sapi_context,
                          handle_parent,
                          &sessions_cmd,
                          &out_private,
                          &out_public,
                          handle,
                          &name,
                          &sessions_rsp);
}


TSS2_RC
tpm_encrypt_decrypt_cfb (
    TSS2_SYS_CONTEXT *sapi_context,
    TPMI_DH_OBJECT    handle,
    TPMI_YES_NO       decrypt,
    TPM2B_MAX_BUFFER *data_in,
    TPM2B_MAX_BUFFER *data_out)
{
    TPMI_ALG_SYM_MODE mode = TPM2_ALG_NULL;
    TPM2B_IV iv_in = TPM2B_IV_INIT;
    TPM2B_IV iv_out = TPM2B_IV_INIT;

    /* session parameters */
    /* command session info */
    /* command session info */
    TSS2L_SYS_AUTH_COMMAND  sessions_cmd = {
        .auths = {{ .sessionHandle = TPM2_RS_PW }},
        .count = 1
    };
    /* response session info */
    TSS2L_SYS_AUTH_RESPONSE  sessions_rsp     = {
        .auths = { 0 },
        .count = 0
    };

    return Tss2_Sys_EncryptDecrypt (sapi_context,
                                    handle,
                                    &sessions_cmd,
                                    decrypt,
                                    mode,
                                    &iv_in,
                                    data_in,
                                    data_out,
                                    &iv_out,
                                    &sessions_rsp);
}

TSS2_RC
tpm_decrypt_cfb (
    TSS2_SYS_CONTEXT *sapi_context,
    TPMI_DH_OBJECT    handle,
    TPM2B_MAX_BUFFER *data_in,
    TPM2B_MAX_BUFFER *data_out)
{
    return tpm_encrypt_decrypt_cfb (sapi_context, handle, YES, data_in, data_out);
}

TSS2_RC
tpm_encrypt_cfb (
    TSS2_SYS_CONTEXT *sapi_context,
    TPMI_DH_OBJECT    handle,
    TPM2B_MAX_BUFFER *data_in,
    TPM2B_MAX_BUFFER *data_out)
{
    return tpm_encrypt_decrypt_cfb (sapi_context, handle, NO, data_in, data_out);
}

TSS2_RC
tpm_encrypt_decrypt_2_cfb (
    TSS2_SYS_CONTEXT *sapi_context,
    TPMI_DH_OBJECT    handle,
    TPMI_YES_NO       decrypt,
    TPM2B_MAX_BUFFER *data_in,
    TPM2B_MAX_BUFFER *data_out)
{
    TPMI_ALG_SYM_MODE mode = TPM2_ALG_NULL;
    TPM2B_IV iv_in = TPM2B_IV_INIT;
    TPM2B_IV iv_out = TPM2B_IV_INIT;

    /* session parameters */
    /* command session info */
    /* command session info */
    TSS2L_SYS_AUTH_COMMAND  sessions_cmd = {
        .auths = {{ .sessionHandle = TPM2_RS_PW }},
        .count = 1
    };
    /* response session info */
    TSS2L_SYS_AUTH_RESPONSE  sessions_rsp     = {
        .auths = { 0 },
        .count = 0
    };

    return Tss2_Sys_EncryptDecrypt2 (sapi_context,
                                     handle,
                                     &sessions_cmd,
                                     data_in,
                                     decrypt,
                                     mode,
                                     &iv_in,
                                     data_out,
                                     &iv_out,
                                     &sessions_rsp);
}

TSS2_RC
tpm_decrypt_2_cfb (
    TSS2_SYS_CONTEXT *sapi_context,
    TPMI_DH_OBJECT    handle,
    TPM2B_MAX_BUFFER *data_in,
    TPM2B_MAX_BUFFER *data_out)
{
    return tpm_encrypt_decrypt_2_cfb (sapi_context, handle, YES, data_in, data_out);
}

TSS2_RC
tpm_encrypt_2_cfb (
    TSS2_SYS_CONTEXT *sapi_context,
    TPMI_DH_OBJECT    handle,
    TPM2B_MAX_BUFFER *data_in,
    TPM2B_MAX_BUFFER *data_out)
{
    return tpm_encrypt_decrypt_2_cfb (sapi_context, handle, NO, data_in, data_out);
}

static TSS2_RC
encrypt_decrypt_cfb (
    TPM2B_MAX_BUFFER *data_out,
    TPM2B_MAX_BUFFER *data_in,
    TPMI_YES_NO decrypt,
    TPM2B_MAX_BUFFER *key,
    TPM2B_IV *iv)
{
    EVP_CIPHER_CTX *ctx;
    const EVP_CIPHER *type;
    TSS2_RC rc = TSS2_SYS_RC_BAD_VALUE;
    int len = 0, sll_rc;

    ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return TSS2_SYS_RC_GENERAL_FAILURE;

    switch (key->size) {
        case 16:
            type = EVP_aes_128_cfb();
            break;
        case 24:
            type = EVP_aes_192_cfb();
            break;
        case 32:
            type = EVP_aes_256_cfb();
            break;
        default:
            goto clean;
    }

    rc = TSS2_SYS_RC_GENERAL_FAILURE;

    if (decrypt) {
        sll_rc = EVP_DecryptInit_ex(ctx, type, NULL, key->buffer, iv->buffer);
        if (sll_rc != 1)
            goto clean;

        sll_rc = EVP_DecryptUpdate(ctx, data_out->buffer, &len,
                            data_in->buffer, data_in->size);
        if (sll_rc != 1)
            goto clean;

        data_out->size = len;

        sll_rc = EVP_DecryptFinal_ex(ctx, data_out->buffer + len, &len);
        if (sll_rc != 1)
            goto clean;

    } else {

        sll_rc = EVP_EncryptInit_ex(ctx, type, NULL, key->buffer, iv->buffer);
        if (sll_rc != 1)
            goto clean;

        sll_rc = EVP_EncryptUpdate(ctx, data_out->buffer, &len,
                            data_in->buffer, data_in->size);
        if (sll_rc != 1)
            goto clean;

        data_out->size = len;

        sll_rc = EVP_EncryptFinal_ex(ctx, data_out->buffer + len, &len);
        if (sll_rc != 1)
            goto clean;
    }

    data_out->size += len;
    rc = TPM2_RC_SUCCESS;

clean:
    EVP_CIPHER_CTX_free(ctx);

    return rc;
}

TSS2_RC
decrypt_cfb (
    TPM2B_MAX_BUFFER *data_out,
    TPM2B_MAX_BUFFER *data_in,
    TPM2B_MAX_BUFFER *key,
    TPM2B_IV *iv)
{
    return encrypt_decrypt_cfb(data_out, data_in, YES, key, iv);
}

TSS2_RC
encrypt_cfb (
    TPM2B_MAX_BUFFER *data_out,
    TPM2B_MAX_BUFFER *data_in,
    TPM2B_MAX_BUFFER *key,
    TPM2B_IV *iv)
{
    return encrypt_decrypt_cfb(data_out, data_in, NO, key, iv);
}

TSS2_RC
hash (
    TPM2_ALG_ID alg,
    const void *data,
    int size,
    TPM2B_DIGEST *out)
{
    switch (alg) {
    case TPM2_ALG_SHA1:
        SHA1(data, size, out->buffer);
        out->size = TPM2_SHA1_DIGEST_SIZE;
        break;
    case TPM2_ALG_SHA256:
        SHA256(data, size, out->buffer);
        out->size = TPM2_SHA256_DIGEST_SIZE;
        break;
    case TPM2_ALG_SHA384:
        SHA384(data, size, out->buffer);
        out->size = TPM2_SHA384_DIGEST_SIZE;
        break;
    case TPM2_ALG_SHA512:
        SHA512(data, size, out->buffer);
        out->size = TPM2_SHA512_DIGEST_SIZE;
        break;
    default:
        return TSS2_SYS_RC_BAD_VALUE;
    }
    return TPM2_RC_SUCCESS;
}

TSS2_RC
hmac(
    TPM2_ALG_ID alg,
    const void *key,
    int key_len,
    TPM2B_DIGEST **buffer_list,
    TPM2B_DIGEST *out)
{
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    HMAC_CTX *ctx;
#else
    HMAC_CTX _ctx;
    HMAC_CTX *ctx = &_ctx;
#endif
    EVP_MD *evp;
    int rc = 1, i;
    unsigned int *buf = NULL, size;
    uint8_t *buf_ptr;

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    /* HMAC_CTX_new and HMAC_CTX_free are new in openSSL 1.1.0 */
    ctx = HMAC_CTX_new();
#else
    HMAC_CTX_init(ctx);
#endif

    if (!ctx)
        return TSS2_SYS_RC_GENERAL_FAILURE;

    switch (alg) {
    case TPM2_ALG_SHA1:
        evp = (EVP_MD *) EVP_sha1();
        out->size = TPM2_SHA1_DIGEST_SIZE;
        break;
    case TPM2_ALG_SHA256:
        evp = (EVP_MD *) EVP_sha256();
        out->size = TPM2_SHA256_DIGEST_SIZE;
        break;
    case TPM2_ALG_SHA384:
        evp = (EVP_MD *) EVP_sha384();
        out->size = TPM2_SHA384_DIGEST_SIZE;
        break;
    case TPM2_ALG_SHA512:
        evp = (EVP_MD *) EVP_sha512();
        out->size = TPM2_SHA512_DIGEST_SIZE;
        break;
    default:
        rc = TSS2_SYS_RC_BAD_VALUE;
        goto out;
    }
    rc = 0;
    buf = calloc(1, out->size);
    if (!buf)
            goto out;

    buf_ptr = (uint8_t *)buf;

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    rc = HMAC_Init_ex(ctx, key, key_len, evp, NULL);
#else
    rc = HMAC_Init(ctx, key, key_len, evp);
#endif

    if (rc != 1)
        goto out;
    for (i = 0; buffer_list[i] != 0; i++) {
        rc = HMAC_Update(ctx, buffer_list[i]->buffer, buffer_list[i]->size);
        if (rc != 1)
            goto out;
    }
    /* buf_ptr has to be 4 bytes alligned for whatever reason */
    rc = HMAC_Final(ctx, buf_ptr, &size);
    if (rc != 1)
        goto out;

    assert(size == out->size);

    memcpy(out->buffer, buf, out->size);

out:
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    HMAC_CTX_free(ctx);
#else
    HMAC_CTX_cleanup(ctx);
#endif

    if (buf)
        free(buf);

    /* In openSSL 1 means success 0 error */
    return rc == 1 ? TPM2_RC_SUCCESS : TSS2_SYS_RC_GENERAL_FAILURE;
}

TSS2_RC
ConcatSizedByteBuffer(
        TPM2B_MAX_BUFFER *result,
        TPM2B *buf)
{
    if (result->size + buf->size > TPM2_MAX_DIGEST_BUFFER)
        return TSS2_SYS_RC_BAD_VALUE;

    memmove(result->buffer + result->size,
            buf->buffer, buf->size);

    result->size += buf->size;
    return TPM2_RC_SUCCESS;
}

TSS2_RC
CompareSizedByteBuffer(
        TPM2B *buffer1,
        TPM2B *buffer2)
{
    if (buffer1->size != buffer2->size)
        return TPM2_RC_FAILURE;

    if (memcmp(buffer1->buffer, buffer2->buffer, buffer1->size))
        return TPM2_RC_FAILURE;

    return TPM2_RC_SUCCESS;
}

void
CatSizedByteBuffer(
        TPM2B *dest,
        TPM2B *src)
{
    if (!dest || !src)
        return;

    memcpy(dest->buffer + dest->size, src->buffer, src->size);
    dest->size += src->size;
}

UINT16
CopySizedByteBuffer(
        TPM2B *dest,
        TPM2B *src)
{
    if (!dest)
        return 0;

    if (!src) {
        dest->size = 0;
        return 0;
    }

    memcpy(dest->buffer, src->buffer, src->size);
    dest->size = src->size;
    return src->size + 2;
}

UINT16
GetDigestSize(TPM2_ALG_ID hash)
{
    switch (hash) {
        case TPM2_ALG_SHA1:
            return TPM2_SHA1_DIGEST_SIZE;
        case TPM2_ALG_SHA256:
            return TPM2_SHA256_DIGEST_SIZE;
        case TPM2_ALG_SHA384:
            return TPM2_SHA384_DIGEST_SIZE;
        case TPM2_ALG_SHA512:
            return TPM2_SHA512_DIGEST_SIZE;
        case TPM2_ALG_SM3_256:
            return TPM2_SM3_256_DIGEST_SIZE;
        default:
            return 0;
    }
}
