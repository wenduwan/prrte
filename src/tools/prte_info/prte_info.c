/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2004-2010 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2007 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2007-2020 Cisco Systems, Inc.  All rights reserved
 * Copyright (c) 2010-2016 Los Alamos National Security, LLC.
 *                         All rights reserved.
 * Copyright (c) 2014-2020 Intel, Inc.  All rights reserved.
 * Copyright (c) 2015      Research Organization for Information Science
 *                         and Technology (RIST). All rights reserved.
 * Copyright (c) 2021-2022 Nanook Consulting.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "prte_config.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#ifdef HAVE_NETDB_H
#    include <netdb.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#    include <sys/param.h>
#endif
#include <errno.h>
#include <signal.h>

#include "src/class/prte_object.h"
#include "src/class/prte_pointer_array.h"
#include "src/mca/base/base.h"
#include "src/mca/prteinstalldirs/prteinstalldirs.h"
#include "src/mca/schizo/base/base.h"
#include "src/prted/pmix/pmix_server.h"
#include "src/util/argv.h"
#include "src/util/basename.h"
#include "src/util/cmd_line.h"
#include "src/util/error.h"
#include "src/util/path.h"
#include "src/util/proc_info.h"
#include "src/util/show_help.h"

#include "constants.h"
#include "src/include/frameworks.h"
#include "src/include/version.h"
#include "src/runtime/prte_locks.h"

#include "src/tools/prte_info/pinfo.h"

/*
 * Public variables
 */

bool prte_info_pretty = true;
prte_cli_result_t prte_info_cmd_line = PRTE_CLI_RESULT_STATIC_INIT;

const char *prte_info_type_all = "all";
const char *prte_info_type_prte = "prte";
const char *prte_info_type_base = "base";

prte_pointer_array_t mca_types = {{0}};

int main(int argc, char *argv[])
{
    int ret = 0;
    bool want_help = false;
    bool cmd_error = false;
    bool acted = false;
    bool want_all = false;
    int i;
    char *str;
    int option_index = 0;   /* getopt_long stores the option index here. */
    int n, opt, rc;
    bool found;
    char *ptr;
    prte_info_item_t *instance;
    char *personality;
    prte_schizo_base_module_t *schizo;
    prte_cli_result_t results;

    /* protect against problems if someone passes us thru a pipe
     * and then abnormally terminates the pipe early */
    signal(SIGPIPE, SIG_IGN);

    prte_tool_basename = prte_basename(argv[0]);
    prte_tool_actual = "prte_info";

    /* Initialize the argv parsing stuff */
    if (PRTE_SUCCESS != (ret = prte_init_util(PRTE_PROC_MASTER))) {
        prte_show_help("help-prte-info.txt", "lib-call-fail", true, "prte_init_util", __FILE__,
                       __LINE__, NULL);
        exit(ret);
    }

    /* open the SCHIZO framework */
    ret = prte_mca_base_framework_open(&prte_schizo_base_framework,
                                       PRTE_MCA_BASE_OPEN_DEFAULT);
    if (PRTE_SUCCESS != ret) {
        PRTE_ERROR_LOG(ret);
        return ret;
    }

    if (PRTE_SUCCESS != (ret = prte_schizo_base_select())) {
        PRTE_ERROR_LOG(ret);
        return ret;
    }

    /* look for any personality specification */
    personality = NULL;
    for (i = 0; NULL != argv[i]; i++) {
        if (0 == strcmp(argv[i], "--personality")) {
            personality = argv[i + 1];
            break;
        }
    }

    /* detect if we are running as a proxy and select the active
     * schizo module for this tool */
    schizo = prte_schizo_base_detect_proxy(personality);
    if (NULL == schizo) {
        prte_show_help("help-schizo-base.txt", "no-proxy", true, prte_tool_basename, personality);
        return 1;
    }
    if (NULL == personality) {
        personality = schizo->name;
    }

    /* parse the input argv to get values, including everyone's MCA params */
    PRTE_CONSTRUCT(&prte_info_cmd_line, prte_cli_result_t);
    ret = schizo->parse_cli(argv, &prte_info_cmd_line, PRTE_CLI_SILENT);
    if (PRTE_SUCCESS != ret) {
        PRTE_DESTRUCT(&prte_info_cmd_line);
        if (PRTE_ERR_SILENT != ret) {
            fprintf(stderr, "%s: command line error (%s)\n", prte_tool_basename, prte_strerror(rc));
        } else {
            ret = PRTE_SUCCESS;
        }
        return ret;
    }
    // we do NOT accept arguments other than our own
    if (NULL != prte_info_cmd_line.tail) {
        str = prte_argv_join(prte_info_cmd_line.tail, ' ');
        ptr = prte_show_help_string("help-pterm.txt", "no-args", false,
                                    prte_tool_basename, str, prte_tool_basename);
        free(str);
        if (NULL != ptr) {
            printf("%s", ptr);
            free(ptr);
        }
        return -1;
    }

    /* setup the mca_types array */
    PRTE_CONSTRUCT(&mca_types, prte_pointer_array_t);
    prte_pointer_array_init(&mca_types, 256, INT_MAX, 128);

    /* add a type for prte itself */
    prte_pointer_array_add(&mca_types, "mca");
    prte_pointer_array_add(&mca_types, "prte");

    /* add a type for hwloc */
    prte_pointer_array_add(&mca_types, "hwloc");

    /* let the pmix server register params */
    pmix_server_register_params();
    /* add those in */
    prte_pointer_array_add(&mca_types, "pmix");

    /* push all the types found by autogen */
    for (i = 0; NULL != prte_frameworks[i]; i++) {
        prte_pointer_array_add(&mca_types, prte_frameworks[i]->framework_name);
    }

    /* Execute the desired action(s) */
    want_all = prte_cmd_line_is_taken(&prte_info_cmd_line, "all");
    if (want_all) {
        prte_info_do_version(want_all);
        acted = true;
    }
    if (want_all || prte_cmd_line_is_taken(&prte_info_cmd_line, "path")) {
        prte_info_do_path(want_all);
        acted = true;
    }
    if (want_all || prte_cmd_line_is_taken(&prte_info_cmd_line, "arch")) {
        prte_info_do_arch();
        acted = true;
    }
    if (want_all || prte_cmd_line_is_taken(&prte_info_cmd_line, "hostname")) {
        prte_info_do_hostname();
        acted = true;
    }
    if (want_all || prte_cmd_line_is_taken(&prte_info_cmd_line, "config")) {
        prte_info_do_config(true);
        acted = true;
    }
    if (want_all || prte_cmd_line_is_taken(&prte_info_cmd_line, "param")) {
        prte_info_do_params(want_all, prte_cmd_line_is_taken(&prte_info_cmd_line, "internal"));
        acted = true;
    }

    /* If no command line args are specified, show default set */

    if (!acted) {
        prte_info_show_prte_version(prte_info_ver_full);
        prte_info_show_path(prte_info_path_prefix, prte_install_dirs.prefix);
        prte_info_do_arch();
        prte_info_do_hostname();
        prte_info_do_config(false);
        prte_info_components_open();
        for (i = 0; i < mca_types.size; ++i) {
            if (NULL == (str = (char *) prte_pointer_array_get_item(&mca_types, i))) {
                continue;
            }
            prte_info_show_component_version(str, prte_info_component_all, prte_info_ver_full,
                                             prte_info_type_all);
        }
    }

    /* All done */
    prte_info_components_close();
    PRTE_DESTRUCT(&mca_types);
    prte_mca_base_close();

    return 0;
}
