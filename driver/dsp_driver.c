/* -*- mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *      vim: sw=8 ts=8 et tw=80
 */

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/sched.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/sched.h>

#include "mce_options.h"
#include "kversion.h"
#include "version.h"

#ifdef BIGPHYS
# include <linux/bigphysarea.h>
#endif

#include <linux/dma-mapping.h>
/* Newer 2.6 kernels use IRQF_SHARED instead of SA_SHIRQ */
#ifdef IRQF_SHARED
#    define IRQ_FLAGS IRQF_SHARED
#else
#    define IRQ_FLAGS SA_SHIRQ
#endif

#include "dsp_driver.h"
#include "mce/dsp_ioctl.h"

/*
 *  dsp_driver.c includes
 */

#include <linux/module.h>
#include <linux/init.h>

#include <asm/uaccess.h>

#include "mce_driver.h"
#include "dsp_ops.h"
#include "proc.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR ("Matthew Hasselfield"); 


/* Internal prototypes */

void  mcedsp_remove(struct pci_dev *pci);

int   mcedsp_probe(struct pci_dev *pci, const struct pci_device_id *id);


/* Data for PCI enumeration */

static const struct pci_device_id pci_ids[] = {
	{ PCI_DEVICE(DSP_VENDORID, DSP_DEVICEID) },
	{ 0 },
};

static struct pci_driver pci_driver = {
	.name = DEVICE_NAME,
	.id_table = pci_ids,
	.probe = mcedsp_probe,
	.remove = mcedsp_remove,
};


/* DSP register wrappers */

static inline volatile int dsp_read_hrxs(dsp_reg_t *dsp) {
	return ioread32((void*)&(dsp->htxr_hrxs));
}

static inline int dsp_read_hstr(dsp_reg_t *dsp) {
	return ioread32((void*)&(dsp->hstr));
}

