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
/* Newer 2.6 kernels use IRQF_SHARED instead of SA_SHIRQ */
#  ifdef IRQF_SHARED
#    define IRQ_FLAGS IRQF_SHARED
#  else
#    define IRQ_FLAGS SA_SHIRQ
#  endif
#else
#  define IRQ_FLAGS 0
#endif

#include "dsp_pci.h"
#include "dsp_driver.h"
#include "mce/dsp_ioctl.h"

struct dsp_dev_t dsp_dev[MAX_CARDS];

/* Internal prototypes */

void  dsp_clear_interrupt(dsp_reg_t *dsp);

void  dsp_pci_remove(struct pci_dev *pci);

int   dsp_pci_probe(struct pci_dev *pci, const struct pci_device_id *id);


/* Data for PCI enumeration */

static const struct pci_device_id pci_ids[] = {
	{ PCI_DEVICE(DSP_VENDORID, DSP_DEVICEID) },
	{ 0 },
};

static struct pci_driver pci_driver = {
	.name = "mce_dsp",
	.id_table = pci_ids,
	.probe = dsp_pci_probe,
	.remove = dsp_pci_remove,
};



/*
  Each command is mapped to a particular DSP host control vector
  address.  Those can be firmware specific, but aren't so far.
*/

enum dsp_vector_type {
	VECTOR_STANDARD, // 4 words into htxr
	VECTOR_QUICK,    // no htxr, no reply expected
};

struct dsp_vector {
	u32 key;
	u32 vector;
	enum dsp_vector_type type;
};

#define NUM_DSP_CMD 14

static struct dsp_vector dsp_vector_set[NUM_DSP_CMD] = {
	{DSP_WRM, 0x8079, VECTOR_STANDARD},
	{DSP_RDM, 0x807B, VECTOR_STANDARD},
	{DSP_VER, 0x807B, VECTOR_STANDARD},
	{DSP_GOA, 0x807D, VECTOR_STANDARD},
	{DSP_STP, 0x807F, VECTOR_STANDARD},
	{DSP_RST, 0x8081, VECTOR_STANDARD},
	{DSP_CON, 0x8083, VECTOR_STANDARD},
	{DSP_HST, 0x8085, VECTOR_STANDARD},
	{DSP_RCO, 0x8087, VECTOR_STANDARD},
	{DSP_QTS, 0x8089, VECTOR_STANDARD},
	{DSP_INT_RST, HCVR_INT_RST, VECTOR_QUICK},
	{DSP_INT_DON, HCVR_INT_DON, VECTOR_QUICK},
	{DSP_SYS_ERR, HCVR_SYS_ERR, VECTOR_QUICK},
	{DSP_SYS_RST, HCVR_SYS_RST, VECTOR_QUICK},
};

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
	/* Note that the regs argument is deprecated in newer kernels,
	   do not use it.  It is left here for compatibility with
	   2.6.18-                                                    */

	struct dsp_dev_t *dev = dsp_dev;
	dsp_reg_t *dsp = dev->dsp;
	dsp_message msg;
	int i = 0;
	int k = 0; 
	int n = sizeof(msg) / sizeof(u32);

	PRINT_INFO(SUBNAME "interrupt entry irq=%#x\n", irq);

	for(k=0; k<MAX_CARDS; k++) {
		dev = dsp_dev + k;
		if (dev_id == dev) {
			dsp = dev->dsp;
			break;
		}
	}

	if (dev_id != dev) return IRQ_NONE;

	// Immediately clear interrupt bit
	dsp_write_hcvr(dsp, HCVR_INT_RST);

	// Read data into dsp_message structure
	while ( i<n && (dsp_read_hstr(dsp) & HSTR_HRRQ) ) {
		((u32*)&msg)[i++] = dsp_read_hrxs(dsp) & DSP_DATAMASK;
	}

	//Completed reads?
	if (i<n)
		PRINT_ERR(SUBNAME "could not obtain entire message.\n");
	
	PRINT_INFO(SUBNAME "%6x %6x %6x %6x\n",
		  msg.type, msg.command, msg.reply, msg.data);

	// Call the generic message handler
	dsp_int_handler(&msg, k);

	// At end, clear DSP handshake bit
	dsp_write_hcvr(dsp, HCVR_INT_DON);

