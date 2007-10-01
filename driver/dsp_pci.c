/*
  dsp_pci.c

  Contains all PCI related code, including register definitions and
  lowest level i/o routines.

  Spoofing can be accomplished at this level by setting up alternate
  handlers for reads and writes to the PCI card.
*/

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/interrupt.h>

#include "mce_options.h"
#include "kversion.h"

#ifdef REALTIME
#include <rtai.h>
#endif

#ifndef OLD_KERNEL
#  include <linux/dma-mapping.h>
#  define IRQ_FLAGS SA_SHIRQ
#else
#  define IRQ_FLAGS 0
#endif

#include "dsp_pci.h"
#include "dsp_driver.h"
#include <dsp_ioctl.h>

struct dsp_dev_t dsp_dev;
struct dsp_dev_t *dev = &dsp_dev;


/*
  Each command is mapped to a particular DSP host control vector
  address.  Those can be firmware specific, but aren't so far.
*/

struct dsp_vector {
	u32 key;
	u32 vector;
};

#define NUM_DSP_CMD 8

static struct dsp_vector dsp_vector_set[NUM_DSP_CMD] = {
	{DSP_WRM, 0x8079},
	{DSP_RDM, 0x807B},
	{DSP_GOA, 0x807D},
	{DSP_STP, 0x807F},
	{DSP_RST, 0x8081},
	{DSP_CON, 0x8083},
	{DSP_HST, 0x8085},
	{DSP_RCO, 0x8087}
};


/* Internal prototypes */

void  dsp_clear_interrupt(dsp_reg_t *dsp);



/* DSP register wrappers */

static inline int dsp_read_hrxs(dsp_reg_t *dsp) {
	return ioread32((void*)&(dsp->htxr_hrxs));
}

static inline int dsp_read_hstr(dsp_reg_t *dsp) {
	return ioread32((void*)&(dsp->hstr));
}

static inline int dsp_read_hctr(dsp_reg_t *dsp) {
	return ioread32((void*)&(dsp->hctr));
}

static inline void dsp_write_htxr(dsp_reg_t *dsp, u32 value) {
	iowrite32(value, (void*)&(dsp->htxr_hrxs));
}

static inline void dsp_write_hcvr(dsp_reg_t *dsp, u32 value) {
	iowrite32(value, (void*)&(dsp->hcvr));
}

static inline void dsp_write_hctr(dsp_reg_t *dsp, u32 value) {
	iowrite32(value, (void*)&(dsp->hctr));
}


#define SUBNAME "pci_int_handler: "

irqreturn_t pci_int_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	dsp_message msg;
	u32 *msg_ptr = (u32*)&msg;
	dsp_reg_t *dsp = dev->dsp;
	int n_reads = sizeof(msg) / sizeof(u32);

#ifdef REALTIME
	PRINT_INFO(SUBNAME "REALTIME interrupt entry irq=%#x\n", irq);
#else	
	PRINT_INFO(SUBNAME "interrupt entry irq=%#x\n", irq);
	if (dev_id != dev) return IRQ_NONE;
#endif

	// Clear DSP interrupt flags
	dsp_clear_interrupt(dsp);

	// Read data into dsp_message structure
	while ( (dsp_read_hstr(dsp) & HSTR_HRRQ) && n_reads--) {
		*(msg_ptr++) = dsp_read_hrxs(dsp) & DSP_DATAMASK;
	}

	//Completed reads?
	if (n_reads)
		PRINT_ERR(SUBNAME "could not obtain entire message.\n");
	
	PRINT_INFO(SUBNAME "%6x %6x %6x %6x\n",
		  msg.type, msg.command, msg.reply, msg.data);

	// Call the generic message handler
	dsp_int_handler( &msg );

	return IRQ_HANDLED;
}

#undef SUBNAME


void dsp_clear_interrupt(dsp_reg_t *dsp)
{
	// Ensure PCI interrupt flags are clear
	dsp_write_hcvr(dsp, HCVR_INT_RST);
	dsp_write_hcvr(dsp, HCVR_INT_DON);
}



/*
  DSP command sending framework

  Modules issue dsp commands and register a callback function with

  dsp_send_command_now

  This looks up the vector address by calling dsp_lookup_vector,
  registers the callback in the device structure, and issues the
  command to the correct vector by calling
  dsp_send_command_now_vector.
*/


#define SUBNAME "dsp_send_command_now_vector: "