static inline int dsp_read_hcvr(dsp_reg_t *dsp) {
	return ioread32((void*)&(dsp->hcvr));
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

int dsp_proc(char *buf, int count, int card)
{
        int len = 0;
        if (count < 1024) {
/*                
	struct dsp_dev_t *dev = dsp_dev + card;
	int len = 0;

        PRINT_INFO(card, "card = %d\n", card);
	if (dev->pci == NULL) 
		return len;
	if (len < count) {
		len += sprintf(buf+len, "    %-15s %25s\n",
			       "bus address:", pci_name(dev->pci));
	}
	if (len < count) {
		len += sprintf(buf+len, "    %-15s %25s\n",
			       "interrupt:",
			       (dev->comm_mode & DSP_PCI_MODE_NOIRQ) ? "polling" : "enabled");
	}
	if (len < count) {
		len += sprintf(buf+len, "    %-32s %#08x\n    %-32s %#08x\n"
			       "    %-32s %#08x\n",
			       "hstr:", dsp_read_hstr(dev->dsp),
			       "hctr:", dsp_read_hctr(dev->dsp),
			       "hcvr:", dsp_read_hcvr(dev->dsp));
	}
	if (len < count) {
		len += sprintf(buf+len, "    %-20s %20s\n",
			       "firmware version:", dev->version_string);
	}
	if (len < count) {
		len += sprintf(buf+len, "    %-30s ", "state:");
		if (dev->state & DDAT_CMD)
			len += sprintf(buf+len, "commanded");
		else
			len += sprintf(buf+len, "     idle");
		if (dev->state & DDAT_RESERVE)
			len += sprintf(buf+len, " (reserved)\n");
		else
			len += sprintf(buf+len, "\n");
	}
*/
        }
	return len;
}




/*****

  NEW

*****/

#include "test/dspioctl.h"
#include "test/new_dsp.h"

typedef struct {

	void*     base;
	unsigned long base_busaddr;
        size_t    size;

        // New data is written at head, consumer data is read at tail.
	volatile
	int       head_index;
	volatile
	int       tail_index;

	// Semaphore should be held when modifying structure, but
	// interrupt routines may modify head_index at any time.

        spinlock_t lock;
	wait_queue_head_t queue;

	// Device lock - controls read access on data device and
	// provides a system for checking whether the system is
	// mid-acquisition.  Should be NULL (for idle) or pointer to
        // reader's filp (for locking).

	void *data_lock;	

} frame_buffer_t;


typedef enum {
        DSP_IDLE = 0,
        DSP_CMD_QUED = 1,
        DSP_CMD_SENT = 2,
        DSP_REP_RECD = 3,
        DSP_REP_HNDL = 4,
        DSP_IGNORE = 1000
} dsp_state_t;

typedef enum {
        MCE_IDLE = 0,
        MCE_CMD_QUED = 1,
        MCE_CMD_SENT = 2,
        MCE_REP_RECD = 3,
        MCE_REP_HNDL = 4,
        MCE_IGNORE = 1000
} mce_state_t;


typedef struct {
        int minor;
        int enabled;

	struct pci_dev *pci;
	dsp_reg_t *reg;

        /* int reply_recd;  // state... 0 is idle, 1 is reqd, 2 is recd. */
        /* struct dsp_reply reply; */
        struct dsp_datagram reply;
        struct dsp_datagram mce_reply;
        struct dsp_datagram buffer_update;
        

        /* __s32 *data_buffer; */
        /* int data_head; */
        /* int data_tail; */
        /* int data_size; */

        frame_buffer_t dframes;

        int reply_buffer_size;
        void* reply_buffer_dma_virt;
        dma_addr_t reply_buffer_dma_handle;

        /* void* reply_buffer; */

        spinlock_t lock;
	wait_queue_head_t queue;
	struct timer_list dsp_timer;
	struct timer_list mce_timer;
        dsp_state_t dsp_state;
        mce_state_t mce_state;

	/* struct semaphore sem; */

	int error;

	/* dsp_ops_state_t state; */

	/* dsp_message msg; */
	/* dsp_command cmd; */
        
} mcedsp_t;

static mcedsp_t dspdata[MAX_CARDS];
static int major = 0;


#define DSP_LOCK_DECLARE_FLAGS unsigned long irqflags
#define DSP_LOCK    spin_lock_irqsave(&dsp->lock, irqflags)
#define DSP_UNLOCK  spin_unlock_irqrestore(&dsp->lock, irqflags)
/* #define DSP_LOCK_DECLARE_FLAGS  */
/* #define DSP_LOCK  */
/* #define DSP_UNLOCK  */


int data_alloc(mcedsp_t *dsp, int mem_size)
{
	frame_buffer_t *dframes = &dsp->dframes;
	int npg = (mem_size + PAGE_SIZE-1) / PAGE_SIZE;
        dma_addr_t bus;

        PRINT_INFO(dsp->minor, "entry\n");

	mem_size = npg * PAGE_SIZE;

#ifdef BIGPHYS
        // Virtual address?
	dframes->base = bigphysarea_alloc_pages(npg, 0, GFP_KERNEL);
        PRINT_ERR(dsp->minor, "BIGPHYS selected\n");

	if (dframes->base==NULL) {
                PRINT_ERR(dsp->minor, "bigphysarea_alloc_pages failed!\n");
		return -ENOMEM;
	}

	// Save physical address for hardware
	// Note virt_to_bus is on the deprecation list... we will want
	// to switch to the DMA-API, dma_map_single does what we want.
        bus = virt_to_bus(virt);
	dframes->base_busaddr = bus;

	// Save the maximum size
	dframes->size = mem_size;
#else

        dframes->size = mem_size;
        dframes->base = dma_alloc_coherent(
                &dsp->pci->dev, dframes->size, &bus, GFP_KERNEL);
        dframes->base_busaddr = (unsigned long)bus;

#endif

	//Debug
        PRINT_INFO(dsp->minor, "buffer: base=%lx, size %li\n",
		   (unsigned long)dframes->base, 
		   (long)dframes->size);
	
	return 0;
}

void data_free(mcedsp_t *dsp)
{
        if (dsp->dframes.base != NULL) {
#ifdef BIGPHYS
                bigphysarea_free_pages(dsp->dframes.base);
#else
                dma_free_coherent(&dsp->pci->dev, dsp->dframes.size,
                                  dsp->dframes.base,
                                  (dma_addr_t)dsp->dframes.base_busaddr);
#endif
        }
                
        dsp->dframes.base = NULL;
}


static void inline mcedsp_set_state_wake(mcedsp_t *dsp,
                                         dsp_state_t new_dsp_state,
                                         mce_state_t new_mce_state) {
        DSP_LOCK_DECLARE_FLAGS;
        DSP_LOCK;
        if (new_dsp_state < DSP_IGNORE) {
                PRINT_ERR(dsp->minor, "%i -> dsp_state\n", new_dsp_state);
                dsp->dsp_state = new_dsp_state;
        }
        if (new_mce_state < MCE_IGNORE) {
                PRINT_ERR(dsp->minor, "%i -> mce_state\n", new_mce_state);
                dsp->mce_state = new_mce_state;
        }
        wake_up_interruptible(&dsp->queue);
        DSP_UNLOCK;
}


irqreturn_t mcedsp_int_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	/* Note that the regs argument is deprecated in newer kernels,
	 * do not use it.  It is left here for compatibility with
	 * -2.6.18                                                    */

        DSP_LOCK_DECLARE_FLAGS;
	mcedsp_t *dsp = dev_id;
        int k;
        int hctr;
        int n_wait;
        int n_wait_max;

        int report_packet = 1;

        int copy_size;
        struct dsp_datagram *gramlet;
        struct dsp_datagram gram;

        // Reject null devs
        if (dsp == NULL) {
                PRINT_ERR(NOCARD, "IRQ with null device\n");
                return IRQ_NONE;
        }

        // Is this really one of ours?
	for(k=0; k<MAX_CARDS; k++) {
                if (dsp == dspdata+k)
                        break;
        }
        if (k==MAX_CARDS)
                return IRQ_NONE;

        PRINT_INFO(dsp->minor, "Entry.\n");

	if (dsp == NULL || dsp->enabled == 0) {
                PRINT_ERR(NOCARD, "IRQ on disabled device\n");
                return IRQ_NONE;
        }

	// Confirm that device has raised interrupt
        n_wait = 0;
        n_wait_max = 100000;
        do {
                if (dsp_read_hstr(dsp->reg) & HSTR_HINT)
                        break;
        } while (++n_wait < n_wait_max);
        if (n_wait >= n_wait_max) {
                PRINT_ERR(dsp->minor,
                          "HINT did not rise in %i cycles; ignoring\n",
                          n_wait);
                return IRQ_NONE;
        }
        if (n_wait != 0) {
                PRINT_ERR(dsp->minor,
                          "HINT took %i cycles to rise; accepting\n",
                          n_wait);
	}

        PRINT_INFO(dsp->minor, "IRQ owned.\n");

        // Hand shake up...
        hctr = dsp_read_hctr(dsp->reg);
        dsp_write_hctr(dsp->reg, hctr | HCTR_HF0);

        // Inspect the header
        gramlet = dsp->reply_buffer_dma_virt;
        copy_size = sizeof(dsp->reply);
        if (gramlet->total_size < copy_size)
                copy_size = gramlet->total_size;

        // Copy into temporary storage...
        memcpy(&gram, dsp->reply_buffer_dma_virt,
               copy_size);

        switch (gram.type) {
        case DGRAM_TYPE_DSP_REP:
                /* DSP reply */
                DSP_LOCK;
                switch(dsp->dsp_state) {
                case DSP_CMD_SENT:
                        report_packet = 0;
                        memcpy(&dsp->reply, &gram, copy_size);
                        dsp->dsp_state = DSP_REP_RECD;
                        PRINT_ERR(dsp->minor, "%i -> dsp_state\n",
                                  dsp->dsp_state);
                        break;
                default:
                        PRINT_ERR(dsp->minor,
                                  "unexpected reply packet (state=%i)\n",
                                  dsp->dsp_state);
                        dsp->dsp_state = DSP_IDLE;
                }
                wake_up_interruptible(&dsp->queue);
                DSP_UNLOCK;
                break;
        case DGRAM_TYPE_MCE_REP:
                /* MCE reply */
                DSP_LOCK;
                switch(dsp->mce_state) {
                case MCE_CMD_SENT:
                        report_packet = 0;
                        memcpy(&dsp->mce_reply, &gram, copy_size);
                        dsp->mce_state = MCE_REP_RECD;
                        PRINT_ERR(dsp->minor, "%i -> mce_state\n",
                                  dsp->mce_state);
                        break;
                default:
                        PRINT_ERR(dsp->minor,
                                  "unexpected MCE reply packet (state=%i)\n",
                                  dsp->mce_state);
                        dsp->mce_state = MCE_IDLE;
                }
                wake_up_interruptible(&dsp->queue);
                DSP_UNLOCK;
                break;
        default:
                PRINT_ERR(dsp->minor, "unknown DGRAM_TYPE\n");
        }

        n_wait = 0;
        n_wait_max = 1000000;
        do {
                if (!(dsp_read_hstr(dsp->reg) & HSTR_HINT))
                        break;
        } while (++n_wait < n_wait_max);
        if (n_wait>=n_wait_max) {
                PRINT_ERR(dsp->minor, "int handler timed out waiting for HINT\n");
        }

        // Hand shake down.
        dsp_write_hctr(dsp->reg, hctr & ~HCTR_HF0);

        if (report_packet) {
                for (k=0; k<32; k++)
                        PRINT_ERR(dsp->minor, " reply %3i=%8x\n", k,
                                  ((__u32*)&gram)[k]);
        }

        PRINT_INFO(dsp->minor, "ok\n");
	return IRQ_HANDLED;
}