/* 	// Clear DSP interrupt flags */
/* 	dsp_clear_interrupt(dsp); */

	PRINT_INFO(SUBNAME "ok\n");
	return IRQ_HANDLED;
}

#undef SUBNAME


void dsp_clear_interrupt(dsp_reg_t *dsp)
{
	// Clear interrupt flags
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

int dsp_send_command_now_vector(dsp_command *cmd, u32 vector, int card) 
{
	struct dsp_dev_t *dev = dsp_dev + card;
	int i = 0;
	int n = sizeof(dsp_command) / sizeof(u32);

	// HSTR must be ready to receive
	if ( !(dsp_read_hstr(dev->dsp) & HSTR_TRDY) ) {
		PRINT_ERR(SUBNAME "HSTR not ready to transmit!\n");
		return -EIO;
	}

	//Write bytes and interrupt
	while ( i<n && (dsp_read_hstr(dev->dsp) & HSTR_HTRQ)!=0 )
		dsp_write_htxr(dev->dsp, ((u32*)cmd)[i++]);

	if (i<n) {
		PRINT_ERR(SUBNAME "HTXR filled up during write! HSTR=%#x\n",
			  dsp_read_hstr(dev->dsp));
		return -EIO;
	}
	
	dsp_write_hcvr(dev->dsp, vector);

	return 0;
}

#undef SUBNAME


#define SUBNAME "dsp_quick_command: "

int dsp_quick_command(u32 vector, int card) 
{
	struct dsp_dev_t *dev = dsp_dev + card;
	PRINT_INFO(SUBNAME "sending vector %#x\n", vector);
	dsp_write_hcvr(dev->dsp, vector);
	return 0;
}

#undef SUBNAME


#define SUBNAME "dsp_lookup_vector: "

struct dsp_vector *dsp_lookup_vector(dsp_command *cmd)
{
	int i;
	for (i = 0; i < NUM_DSP_CMD; i++)
		if (dsp_vector_set[i].key == cmd->command)
			return dsp_vector_set+i;
	
	PRINT_ERR(SUBNAME "could not identify command %#x\n",
		  cmd->command);
	return NULL;
}

#undef SUBNAME


#define SUBNAME "dsp_send_command_now: "

int dsp_send_command_now(dsp_command *cmd, int card) 
{
	struct dsp_vector *vect = dsp_lookup_vector(cmd);

	PRINT_INFO(SUBNAME "cmd=%06x\n", cmd->command);

	if (vect==NULL) return -ERESTARTSYS;
	
	switch (vect->type) {

	case VECTOR_STANDARD:
		return dsp_send_command_now_vector(cmd, vect->vector, card);

	case VECTOR_QUICK:
		// FIXME: these don't reply so they'll always time out.
		return dsp_quick_command(vect->vector, card);

	}

	PRINT_ERR(SUBNAME
		  "unimplemented vector command type %06x for command %06x\n",
		  vect->type, cmd->command);
	return -ERESTARTSYS;
}

#undef SUBNAME


	

/*
  Memory allocation

  This is lame, but DMA-able memory likes a pci device in old kernels.
*/


void* dsp_allocate_dma(ssize_t size, unsigned int* bus_addr_p)
{
//ISSUE: mce will call these with out card info currently
#ifdef OLD_KERNEL
	return pci_alloc_consistent(dev->pci, size, bus_addr_p);
#else
	return dma_alloc_coherent(NULL, size, (dma_addr_t*)bus_addr_p,
				  GFP_KERNEL);
#endif
}

void  dsp_free_dma(void* buffer, int size, unsigned int bus_addr)
{
//ISSUE: mce will call these with out card info currently
#ifdef OLD_KERNEL
 	  pci_free_consistent (dev->pci, size, buffer, bus_addr);
#else
	  dma_free_coherent(NULL, size, buffer, bus_addr);
#endif
}


int dsp_pci_flush()
{
	//ISSUE: unknown caller & needs card info
	struct dsp_dev_t *dev = dsp_dev;
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

int dsp_pci_remove_handler(struct dsp_dev_t *dev)
{
	struct pci_dev *pci = dev->pci;

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

int dsp_pci_set_handler(struct dsp_dev_t *dev,
			irq_handler_t handler,
			char *dev_name)
{
	struct pci_dev *pci = dev->pci;
	int err = 0;
	int cfg_irq = 0;

	if (pci==NULL || dev==NULL) {
		PRINT_ERR(SUBNAME "Null pointers! pci=%lx dev=%lx\n",
			  (long unsigned)pci, (long unsigned)dev);
		return -ERESTARTSYS;
	}
	
	pci_read_config_byte(pci, PCI_INTERRUPT_LINE, (char*)&cfg_irq);
	PRINT_INFO(SUBNAME "pci has irq %i and config space has irq %i\n",
		   pci->irq, cfg_irq);

	// Free existing handler
	if (dev->int_handler!=NULL)
		dsp_pci_remove_handler(dev);
	
#ifdef REALTIME
	PRINT_ERR(SUBNAME "request REALTIME irq %#x\n", pci->irq);
	rt_disable_irq(pci->irq);
	err = rt_request_global_irq(pci->irq, (void*)handler);
#else
	PRINT_INFO(SUBNAME "requesting irq %#x\n", pci->irq);
	err = request_irq(pci->irq, handler, IRQ_FLAGS, dev_name, dev);
#endif

	if (err!=0) {
		PRINT_ERR(SUBNAME "irq request failed with error code %#x\n",
			  -err);
		return err;
	}

#ifdef REALTIME
	rt_startup_irq(pci->irq);
	rt_enable_irq(pci->irq);
#endif

	dev->int_handler = handler;
	return 0;
}

#undef SUBNAME


#define SUBNAME "dsp_probe: "

int dsp_pci_probe(struct pci_dev *pci, const struct pci_device_id *id)
{
	struct dsp_dev_t *dev = dsp_dev;
	int err = 0;
	int i = 0;

	PRINT_INFO(SUBNAME "entry\n");

	for(i=0; i<MAX_CARDS; i++) {
		dev = dsp_dev + i;
		if(dev->pci == NULL) break;
	} 	

	if (pci==NULL) {
		PRINT_ERR(SUBNAME "Called with NULL pci_dev!\n");
		err = -EPERM;
		goto fail;
	}

	if (dev->pci != NULL) {
		PRINT_ERR(SUBNAME "called after device configured or dsp_dev[] is full.\n");
		err = -EPERM;
		goto fail;
	}

	err = pci_enable_device(pci);
	if (err) goto fail;

	// Map i/o registers into a dsp_reg_t structure
	dev->dsp = (dsp_reg_t *)ioremap(pci_resource_start(pci, 0) & 
					PCI_BASE_ADDRESS_MEM_MASK,
					sizeof(*dev->dsp));
	if (dev->dsp==NULL) {
		PRINT_ERR(SUBNAME "Could not map PCI registers!\n");
		err = -EIO;
		goto fail;
	}

	// Mark PCI card as bus master
	pci_set_master(pci);
	
	// Configure the card for our purposes
	dsp_pci_configure(dev->dsp);

	// Mark dev->pci as non-null to show successful config.
	dev->pci = pci;

	// Install the interrupt handler (cast necessary for backward compat.)
	dev->int_handler = NULL;
	err = dsp_pci_set_handler(dev, (irq_handler_t)pci_int_handler,
				  "mce_dsp");
	if (err) goto fail;

	// Call init function for higher levels.
	dsp_driver_probe(i);

	PRINT_INFO(SUBNAME "ok\n");
	return 0;

fail:
	PRINT_ERR(SUBNAME "failed with code %i\n", err);
	return -1;
}

#undef SUBNAME

#define SUBNAME "dsp_pci_remove: "

void dsp_pci_remove(struct pci_dev *pci)
{
	int i = 0;

	PRINT_INFO(SUBNAME "entry\n");

	if (pci == NULL) {
		PRINT_ERR(SUBNAME "called with null pointer!\n");
		return;
	}
	
	for(i=0; i < MAX_CARDS; i++) {
		struct dsp_dev_t *dev = dsp_dev + i;
		
		if(pci == dev->pci) {
			
			// Disable higher-level features first
			dsp_driver_remove(i);
			
			// Remove int handler, clear remaining ints, unmap i/o memory
			dsp_pci_remove_handler(dev);
			
			if (dev->dsp!=NULL) {
				dsp_clear_interrupt(dev->dsp);
				iounmap(dev->dsp);
				dev->dsp = NULL;
			}
			
			dev->pci = NULL;
			PRINT_INFO(SUBNAME "ok\n");
			return;
		}
	}

	PRINT_ERR(SUBNAME "called with unknown device!\n");
	return;
}

#undef SUBNAME

#define SUBNAME "dsp_pci_init: "

int dsp_pci_init(char *dev_name)
{
	struct dsp_dev_t *dev = dsp_dev;
	int err = 0;
	int i = 0;

	PRINT_INFO(SUBNAME "entry\n");

	for( i = 0; i < MAX_CARDS; i++) {
		dev = dsp_dev + i;
		memset(dev, 0, sizeof(dev));
	}

	err = pci_register_driver(&pci_driver);

	if (err) {
		PRINT_ERR(SUBNAME
			  "pci_register_driver failed with code %i.\n", err);
		return -1;
	}			  

	PRINT_INFO(SUBNAME "ok\n");
	return 0;
}

#undef SUBNAME

#define SUBNAME "dsp_pci_cleanup: "

int dsp_pci_cleanup()
{
	int i = 0;
	
	PRINT_INFO(SUBNAME "entry\n");

	pci_unregister_driver(&pci_driver);

	for(i=0; i < MAX_CARDS; i++) {
		struct dsp_dev_t *dev = dsp_dev + i;
		if ( dev->pci != NULL ) {
			PRINT_ERR(SUBNAME "failed to zero dev->pci for card; %i!\n", i);
		}
	}

	PRINT_INFO(SUBNAME "ok\n");

	return 0;
		
}

#undef SUBNAME


#define SUBNAME "dsp_driver_ioctl: "

int dsp_pci_ioctl(unsigned int iocmd, unsigned long arg, int card)
{
	struct dsp_dev_t *dev = dsp_dev + card;

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
			/* Cast on handler is necessary for backward compat. */
			if (dsp_pci_set_handler(dev, (irq_handler_t)pci_int_handler,
						"mce_hacker") < 0) {
				PRINT_ERR(SUBNAME "Could not install interrupt handler!\n");
				return -1;
			}
		} else {
			PRINT_IOCT(SUBNAME "Disabling interrupt\n");
			dsp_pci_remove_handler(dev);
		}
		break;

	default:
		PRINT_ERR(SUBNAME "I don't handle iocmd=%ui\n", iocmd);
		return -1;
	}

	return 0;
}

#undef SUBNAME


int dsp_pci_proc(char *buf, int count)
{
	struct dsp_dev_t *dev = dsp_dev;

	int len = 0;
	if (len < count) {
		len += sprintf(buf+len, "    hstr:     %#06x\n"
			                "    hctr:     %#06x\n",
			       dsp_read_hstr(dev->dsp),
			       dsp_read_hctr(dev->dsp));
	}
	
	return len;
}
