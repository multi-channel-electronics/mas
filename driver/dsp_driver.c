/* -*- mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *      vim: sw=8 ts=8 et tw=80
 */


#include <linux/module.h>
#include <linux/init.h>
#include <asm/uaccess.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/sched.h>
#include <linux/pci.h>

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
#include "dsp_regs.h"

#include "mce/new_dspioctl.h"
#include "mce/new_dsp.h"

/*
 *  dsp_driver.c includes
 */

MODULE_LICENSE("GPL");
MODULE_AUTHOR ("Matthew Hasselfield"); 


/* Internal prototypes */

void  mcedsp_remove(struct pci_dev *pci);

int   mcedsp_probe(struct pci_dev *pci, const struct pci_device_id *id);

# define DMA_ADDR_ALIGN 1024
# define DMA_ADDR_MASK (0xffffffff ^ (DMA_ADDR_ALIGN-1))

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



typedef enum {
        DSP_IDLE = 0,
//        DSP_CMD_QUED = 1,
        DSP_CMD_SENT = 2,
        DSP_REP_RECD = 3,
//        DSP_REP_HNDL = 4,
        DSP_IGNORE = 1000
} dsp_state_t;

typedef enum {
        MCE_IDLE = 0,
//        MCE_CMD_QUED = 1,
        MCE_CMD_SENT = 2,
        MCE_REP_RECD = 3,
//        MCE_REP_HNDL = 4,
        MCE_IGNORE = 1000
} mce_state_t;


typedef struct {
        int minor;
        int enabled;

	struct pci_dev *pci;
	dsp_reg_t *reg;

        struct dsp_datagram reply;
        struct dsp_datagram mce_reply;
        struct dsp_datagram buffer_update;
        
        frame_buffer_t dframes;
        void* data_lock;

        int reply_buffer_size;
        volatile
        void* reply_buffer_dma_virt;
        dma_addr_t reply_buffer_dma_handle;

        spinlock_t lock;
	wait_queue_head_t queue;
        struct tasklet_struct grantlet;

	struct timer_list dsp_timer;
	struct timer_list mce_timer;
        dsp_state_t dsp_state;
        mce_state_t mce_state;

        int dsp_cmd_flags;

	int error;

} mcedsp_t;

static mcedsp_t dspdata[MAX_CARDS];
static int major = 0;


#define DSP_LOCK_DECLARE_FLAGS unsigned long irqflags
#define DSP_LOCK    spin_lock_irqsave(&dsp->lock, irqflags)
#define DSP_UNLOCK  spin_unlock_irqrestore(&dsp->lock, irqflags)
/* #define DSP_LOCK_DECLARE_FLAGS  */
/* #define DSP_LOCK  */
/* #define DSP_UNLOCK  */


static int try_send_cmd(mcedsp_t *dsp, struct dsp_command *cmd,
                        int nonblock);
static int try_get_reply(mcedsp_t *dsp, struct dsp_datagram *gram,
                         int nonblock);

static int try_get_mce_reply(mcedsp_t *dsp, struct dsp_datagram *gram,
                             int nonblock);

static int upload_rep_buffer(mcedsp_t *dsp, int enable, int n_tries) {
        int err = -ENOMEM;
        struct dsp_command *cmd;
        struct dsp_datagram *gram;
        __u32 addr = (__u32) dsp->reply_buffer_dma_handle;

        cmd = kmalloc(sizeof(*cmd), GFP_KERNEL);
        gram = kmalloc(sizeof(*gram), GFP_KERNEL);
        if (cmd == NULL || gram == NULL)
                goto free_and_out;
        
        if (!enable)
                addr = 0;
        cmd->cmd = DSP_CMD_SET_REP_BUF;
        cmd->flags = DSP_EXPECT_DSP_REPLY;
        cmd->owner = 0;
        cmd->timeout_us = 0;
        cmd->size = 2;
        cmd->data_size = 1;
        cmd->data[0] = addr;

        PRINT_ERR(dsp->minor, "Informing DSP of bus address=%x\n", addr);

        err = try_send_cmd(dsp, cmd, 0);
        if (err != 0) {
                PRINT_ERR(dsp->minor, "Command failed. [%i]\n", err);
                goto free_and_out;
        }
        err = try_get_reply(dsp, gram, 0);
        if (err != 0) {
                PRINT_ERR(dsp->minor, "Reply failed. [%i]\n", err);
                goto free_and_out;
        }

free_and_out:
        if (gram != NULL)
                kfree(gram);
        if (cmd != NULL)
                kfree(cmd);
        return err;
}

