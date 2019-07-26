/* SPDX-License-Identifier: BSD-2 */
/*
 * Copyright (c) 2015 - 2018 Intel Corporation
 * All rights reserved.
 */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <poll.h>
#include <unistd.h>

#include "tss2_tcti.h"
#include "tss2_tcti_device.h"

#include "tcti-common.h"
#include "tcti-device.h"
#include "util/io.h"
#define LOGMODULE tcti
#include "util/log.h"

#define TCTI_DEVICE_DEFAULT "/dev/tpm0"
/*
 * This function wraps the "up-cast" of the opaque TCTI context type to the
 * type for the device TCTI context. The only safe-guard we have to ensure
 * this operation is possible is the magic number for the device TCTI context.
 * If passed a NULL context, or the magic number check fails, this function
 * will return NULL.
 */
TSS2_TCTI_DEVICE_CONTEXT*
tcti_device_context_cast (TSS2_TCTI_CONTEXT *tcti_ctx)
{
    if (tcti_ctx != NULL && TSS2_TCTI_MAGIC (tcti_ctx) == TCTI_DEVICE_MAGIC) {
        return (TSS2_TCTI_DEVICE_CONTEXT*)tcti_ctx;
    }
    return NULL;
}
/*
 * This function down-casts the device TCTI context to the common context
 * defined in the tcti-common module.
 */
TSS2_TCTI_COMMON_CONTEXT*
tcti_device_down_cast (TSS2_TCTI_DEVICE_CONTEXT *tcti_dev)
{
    if (tcti_dev == NULL) {
        return NULL;
    }
    return &tcti_dev->common;
}

TSS2_RC
tcti_device_transmit (
    TSS2_TCTI_CONTEXT *tctiContext,
    size_t command_size,
    const uint8_t *command_buffer)
{
    TSS2_TCTI_DEVICE_CONTEXT *tcti_dev = tcti_device_context_cast (tctiContext);
    TSS2_TCTI_COMMON_CONTEXT *tcti_common = tcti_device_down_cast (tcti_dev);
    TSS2_RC rc = TSS2_RC_SUCCESS;
    ssize_t size;

    if (tcti_dev == NULL) {
        return TSS2_TCTI_RC_BAD_CONTEXT;
    }
    rc = tcti_common_transmit_checks (tcti_common, command_buffer);
    if (rc != TSS2_RC_SUCCESS) {
        return rc;
    }
    LOGBLOB_DEBUG (command_buffer,
                   command_size,
                   "sending %zu byte command buffer:",
                   command_size);
    size = write_all (tcti_dev->fd,
                      command_buffer,
                      command_size);
    if (size < 0) {
        return TSS2_TCTI_RC_IO_ERROR;
    } else if ((size_t)size != command_size) {
        LOG_ERROR ("wrong number of bytes written. Expected %zu, wrote %zd.",
                   command_size,
                   size);
        return TSS2_TCTI_RC_IO_ERROR;
    }

    tcti_common->state = TCTI_STATE_RECEIVE;
    return TSS2_RC_SUCCESS;
}
/*
 * This receive function deviates from the spec a bit. Calling this function
 * with a NULL 'tctiContext' parameter *should* result in the required size for
 * the response buffer being returned to the caller. We would typically do this
 * by reading the response's header and then returning the size to the caller.
 * We can't do that on account of the TPM2 kernel driver closing any connection
 * that doesn't read the whole response buffer in one 'read' call.
 *
 * Instead, if the caller queries the size, we return 4k just to be on the
 * safe side. We do *not* however verify that the provided buffer is large
 * enough to hold the full response (we can't). If the caller provides us with
 * a buffer less than 4k we'll read as much of the response as we can given
 * the size of the buffer. If we get enough of the response to read the size
 * field, we check to see if the buffer was large enough to get the full
 * response. If the response header claims it's larger than the provided
 * buffer we print a warning. This allows "expert applications" to
 * precalculate the required response buffer size for whatever commands they
 * may send.
 */