void mcedsp_dsp_timeout(unsigned long data)
{
	mcedsp_t *dsp = (mcedsp_t*)data;
        if (dsp->dsp_state == DSP_IDLE) {
                PRINT_INFO(dsp->minor, "Unexpected timeout.\n");
                return;
        }

        PRINT_ERR(dsp->minor, "DSP command timed out in state=%i\n",
                  dsp->dsp_state);
        mcedsp_set_state_wake(dsp, DSP_IDLE, MCE_IGNORE);
}

void mcedsp_mce_timeout(unsigned long data)
{
	mcedsp_t *dsp = (mcedsp_t*)data;
        if (dsp->mce_state == MCE_IDLE) {
                PRINT_INFO(dsp->minor, "Unexpected timeout.\n");
                return;
        }

        PRINT_ERR(dsp->minor, "MCE command timed out in state=%i\n",
                  dsp->mce_state);
        mcedsp_set_state_wake(dsp, DSP_IGNORE, MCE_IDLE);
}


int mcedsp_probe(struct pci_dev *pci, const struct pci_device_id *id)
{
	int card;
        int err = 0;
	mcedsp_t *dsp = NULL;
        PRINT_INFO(NOCARD, "entry\n");

        // Find open slot
        for (card=0; card<MAX_CARDS; card++) {
                dsp = dspdata + card;
                if (dsp->pci == NULL)
                        break;
        }

        if (card==MAX_CARDS) {
                PRINT_ERR(NOCARD, "too many cards, dspdata is full.\n");
                return -ENODEV;
        }

        PRINT_INFO(NOCARD, "assigned to minor=%i\n", card);
        dsp->minor = card;

        // Kernel structures...
        init_waitqueue_head(&dsp->queue);
	init_timer(&dsp->dsp_timer);
	dsp->dsp_timer.function = mcedsp_dsp_timeout;
	dsp->dsp_timer.data = (unsigned long)dsp;

	init_timer(&dsp->mce_timer);
	dsp->mce_timer.function = mcedsp_mce_timeout;
	dsp->mce_timer.data = (unsigned long)dsp;

        // Now we can set up the kernel to talk to the card
	// PCI paperwork
	if ((err = pci_enable_device(pci))) {
                PRINT_ERR(card, "pci_enable_device failed.\n");
                goto fail;
        }
        // This was recently moved up; used to be under ioremap
	pci_set_master(pci);

	if (pci_request_regions(pci, DEVICE_NAME)!=0) {
                PRINT_ERR(card, "pci_request_regions failed.\n");
		err = -1;
		goto fail;
	}

        dsp->reg = (dsp_reg_t *)ioremap_nocache(pci_resource_start(pci, 0) &
						PCI_BASE_ADDRESS_MEM_MASK,
						sizeof(*dsp->reg));
	if (dsp->reg==NULL) {
                PRINT_ERR(card, "Could not map PCI registers!\n");
		pci_release_regions(pci);
		err = -EIO;
		goto fail;
	}

        // Allocate the frame data buffer
        if (data_alloc(dsp, 10000000)!=0) {
                PRINT_ERR(card, "data buffer allocation failed.\n");
                err = -1;
                goto fail;
        }

        // Allocate DMA-ready reply buffer.
        dsp->reply_buffer_size = 1024;
        dsp->reply_buffer_dma_virt =
                dma_alloc_coherent(&pci->dev, dsp->reply_buffer_size,
                                   &dsp->reply_buffer_dma_handle,
                                   GFP_KERNEL);

        if (dsp->reply_buffer_dma_virt==NULL) {
                PRINT_ERR(card, "Failed to allocate DMA memory.\n");
        }

        // Interrupt handler?  Maybe this can wait til later...
	err = request_irq(pci->irq, (irq_handler_t)mcedsp_int_handler,
                          IRQ_FLAGS, DEVICE_NAME, dsp);
	if (err!=0) {
                PRINT_ERR(card, "irq request failed with error code %#x\n",
			  -err);
	}
        PRINT_ERR(card, "new interrupt handler installed.\n");

        //Ok, we made it.
        dsp->pci = pci;
        dsp->enabled = 1;

        return 0;

fail:
        PRINT_ERR(NOCARD, "failed, calling removal routine.\n");
	mcedsp_remove(pci);
	return -1;
}


