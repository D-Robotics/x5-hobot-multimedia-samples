#ifndef __MEM_ION_H__
#define __MEM_ION_H__

#include "hb_mem_mgr.h"
#include "common_utils.h"

enum cap_buf_type {
  CAP_BUF_CUSTOM = 0,
  CAP_BUF_CUSTOM_EXT = 1,
  CAP_BUF_INTERNAL = 2,
};


typedef int64_t ion_user_handle_t;
typedef int32_t ion_share_handle_t;

enum ion_heap_type {
	ION_HEAP_TYPE_SYSTEM,	/**< memory allocated via vmalloc */
	ION_HEAP_TYPE_SYSTEM_CONTIG,	/**< memory allocated via kmalloc */
	ION_HEAP_TYPE_CARVEOUT,	/**< memory allocated from a prereserved
							 * carveout heap, allocations are physically
							 * contiguous*/
	ION_HEAP_TYPE_CHUNK,	/**< chunk memory */
	ION_HEAP_TYPE_DMA,		/**< memory allocated via DMA API*/
	ION_HEAP_TYPE_CUSTOM,	/**< memory allocaedf from SRAM heap*/
	ION_HEAP_TYPE_CMA_RESERVED,	/**< memory allocated from a prereserved
								 * carveout heap, allocations are physically
								 * contiguous*/
	ION_HEAP_TYPE_SRAM_LIMIT,	/**< memory allocated from sram limit heap.*/
	ION_HEAP_TYPE_INLINE_ECC,	/**< memory allocated from inline ecc heap.*/
	ION_NUM_HEAPS = 16,
};

#define ION_HEAP_SYSTEM_MASK (1U << ION_HEAP_TYPE_SYSTEM)					/**< system heap mask */
#define ION_HEAP_SYSTEM_CONTIG_MASK (1U << ION_HEAP_TYPE_SYSTEM_CONTIG)		/**< system contigious heap mask */
#define ION_HEAP_CARVEOUT_MASK (1U << ION_HEAP_TYPE_CARVEOUT)				/**< carveout heap mask */
#define ION_HEAP_CHUNK_MASK (1U << ION_HEAP_TYPE_CHUNK)						/**< chunk heap mask */
#define ION_HEAP_DMA_MASK (1U << ION_HEAP_TYPE_DMA)							/**< dma heap mask */
#define ION_HEAP_CUSTOM_MASK (1U << ION_HEAP_TYPE_CUSTOM)					/**< custom heap mask */
#define ION_HEAP_CMA_RESERVED_MASK (1U << ION_HEAP_TYPE_CMA_RESERVED)		/**< cma reserved heap mask */
#define ION_HEAP_TYPE_SRAM_LIMIT_MASK	(1U << ION_HEAP_TYPE_SRAM_LIMIT)	/**< limit sram heap mask.*/
#define ION_HEAP_TYPE_INLINE_ECC_MASK	(1U << ION_HEAP_TYPE_INLINE_ECC)		/**< inline ecc heap mask.*/


#define DEFAULT_ION_ALIGNMENT 0x10	/**< ion default aligment.*/

struct ion_allocation_data {
	size_t len;						/**< size of the allocation */
	size_t align;					/**< required alignment of the allocation */
	unsigned int heap_mask;			/**< mask of heap ids to allocate from */
	unsigned int flags;				/**< flags passed to heap */
	ion_user_handle_t handle;		/**< pointer that will be populated with a cookie to use to refer to this allocation */
	ion_share_handle_t sh_handle;	/**< pointer that will be populated with a cookie to use to refer to share buffer */
};

struct ion_handle_data {
	ion_user_handle_t handle;	/**< a handle */
};

struct ion_share_and_phy_data {
	ion_user_handle_t handle;	/**< ion handle id.*/
	int fd;						/**< share fd of ion buffer.*/
	uint64_t paddr;				/**< buffer physical address.*/
	uint64_t len;				/**< buffer size.*/
	uint64_t reserved;			/**< reserved.*/
};


#define ION_IOC_MAGIC 0x49U
/**
 * DOC: ION_IOC_ALLOC - allocate memory
 *
 * Takes an ion_allocation_data struct and returns it with the handle field
 * populated with the opaque handle for the allocation.
 */
/**
 * @def ION_IOC_ALLOC
 * ion driber alloc buffer command
 */
#define ION_IOC_ALLOC		_IOWR(ION_IOC_MAGIC, 0U, \
				      struct ion_allocation_data)

/**
 * DOC: ION_IOC_FREE - free memory
 *
 * Takes an ion_handle_data struct and frees the handle.
 */
#define ION_IOC_FREE		_IOWR(ION_IOC_MAGIC, 1U, struct ion_handle_data)	/**< ion driver buffer free command */

/**
 * DOC: ION_IOC_MAP - get a file descriptor to mmap
 *
 * Takes an ion_fd_data struct with the handle field populated with a valid
 * opaque handle.  Returns the struct with the fd field set to a file
 * descriptor open in the current address space.  This file descriptor
 * can then be used as an argument to mmap.
 */
#define ION_IOC_MAP		_IOWR(ION_IOC_MAGIC, 2U, struct ion_fd_data)	/**< ion driver map fd command */

#define ION_IOC_SHARE_AND_PHY  _IOWR(ION_IOC_MAGIC, 22U, struct ion_share_and_phy_data)	/**< ion driver relates buffer with share fd and get physical address command*/

int32_t mem_ion_open(void);
int32_t mem_ion_close(void);
int32_t mem_ion_alloc_graph_buf(int32_t w, int32_t h, int32_t format, int64_t flags,
			int32_t stride, int32_t vstride, hb_mem_graphic_buf_t * buf);
int32_t mem_ion_free_graph_buf(hb_mem_graphic_buf_t * buf);
int32_t mem_info_check(hbn_vnode_image_t *image);

#endif // __MEM_ION_H__