int data_alloc(mcedsp_t *dsp, int mem_size)
{
	frame_buffer_t *dframes = &dsp->dframes;
	int npg = (mem_size + PAGE_SIZE-1) / PAGE_SIZE;
        int i = 0;
        dma_addr_t bus;
        PRINT_INFO(dsp->minor, "entry\n");

	mem_size = npg * PAGE_SIZE;

#ifdef BIGPHYS
        // Virtual address?
	dframes->blocks[0].base = bigphysarea_alloc_pages(npg, 0, GFP_KERNEL);
        PRINT_ERR(dsp->minor, "BIGPHYS selected\n");

	if (dframes->blocks[0].base==NULL) {
                PRINT_ERR(dsp->minor, "bigphysarea_alloc_pages failed!\n");
		return -ENOMEM;
	}

	// Save physical address for hardware
	// Note virt_to_bus is on the deprecation list... we will want
	// to switch to the DMA-API, dma_map_single does what we want.
        bus = virt_to_bus(dframes->blocks[0].base);
	dframes->blocks[0].base_busaddr = bus;
	dframes->blocks[0].size = mem_size;
        dframes->n_blocks = 1;
        dframes->total_size = mem_size;

#else
        // Allocate up to 20 x 1MB blocks
        dframes->n_blocks = 0;
        dframes->total_size = 0;
        for (i=0; i<DSP_MAX_MEM_BLOCKS; i++) {
                dsp_memblock_t *mem = &dsp->dframes.blocks[i];
                mem->size = BLOCK_ALLOC_SIZE;
                mem->base = dma_alloc_coherent(
                        &dsp->pci->dev, mem->size, &bus, GFP_KERNEL);
                if (mem->base == NULL) {
                        break;
                } 
                mem->base_busaddr = (unsigned long)bus;
                dframes->total_size += mem->size;
                dframes->n_blocks++;
                if (dframes->total_size > mem_size)
                        break;
        }
        PRINT_ERR(dsp->minor, "Allocated %i blocks of size %i "
                  "to data buffer\n", dframes->n_blocks, BLOCK_ALLOC_SIZE);
#endif

	return 0;
}

void data_free(mcedsp_t *dsp)
{
#ifdef BIGPHYS
        if (dsp->dframes.blocks[0].base != NULL)
                bigphysarea_free_pages((void*)dsp->dframes.blocks[0].base);
#else
        int i;
        for (i=0; i<DSP_MAX_MEM_BLOCKS; i++) {
                dsp_memblock_t *mem = &dsp->dframes.blocks[i];
                dma_addr_t bus = mem->base_busaddr;
                void* base = (void*)mem->base;
                if (base != NULL)
                        dma_free_coherent(&dsp->pci->dev, mem->size, base, bus);
                mem->base = NULL;
        }
#endif
}


/* set_transfer_params
 *
 * Update data transfer parameters internally, and write them to the
 * card.
 *
 * enable should be 0 or 1, to disable / enable data transfers.
 * Disabling transfers results in bus address 0 being written to the
 * card.
 *
 * If data_size <= 0, the buffer division parameters are not updated.
 *
 * If qt_inform <= 0, the information frame count trigger information
 * is not updated.
 */

#ifdef DFADF
static int set_transfer_params(mcedsp_t *dsp, int enable, int data_size,
                               int qt_inform)
{
        DSP_LOCK_DECLARE_FLAGS;
        int err = -ENOMEM;
        struct dsp_command *cmd;
        struct dsp_datagram *gram;
        frame_buffer_t *dframes = &dsp->dframes;

        cmd = kmalloc(sizeof(*cmd), GFP_KERNEL);
        gram = kmalloc(sizeof(*gram), GFP_KERNEL);
        if (cmd == NULL || gram == NULL)
                goto free_and_exit;

        if (data_size > 0) {
                // Note that dividing by 0 causes a kernel panic, so don't do it.
                int frame_size = (data_size + DMA_ADDR_ALIGN - 1) & DMA_ADDR_MASK;
                int n_frames = dframes->size / frame_size;
                if (enable && (data_size <=0 || n_frames <= 0)) {
                        PRINT_ERR(dsp->minor,
                                  "invalid configuration: %i frames of "
                                  "size %i, payload %i.\n",
                                  n_frames, frame_size, data_size);
                        err = -1; // Hmm..
                        goto free_and_exit;
                }
                DSP_LOCK;
                dframes->frame_size = frame_size;
                dframes->data_size = data_size;
                dframes->n_frames = n_frames;
                DSP_UNLOCK;
        }
        if (qt_inform > 0) {
                DSP_LOCK;
                dframes->update_interval = qt_inform;
                DSP_UNLOCK;
        }

        /* This isn't really an option; the card resets the buffer
         * when it gets the SET_DATA_BUF command. */
        DSP_LOCK;
        dframes->head_index = 0;
        dframes->tail_index = 0;
        dframes->last_grant = 0;
        dframes->qt_configs++;
        DSP_UNLOCK;

        // Inform the card...
        cmd->cmd = DSP_CMD_SET_DATA_BUF;
        cmd->flags = DSP_EXPECT_DSP_REPLY;
        if (enable)
                cmd->data[0] = dframes->base_busaddr;
        else
                cmd->data[0] = 0;
        cmd->data[1] = dframes->n_frames;
        cmd->data[2] = dframes->frame_size;
        cmd->data[3] = dframes->data_size;
        cmd->data[4] = dframes->update_interval;
        cmd->data[5] = DSP_INFORM_COUNTS; // Timer expiry
        cmd->data[6] = dframes->head_index;
        cmd->data[7] = dframes->tail_index;
        cmd->data[8] = 0; /* drops */
        cmd->data_size = 9;

        cmd->size = cmd->data_size + 1;

        err = try_send_cmd(dsp, cmd, 0);
        if (err != 0) {
                PRINT_ERR(dsp->minor, "Command failure.\n");
                goto free_and_exit;
        }
        err = try_get_reply(dsp, gram, 0);
        if (err != 0) {
                PRINT_ERR(dsp->minor, "Reply failure.\n");
                goto free_and_exit;
        }

free_and_exit:
        if (cmd != NULL)
                kfree(cmd);
        if (gram != NULL)
                kfree(gram);
        return err;
}
#endif