void mcedsp_remove(struct pci_dev *pci)
{
	int card;
	mcedsp_t *dsp = NULL;

        PRINT_INFO(NOCARD, "entry\n");
	if (pci == NULL) {
                PRINT_ERR(NOCARD, "called with null pointer, ignoring.\n");
		return;
	}

	// Match to existing card
	for (card=0; card < MAX_CARDS; card++) {
                dsp = dspdata + card;
		if (pci == dsp->pci)
			break;
	}
	if (card >= MAX_CARDS) {
                PRINT_ERR(card, "could not match configured device, "
                                "ignoring.\n");
		return;
	}
			
        dsp->enabled = 0;

	// Disable higher-level features first
	del_timer_sync(&dsp->dsp_timer);
	del_timer_sync(&dsp->mce_timer);

        if (dsp->pci != NULL)
                free_irq(dsp->pci->irq, dsp);

        if (dsp->reply_buffer_dma_virt != NULL)
                dma_free_coherent(&dsp->pci->dev, dsp->reply_buffer_size,
                                  dsp->reply_buffer_dma_virt,
                                  dsp->reply_buffer_dma_handle);

        data_free(dsp);

        //Unmap i/o...
        iounmap(dsp->reg);

        // Call release_regions *after* disable_device.
        pci_disable_device(dsp->pci);
        pci_release_regions(dsp->pci);

        dsp->pci = NULL;

        PRINT_INFO(NOCARD, "ok\n");
}

