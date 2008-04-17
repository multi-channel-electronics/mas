/*
  dsp_fake.c

  This replaces dsp_pci.c by providing routines that imitate the
  astrocam DSP card behaviour.  Sort of.  Ok, it doesn't do a very
  good job of imitating the DSP, but it will trigger MCE command
  loading and handle the generation of "interrupts" due to MCE
  replies.

*/

#include <linux/module.h>
#include <linux/kernel.h>

#include <mce_options.h>
#include <kversion.h>

#ifdef OLD_KERNEL
#  include <linux/sched.h>
#  include <linux/slab.h>
#  include <asm/io.h>
#else
#  include <linux/dma-mapping.h>
#endif

#include <linux/interrupt.h>


// These expose *_int_handler
#include <dsp_driver.h>
#include <mce_driver.h>

#include "dsp_fake.h"
#include "dsp_state.h"
#include "mce_fake.h"
#include <mce/dsp_ioctl.h>


/*
  DSP command sending framework

  - The most straight-forward approach to sending DSP commands is to
    call dsp_command with pointers to the full dsp_command structure
    and an empty dsp_message structure.

  - A non-blocking version of the above that allows the caller to
    specify a callback routine that will be run upon receipt of the
    DSP ack/err interrupt message is exposed as dsp_command_nonblock.
    The calling module must notify the dsp module that the message has
    been processed by calling dsp_clear_commflags; this cannot be done
    from within the callback routine!

  - The raw, no-semaphore-obtaining, no-flag-checking-or-setting,
    non-invalid-command-rejecting command issuer is dsp_command_now.
    Don't use it.  Only I'm allowed to use it.

*/


#define SUBNAME "dsp_send_command_now: "

int dsp_send_command_now(dsp_command *cmd) 
{
	if (dsp_state_command(cmd)) {
		PRINT_ERR(SUBNAME "dsp_state_command failed!?\n");
		return -1;
	}
	return 0;
}

#undef SUBNAME


	

/******************************************************************/


void* dsp_allocate_dma(ssize_t size, unsigned int* bus_addr_p)
{
#ifdef OLD_KERNEL
	void *ptr = kmalloc(size, GFP_KERNEL);
	if (ptr != NULL) *bus_addr_p = virt_to_bus(ptr);
	return ptr;
#else
	return dma_alloc_coherent(NULL, size, bus_addr_p, GFP_KERNEL);
#endif
}

void  dsp_free_dma(void* buffer, int size, unsigned int bus_addr)
{
#ifdef OLD_KERNEL
	if (buffer!=NULL) kfree(buffer);
#else
	dma_free_coherent(NULL, size, buffer, bus_addr);
#endif
}



#define SUBNAME "dsp_fake_init: "

int dsp_fake_init(char *dev_name)
{
	int err = -1;
	PRINT_INFO(SUBNAME "entry\n");

	mce_fake_init();
	dsp_state_init();
	dsp_state_set_handler(dsp_int_handler);

	err = 0;
// out:
	if (err==0) PRINT_INFO(SUBNAME "ok\n");
	return err;
}

#undef SUBNAME


int dsp_fake_cleanup()
{
	mce_fake_cleanup();
	dsp_state_cleanup();

	return 0;
}

#define SUBNAME "dsp_fake_ioctl: "

int dsp_pci_ioctl(unsigned int iocmd, unsigned long arg)
{
	switch (iocmd) {
	case DSPDEV_IOCT_CORE:
		
		PRINT_IOCT(SUBNAME "Fake_DSP doesn't have registers!\n");
		
		break;

	case DSPDEV_IOCT_CORE_IRQ:

		PRINT_IOCT(SUBNAME "Fake_DSP can't dis/enable irq!\n");

		break;

	default:
		PRINT_ERR(SUBNAME "I don't handle iocmd=%ui\n", iocmd);
		return -1;
	}

	return 0;
}

int dsp_pci_proc(char *buf, int count)
{
	int len = 0;
	if (len < count) {
		len += sprintf(buf+len, "    (pci emulator!)\n");
	}
	
	return len;
}
