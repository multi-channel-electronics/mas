/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#ifndef _PCI_OPS_H
#define _PCI_OPS_H


/* Prototypes */

int dsp_ops_probe(int card);

void dsp_ops_cleanup(void);

int dsp_ops_init(void);

#endif