__s32 read_pair(mcedsp_t *dsp) {
        __s32 i1, i2;
        while (!(dsp_read_hstr(dsp->reg) & HSTR_HRRQ));
        i1 = dsp_read_hrxs(dsp->reg);
        while (!(dsp_read_hstr(dsp->reg) & HSTR_HRRQ));
        i2 = dsp_read_hrxs(dsp->reg);
        return (i1 & 0xffff) << 16 | (i2 & 0xffff);
}

/*
int read_hrxs_packet(mcedsp_t *dsp) {
        int word, code, size;
        int i;

        // Anything there?
        if (!(dsp_read_hstr(dsp->reg) & HSTR_HRRQ))
                return -EBUSY;

        // Yes, something is there.
        word = read_pair(dsp);
        code = (word >> 16) & 0xff;
        size = (word & 0xffff);
        
        if (code < DSP_CODE_REPLY_MAX) {
                // This is a reply to a command we sent.
                if (dsp->reply_recd == 2)
                        // Bad!  The driver contains an outstanding reply.
                        PRINT_ERR(NOCARD, "Reply trashes existing reply.");
                // Fill the reply.
                dsp->reply.size = size+1;
                dsp->reply.data[0] = word;
                for (i=1; i<dsp->reply.size; i++)
                        dsp->reply.data[i] = read_pair(dsp);
                dsp->reply_recd = 2;
        } else {
                PRINT_ERR(NOCARD, "MCE packet - size %i!\n", size);
                for (i=0; i<size; i++) {
                        dsp->data_buffer[dsp->data_head++] = read_pair(dsp);
                }
                PRINT_ERR(NOCARD, "Buffer state: %i %i\n",
                          dsp->data_head, dsp->data_tail);
        }
        return 0;
}
*/



int try_send_cmd(mcedsp_t *dsp, struct dsp_command *cmd) {
        int i;
        int n_wait = 0;
        int n_wait_max = 10000;
        __u16 *data;

        if (!(dsp_read_hstr(dsp->reg) & HSTR_HTRQ))
                return -EBUSY;

        data = (__u16*)&(cmd->cmd);
        for (i=0; i < cmd->size*2; i++) {
                while (!(dsp_read_hstr(dsp->reg) & HSTR_HTRQ) && ++n_wait < n_wait_max);
                dsp_write_htxr(dsp->reg, data[i]);
                /* PRINT_INFO(dsp->minor, "wrote %i=%x\n", i, data[i]); */
        }
        if (n_wait >= n_wait_max) {
                PRINT_ERR(dsp->minor,
                          "failed to write command while waiting for HTRQ.\n");
                return -EIO;
        }
        return 0;
}

