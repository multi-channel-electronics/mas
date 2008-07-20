#ifndef _DSP_PCI_H_
#define _DSP_PCI_H_

#include <linux/interrupt.h>
#include "mce/dsp.h"


/* PCI DMA alignment */

#define DMA_ADDR_ALIGN 1024
#define DMA_ADDR_MASK (0xffffffff ^ (DMA_ADDR_ALIGN-1))


/* PCI register definitions */

#define HSTR_HRRQ 0x4
#define HSTR_HTRQ 0x2
#define HSTR_TRDY 0x1

/* DSP PCI vendor/device id */
#define DSP_VENDORID 0x1057
#define DSP_DEVICEID 0x1801


/* DSP fast interrupt vectors */

#define HCVR_INT_RST     0x8073 /* Clear DSP interrupt */
#define HCVR_INT_DON     0x8075 /* Clear interrupt flag */
#define HCVR_INT_RPC     0x808D /* Clear RP buffer flag */
#define HCVR_SYS_ERR     0x8077 /* Set fatal error flag */
#define HCVR_SYS_RST     0x808B /* Set fatal error flag */

#define PCI_MAX_FLUSH       256

#define DSP_PCI_MODE      0x900 /* for 32->24 bit conversion */


#pragma pack(1)

typedef struct {

	volatile u32 unused1[4];
	volatile u32 hctr;      // Host control register
	volatile u32 hstr;      // Host status register
	volatile u32 hcvr;      // Host command vector register(base+$018)
	volatile u32 htxr_hrxs; // Host transmit / receive data
	volatile u32 unused2[16384-32];

} dsp_reg_t;

#pragma pack()


struct dsp_dev_t {

	struct pci_dev *pci;

	dsp_reg_t *dsp;

	irq_handler_t int_handler;

};


/* Prototypes */

int   dsp_pci_init( char *dev_name );

int   dsp_pci_cleanup(void);

int   dsp_pci_proc(char *buf, int count);

int   dsp_pci_ioctl(unsigned int iocmd, unsigned long arg);

int   dsp_send_command_now( dsp_command *cmd );

int   dsp_send_command_now_vector( dsp_command *cmd, u32 vector );

int   dsp_pci_set_handler(struct pci_dev *pci,
			  irq_handler_t handler,
			  char *dev_name);

int   dsp_pci_flush(void);

int   dsp_pci_hstr(void);

void* dsp_allocate_dma(ssize_t size, unsigned long* bus_addr_p);

void  dsp_free_dma(void* buffer, int size, unsigned long bus_addr);

#endif
