#include <stdbool.h>
#include <stdlib.h>

#include "tss2_sys.h"

#define LOGMODULE test
#include "util/log.h"
#include "test-options.h"
#include "context-util.h"

int
main (int argc, char *argv[])
{
    TSS2_RC rc;
    TSS2_SYS_CONTEXT *sapi_context;

    test_opts_t opts = {
        .tcti_type      = TCTI_DEFAULT,
        .device_file    = DEVICE_PATH_DEFAULT,
        .socket_address = HOSTNAME_DEFAULT,
        .socket_port    = PORT_DEFAULT,
    };

    get_test_opts_from_env (&opts);
    if (sanity_check_test_opts (&opts) != 0)
        exit (1);

    sapi_context = sapi_init_from_opts (&opts);
    if (sapi_context == NULL)
        exit (1);

    rc = Tss2_Sys_Startup(sapi_context, TPM2_SU_CLEAR);
    if (rc != TSS2_RC_SUCCESS && rc != TPM2_RC_INITIALIZE) {
        LOG_ERROR("TPM Startup FAILED! Response Code : 0x%x", rc);
        exit(1);
    }

    sapi_teardown_full (sapi_context);
    return 0;
}