long mcedsp_ioctl(struct file *filp, unsigned int iocmd, unsigned long arg)
{
//	struct filp_pdata *fpdata = filp->private_data;
//	struct dsp_ops_t *dops = dsp_ops + fpdata->minor;
        mcedsp_t *dsp = (mcedsp_t*)filp->private_data;
        int card = dsp->minor;

        struct dsp_command cmd;
        int i;
        unsigned x;
        DSP_LOCK_DECLARE_FLAGS;

        int nonblock = (filp->f_flags & O_NONBLOCK);

//        PRINT_INFO(card, "entry! minor=%d\n", card);

	switch(iocmd) {
        case DSPIOCT_R_HRXS:
                return dsp_read_hrxs(dsp->reg);
        case DSPIOCT_R_HSTR:
                return dsp_read_hstr(dsp->reg);
        case DSPIOCT_R_HCVR:
                return dsp_read_hcvr(dsp->reg);
        case DSPIOCT_R_HCTR:
                return dsp_read_hctr(dsp->reg);

        case DSPIOCT_W_HTXR:
                dsp_write_htxr(dsp->reg, (int)arg);
                return 0;
        case DSPIOCT_W_HCVR:
                dsp_write_hcvr(dsp->reg, (int)arg);
                return 0;
        case DSPIOCT_W_HCTR:
                dsp_write_hctr(dsp->reg, (int)arg);
                return 0;

        case DSPIOCT_COMMAND:
                PRINT_ERR(dsp->minor, "send_command\n");
                DSP_LOCK;
                while (dsp->dsp_state != DSP_IDLE) {
                        int last_state = dsp->dsp_state;
                        DSP_UNLOCK;
                        if (nonblock)
                                return -EBUSY;
                        // Wait for state change?
                        PRINT_ERR(dsp->minor, "wait once...\n");
                        if (wait_event_interruptible(
                                    dsp->queue, (dsp->dsp_state!=last_state))!=0)
                                return -ERESTARTSYS;
                        DSP_LOCK;
                }
                dsp->dsp_state = DSP_CMD_QUED;
                DSP_UNLOCK;

                PRINT_ERR(dsp->minor, "attempt send.\n");

                // Copy command from user
                copy_from_user(&cmd, (const void __user *)arg, sizeof(cmd));

                // Clear reply state
                //FIXME: register this for later writing.
                DSP_LOCK;
                if (try_send_cmd(dsp, &cmd)!=0) {
                        dsp->dsp_state = DSP_IDLE;
                        wake_up_interruptible(&dsp->queue);
                        DSP_UNLOCK;
                        return -EBUSY;
                }

                // Stil with the lock...
                if (cmd.flags & DSP_EXPECT_MCE_REPLY) {
                        dsp->mce_state = MCE_CMD_SENT;
                        mod_timer(&dsp->mce_timer, jiffies + HZ);
                }

                if (cmd.flags & DSP_EXPECT_DSP_REPLY) {
                        dsp->dsp_state = DSP_CMD_SENT;
                        mod_timer(&dsp->dsp_timer, jiffies + HZ);
                } else {
                        dsp->dsp_state = DSP_IDLE;
                }
                PRINT_ERR(dsp->minor, "states dsp=%i mce=%i\n",
                          dsp->dsp_state, dsp->mce_state);

                wake_up_interruptible(&dsp->queue);
                DSP_UNLOCK;
                
                return 0;

        case DSPIOCT_GET_DSP_REPLY:
                /* Check for reply.  Only block if a command has been
                 * sent and thus a reply is expected in the near
                 * future. */
                PRINT_ERR(dsp->minor, "get_reply\n");
                DSP_LOCK;
                while (dsp->dsp_state != DSP_REP_RECD) {
                        int last_state = dsp->dsp_state;
                        DSP_UNLOCK;
                        if (nonblock ||
                            dsp->dsp_state == DSP_IDLE ||
                            dsp->dsp_state == DSP_REP_HNDL)
                                return -EBUSY;
                        // Wait for state change?
                        PRINT_ERR(dsp->minor, "wait once...\n");
                        if (wait_event_interruptible(
                                    dsp->queue, (dsp->dsp_state!=last_state))!=0)
                                return -ERESTARTSYS;
                        DSP_LOCK;
                }
                PRINT_ERR(dsp->minor, "process reply.\n");
                // Mark that we're handling the reply and release lock
                dsp->dsp_state = DSP_REP_HNDL;
                DSP_UNLOCK;

                copy_to_user((void __user*)arg, &dsp->reply, sizeof(dsp->reply));

                // Mark DSP as idle.
                del_timer_sync(&dsp->dsp_timer);
                mcedsp_set_state_wake(dsp, DSP_IDLE, MCE_IGNORE);

                return 0;

        case DSPIOCT_GET_MCE_REPLY:
                /* Check for reply.  Only block if a command has been
                 * sent and thus a reply is expected in the near
                 * future. */
                PRINT_ERR(dsp->minor, "get_reply\n");
                DSP_LOCK;
                while (dsp->mce_state != MCE_REP_RECD) {
                        int last_state = dsp->mce_state;
                        DSP_UNLOCK;
                        if (nonblock ||
                            dsp->mce_state == MCE_IDLE ||
                            dsp->mce_state == MCE_REP_HNDL)
                                return -EBUSY;
                        // Wait for state change?
                        PRINT_ERR(dsp->minor, "get_mce_rep: wait once...\n");
                        if (wait_event_interruptible(dsp->queue,
                                    (dsp->mce_state!=last_state))!=0)
                                return -ERESTARTSYS;
                        DSP_LOCK;
                }
                PRINT_ERR(dsp->minor, "process reply.\n");
                // Mark that we're handling the reply and release lock
                dsp->mce_state = MCE_REP_HNDL;
                DSP_UNLOCK;

                copy_to_user((void __user*)arg, &dsp->mce_reply,
                             sizeof(dsp->mce_reply));

                // Mark DSP as idle.
                del_timer_sync(&dsp->mce_timer);
                mcedsp_set_state_wake(dsp, DSP_IGNORE, MCE_IDLE);

                return 0;

        case DSPIOCT_SET_REP_BUF:
                // Hand craftily upload the bus address.
                x = (unsigned) dsp->reply_buffer_dma_handle;
                PRINT_INFO(card, "Informing DSP of bus address=%lx\n",
                           (long)dsp->reply_buffer_dma_handle);

                cmd.cmd = CMD_SET_REP_BUF;
                cmd.data_size = 1;
                cmd.data[0] = x;
                /* cmd.data[0] = 0x090002; */
                /* cmd.data[1] = (x) & 0xffff; */
                /* cmd.data[2] = (x >> 16) & 0xffff; */
                cmd.size = 2;
                cmd.flags = 0;
                cmd.owner = 0;
                cmd.timeout_us = 10000;

                //FIXME: register this for later writing.
                
                if (try_send_cmd(dsp, &cmd) != 0)
                        return -EBUSY;

                return 0;

        case DSPIOCT_SET_DATA_BUF:
                // Hand craftily upload the bus address.
                x = (unsigned) dsp->dframes.base_busaddr;
                PRINT_INFO(card, "Informing DSP of bus address=%#x\n", x);

                cmd.data[0] = 0x0a0002;
                cmd.data[1] = (x) & 0xffff;
                cmd.data[2] = (x >> 16) & 0xffff;
                memset(&(cmd.data[3]), 0, 4*16);
                cmd.size = 7;
                cmd.flags = 0;
                cmd.owner = 0;
                cmd.timeout_us = 10000;

                //FIXME: register this for later writing.
                
                if (try_send_cmd(dsp, &cmd) != 0)
                        return -EBUSY;

                return 0;

        case DSPIOCT_TRIGGER_FAKE:
                // Hand craftily upload the bus address.
                PRINT_INFO(card, "Triggering DSP write\n");

                // Clear data buffer?
                memset(dsp->dframes.base, 0, 256);

                cmd.data[0] = 0x310000;
                cmd.size = 1;
                //cmd.reply_reqd = 0;
                cmd.flags = 0;

                if (try_send_cmd(dsp, &cmd)!=0)
                        return -EBUSY;
                return 0;

        case DSPIOCT_DUMP_BUF:
                for (i=0; i<12; i++) {
                        PRINT_ERR(card, "buffer %3i = %4x\n", i,
                                  ((__u32*)dsp->dframes.base)[i]);
                }
                return 0;

	default:
                PRINT_INFO(card, "unknown ioctl\n");
                return 0;
	}
	return 0;
}

