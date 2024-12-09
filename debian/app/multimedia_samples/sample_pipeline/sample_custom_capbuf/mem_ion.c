
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include "./mem_ion.h"

int32_t g_mem_ion_fd = -1;
int32_t g_mem_ion_cnt = 0;
static pthread_mutex_t g_mem_ion_mutex = PTHREAD_MUTEX_INITIALIZER; /* PRQA S 3004 */

int32_t mem_info_check(hbn_vnode_image_t *image)
{
	int32_t ret = 0;

	if (image->info.bufferindex > 15 || image->info.bufferindex < 0) {
		ret = -1;
	}
	if (image->buffer.share_id[0] <= 0) {
		ret = -1;
	}
	if (image->buffer.is_contig == 0) {
		ret = -1;
	}

	if (ret != 0) {
		printf("*** %s fail\n", __func__);
	}

	return ret;
}


int32_t mem_ion_open(void)
{
	int ret = 0;

	(void)pthread_mutex_lock(&g_mem_ion_mutex);
	if (g_mem_ion_fd > 0) {
		g_mem_ion_cnt++;
		(void)pthread_mutex_unlock(&g_mem_ion_mutex);
		return 0;
	}
	g_mem_ion_fd = open("/dev/ion", O_RDWR);
	if (g_mem_ion_fd < 0) {
		ret = g_mem_ion_fd;
		printf("Fail to open ion device\n");
	}
	(void)pthread_mutex_unlock(&g_mem_ion_mutex);

	return ret;
}

int32_t mem_ion_close(void)
{
	int ret = 0;

	(void)pthread_mutex_lock(&g_mem_ion_mutex);
	if (--g_mem_ion_cnt == 0) {
		ret = close(g_mem_ion_fd);
		if (ret < 0) {
			printf("Fail to close ion device ret %d\n", ret);
		}
	}
	(void)pthread_mutex_unlock(&g_mem_ion_mutex);
	return ret;
}

int32_t mem_ion_share_and_phys(ion_user_handle_t handle,
		int32_t *share_fd, void **paddr, size_t *len)
{
	int32_t ret;

	struct ion_share_and_phy_data data = {
		.handle = handle,
	};

	ret = ioctl(g_mem_ion_fd, ION_IOC_SHARE_AND_PHY, (void *)&data);
	if (ret < 0) {
		printf("Fail to ION_IOC_SHARE_AND_PHY\n");
	}
	*share_fd = data.fd;
	*paddr = (void *)data.paddr;
	*len = data.len;

	return ret;
}

int32_t mem_ion_alloc(int32_t fd, size_t len, size_t align, uint32_t heap_mask,
				uint32_t flags, ion_user_handle_t *handle, ion_share_handle_t *share_hd)
{
	int32_t ret;
	struct ion_allocation_data data = {
		.len = len,
		.align = align,
		.heap_mask = heap_mask,
		.flags = flags,
	};

	if (handle == NULL)
		return -1;

	ret = ioctl(fd, ION_IOC_ALLOC, (void *)&data);/* PRQA S 4513 */
	if (ret < 0) {
		printf("Fail to ION_IOC_ALLOC ret %d\n", ret);
		return -1;
	}

	*handle = data.handle;
	*share_hd = data.sh_handle;

	return ret;
}

int32_t mem_ion_alloc_graph_buf(int32_t w, int32_t h, int32_t format, int64_t flags,
			int32_t stride, int32_t vstride, hb_mem_graphic_buf_t * buf)
{
	uint32_t heap_mask;
	size_t luma_size;
	size_t total_size;
	int32_t share_fd;
	uint64_t phys_addr;
	size_t len;
	int32_t ret;
	ion_user_handle_t handle;
	ion_share_handle_t sh_handle;

	heap_mask = ION_HEAP_CMA_RESERVED_MASK;
	luma_size = stride * h;
	total_size = luma_size * 1.5;

	ret = mem_ion_alloc(g_mem_ion_fd, total_size, DEFAULT_ION_ALIGNMENT,
		heap_mask, 0, &handle, &sh_handle);
	if (ret < 0) {
		printf("Fail to alloc ret %d\n", ret);
	}

	printf("*** handle %ld, sh_handle %d\n", handle, sh_handle);
	ret = mem_ion_share_and_phys(handle, &share_fd, (void **)&phys_addr, &len);
	if (ret < 0) {
		printf("Fail to alloc ret %d\n", ret);
	}
	printf("*** handle %ld, share_fd %d, phys_addr 0x%lx, len 0x%lx\n",
		handle, share_fd, phys_addr, len);

	buf->share_id[0] = sh_handle;
	buf->phys_addr[0] = phys_addr;
	buf->phys_addr[1] = phys_addr + luma_size;

	buf->width = w;
	buf->height = h;
	buf->stride = stride;

	buf->is_contig = 1;
	buf->offset[0] = handle; // TODO
	buf->virt_addr[0] = (uint8_t *)mmap(NULL, total_size, (0x3), MAP_SHARED, share_fd, 0);
	buf->virt_addr[1] = buf->virt_addr[0] + luma_size;
	buf->size[0] = luma_size;
	buf->size[1] = total_size - luma_size;

	printf("*** handle %ld, share_fd %d, virt_addr 0x%p, virt_addr1 0x%p\n",
		handle, share_fd, buf->virt_addr[0], buf->virt_addr[1]);

	return ret;
}

int32_t mem_ion_free(int32_t fd, ion_user_handle_t handle)
{
	struct ion_handle_data data = {
		.handle = handle,
	};
	return ioctl(fd, ION_IOC_FREE, (void *)&data);
}

int32_t mem_ion_free_graph_buf(hb_mem_graphic_buf_t * buf)
{
	return mem_ion_free(g_mem_ion_fd, buf->offset[0]);
}