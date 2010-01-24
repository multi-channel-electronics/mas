#ifndef _MAS_MEMORY_H_
#define _MAS_MEMORY_H_

#ifdef BIGPHYS
# include <linux/bigphysarea.h>
#endif

#include "dsp_driver.h"

/* Buffer types */

#define DATAMEM_KMALLOC    0
#define DATAMEM_BIGPHYS    1
#define DATAMEM_PCI        2


typedef struct frame_buffer_mem_struct frame_buffer_mem_t;

struct frame_buffer_mem_struct {
	int type;
	void *base;
	unsigned long bus_addr;
	int size;
	void (*free)(frame_buffer_mem_t* data);
	struct device *dev;
};


/* Special allocators */

frame_buffer_mem_t* basicmem_alloc(int mem_size, struct device *dev);
frame_buffer_mem_t* bigphys_alloc(int mem_size, struct device *dev);
frame_buffer_mem_t *pcimem_alloc(int size, struct device *dev);


// Ensure that address alignment is configured

#ifndef DMA_ADDR_ALIGN
# define DMA_ADDR_ALIGN 1024
# define DMA_ADDR_MASK (0xffffffff ^ (DMA_ADDR_ALIGN-1))
#endif




#endif