int mcedsp_mmap(struct file *filp, struct vm_area_struct *vma)
{
        mcedsp_t *dsp = (mcedsp_t*)filp->private_data;
        int card = dsp->minor;

	// Mark memory as reserved (prevents core dump inclusion) and
	// IO (prevents caching)
	vma->vm_flags |= VM_IO | VM_RESERVED;

	// Do args checking on vma... start, end, prot.
        PRINT_INFO(card, "mapping %#lx bytes to user address %#lx\n",
		   vma->vm_end - vma->vm_start, vma->vm_start);

	//remap_pfn_range(vma, virt, phys_page, size, vma->vm_page_prot);
	remap_pfn_range(vma, vma->vm_start,
			virt_to_phys(dsp->dframes.base) >> PAGE_SHIFT,
			vma->vm_end - vma->vm_start, vma->vm_page_prot);
	return 0;
}


int mcedsp_open(struct inode *inode, struct file *filp)
{
	/* struct filp_pdata *fpdata; */
	int minor = iminor(inode);
        PRINT_INFO(minor, "entry! iminor(inode)=%d\n", minor);

        // Is this a real device?
        if (minor >= MAX_CARDS || !dspdata[minor].enabled) {
                PRINT_ERR(minor, "card %i not enabled.\n", minor);
		return -ENODEV;
	}

        // FIXME: populate .owner in fops?
	if(!try_module_get(THIS_MODULE))
		return -1;

        
	/* fpdata = kmalloc(sizeof(struct filp_pdata), GFP_KERNEL); */
        /* fpdata->minor = iminor(inode); */
	filp->private_data = dspdata + minor;

        PRINT_INFO(minor, "ok\n");
	return 0;
}

