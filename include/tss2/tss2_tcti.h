/* SPDX-License-Identifier: BSD-2 */
/*
 * Copyright (c) 2015 - 2018, Intel Corporation
 *
 * Copyright 2015, Andreas Fuchs @ Fraunhofer SIT
 *
 * All rights reserved.
 */
#ifndef TSS2_TCTI_H
#define TSS2_TCTI_H

#include <stdint.h>
#include <stddef.h>
#include "tss2_common.h"
#include "tss2_tpm2_types.h"

#ifndef TSS2_API_VERSION_1_2_1_108
#error Version mismatch among TSS2 header files.
#endif  /* TSS2_API_VERSION_1_2_1_108 */

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#include <poll.h>
typedef struct pollfd TSS2_TCTI_POLL_HANDLE;
#elif defined(_WIN32)
#include <windows.h>
typedef HANDLE TSS2_TCTI_POLL_HANDLE;
#else
typedef void TSS2_TCTI_POLL_HANDLE;
#ifndef TSS2_TCTI_SUPPRESS_POLL_WARNINGS
#pragma message "Info: Platform not supported for TCTI_POLL_HANDLES"
#endif
#endif

/* The following are used to configure timeout characteristics. */
#define  TSS2_TCTI_TIMEOUT_BLOCK    -1
#define  TSS2_TCTI_TIMEOUT_NONE     0

/* Macros to simplify access to values in common TCTI structure */
#define TSS2_TCTI_MAGIC(tctiContext) \
    ((TSS2_TCTI_CONTEXT_COMMON_V1*)tctiContext)->magic
#define TSS2_TCTI_VERSION(tctiContext) \
    ((TSS2_TCTI_CONTEXT_COMMON_V1*)tctiContext)->version
#define TSS2_TCTI_TRANSMIT(tctiContext) \
    ((TSS2_TCTI_CONTEXT_COMMON_V1*)tctiContext)->transmit
#define TSS2_TCTI_RECEIVE(tctiContext) \
    ((TSS2_TCTI_CONTEXT_COMMON_V1*)tctiContext)->receive
#define TSS2_TCTI_FINALIZE(tctiContext) \
    ((TSS2_TCTI_CONTEXT_COMMON_V1*)tctiContext)->finalize
#define TSS2_TCTI_CANCEL(tctiContext) \
    ((TSS2_TCTI_CONTEXT_COMMON_V1*)tctiContext)->cancel
#define TSS2_TCTI_GET_POLL_HANDLES(tctiContext) \
    ((TSS2_TCTI_CONTEXT_COMMON_V1*)tctiContext)->getPollHandles
#define TSS2_TCTI_SET_LOCALITY(tctiContext) \
    ((TSS2_TCTI_CONTEXT_COMMON_V1*)tctiContext)->setLocality
#define TSS2_TCTI_MAKE_STICKY(tctiContext) \
    ((TSS2_TCTI_CONTEXT_COMMON_V2*)tctiContext)->makeSticky

/* Macros to simplify invocation of functions from the common TCTI structure */
#define Tss2_Tcti_Transmit(tctiContext, size, command) \
    ((tctiContext == NULL) ? TSS2_TCTI_RC_BAD_CONTEXT: \
    (TSS2_TCTI_VERSION(tctiContext) < 1) ? \
        TSS2_TCTI_RC_ABI_MISMATCH: \
    (TSS2_TCTI_TRANSMIT(tctiContext) == NULL) ? \
        TSS2_TCTI_RC_NOT_IMPLEMENTED: \
    TSS2_TCTI_TRANSMIT(tctiContext)(tctiContext, size, command))
#define Tss2_Tcti_Receive(tctiContext, size, response, timeout) \
    ((tctiContext == NULL) ? TSS2_TCTI_RC_BAD_CONTEXT: \
    (TSS2_TCTI_VERSION(tctiContext) < 1) ? \
        TSS2_TCTI_RC_ABI_MISMATCH: \
    (TSS2_TCTI_RECEIVE(tctiContext) == NULL) ? \
        TSS2_TCTI_RC_NOT_IMPLEMENTED: \
    TSS2_TCTI_RECEIVE(tctiContext)(tctiContext, size, response, timeout))
#define Tss2_Tcti_Finalize(tctiContext) \
    do { \
        if ((tctiContext != NULL) && \
            (TSS2_TCTI_VERSION(tctiContext) >= 1) && \
            (TSS2_TCTI_FINALIZE(tctiContext) != NULL)) \
        { \
            TSS2_TCTI_FINALIZE(tctiContext)(tctiContext); \
        } \
    } while (0)
#define Tss2_Tcti_Cancel(tctiContext) \
    ((tctiContext == NULL) ? TSS2_TCTI_RC_BAD_CONTEXT: \
    (TSS2_TCTI_VERSION(tctiContext) < 1) ? \
        TSS2_TCTI_RC_ABI_MISMATCH: \
    (TSS2_TCTI_CANCEL(tctiContext) == NULL) ? \
        TSS2_TCTI_RC_NOT_IMPLEMENTED: \
    TSS2_TCTI_CANCEL(tctiContext)(tctiContext))