static int set_transfer_params_multi(mcedsp_t *dsp, int enable, int data_size)
{
        DSP_LOCK_DECLARE_FLAGS;
        int err = -ENOMEM;
        int block_i;
        int start_frame = 0;
        int frame_size;
        struct dsp_command *cmd;
        struct dsp_datagram *gram;
        frame_buffer_t *dframes = &dsp->dframes;

        /* This isn't really an option; the card resets the buffer
         * when it gets the SET_DATA_BUF command. */
        DSP_LOCK;
        dframes->head_index = 0;
        dframes->tail_index = 0;
        dframes->last_grant = 0;
        dframes->qt_configs++;

        if (data_size <= 0)
                data_size = 1024;

        /* Redivide the buffers */
        frame_size = (data_size + DMA_ADDR_ALIGN - 1) & DMA_ADDR_MASK;
        for (block_i=0; block_i<dframes->n_blocks; block_i++) {
                int n_frames = dframes->blocks[block_i].size / frame_size;
                dframes->blocks[block_i].start_frame = start_frame;
                start_frame += n_frames;
                dframes->blocks[block_i].end_frame = start_frame;
                PRINT_INFO(dsp->minor, "data block: %i %i %i %i\n",
                          dframes->blocks[block_i].size,
                          frame_size,
                          dframes->blocks[block_i].start_frame,
                          dframes->blocks[block_i].end_frame);
        }
        dframes->n_frames = start_frame;
        dframes->data_size = data_size;
        dframes->frame_size = frame_size;
        DSP_UNLOCK;

        /* Create the command for DSP */
        cmd = kmalloc(sizeof(*cmd), GFP_KERNEL);
        gram = kmalloc(sizeof(*gram), GFP_KERNEL);
        if (cmd == NULL || gram == NULL)
                goto free_and_exit;
        
        cmd->cmd = DSP_CMD_SET_DATA_MULTI;
        cmd->flags = DSP_EXPECT_DSP_REPLY;
        cmd->data[0] = dframes->n_frames;
        cmd->data[1] = dframes->frame_size;
        cmd->data[2] = dframes->data_size;
        cmd->data[3] = 0;
        for (block_i=0; block_i<dframes->n_blocks; block_i++) {
                // Don't store trivial blocks...
                int start = dframes->blocks[block_i].start_frame;
                int end = dframes->blocks[block_i].end_frame;
                if (start == end)
                        continue;
                cmd->data[4+cmd->data[3]*3+0] =
                        dframes->blocks[block_i].base_busaddr;
                cmd->data[4+cmd->data[3]*3+1] = start;
                cmd->data[4+cmd->data[3]*3+2] = end;
                cmd->data[3]++;
        }
        cmd->data_size = 4 + cmd->data[3]*3;
        cmd->size = cmd->data_size + 1;

        err = try_send_cmd(dsp, cmd, 0);
        if (err != 0) {
                PRINT_ERR(dsp->minor, "Command failure.\n");
                goto free_and_exit;
        }
        err = try_get_reply(dsp, gram, 0);
        if (err != 0) {
                PRINT_ERR(dsp->minor, "Reply failure.\n");
                goto free_and_exit;
        }

free_and_exit:
        if (cmd != NULL)
                kfree(cmd);
        if (gram != NULL)
                kfree(gram);
        return err;
}


static int send_set_tail_inform(mcedsp_t *dsp, int inform_count, int nonblock)
{
        DSP_LOCK_DECLARE_FLAGS;
        struct dsp_command cmd;
        __u32 tail;

        if (!dsp->enabled)
                return -ENODEV;

        DSP_LOCK;
        tail = dsp->dframes.tail_index;
        dsp->dframes.last_grant = tail;
        DSP_UNLOCK;

        cmd.cmd = DSP_CMD_SET_TAIL_INF;
        cmd.flags = DSP_EXPECT_DSP_REPLY | 
                DSP_IGNORE_DSP_REPLY;
        cmd.owner = 0;
        cmd.timeout_us = 0;
        cmd.size = 3;
        cmd.data_size = 2;
        cmd.data[0] = tail;
        cmd.data[1] = inform_count;

        return try_send_cmd(dsp, &cmd, nonblock);
}