int mcedsp_release(struct inode *inode, struct file *filp)
{
        mcedsp_t *dsp = (mcedsp_t*)filp->private_data;
        int card = dsp->minor;

        PRINT_INFO(card, "entry!\n");

	module_put(THIS_MODULE);

        PRINT_INFO(card, "ok\n");
	return 0;
}


struct file_operations mcedsp_fops = 
{
	.owner=   THIS_MODULE,
	.open=    mcedsp_open,
	.release= mcedsp_release,
	/* .read=    dsp_read, */
	/* .write=   dsp_write, */
	.unlocked_ioctl= mcedsp_ioctl,
};


static int mcedsp_proc(char *buf, char **start, off_t offset, int count,
                       int *eof, void *data)
{
	int len = 0;
	int card;

        // You only get one shot.
        *eof = 1;

        if (count < 100)
                return 0;

        len += sprintf(buf+len,"mce_dsp driver version %s\n", "<new!>");
        if (count < 512) {
                len += sprintf(buf+len,"    output abbreviated!\n");
                return len;
        }

        len += sprintf(buf+len,"    bigphys:  "
#ifdef BIGPHYS
                       "yes\n"
#else
                       "no\n"
#endif
                );

	for(card=0; card<MAX_CARDS; card++) {
                mcedsp_t* dsp = dspdata + card;
                if (!dsp->enabled)
                        continue;

                len += sprintf(buf+len,"\nCARD: %d\n\n", card);
                len += sprintf(buf+len,"  dsp command state: %i\n",
                               dsp->dsp_state);
                len += sprintf(buf+len,"  mce command state: %i\n",
                               dsp->dsp_state);
        }

	return len;

}


inline int mcedsp_driver_init(void)
{
	int i = 0;
	int err = 0;

        PRINT_ERR(NOCARD, "MAS driver version %s\n", VERSION_STRING);
	for(i=0; i<MAX_CARDS; i++) {
                mcedsp_t *dev = dspdata + i;
		memset(dev, 0, sizeof(*dev));
                dev->minor = i;
                spin_lock_init(&dev->lock);
	}
  
        create_proc_read_entry("mce_dsp", 0, NULL, mcedsp_proc, NULL);

	err = register_chrdev(0, DEVICE_NAME, &mcedsp_fops);
        major = err;

	err = pci_register_driver(&pci_driver);
	if (err) {
                PRINT_ERR(NOCARD, "pci_register_driver failed with code %i.\n",
                                err);
		err = -1;
		goto out;
	}			  

        PRINT_INFO(NOCARD, "ok\n");
	return 0;
out:
        PRINT_ERR(NOCARD, "exiting with error\n");
	return err;
}

void mcedsp_driver_cleanup(void)
{
        PRINT_INFO(NOCARD, "entry\n");

	pci_unregister_driver(&pci_driver);

        unregister_chrdev(major, DEVICE_NAME);

	remove_proc_entry("mce_dsp", NULL);

        PRINT_INFO(NOCARD, "driver removed\n");
}    

module_init(mcedsp_driver_init);
module_exit(mcedsp_driver_cleanup);
