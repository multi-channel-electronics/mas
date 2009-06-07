#ifndef _DSP_DRIVER_H_
#define _DSP_DRIVER_H_

#include <linux/interrupt.h>
#include "mce/dsp.h"
#include "mce/dsp_errors.h"

#ifdef FAKEMCE
#  include "fake_mce/dsp_fake.h"
#endif

#define DSPDEV_NAME "mce_dsp"
#define DSP_DEFAULT_TIMEOUT (HZ*500/1000)

/* DSP code versions */

#define DSP_U0103          0x550103
#define DSP_U0104          0x550104
#define DSP_U0105          0x550105


/* PCI DMA alignment */

#define DMA_ADDR_ALIGN 1024
#define DMA_ADDR_MASK (0xffffffff ^ (DMA_ADDR_ALIGN-1))

/* PCI register definitions */

#define HSTR_HC3  0x8
#define HSTR_HRRQ 0x4
#define HSTR_HTRQ 0x2
#define HSTR_TRDY 0x1

#define HCTR_HF0  0x08        /* Hand-shake bit */
#define HCTR_HF1  0x10        /* Enable hand-shaking */
#define HCTR_HF2  0x20        /* Disable DSP interrupts (polling mode) */

/* DSP PCI vendor/device id */

#define DSP_VENDORID 0x1057
#define DSP_DEVICEID 0x1801

/* DSP interrupt vectors */

#define HCVR_HC          0x0001               /* Do-this-now bit */
#define HCVR_HNMI        0x8000               /* Non-maskable HI-32 interrupt */

#define HCVR_INT_RST     (HCVR_HNMI | 0x0073) /* Clear DSP interrupt */
#define HCVR_INT_DON     (HCVR_HNMI | 0x0075) /* Clear interrupt flag */
#define HCVR_SYS_ERR     (HCVR_HNMI | 0x0077) /* Set fatal error flag */
#define HCVR_SYS_RST     (HCVR_HNMI | 0x008B) /* Immediate system reset */
#define HCVR_INT_RPC     (HCVR_HNMI | 0x008D) /* Clear RP buffer flag */
#define HCVR_SYS_MODE    (HCVR_HNMI | 0x008F) /* Set DSP mode */


/* HCTR default */

#define DSP_PCI_MODE      0x900 /* for 32->24 bit conversion */


#define PCI_MAX_FLUSH       256

/* Soft interrupt generation timer frequency */

#define DSP_POLL_FREQ       100
#define DSP_POLL_JIFFIES    (HZ / DSP_POLL_FREQ + 1)

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

typedef enum { DSP_PCI, DSP_POLL } dsp_int_mode;

typedef int (*dsp_callback)(int, dsp_message*, int);

typedef int (*dsp_handler)(dsp_message *, unsigned long data);

/* Prototypes */

int   dsp_send_command(dsp_command *cmd, dsp_callback callback,
		       int card);

int   dsp_send_command_wait(dsp_command *cmd, dsp_message *msg,
			    int card);

void  dsp_clear_RP(int card);

int   dsp_driver_ioctl(unsigned int iocmd, unsigned long arg, int card);

int   dsp_proc(char *buf, int count, int card);

int   dsp_set_msg_handler(u32 code, dsp_handler handler, unsigned long data,
			  int card);

int   dsp_clear_handler(u32 code, int card);

int   dsp_pci_flush(void);

int   dsp_pci_hstr(void);

void* dsp_allocate_dma(ssize_t size, unsigned long* bus_addr_p);

void  dsp_free_dma(void* buffer, int size, unsigned long bus_addr);

#endif
