/* -*- mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *      vim: sw=2 ts=2 et tw=80
 */
#ifndef _PCI_OPS_H
#define _PCI_OPS_H


/* Prototypes */

int dsp_ops_probe(int card);

void dsp_ops_cleanup(void);

int dsp_ops_init(void);

#endif
