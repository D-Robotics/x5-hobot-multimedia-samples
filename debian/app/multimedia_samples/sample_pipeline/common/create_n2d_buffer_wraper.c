#include "create_n2d_buffer_wraper.h"

n2d_error_t create_n2d_buffer_from_phyaddr_continuous_memory(n2d_buffer_t *n2d_buffer,
	n2d_buffer_format_t format, n2d_uintptr_t phys_addr, int width, int height){
	n2d_error_t error = N2D_SUCCESS;

	memset(n2d_buffer, 0, sizeof(n2d_buffer_t));
	n2d_buffer->width = width;
    n2d_buffer->height = height;

    n2d_buffer->format = format;	// N2D_NV12
    n2d_buffer->orientation = N2D_0;
    n2d_buffer->srcType = N2D_SOURCE_DEFAULT;
    n2d_buffer->tiling = N2D_LINEAR;
    n2d_buffer->cacheMode = N2D_CACHE_128;
    n2d_buffer->alignedw = gcmALIGN(n2d_buffer->width, 64);
    n2d_buffer->alignedh = n2d_buffer->height;

	float nv12_bpp = gcmALIGN(16, 8) * 1.0f / 8;
    n2d_buffer->stride = gcmALIGN(gcmFLOAT2INT(n2d_buffer->alignedw * nv12_bpp), 64);

	n2d_uintptr_t n2d_buffer_handle;
	n2d_user_memory_desc_t n2d_buffer_mem_desc;
    n2d_buffer_mem_desc.flag = N2D_WRAP_FROM_USERMEMORY;
    n2d_buffer_mem_desc.logical = 0; // MUST BE ZERO!!!
    n2d_buffer_mem_desc.physical = phys_addr;

    n2d_buffer_mem_desc.size = n2d_buffer->stride * n2d_buffer->alignedh * 3 / 2;
    error = n2d_wrap(&n2d_buffer_mem_desc, &n2d_buffer_handle);
    if (N2D_IS_ERROR(error)){
        return error;
    }
    n2d_buffer->handle = n2d_buffer_handle;
    error = n2d_map(n2d_buffer);
    if (N2D_IS_ERROR(error)){
        return error;
    }

	return N2D_SUCCESS;
}


n2d_error_t create_n2d_buffer_from_hbm_graphic(n2d_buffer_t *n2d_buffer, hb_mem_graphic_buf_t *hbm_buffer){
	n2d_error_t error = N2D_SUCCESS;

	// printf("==n2d: %ld  %ld hbm_buffer->width %d hbm_buffer->height %d stride%d %d\n",
	// 	hbm_buffer->phys_addr[0],
	// 	hbm_buffer->phys_addr[1],
	// 	hbm_buffer->width,
	// 	hbm_buffer->height,
	// 	hbm_buffer->stride,
	// 	hbm_buffer->vstride);

	error = create_n2d_buffer_from_phyaddr_continuous_memory(n2d_buffer, N2D_NV12,
		(n2d_uintptr_t)hbm_buffer->phys_addr[0], hbm_buffer->width, hbm_buffer->height);

	// n2d_buffer->memory = hbm_buffer->virt_addr[0];
	// n2d_buffer->uv_memory[0] = hbm_buffer->virt_addr[0];
	// n2d_buffer->uv_memory[1] = hbm_buffer->virt_addr[1];
	return error;
}

n2d_error_t create_n2d_buffer_from_hbm_common(n2d_buffer_t *n2d_buffer,
	hb_mem_common_buf_t *hbm_buffer, int width, int height){
	n2d_error_t error = N2D_SUCCESS;

	error =  create_n2d_buffer_from_phyaddr_continuous_memory(n2d_buffer, N2D_NV12,
		(n2d_uintptr_t)hbm_buffer->phys_addr, width, height);

	// n2d_buffer->memory = hbm_buffer->virt_addr;

	// n2d_buffer->uv_memory[0] = hbm_buffer->virt_addr;
	// n2d_buffer->uv_memory[1] = (n2d_pointer)((uint64_t)hbm_buffer->virt_addr + width * height);

	// n2d_buffer->stride = width;

	return error;
}

int create_hbm_graphic_buffer_from_normal_memory(hb_mem_graphic_buf_t *hbn_mem_src,
	int width, int height, mem_pixel_format_t format , void *virt_addr){

	if(format != MEM_PIX_FMT_NV12){
		printf("create_hbm_graphic_buffer_from_normal_memory failed not support format %d\n", format);
		return -1;
	}
	int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN
				| HB_MEM_USAGE_CACHED |HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF;
	int ret = hb_mem_alloc_graph_buf(width, height, MEM_PIX_FMT_NV12, flags, 0, 0, hbn_mem_src);
	if(ret != 0){
		printf("hb_mem_alloc_graph_buf failed :%d\n", ret);
		return -1;
	}

	memcpy(hbn_mem_src->virt_addr[0], virt_addr, width * height * 1.5);

	ret = hb_mem_flush_buf_with_vaddr((uint64_t)hbn_mem_src->virt_addr[0], hbn_mem_src->size[0]);
	if (ret < 0) {
		hb_mem_free_buf(hbn_mem_src->fd[0]);
		printf("hb_mem_flush_buf_with_vaddr[0] failed :%d\n", ret);
		return -1;
	}
	ret = hb_mem_flush_buf_with_vaddr((uint64_t)hbn_mem_src->virt_addr[1], hbn_mem_src->size[1]);
	if (ret < 0) {
		hb_mem_free_buf(hbn_mem_src->fd[0]);
		printf("hb_mem_flush_buf_with_vaddr[1] failed :%d\n", ret);
		return -1;
	}
	return 0;
}