int dsp_send_command_now_vector(dsp_command *cmd, u32 vector) 
{
	int i = 0;
	int n = sizeof(dsp_command) / sizeof(u32);
       
	// HSTR must be ready to receive
	if ( !(dsp_read_hstr(dev->dsp) & HSTR_TRDY) ) {
		PRINT_ERR(SUBNAME "HSTR not ready to recieve!\n");
		return -1;
	}

	//Write bytes and interrupt
	while ( i<n && (dsp_read_hstr(dev->dsp) & HSTR_HTRQ)!=0 )
		dsp_write_htxr(dev->dsp, ((u32*)cmd)[i++]);

	if (i<n) {
		PRINT_ERR(SUBNAME "HTXR filled up during write! HSTR=%#x\n",
			  dsp_read_hstr(dev->dsp));
		return -1;
	}
	
	dsp_write_hcvr(dev->dsp, vector);

	return 0;
}

#undef SUBNAME


#define SUBNAME "dsp_lookup_vector: "

int dsp_lookup_vector(dsp_command *cmd)
{
	int i;
	for (i = 0; i < NUM_DSP_CMD; i++)
		if (dsp_vector_set[i].key == cmd->command)
			return dsp_vector_set[i].vector;

	PRINT_ERR(SUBNAME "could not identify command %#x\n",
		  cmd->command);
	return -1;
}

#undef SUBNAME


#define SUBNAME "dsp_send_command_now: "

int dsp_send_command_now(dsp_command *cmd) 
{
	int vector = dsp_lookup_vector(cmd);
	if (vector<0) return -1;
	return dsp_send_command_now_vector(cmd, vector);
}

#undef SUBNAME


	

/*
  Memory allocation

  This is lame, but DMA-able memory likes a pci device in old kernels.
*/


void* dsp_allocate_dma(ssize_t size, unsigned int* bus_addr_p)
{
#ifdef OLD_KERNEL
	return pci_alloc_consistent(dev->pci, size, bus_addr_p);
#else
	return dma_alloc_coherent(NULL, size, bus_addr_p, GFP_KERNEL);
#endif
}

void  dsp_free_dma(void* buffer, int size, unsigned int bus_addr)
{
#ifdef OLD_KERNEL
 	  pci_free_consistent (dev->pci, size, buffer, bus_addr);
#else
	  dma_free_coherent(NULL, size, buffer, bus_addr);
#endif
}


int dsp_pci_flush()
{
	dsp_reg_t *dsp = dev->dsp;

	int count = 0, tmp;

	PRINT_INFO("dsp_pci_flush:");
	while ((dsp_read_hstr(dsp) & HSTR_HRRQ) && (count++ < PCI_MAX_FLUSH))
	{
		tmp = dsp_read_hrxs(dsp);
		if (count<4) PRINT_INFO(" %x", tmp);
		else if (count==4) PRINT_INFO(" ...");
	}

	PRINT_INFO("\n");

	if (dsp_read_hstr(dsp) & HSTR_HRRQ) {
		PRINT_ERR("dsp_pci_flush: could not empty HRXS!\n");
		return -1;
	}

	return 0;
}

int dsp_pci_configure(dsp_reg_t *dsp)
{
	dsp_clear_interrupt(dsp);

	dsp_write_hctr(dsp, DSP_PCI_MODE);

	return 0;
}


#define SUBNAME "dsp_pci_remove_handler: "

int dsp_pci_remove_handler(struct pci_dev *pci)
{
	if (dev->int_handler==NULL) {
		PRINT_INFO(SUBNAME "no handler installed\n");
		return 0;
	}

#ifdef REALTIME
	rt_disable_irq(pci->irq);
	rt_free_global_irq(pci->irq);
#else
	free_irq(pci->irq, dev);
#endif
	dev->int_handler = NULL;

	return 0;
}

#undef SUBNAME


#define SUBNAME "dsp_pci_set_handler: "

/******

 dsp_pci_set_handler

 Do not try to change the IRQ.  It's not permitted.

******/


