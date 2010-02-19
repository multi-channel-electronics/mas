#include <linux/pci.h>

#include "driver.h"

/* Convenient wrappers for DMA mapping */

static int map(struct device *dev, void *base, int size, dma_addr_t *bus_addr)
{
	if (dev==NULL) return 0;
	*bus_addr = dma_map_single(dev, base, size, DMA_FROM_DEVICE);
	return dma_mapping_error(*bus_addr);
}

static void unmap(struct device *dev, dma_addr_t bus_addr, int size)
{
	if (dev==NULL) return;
	dma_unmap_single(dev, bus_addr, size, DMA_FROM_DEVICE);
}	


/*
 * Memory allocation routines -- common interface to bigphysarea, PCI
 * DMA-able RAM, or kmalloc if you're desperate.
 *
 * PCI DMA seems to permit allocation of several MB in a single block.
 * This makes bigphysarea mostly obsolete.
 */


#ifdef BIGPHYS

#define SUBNAME "bigphys_free: "
static void bigphys_free(frame_buffer_mem_t* mem)
{
	if (mem == NULL || mem->base == NULL)
		return;
	PRINT_ERR(SUBNAME "freeing\n");
	unmap(mem->dev, mem->bus_addr, mem->size);
	bigphysarea_free_pages(mem->base);
	kfree(mem);
}
#undef SUBNAME


#define SUBNAME "bigphys_alloc: "
frame_buffer_mem_t* bigphys_alloc(int size, struct device *dev)
{
	caddr_t base;
	dma_addr_t bus_addr = 0;
	frame_buffer_mem_t* mem = NULL;

	int npg = (size + PAGE_SIZE-1) / PAGE_SIZE;
	PRINT_INFO(SUBNAME "entry\n");
	size = npg * PAGE_SIZE;

        // Virtual address?
	base = bigphysarea_alloc_pages(npg, 0, GFP_KERNEL);
	PRINT_ERR(SUBNAME "BIGPHYS selected\n");

	if (base==NULL) {
		PRINT_ERR(SUBNAME "bigphysarea_alloc_pages failed!\n");
		return NULL;
	}

	// Attempt to map to PCI device
	if (map(dev, base, size, &bus_addr)) {
		bigphysarea_free_pages(base);
		PRINT_ERR("could not map bigphys pages for DMA.\n");
		return NULL;
	}

	mem = kmalloc(sizeof(*mem), GFP_KERNEL);
	mem->type = DATAMEM_BIGPHYS;
	mem->base = base;
	mem->size = size;
	mem->bus_addr = bus_addr;
	mem->dev = dev;
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
	unmap(mem->dev, mem->bus_addr, mem->size);
	kfree(mem->base);
	kfree(mem);
}
#undef SUBNAME

#define SUBNAME "basicmem_alloc: "
frame_buffer_mem_t* basicmem_alloc(int size, struct device *dev)
{
	frame_buffer_mem_t* mem = NULL;
	dma_addr_t bus_addr;
	void *base = NULL;
	PRINT_INFO(SUBNAME "entry\n");

	base = kmalloc(size, GFP_KERNEL);
	if (base == NULL) {
		PRINT_ERR(SUBNAME "kmalloc(%i) failed!\n", size);
		return NULL;
	}
	if (map(dev, base, size, &bus_addr)) {
		kfree(base);
		PRINT_ERR("could not map bigphys pages for DMA.\n");
		return NULL;
	}

	mem = kmalloc(sizeof(*mem), GFP_KERNEL);
	mem->type = DATAMEM_KMALLOC;
	mem->base = base;
	mem->size = size;
	mem->dev = NULL;
	mem->free = &basicmem_free;

	//Debug
	PRINT_INFO(SUBNAME "buffer: base=%lx + %x\n",
		   (unsigned long)mem->base,
		   mem->size);
	
	return mem;
}
#undef SUBNAME


static void pcimem_free(frame_buffer_mem_t *mem)
{
	if (mem->base == NULL)
		return;
	PRINT_INFO("freeing %i from device %p (%p, %lx)\n",
		   mem->size, mem->dev, (void*)mem->bus_addr,
		   (unsigned long)mem->base);
	dma_free_coherent(mem->dev, mem->size,
			  mem->base, (dma_addr_t)mem->bus_addr);
	kfree(mem);
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
	mem->dev = dev;
	mem->free = pcimem_free;
	return mem;
}