/* In the grant task, we send updates of the tail_index to the DSP.
 * We don't update the inform interval, and don't block (it's a
 * tasklet).
 */

void grant_task(unsigned long data)
{
        mcedsp_t *dsp = (mcedsp_t*)data;
        int err = 0;
        if ((err=send_set_tail_inform(dsp, 0, 1)) != 0)
                PRINT_ERR(dsp->minor, "failed to grant, err=%i\n",
                          err);
}


static void inline mcedsp_set_state_wake(mcedsp_t *dsp,
                                         dsp_state_t new_dsp_state,
                                         mce_state_t new_mce_state) {
        DSP_LOCK_DECLARE_FLAGS;
        DSP_LOCK;
        if (new_dsp_state < DSP_IGNORE) {
                PRINT_INFO(dsp->minor, "%i -> dsp_state\n", new_dsp_state);
                dsp->dsp_state = new_dsp_state;
        }
        if (new_mce_state < MCE_IGNORE) {
                PRINT_INFO(dsp->minor, "%i -> mce_state\n", new_mce_state);
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
        gramlet = (void*)dsp->reply_buffer_dma_virt;
        copy_size = sizeof(dsp->reply);
        if (gramlet->total_size * sizeof(__u32) > copy_size) {
                PRINT_ERR(dsp->minor, "datagram too large (%i); "
                          "truncating (%i).\n",
                          (int)(gramlet->total_size*sizeof(__u32)),
                          copy_size);
        } else {
                copy_size = gramlet->total_size * sizeof(__u32);
        }                

        // Copy into temporary storage...
        memcpy(&gram, (void*)dsp->reply_buffer_dma_virt,
               copy_size);
        /* { */
        /*         int j; */
        /*         for (j=0; j<copy_size/4; j++) { */
        /*                 PRINT_ERR(dsp->minor, "%3i = %x\n", */
        /*                           j, ((__u32*)&gram)[j]); */
        /*         } */
        /* } */

        switch (gram.type) {
        case DGRAM_TYPE_DSP_REP:
                /* DSP reply */
                DSP_LOCK;
                switch(dsp->dsp_state) {
                case DSP_CMD_SENT:
                        report_packet = 0;
                        /* Some replies are just for hand-shaking and
                         * don't need to be processed */
                        if (dsp->dsp_cmd_flags & DSP_IGNORE_DSP_REPLY) {
                                del_timer(&dsp->dsp_timer);
                                dsp->dsp_state = DSP_IDLE;
                                break;
                        }
                        memcpy(&dsp->reply, &gram, copy_size);
                        dsp->dsp_state = DSP_REP_RECD;
                        PRINT_INFO(dsp->minor, "%i -> dsp_state\n",
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
                        PRINT_INFO(dsp->minor, "%i -> mce_state\n",
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
        case DGRAM_TYPE_BUF_INFO:
                PRINT_INFO(dsp->minor, "info received; %i\n", gram.buffer[0]);
                report_packet = 0;
                DSP_LOCK;
                dsp->dframes.head_index = gram.buffer[0];
                k = (dsp->dframes.head_index - dsp->dframes.last_grant + 
                     dsp->dframes.n_frames) % dsp->dframes.n_frames;
                DSP_UNLOCK;
                /* PRINT_INFO(dsp->minor, "scheduling update!\n"); */
                /* Limits updates to "rare".  The biggest reason to do
                   this is to not bother in the case when only single
                   frames are being read. */
                if (k > dsp->dframes.n_frames / 4) {
                        PRINT_INFO(dsp->minor, "scheduling grant of %i frames\n", k);
                        tasklet_schedule(&dsp->grantlet);
                }
       
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
        int hctr;
        int n_waits;
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
        dsp->pci = pci;

        // Kernel structures...
        init_waitqueue_head(&dsp->queue);
        tasklet_init(&dsp->grantlet, grant_task, (unsigned long)dsp);

	init_timer(&dsp->dsp_timer);
	dsp->dsp_timer.function = mcedsp_dsp_timeout;
	dsp->dsp_timer.data = (unsigned long)dsp;

	init_timer(&dsp->mce_timer);
	dsp->mce_timer.function = mcedsp_mce_timeout;
	dsp->mce_timer.data = (unsigned long)dsp;

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
                goto fail;
        }

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
		err = -EIO;
		goto fail;
	}

        // Raise handshake bit.
        hctr = dsp_read_hctr(dsp->reg);
        dsp_write_hctr(dsp->reg, hctr | HCTR_HF2);

        // Wait for DSP to ack support of this driver
        n_waits = 100000;
        while (n_waits-- > 0) {
                if (dsp_read_hstr(dsp->reg) & HSTR_HC4)
                        break;
        }
        if (n_waits <= 0) {
                dsp_write_hctr(dsp->reg, hctr & ~HCTR_HF2);
                PRINT_ERR(card, "card did not hand-shake.\n");
                goto fail;
        }

        // Interrupt handler?  Maybe this can wait til later...
	err = request_irq(pci->irq, (irq_handler_t)mcedsp_int_handler,
                          IRQ_FLAGS, DEVICE_NAME, dsp);
	if (err!=0) {
                PRINT_ERR(card, "irq request failed with error code %#x\n",
			  -err);
                goto fail;
	}

        //Ok, we made it.
        dsp->enabled = 1;

        // Configure reply and data buffers
        if (upload_rep_buffer(dsp, 1, 10000) != 0) {
                PRINT_ERR(card, "could not configure reply buffer.\n");
                goto fail;
        }

        /* if (set_transfer_params(dsp, 1, 1000, 1000) != 0) { */
        /*         PRINT_ERR(card, "could not configure data buffer.\n"); */
        /*         goto fail; */
        /* }         */

        return 0;

fail:
        PRINT_ERR(NOCARD, "failed, calling removal routine.\n");
	mcedsp_remove(pci);
	return -1;
}


void mcedsp_remove(struct pci_dev *pci)
{
        DSP_LOCK_DECLARE_FLAGS;
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
			
        DSP_LOCK;
        dsp->enabled = 0;
        DSP_UNLOCK;

	// Disable higher-level features first
	del_timer_sync(&dsp->dsp_timer);
	del_timer_sync(&dsp->mce_timer);
        tasklet_kill(&dsp->grantlet);

        if (dsp->pci != NULL)
                free_irq(dsp->pci->irq, dsp);

        if (dsp->reply_buffer_dma_virt != NULL)
                dma_free_coherent(&dsp->pci->dev, dsp->reply_buffer_size,
                                  (void*)dsp->reply_buffer_dma_virt,
                                  dsp->reply_buffer_dma_handle);

        data_free(dsp);

        //Unmap i/o...
        if (dsp->reg != NULL)
                iounmap(dsp->reg);

        // Call release_regions *after* disable_device.
        pci_disable_device(dsp->pci);
        pci_release_regions(dsp->pci);

        dsp->pci = NULL;

        PRINT_INFO(NOCARD, "ok\n");
}


static int raw_send_data(mcedsp_t *dsp, __u32 *buf, int count)
{
        /* Recast to buffer of __u16, since DSP is currently
         * configured to truncate upper byte. */
        __u16 *buf16 = (__u16*)buf;
        int count16 = count * 2;

        int n_wait = 0;
        int n_wait_max = 10000;
        int i;

        if (!(dsp_read_hstr(dsp->reg) & HSTR_HTRQ))
                return -EBUSY;

        for (i=0; i<count16; i++) {
                while (!(dsp_read_hstr(dsp->reg) & HSTR_HTRQ) &&
                       ++n_wait < n_wait_max);
                dsp_write_htxr(dsp->reg, buf16[i]);
                PRINT_INFO(dsp->minor, "wrote %i=%x\n", i, (int)buf16[i]);
        }
        if (n_wait >= n_wait_max) {
                PRINT_ERR(dsp->minor,
                          "failed to write command while waiting for HTRQ.\n");
                return -EIO;
        }
        return 0;
}


static int try_send_cmd(mcedsp_t *dsp, struct dsp_command *cmd, int nonblock)
{
        DSP_LOCK_DECLARE_FLAGS;
        int need_mce = cmd->flags & DSP_EXPECT_MCE_REPLY;
 
        PRINT_INFO(dsp->minor, "DSP command code=%#x, size=%#x, flags %#x\n",
                   cmd->cmd, cmd->size, cmd->flags);
      
        // Require DSP_IDLE and possibly MCE_IDLE.  Block if allowed.
        DSP_LOCK;
        while ((dsp->dsp_state != DSP_IDLE) ||
               (need_mce && (dsp->mce_state != MCE_IDLE))) {
                int last_dsp = dsp->dsp_state;
                int last_mce = dsp->mce_state;
                DSP_UNLOCK;
                // Block, if allowed, for conditions.
                if (nonblock) {
                        PRINT_ERR(dsp->minor,
                                  "nonblock.  turns out state=%i\n", last_dsp);
                        return -EBUSY;
                }
                if (wait_event_interruptible(
                            dsp->queue,
                            (dsp->dsp_state != last_dsp ||
                             (need_mce && (dsp->mce_state != last_mce)))
                            ) != 0)
                        return -ERESTARTSYS;
                DSP_LOCK;
        }

        if (raw_send_data(dsp, (__u32*)&cmd->cmd, cmd->size) != 0) {
                // Restore state and return error.
                dsp->dsp_state = DSP_IDLE;
                wake_up_interruptible(&dsp->queue);
                DSP_UNLOCK;
                return -EIO;
        }

        // Update state depending on expections for reply.
        dsp->dsp_cmd_flags = cmd->flags;
        if (dsp->dsp_cmd_flags & DSP_EXPECT_MCE_REPLY) {
                dsp->mce_state = MCE_CMD_SENT;
                dsp->dsp_cmd_flags |= DSP_EXPECT_DSP_REPLY | 
                        DSP_IGNORE_DSP_REPLY;
                mod_timer(&dsp->mce_timer, jiffies + HZ);
        }
        if (dsp->dsp_cmd_flags & DSP_EXPECT_DSP_REPLY) {
                dsp->dsp_state = DSP_CMD_SENT;
                mod_timer(&dsp->dsp_timer, jiffies + HZ/2);
        } else {
                dsp->dsp_state = DSP_IDLE;
        }
        wake_up_interruptible(&dsp->queue);
        DSP_UNLOCK;

        return 0;
}


static int try_get_reply(mcedsp_t *dsp, struct dsp_datagram *gram,
                         int nonblock)
{
        DSP_LOCK_DECLARE_FLAGS;

        DSP_LOCK;
        while (dsp->dsp_state != DSP_REP_RECD) {
                int last_state = dsp->dsp_state;
                DSP_UNLOCK;
                if (nonblock || dsp->dsp_state == DSP_IDLE)
                        return -EBUSY;
                if (wait_event_interruptible(
                            dsp->queue, (dsp->dsp_state!=last_state))!=0)
                        return -ERESTARTSYS;
                DSP_LOCK;
        }
        PRINT_INFO(dsp->minor, "process reply.\n");
        del_timer(&dsp->dsp_timer);
        memcpy(gram, &dsp->reply, sizeof(*gram));
        dsp->dsp_state = DSP_IDLE;
        wake_up_interruptible(&dsp->queue);
        DSP_UNLOCK;

        return 0;
}

static int try_get_mce_reply(mcedsp_t *dsp, struct dsp_datagram *gram,
                             int nonblock)
{
        DSP_LOCK_DECLARE_FLAGS;

        DSP_LOCK;
        while (dsp->mce_state != MCE_REP_RECD) {
                int last_state = dsp->mce_state;
                DSP_UNLOCK;
                if (nonblock || dsp->mce_state == MCE_IDLE)
                        return -EBUSY;
                if (wait_event_interruptible(
                            dsp->queue, (dsp->mce_state != last_state))!=0)
                        return -ERESTARTSYS;
                DSP_LOCK;
        }
        PRINT_INFO(dsp->minor, "process MCE reply.\n");
        del_timer(&dsp->mce_timer);
        memcpy(gram, &dsp->mce_reply, sizeof(*gram));
        dsp->mce_state = MCE_IDLE;
        wake_up_interruptible(&dsp->queue);
        DSP_UNLOCK;

        return 0;
}


/* Data device locking support */

static int data_lock_operation(mcedsp_t *dsp, int operation, void *filp)
{
        DSP_LOCK_DECLARE_FLAGS;
	int ret_val = 0;

	switch (operation) {
	case LOCK_QUERY:
		return dsp->data_lock != NULL;

	case LOCK_RESET:
		dsp->data_lock = NULL;
		return 0;

	case LOCK_DOWN:
	case LOCK_UP:
                DSP_LOCK;
		if (operation == LOCK_DOWN) {
			if (dsp->data_lock != NULL) {
				ret_val = -EBUSY;
			} else {
				dsp->data_lock = filp;
			}
		} else /* LOCK_UP */ {
			if (dsp->data_lock == filp) {
				dsp->data_lock = NULL;
			}
		}
                DSP_UNLOCK;
		return ret_val;
	}
        PRINT_ERR(dsp->minor, "unknown operation (%i).\n", operation);
	return -1;
}


long mcedsp_ioctl(struct file *filp, unsigned int iocmd, unsigned long arg)
{
        mcedsp_t *dsp = (mcedsp_t*)filp->private_data;
        int card = dsp->minor;

        struct dsp_command *cmd;
        struct dsp_datagram *gram;
        int x, err;

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
                PRINT_INFO(card, "send command\n");
                cmd = kmalloc(sizeof(*cmd), GFP_KERNEL);
                copy_from_user(cmd, (const void __user *)arg, sizeof(*cmd));
                err = try_send_cmd(dsp, cmd, nonblock) != 0;
                kfree(cmd);
                return err;

        case DSPIOCT_GET_DSP_REPLY:
                PRINT_INFO(card, "get DSP reply\n");
                gram = kmalloc(sizeof(*gram), GFP_KERNEL);
                err = try_get_reply(dsp, gram, nonblock);
                if (err == 0)
                        copy_to_user((void __user*)arg, gram, sizeof(*gram));
                kfree(gram);
                return err;

        case DSPIOCT_GET_MCE_REPLY:
                PRINT_INFO(card, "get MCE reply\n");
                gram = kmalloc(sizeof(*gram), GFP_KERNEL);
                err = try_get_mce_reply(dsp, gram, nonblock);
                if (err == 0)
                        copy_to_user((void __user*)arg, gram, sizeof(*gram));
                kfree(gram);
                return err;

/* Frame data stuff */
                
	case DSPIOCT_QUERY:
                PRINT_INFO(card, "query\n");
		switch (arg) {
		case QUERY_HEAD:
			return dsp->dframes.head_index;
		case QUERY_TAIL:
			return dsp->dframes.tail_index;
		case QUERY_MAX:
			return dsp->dframes.n_frames;
		/* case QUERY_PARTIAL: */
		/* 	return dsp->dframes.partial; */
		case QUERY_DATASIZE:			
			return dsp->dframes.data_size;
		case QUERY_FRAMESIZE:
			return dsp->dframes.frame_size;
		case QUERY_BUFSIZE:
			return dsp->dframes.total_size;
/*                case QUERY_LTAIL:
                        return fpdata->leech_tail;
                case QUERY_LPARTIAL:
                        return fpdata->leech_partial;
                case QUERY_LVALID:
                        return (int)(fpdata->leech_acq == dsp->dframes.acq_index);
*/
		default:
			return -1;
		}
		break;

                
	case DSPIOCT_SET_DATASIZE:
                // Configure QT stuff for the data payload size in arg.
                PRINT_INFO(dsp->minor, "set frame size %i\n", (int)arg);
                return set_transfer_params_multi(dsp, 1, arg);

	case DSPIOCT_SET_NFRAMES:
                // Set the number of frames to expect.
                PRINT_INFO(dsp->minor, "set n_frames %i\n", (int)arg);
                return send_set_tail_inform(dsp, (int)arg, nonblock);
/*
	case DATADEV_IOCT_FAKE_STOPFRAME:
                PRINT_ERR(card, "fake_stopframe initiated!\n");
		return data_frame_fake_stop(card);
*/
	case DSPIOCT_EMPTY:
                DSP_LOCK;
                dsp->dframes.head_index = 0;
                dsp->dframes.tail_index = 0;
                DSP_UNLOCK;
                return 0;
/*
	case DSPIOCT_QT_CONFIG:
                PRINT_INFO(card, "configure Quiet Transfer mode "
			   "[inform=%li]\n", arg);
		return set_transfer_params(dsp, 1, -1, arg);

	case DSPIOCT_QT_ENABLE:
                PRINT_INFO(card, "enable/disable quiet Transfer mode "
			   "[on=%li]\n", arg);
                return set_transfer_params(dsp, arg, -1, -1);
*/
        case DSPIOCT_FRAME_POLL:
                /* Return offset into memmapped buffer of the next
                   consumable frame, or -1 if none available. */
                DSP_LOCK;
                x = dsp->dframes.tail_index;
                if (x == dsp->dframes.head_index)
                        x = -1;
                else {
                        int offset = 0;
                        int block_i = 0;
                        while (x < dsp->dframes.blocks[block_i].start_frame ||
                               x >= dsp->dframes.blocks[block_i].end_frame) {
                                offset += dsp->dframes.blocks[block_i].size;
                                block_i++;
                        }
                        offset += dsp->dframes.frame_size *
                                (x - dsp->dframes.blocks[block_i].start_frame);
                        x = offset;
                }
                DSP_UNLOCK;
                return x;

	case DSPIOCT_FRAME_CONSUME:
                DSP_LOCK;
                if ((dsp->dframes.n_frames > 0) &&
                    (dsp->dframes.tail_index != dsp->dframes.head_index)) {
                        dsp->dframes.tail_index = 
                                (dsp->dframes.tail_index + 1) %
                                dsp->dframes.n_frames;
                }
                DSP_UNLOCK;
		return 0;

	case DSPIOCT_DATA_LOCK:
		return data_lock_operation(dsp, arg, filp);

	default:
                PRINT_ERR(card, "unknown ioctl (%#x)\n", iocmd);
                return -1;
	}
	return 0;
}

int mcedsp_mmap(struct file *filp, struct vm_area_struct *vma)
{
        int i;
        int offset = 0;
        mcedsp_t *dsp = (mcedsp_t*)filp->private_data;

	// Mark memory as reserved (prevents core dump inclusion) and
	// IO (prevents caching)
	vma->vm_flags |= VM_IO | VM_RESERVED;

	// Do args checking on vma... start, end, prot.
        PRINT_INFO(dsp->minor, "mapping %#lx bytes to user address %#lx\n",
		   vma->vm_end - vma->vm_start, vma->vm_start);

	//remap_pfn_range(vma, virt, phys_page, size, vma->vm_page_prot);
        for (i=0; i<dsp->dframes.n_blocks; i++) {
                dsp_memblock_t *mem = dsp->dframes.blocks + i;
                if (mem->base == NULL)
                        continue;
                if (vma->vm_end - vma->vm_start - offset < mem->size) {
                        PRINT_ERR(dsp->minor, "user space vma not large enough\n");
                        break;
                }
                remap_pfn_range(vma, vma->vm_start + offset,
                                virt_to_phys(mem->base) >> PAGE_SHIFT,
                                mem->size, vma->vm_page_prot);
                offset += mem->size;
        }
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

	filp->private_data = dspdata + minor;

        PRINT_INFO(minor, "ok\n");
	return 0;
}

int mcedsp_release(struct inode *inode, struct file *filp)
{
        mcedsp_t *dsp = filp->private_data;
	module_put(THIS_MODULE);
        if (dsp != NULL)
                data_lock_operation(dsp, LOCK_UP, filp);
	return 0;
}


struct file_operations mcedsp_fops = 
{
	.owner=   THIS_MODULE,
	.open=    mcedsp_open,
	.release= mcedsp_release,
        .mmap=    mcedsp_mmap,
	/* .read=    dsp_read, */
	/* .write=   dsp_write, */
	.unlocked_ioctl= mcedsp_ioctl,
};


static int mcedsp_proc(char *buf, char **start, off_t offset, int count,
                       int *eof, void *data)
{
	int len = 0;
	int card, i;

        // You only get one shot.
        *eof = 1;

        if (count < 100)
                return 0;

        len += sprintf(buf+len,"mce_dsp driver version %s\n", VERSION_STRING);
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

                len += sprintf(buf+len,"\nCARD: %d\n", card);
		len += sprintf(buf+len, "  %-20s %20s\n",
			       "PCI bus address:", pci_name(dsp->pci));
                len += sprintf(buf+len,"  Commander states:\n"
                               "    %-20s %18i\n"
                               "    %-20s %18i\n",
                               "DSP channel:", dsp->dsp_state,
                               "MCE channel:", dsp->mce_state);
                len += sprintf(buf+len,
                               "  PCI regs:\n"
                               "    %-30s %#08x\n"
                               "    %-30s %#08x\n"
                               "    %-30s %#08x\n",
                               "HSTR:", dsp_read_hstr(dsp->reg),
                               "HCTR:", dsp_read_hctr(dsp->reg),
                               "HCVR:", dsp_read_hcvr(dsp->reg));
                len += sprintf(buf+len,
                               "  Buffer physical addresses:\n"
                               "    %-28s %#10x\n"
                               "    %-28s\n",
                               "reply:", (unsigned)dsp->reply_buffer_dma_handle,
                               "data:");
                for (i=0; i<dsp->dframes.n_blocks; i++)
                        len += sprintf(buf+len, "      %24i @ %#010x\n",
                                       (int)dsp->dframes.blocks[i].size,
                                       (unsigned)dsp->dframes.blocks[i].base_busaddr);
                len += sprintf(buf+len,
                               "  Circular buffer state:\n"
                               "    %-28s %10i\n"
                               "    %-28s %10i\n"
                               "    %-28s %10i\n"
                               "    %-28s %10i\n"
                               "    %-28s %10i\n"
                               "    %-28s %10i\n"
                               "    %-28s %10i\n"
                               "    %-28s %10i\n",
                               "total_size:", (int)dsp->dframes.total_size,
                               "data_size:", dsp->dframes.data_size,
                               "container:", dsp->dframes.frame_size,
                               "n_frames:", dsp->dframes.n_frames,
                               "head_index:", dsp->dframes.head_index,
                               "tail_index:", dsp->dframes.tail_index,
                               "last_grant:", dsp->dframes.last_grant,
                               "reconfigs:", dsp->dframes.qt_configs);
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
  
	err = pci_register_driver(&pci_driver);
	if (err) {
                PRINT_ERR(NOCARD, "pci_register_driver failed with code %i.\n",
                                err);
		err = -1;
		goto out;
	}			  

	err = register_chrdev(0, DEVICE_NAME, &mcedsp_fops);
        major = err;

        create_proc_read_entry("mce_dsp", 0, NULL, mcedsp_proc, NULL);

        PRINT_INFO(NOCARD, "ok\n");
	return 0;
out:
        PRINT_ERR(NOCARD, "exiting with error\n");
	return err;
}

void mcedsp_driver_cleanup(void)
{
        PRINT_INFO(NOCARD, "entry\n");

	remove_proc_entry("mce_dsp", NULL);
        unregister_chrdev(major, DEVICE_NAME);
	pci_unregister_driver(&pci_driver);

        PRINT_INFO(NOCARD, "driver removed\n");
}    

module_init(mcedsp_driver_init);
module_exit(mcedsp_driver_cleanup);