#define Tss2_Tcti_GetPollHandles(tctiContext, handles, num_handles) \
    ((tctiContext == NULL) ? TSS2_TCTI_RC_BAD_CONTEXT: \
    (TSS2_TCTI_VERSION(tctiContext) < 1) ? \
        TSS2_TCTI_RC_ABI_MISMATCH: \
    (TSS2_TCTI_GET_POLL_HANDLES(tctiContext) == NULL) ? \
        TSS2_TCTI_RC_NOT_IMPLEMENTED: \
    TSS2_TCTI_GET_POLL_HANDLES(tctiContext)(tctiContext, handles, num_handles))
#define Tss2_Tcti_SetLocality(tctiContext, locality) \
    ((tctiContext == NULL) ? TSS2_TCTI_RC_BAD_CONTEXT: \
    (TSS2_TCTI_VERSION(tctiContext) < 1) ? \
        TSS2_TCTI_RC_ABI_MISMATCH: \
    (TSS2_TCTI_SET_LOCALITY(tctiContext) == NULL) ? \
        TSS2_TCTI_RC_NOT_IMPLEMENTED: \
    TSS2_TCTI_SET_LOCALITY(tctiContext)(tctiContext, locality))
#define Tss2_Tcti_MakeSticky(tctiContext, handle, sticky) \
    ((tctiContext == NULL) ? TSS2_TCTI_RC_BAD_CONTEXT: \
    (TSS2_TCTI_VERSION(tctiContext) < 2) ? \
        TSS2_TCTI_RC_ABI_MISMATCH: \
    (TSS2_TCTI_MAKE_STICKY(tctiContext) == NULL) ? \
        TSS2_TCTI_RC_NOT_IMPLEMENTED: \
    TSS2_TCTI_MAKE_STICKY(tctiContext)(tctiContext, handle, sticky))

typedef struct TSS2_TCTI_OPAQUE_CONTEXT_BLOB TSS2_TCTI_CONTEXT;

/*
 * Types for TCTI functions. These are used for pointers to functions in the
 * TCTI context structure.
 */
typedef TSS2_RC (*TSS2_TCTI_TRANSMIT_FCN) (
    TSS2_TCTI_CONTEXT *tctiContext,
    size_t size,
    uint8_t const *command);
typedef TSS2_RC (*TSS2_TCTI_RECEIVE_FCN) (
    TSS2_TCTI_CONTEXT *tctiContext,
    size_t *size,
    uint8_t *response,
    int32_t timeout);
typedef void (*TSS2_TCTI_FINALIZE_FCN) (
    TSS2_TCTI_CONTEXT *tctiContext);
typedef TSS2_RC (*TSS2_TCTI_CANCEL_FCN) (
    TSS2_TCTI_CONTEXT *tctiContext);
typedef TSS2_RC (*TSS2_TCTI_GET_POLL_HANDLES_FCN) (
    TSS2_TCTI_CONTEXT *tctiContext,
    TSS2_TCTI_POLL_HANDLE *handles,
    size_t *num_handles);
typedef TSS2_RC (*TSS2_TCTI_SET_LOCALITY_FCN) (
    TSS2_TCTI_CONTEXT *tctiContext,
    uint8_t locality);
typedef TSS2_RC (*TSS2_TCTI_MAKE_STICKY_FCN) (
    TSS2_TCTI_CONTEXT *tctiContext,
    TPM2_HANDLE *handle,
    uint8_t sticky);
typedef TSS2_RC (*TSS2_TCTI_INIT_FUNC) (
    TSS2_TCTI_CONTEXT *tctiContext,
    size_t *size,
    const char *config);

/* current version #1 known to this implementation */
typedef struct {
    uint64_t magic;
    uint32_t version;
    TSS2_TCTI_TRANSMIT_FCN transmit;
    TSS2_TCTI_RECEIVE_FCN receive;
    TSS2_TCTI_FINALIZE_FCN finalize;
    TSS2_TCTI_CANCEL_FCN cancel;
    TSS2_TCTI_GET_POLL_HANDLES_FCN getPollHandles;
    TSS2_TCTI_SET_LOCALITY_FCN setLocality;
} TSS2_TCTI_CONTEXT_COMMON_V1;

typedef struct {
    TSS2_TCTI_CONTEXT_COMMON_V1 v1;
    TSS2_TCTI_MAKE_STICKY_FCN makeSticky;
} TSS2_TCTI_CONTEXT_COMMON_V2;

typedef TSS2_TCTI_CONTEXT_COMMON_V2 TSS2_TCTI_CONTEXT_COMMON_CURRENT;

#define TSS2_TCTI_INFO_SYMBOL "Tss2_Tcti_Info"

typedef struct {
    uint32_t version;
    const char *name;
    const char *description;
    const char *config_help;
    TSS2_TCTI_INIT_FUNC init;
} TSS2_TCTI_INFO;

typedef const TSS2_TCTI_INFO* (*TSS2_TCTI_INFO_FUNC) (void);

#endif
