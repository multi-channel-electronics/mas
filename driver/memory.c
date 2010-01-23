#include <linux/pci.h>
#include "kversion.h"
#include "mce_options.h"
#include "memory.h"

#ifdef BIGPHYS

#define SUBNAME "bigphys_free: "
static void bigphys_free(frame_buffer_mem_t* mem)
{
	if (mem == NULL || mem->base == NULL)
		return;
	PRINT_ERR(SUBNAME "freeing\n");
	bigphysarea_free_pages(mem->base);
	mem->base = NULL;
}
#undef SUBNAME

#define SUBNAME "bigphys_alloc: "
frame_buffer_mem_t* bigphys_alloc(int mem_size)
{
	caddr_t virt;
	frame_buffer_mem_t* mem = NULL;
	int npg = (mem_size + PAGE_SIZE-1) / PAGE_SIZE;
	PRINT_INFO(SUBNAME "entry\n");

	mem_size = npg * PAGE_SIZE;

        // Virtual address?
	virt = bigphysarea_alloc_pages(npg, 0, GFP_KERNEL);
	PRINT_ERR(SUBNAME "BIGPHYS selected\n");

	if (virt==NULL) {
		PRINT_ERR(SUBNAME "bigphysarea_alloc_pages failed!\n");
		return -ENOMEM;
	}

	mem = kmalloc(sizeof(*mem), GFP_KERNEL);
	mem->type = DATAMEM_BIGPHYS;
	mem->base = virt;
	mem->size = mem_size;
	// Note virt_to_bus is on the deprecation list... we will want
	// to switch to the DMA-API, dma_map_single does what we want.
	mem->private_data = virt_to_bus(virt);
	mem->free = bigphys_free;

	//Debug
	PRINT_INFO(SUBNAME "buffer: base=%lx + %x\n",
		   (unsigned long)mem->base,
		   mem->size);
	
	return mem;
}
#undef SUBNAME

#endif /* BIGPHYS */

#define SUBNAME "basicmem_free: "
static void basicmem_free(frame_buffer_mem_t* mem)
{
	if (mem == NULL || mem->base == NULL)
		return;
	PRINT_ERR(SUBNAME "freeing\n");
	kfree(mem->base);
	mem->base = NULL;
}
#undef SUBNAME

#define SUBNAME "basicmem_alloc: "
frame_buffer_mem_t* basicmem_alloc(long int mem_size)
{
	frame_buffer_mem_t* mem = NULL;
	void *data = NULL;
	PRINT_INFO(SUBNAME "entry\n");

	data = kmalloc(mem_size, GFP_KERNEL);
	if (data == NULL) {
		PRINT_ERR(SUBNAME "kmalloc(%li) failed!\n", mem_size);
		return NULL;
	}
	mem = kmalloc(sizeof(*mem), GFP_KERNEL);
	mem->type = DATAMEM_KMALLOC;
	mem->base = data;
	mem->size = mem_size;
	mem->private_data = 0;
	mem->free = &basicmem_free;

	//Debug
	PRINT_INFO(SUBNAME "buffer: base=%lx + %x\n",
		   (unsigned long)mem->base,
		   mem->size);
	
	return mem;
}
#undef SUBNAME


void pcimem_free(frame_buffer_mem_t *mem)
{
	if (mem == NULL || mem->base == NULL)
		return;
	PRINT_INFO("freeing %i from device %p (%p, %lx)\n",
		   mem->size, (void*)mem->private_data, (void*)mem->bus_addr,
		   (unsigned long)mem->base);
	dma_free_coherent((struct device*)mem->private_data, mem->size,
			  mem->base, (dma_addr_t)mem->bus_addr);
	mem->base = NULL;
}

frame_buffer_mem_t *pcimem_alloc(int size, struct device *dev)
{
	frame_buffer_mem_t *mem;
	unsigned long bus_addr;
	void *base = dma_alloc_coherent(dev, size, (dma_addr_t*)&bus_addr,
					GFP_KERNEL);
	PRINT_INFO("allocating %i from device %p (%p, %lx)\n",
		   size, dev, (void*)bus_addr,
		   (unsigned long)base);
	if (base == NULL)
		return NULL;
	mem = kmalloc(sizeof(*mem), GFP_KERNEL);
	mem->type = DATAMEM_PCI;
	mem->base = base;
	mem->size = size;
	mem->bus_addr = bus_addr;
	mem->private_data = (unsigned long)dev;
	mem->free = &pcimem_free;
	return mem;
}
