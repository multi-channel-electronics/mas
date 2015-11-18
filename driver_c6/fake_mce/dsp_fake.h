#ifndef _DSP_PCI_H_
#define _DSP_PCI_H_

/* PCI DMA alignment */

#define DMA_ADDR_ALIGN 1024
#define DMA_ADDR_MASK (0xffffffff ^ (DMA_ADDR_ALIGN-1))


/* Prototypes */

int   dsp_fake_init( char *dev_name );

int   dsp_fake_cleanup(void);

/* int   dsp_send_command_now( dsp_command *cmd, int card ); */

/* void* dsp_allocate_dma(ssize_t size, unsigned int* bus_addr_p); */

/* void  dsp_free_dma(void* buffer, int size, unsigned int bus_addr); */

/* int fake_int_handler(dsp_message *msg); */

/* int dsp_pci_ioctl(unsigned int iocmd, unsigned long arg); */

/* int dsp_pci_proc(char *buf, int count); */

#endif