TSS2_RC
tcti_device_receive (
    TSS2_TCTI_CONTEXT *tctiContext,
    size_t *response_size,
    uint8_t *response_buffer,
    int32_t timeout)
{
    TSS2_TCTI_DEVICE_CONTEXT *tcti_dev = tcti_device_context_cast (tctiContext);
    TSS2_TCTI_COMMON_CONTEXT *tcti_common = tcti_device_down_cast (tcti_dev);
    TSS2_RC rc = TSS2_RC_SUCCESS;
    ssize_t size = 0;
    struct pollfd fds = {0};
    int rc_poll, nfds = 1;

    if (tcti_dev == NULL) {
        return TSS2_TCTI_RC_BAD_CONTEXT;
    }
    rc = tcti_common_receive_checks (tcti_common, response_size);
    if (rc != TSS2_RC_SUCCESS) {
        return rc;
    }
#ifndef TCTI_ASYNC
    /* For async the valid timeout values are -1 - block forever,
     * 0 - nonblocking, and any positive value - the actual timeout
     * value in millisec.
     * For sync the only valid value is -1 - block forever.
     */
    if (timeout != TSS2_TCTI_TIMEOUT_BLOCK) {
        LOG_WARNING ("The underlying IPC mechanism does not support "
                     "asynchronous I/O. The 'timeout' parameter must be "
                     "TSS2_TCTI_TIMEOUT_BLOCK");
        return TSS2_TCTI_RC_BAD_VALUE;
    }
#endif
    if (response_buffer == NULL) {
        LOG_DEBUG ("Caller queried for size but linux kernel doesn't allow this. "
                   "Returning 4k which is the max size for a response buffer.");
        *response_size = 4096;
        return TSS2_RC_SUCCESS;
    }
    if (*response_size < 4096) {
        LOG_INFO ("Caller provided buffer that *may* not be large enough to "
                  "hold the response buffer.");
    }
    /*
     * The kernel driver will only return a response buffer in a single read
     * operation. If we try to read again before sending another command
     * the kernel will close the file descriptor and we'll get an EOF.
     */
    fds.fd = tcti_dev->fd;
    fds.events = POLLIN;

    rc_poll = poll(&fds, nfds, timeout);
    if (rc_poll < 0) {
        LOG_ERROR ("Failed to poll for response from fd %d, got errno %d: %s",
                   tcti_dev->fd, errno, strerror (errno));
        return TSS2_TCTI_RC_IO_ERROR;
    } else if (rc_poll == 0) {
        LOG_INFO ("Poll timed out on fd %d.", tcti_dev->fd);
        return TSS2_TCTI_RC_TRY_AGAIN;
    } else if (fds.revents == POLLIN) {
        TEMP_RETRY (size, read(tcti_dev->fd, response_buffer, *response_size));
        if (size < 0) {
            LOG_ERROR ("Failed to read response from fd %d, got errno %d: %s",
               tcti_dev->fd, errno, strerror (errno));
            return TSS2_TCTI_RC_IO_ERROR;
        }
    }
    if (size == 0) {
        LOG_WARNING ("Got EOF instead of response.");
        rc = TSS2_TCTI_RC_NO_CONNECTION;
        goto out;
    }
    LOGBLOB_DEBUG(response_buffer, size, "Response Received");
    if (size < (ssize_t)TPM_HEADER_SIZE) {
        LOG_ERROR ("Received %zu bytes, not enough to hold a TPM2 response "
                   "header.", size);
        rc = TSS2_TCTI_RC_GENERAL_FAILURE;
        goto out;
    }
    rc = header_unmarshal (response_buffer, &tcti_common->header);
    if (rc != TSS2_RC_SUCCESS) {
        goto out;
    }
    if ((size_t)size != tcti_common->header.size) {
        LOG_WARNING ("TPM2 header size disagrees with number of bytes read "
                     "from fd %d. Header says %u but we read %zu bytes.",
                     tcti_dev->fd, tcti_common->header.size, size);
    }
    if (*response_size < tcti_common->header.size) {
        LOG_WARNING ("TPM2 response header size is larger than the provided "
                     "buffer: future use of this TCTI will likely fail.");
        rc = TSS2_TCTI_RC_GENERAL_FAILURE;
    }
    *response_size = size;
    /*
     * Executing code beyond this point transitions the state machine to
     * TRANSMIT. Another call to this function will not be possible until
     * another command is sent to the TPM.
     */
out:
    tcti_common->state = TCTI_STATE_TRANSMIT;

    return rc;
}

void
tcti_device_finalize (
    TSS2_TCTI_CONTEXT *tctiContext)
{
    TSS2_TCTI_DEVICE_CONTEXT *tcti_dev = tcti_device_context_cast (tctiContext);
    TSS2_TCTI_COMMON_CONTEXT *tcti_common = tcti_device_down_cast (tcti_dev);

    if (tcti_dev == NULL) {
        return;
    }
    close (tcti_dev->fd);
    tcti_common->state = TCTI_STATE_FINAL;
}

