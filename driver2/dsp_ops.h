/* -*- mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *      vim: sw=8 ts=8 et tw=80
 */
#ifndef _PCI_OPS_H
#define _PCI_OPS_H


/* Prototypes */

int dsp_ops_probe(int card);

void dsp_ops_cleanup(void);

int dsp_ops_init(void);

#endif