int dsp_pci_set_handler(struct pci_dev *pci,
			irqreturn_t(*handler)(int,void*,struct pt_regs*),
			char *dev_name)
{
	int err = 0;

	if (pci==NULL || dev==NULL) {
		PRINT_ERR(SUBNAME "Null pointers! pci=%lx dev=%lx\n",
			  (long unsigned)pci, (long unsigned)dev);
		return -1;
	}
	
	// Free existing handler
	if (dev->int_handler!=NULL)
		dsp_pci_remove_handler(pci);
	
#ifdef REALTIME
	PRINT_ERR(SUBNAME "request REALTIME irq %#x\n", new_irq);
	rt_disable_irq(new_irq);
	err = rt_request_global_irq(new_irq, (void*)handler);
#else
	PRINT_INFO(SUBNAME "requesting irq %#x\n", new_irq);
	err = request_irq(pci->irq, handler, IRQ_FLAGS, dev_name, dev);
#endif

	if (err!=0) {
		PRINT_ERR(SUBNAME "irq request failed with error code %#x\n",
			  -err);
		return -1;
	}

#ifdef REALTIME
	rt_startup_irq(pci->irq);
	rt_enable_irq(pci->irq);
#endif

	dev->int_handler = handler;

	return 0;
}

#undef SUBNAME


#define SUBNAME "dsp_pci_init: "

int dsp_pci_init(char *dev_name)
{
	int err = -1;
	PRINT_INFO(SUBNAME "entry\n");

	dev->int_handler = NULL;

#ifdef OLD_KERNEL
	dev->pci = (struct pci_dev *)
		pci_find_device(DSP_VENDORID, DSP_DEVICEID, NULL);
# else
	dev->pci = (struct pci_dev *)
		pci_get_device(DSP_VENDORID, DSP_DEVICEID, NULL);
# endif

	if (dev->pci==NULL) {
		PRINT_ERR(SUBNAME "Could not find PCI card!\n");
		goto out;
	}

	// Map i/o registers into a dsp_reg_t structure
	dev->dsp = (dsp_reg_t *)ioremap(pci_resource_start(dev->pci, 0) & 
					PCI_BASE_ADDRESS_MEM_MASK,
					sizeof(*dev->dsp));
	if (dev->dsp==NULL) {
		PRINT_ERR(SUBNAME "Could not map PCI registers!\n");
		goto out;
	}

	// Mark PCI card as bus master
	pci_set_master(dev->pci);
	
	// Enable card burst mode?  Sure!
	if (pci_set_mwi(dev->pci)) return -3;

	// Set up 32->24 bit communication channels
	dsp_pci_configure(dev->dsp);

	// Install the interrupt handler
	if (dsp_pci_set_handler(dev->pci, pci_int_handler, dev_name) < 0) {
		PRINT_ERR(SUBNAME "Could not install interrupt handler!\n");
		goto out;
	}

	err = 0;
 out:
	if (err==0) PRINT_INFO(SUBNAME "ok\n");
	return err;
}

#undef SUBNAME


int dsp_pci_cleanup()
{
	if ( dev->pci == NULL )
		return 0;
		
	// Remove int handler, clear remaining ints, unmap i/o memory
	dsp_pci_remove_handler(dev->pci);

	if (dev->dsp!=NULL) {
		dsp_clear_interrupt(dev->dsp);
		iounmap(dev->dsp);
		dev->dsp = NULL;
	}

#ifndef OLD_KERNEL
	pci_dev_put(dev->pci);
#endif

	dev->pci = NULL;
	return 0;
}


#define SUBNAME "dsp_driver_ioctl: "

int dsp_pci_ioctl(unsigned int iocmd, unsigned long arg)
{
	switch (iocmd) {
	case DSPDEV_IOCT_CORE:
		
		if (dev->pci == NULL) {
			PRINT_IOCT(SUBNAME "pci structure is null\n");
			return 0;
		}
		if (dev->dsp == NULL) {
			PRINT_IOCT(SUBNAME
				   "pci-dsp memory structure is null\n");
			return 0;
		}
		PRINT_IOCT(SUBNAME "hstr=%#06x hctr=%#06x\n",
			   dsp_read_hstr(dev->dsp), dsp_read_hctr(dev->dsp));
		
		break;

	case DSPDEV_IOCT_CORE_IRQ:
		if (arg) {
			PRINT_IOCT(SUBNAME "Enabling interrupt\n");
			if (dsp_pci_set_handler(dev->pci, pci_int_handler,
						"mce_hacker") < 0) {
				PRINT_ERR(SUBNAME "Could not install interrupt handler!\n");
				return -1;
			}
		} else {
			PRINT_IOCT(SUBNAME "Disabling interrupt\n");
			dsp_pci_remove_handler(dev->pci);
		}
		break;

	default:
		PRINT_ERR(SUBNAME "I don't handle iocmd=%ui\n", iocmd);
		return -1;
	}

	return 0;
}

#undef SUBNAME