TSS2_RC
tcti_device_cancel (
    TSS2_TCTI_CONTEXT *tctiContext)
{
    /* Linux driver doesn't expose a mechanism to cancel commands. */
    (void)(tctiContext);
    return TSS2_TCTI_RC_NOT_IMPLEMENTED;
}

TSS2_RC
tcti_device_get_poll_handles (
    TSS2_TCTI_CONTEXT *tctiContext,
    TSS2_TCTI_POLL_HANDLE *handles,
    size_t *num_handles)
{
#ifdef TCTI_ASYNC
    TSS2_TCTI_DEVICE_CONTEXT *tcti_dev = tcti_device_context_cast (tctiContext);

    if (tcti_dev == NULL) {
        return TSS2_TCTI_RC_BAD_CONTEXT;
    }

    if (handles == NULL || num_handles == NULL) {
        return TSS2_TCTI_RC_BAD_REFERENCE;
    }

    *num_handles = 1;
    handles->fd = tcti_dev->fd;
    return TSS2_RC_SUCCESS;
#else
    (void)(tctiContext);
    (void)(handles);
    (void)(num_handles);
    return TSS2_TCTI_RC_NOT_IMPLEMENTED;
#endif
}

TSS2_RC
tcti_device_set_locality (
    TSS2_TCTI_CONTEXT *tctiContext,
    uint8_t locality)
{
    /*
     * Linux driver doesn't expose a mechanism for user space applications
     * to set locality.
     */
    (void)(tctiContext);
    (void)(locality);
    return TSS2_TCTI_RC_NOT_IMPLEMENTED;
}

TSS2_RC
Tss2_Tcti_Device_Init (
    TSS2_TCTI_CONTEXT *tctiContext,
    size_t *size,
    const char *conf)
{
    TSS2_TCTI_DEVICE_CONTEXT *tcti_dev;
    TSS2_TCTI_COMMON_CONTEXT *tcti_common;
    const char *dev_path = conf != NULL ? conf : TCTI_DEVICE_DEFAULT;

    if (tctiContext == NULL && size == NULL) {
        return TSS2_TCTI_RC_BAD_VALUE;
    } else if (tctiContext == NULL) {
        *size = sizeof (TSS2_TCTI_DEVICE_CONTEXT);
        return TSS2_RC_SUCCESS;
    }

    /* Init TCTI context */
    TSS2_TCTI_MAGIC (tctiContext) = TCTI_DEVICE_MAGIC;
    TSS2_TCTI_VERSION (tctiContext) = TCTI_VERSION;
    TSS2_TCTI_TRANSMIT (tctiContext) = tcti_device_transmit;
    TSS2_TCTI_RECEIVE (tctiContext) = tcti_device_receive;
    TSS2_TCTI_FINALIZE (tctiContext) = tcti_device_finalize;
    TSS2_TCTI_CANCEL (tctiContext) = tcti_device_cancel;
    TSS2_TCTI_GET_POLL_HANDLES (tctiContext) = tcti_device_get_poll_handles;
    TSS2_TCTI_SET_LOCALITY (tctiContext) = tcti_device_set_locality;
    TSS2_TCTI_MAKE_STICKY (tctiContext) = tcti_make_sticky_not_implemented;
    tcti_dev = tcti_device_context_cast (tctiContext);
    tcti_common = tcti_device_down_cast (tcti_dev);
    tcti_common->state = TCTI_STATE_TRANSMIT;
    memset (&tcti_common->header, 0, sizeof (tcti_common->header));
    tcti_common->locality = 3;

    tcti_dev->fd = open (dev_path, O_RDWR | O_NONBLOCK);
    if (tcti_dev->fd < 0) {
        LOG_ERROR ("Failed to open device file %s: %s",
                   dev_path, strerror (errno));
        return TSS2_TCTI_RC_IO_ERROR;
    }

    return TSS2_RC_SUCCESS;
}

const TSS2_TCTI_INFO tss2_tcti_info = {
    .version = TCTI_VERSION,
    .name = "tcti-device",
    .description = "TCTI module for communication with Linux kernel interface.",
    .config_help = "Path to TPM character device. Default value is: "
        "TCTI_DEVICE_DEFAULT",
    .init = Tss2_Tcti_Device_Init,
};

const TSS2_TCTI_INFO*
Tss2_Tcti_Info (void)
{
    return &tss2_tcti_info;
}
