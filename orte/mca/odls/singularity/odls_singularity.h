/*
 * Copyright (c) 2004-2008 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2019      Sylabs, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/**
 * @file:
 */

#ifndef ORTE_ODLS_SINGULARITY_H
#define ORTE_ODLS_SINGULARITY_H

#include "orte_config.h"

#include "orte/mca/mca.h"

#include "orte/mca/odls/odls.h"

BEGIN_C_DECLS

/*
 * Module open / close
 */
int orte_odls_singularity_component_open(void);
int orte_odls_singularity_component_close(void);
int orte_odls_singularity_component_query(mca_base_module_t **module, int *priority);
int orte_odls_singularity_component_register(void);

/*
 * ODLS singularity module
 */
extern orte_odls_base_module_t orte_odls_singularity_module;
ORTE_MODULE_DECLSPEC extern orte_odls_base_component_t mca_odls_singularity_component;

END_C_DECLS

#endif /* ORTE_ODLS_SINGULARITY_H */
