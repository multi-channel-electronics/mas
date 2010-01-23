#include <linux/module.h>
#include <linux/proc_fs.h>

#include "mce_options.h"
#include "dsp_driver.h"
#include "mce_driver.h"
#include "data.h"
#include "dsp_ops.h"
#include "mce_ops.h"
#include "data_ops.h"
#include "proc.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR ("Matthew Hasselfield"); 

static void mceds_cleanup(void)
{
	PRINT_INFO("entry\n");

	// Kill device interface
	remove_proc_entry("mce_dsp", NULL);
	data_ops_cleanup();
	mce_ops_cleanup();
	dsp_ops_cleanup();

	// Shut down mce devices
	mce_cleanup();

	// Shut down PCI devices
	dsp_driver_cleanup();

	PRINT_INFO("driver removed\n");
}    

static int mceds_init(void)
{
	int i = 0;
	int err = 0;

	// Initialize MCE structs
	err = mce_init();
	if (err != 0) goto out;

	// Find PCI cards
	err = dsp_driver_init();
	if (err != 0) goto out;

	//Associate PCI devices to MCE devices
	for (i=0; i<MAX_CARDS; i++)
		mce_interfaces[i] = dsp_get_mce(i);

	// MCE emulators?
//	mce_faker_init(1, 1);

	// File operations
	err = dsp_ops_init();
	if(err != 0) goto out;

	err = mce_ops_init();
	if (err != 0) goto out;

	err = data_ops_init();
	if (err != 0) goto out;

	create_proc_read_entry("mce_dsp", 0, NULL, read_proc, NULL);

	PRINT_INFO("ok\n");
	return 0;
out:
	PRINT_ERR("exiting with error\n");
	mceds_cleanup();
	return err;
}

module_init(mceds_init);
module_exit(mceds_cleanup);
