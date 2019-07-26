/* SPDX-License-Identifier: BSD-2 */
/***********************************************************************
 * Copyright (c) 2017-2018, Intel Corporation
 *
 * All rights reserved.
 ***********************************************************************/
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#define LOGMODULE test
#include "util/log.h"
#include "sapi-util.h"
#include "test-esapi.h"
#include "test.h"

#define ENC_STR "test-data-test-data-test-data"

/*
 * This test is intended to exercise the EncryptDecrypt2 command. We start by
 * creating a primary key, then a 128 bit AES key in CFB mode under it. We
 * then encrypt a well known string with this key, and then decrypt that same
 * string. The test is successful if the original string and the decrypted
 * string are the same.
 */
int
test_invoke (TSS2_SYS_CONTEXT *sapi_context)
{
    TSS2_RC rc;
    TPM2_HANDLE handle_parent, handle;
    TPM2B_MAX_BUFFER data_in = { 0 };
    TPM2B_MAX_BUFFER data_encrypt = TPM2B_MAX_BUFFER_INIT;
    TPM2B_MAX_BUFFER data_decrypt = TPM2B_MAX_BUFFER_INIT;

    data_in.size = strlen (ENC_STR);
    strcpy ((char*)data_in.buffer, ENC_STR);

    rc = create_primary_rsa_2048_aes_128_cfb (sapi_context, &handle_parent);
    if (rc != TSS2_RC_SUCCESS) {
        LOG_ERROR("Failed to create primary RSA 2048 key: 0x%" PRIx32 "",
                    rc);
        exit(1);
    }

    rc = create_aes_128_cfb (sapi_context, handle_parent, &handle);
    if (rc != TSS2_RC_SUCCESS) {
        LOG_ERROR("Failed to create child AES 128 key: 0x%" PRIx32 "", rc);
        exit(1);
    }

    LOG_INFO("Encrypting data: \"%s\" with key handle: 0x%08" PRIx32,
               data_in.buffer, handle);
    rc = tpm_encrypt_cfb (sapi_context, handle, &data_in, &data_encrypt);

    if (rc == TPM2_RC_COMMAND_CODE) {
        LOG_WARNING("Encrypt/Decrypt 2 not supported by TPM");
        rc = Tss2_Sys_FlushContext(sapi_context, handle_parent);
        if (rc != TSS2_RC_SUCCESS) {
            LOG_ERROR("Tss2_Sys_FlushContext failed with 0x%"PRIx32, rc);
            return 99; /* fatal error */
        }
        rc = Tss2_Sys_FlushContext(sapi_context, handle);
        if (rc != TSS2_RC_SUCCESS) {
            LOG_ERROR("Tss2_Sys_FlushContext failed with 0x%"PRIx32, rc);
            return 99; /* fatal error */
        }
        return EXIT_SKIP;
    }

    if (rc != TSS2_RC_SUCCESS) {
        LOG_ERROR("Failed to encrypt buffer: 0x%" PRIx32 "", rc);
        exit(1);
    }

    rc = tpm_decrypt_cfb (sapi_context, handle, &data_encrypt, &data_decrypt);
    if (rc != TSS2_RC_SUCCESS) {
        LOG_ERROR("Failed to encrypt buffer: 0x%" PRIx32 "", rc);
        exit(1);
    }
    LOG_INFO("Decrypted data: \"%s\" with key handle: 0x%08" PRIx32,
               data_decrypt.buffer, handle);
    if (strcmp ((char*)data_in.buffer, (char*)data_decrypt.buffer)) {
        LOG_ERROR("Decrypt succeeded but decrypted data != to input data");
        exit(1);
    }

    rc = Tss2_Sys_FlushContext(sapi_context, handle_parent);
    if (rc != TSS2_RC_SUCCESS) {
        LOG_ERROR("Tss2_Sys_FlushContext failed with 0x%"PRIx32, rc);
        return 99; /* fatal error */
    }
    rc = Tss2_Sys_FlushContext(sapi_context, handle);
    if (rc != TSS2_RC_SUCCESS) {
        LOG_ERROR("Tss2_Sys_FlushContext failed with 0x%"PRIx32, rc);
        return 99; /* fatal error */
    }

    return 0;
}
