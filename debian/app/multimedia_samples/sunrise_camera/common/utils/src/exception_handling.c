#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <ucontext.h>
#include <sys/wait.h>
#include <pthread.h>
#include <errno.h>
#include <ucontext.h>
#include <sys/procfs.h>
#include <sys/ucontext.h>
#include <sys/uio.h>
#include <sys/ptrace.h>
#include <stdint.h>
#include <time.h>
#include <sys/mman.h>
#include <elf.h>
#include <dlfcn.h>
#include <linux/unistd.h>

#include "exception_handling.h"

#define BUFFER_SIZE     1024
#define FUNCNAME_SIZE   64
#define MAX_PATH        256     /* max len of file pathname */
#define LOGFILE_SIZE    512    /* the log file limited (KB) */
#define COMM_LEN        16      /* max len of thread name */

#define THREAD_USER	1			/*user thread*/
#define THREAD_KERNEL	2			/*kernel thread*/
#define THREAD_NOEXIST	3			/*thread no exist */

#define INVALID_INDEX -1

#define min(x,y) ((x) < (y) ? (x) : (y))

typedef struct map_entry {
	unsigned long start;
	unsigned long end;
	unsigned long inode;
	unsigned long long offset;
	char perms[5];
	char dev[7];
	char pathname[MAX_PATH];
}map_entry_t;

typedef uintptr_t   ptr_t;
typedef struct sys_regs {
	ptr_t fp;
	ptr_t sp;
	ptr_t pc;
} regs_t;

#ifndef arch_get_func_addr
# define arch_get_func_addr(addr, v)		(*(v) = addr, 0)
#endif

struct elf_image{
	void *image;		/* pointer to mmap'd image */
	size_t size;		/* (file-) size of the image */
};

typedef struct symbol_info{
	unsigned long addr;
	unsigned long offset;
	char func_name[FUNCNAME_SIZE];
	char filename[MAX_PATH];
} symbol_info_t;

#if defined __aarch64__ || defined __x86_64__
#ifndef ELF_CLASS
#define ELF_CLASS	ELFCLASS64
#endif
#else
#ifndef ELF_CLASS
#define ELF_CLASS	ELFCLASS32
#endif
#endif

#if ELF_CLASS == ELFCLASS32
# define ELF_W(x)	ELF32_##x
# define Elf_W(x)	Elf32_##x
#else
# define ELF_W(x)	ELF64_##x
# define Elf_W(x)	Elf64_##x
#endif

typedef unsigned _Unwind_Word __attribute__((__mode__(__word__)));
typedef signed _Unwind_Sword __attribute__((__mode__(__word__)));
typedef unsigned _Unwind_Ptr __attribute__((__mode__(__pointer__)));
typedef unsigned _Unwind_Internal_Ptr __attribute__((__mode__(__pointer__)));
typedef _Unwind_Word _uw;
typedef unsigned _uw64 __attribute__((mode(__DI__)));
typedef unsigned _uw16 __attribute__((mode(__HI__)));
typedef unsigned _uw8 __attribute__((mode(__QI__)));

typedef enum {
	_URC_OK = 0,       /* operation completed successfully */
	_URC_FOREIGN_EXCEPTION_CAUGHT = 1,
	_URC_END_OF_STACK = 5,
	_URC_HANDLER_FOUND = 6,
	_URC_INSTALL_CONTEXT = 7,
	_URC_CONTINUE_UNWIND = 8,
	_URC_FAILURE = 9   /* unspecified failure of some kind */
}_Unwind_Reason_Code;

typedef enum {
	_US_VIRTUAL_UNWIND_FRAME = 0,
	_US_UNWIND_FRAME_STARTING = 1,
	_US_UNWIND_FRAME_RESUME = 2,
	_US_ACTION_MASK = 3,
	_US_FORCE_UNWIND = 8,
	_US_END_OF_STACK = 16
}_Unwind_State;

/* Provided only for for compatibility with existing code.  */
typedef int _Unwind_Action;
#define _UA_SEARCH_PHASE	1
#define _UA_CLEANUP_PHASE	2
#define _UA_HANDLER_FRAME	4
#define _UA_FORCE_UNWIND	8
#define _UA_END_OF_STACK	16
#define _URC_NO_REASON 	_URC_OK

typedef struct _Unwind_Control_Block _Unwind_Control_Block;
typedef struct _Unwind_Context _Unwind_Context;
typedef _uw _Unwind_EHT_Header;

/* UCB: */
struct _Unwind_Control_Block {
	char exception_class[8];
	void (*exception_cleanup)(_Unwind_Reason_Code, _Unwind_Control_Block *);
	/* Unwinder cache, private fields for the unwinder's use */
	struct {
		_uw reserved1;  /* Forced unwind stop fn, 0 if not forced */
		_uw reserved2;  /* Personality routine address */
		_uw reserved3;  /* Saved callsite address */
		_uw reserved4;  /* Forced unwind stop arg */
		_uw reserved5;
	} unwinder_cache;
	/* Propagation barrier cache (valid after phase 1): */
	struct {
		_uw sp;
		_uw bitpattern[5];
	} barrier_cache;
	/* Cleanup cache (preserved over cleanup): */
	struct {
		_uw bitpattern[4];
	} cleanup_cache;
	/* Pr cache (for pr's benefit): */
	struct {
		_uw fnstart;			/* function start address */
		_Unwind_EHT_Header *ehtp;	/* pointer to EHT entry header word */
		_uw additional;		/* additional data */
		_uw reserved1;
	} pr_cache;
	long long int :0;	/* Force alignment to 8-byte boundary */
};

/* Virtual Register Set*/
typedef enum {
	_UVRSC_CORE = 0,      /* integer register */
	_UVRSC_VFP = 1,       /* vfp */
	_UVRSC_FPA = 2,       /* fpa */
	_UVRSC_WMMXD = 3,     /* Intel WMMX data register */
	_UVRSC_WMMXC = 4      /* Intel WMMX control register */
} _Unwind_VRS_RegClass;

typedef enum {
	_UVRSD_UINT32 = 0,
	_UVRSD_VFPX = 1,
	_UVRSD_FPAX = 2,
	_UVRSD_UINT64 = 3,
	_UVRSD_FLOAT = 4,
	_UVRSD_DOUBLE = 5
} _Unwind_VRS_DataRepresentation;

typedef enum {
	_UVRSR_OK = 0,
	_UVRSR_NOT_IMPLEMENTED = 1,
	_UVRSR_FAILED = 2
} _Unwind_VRS_Result;

typedef _Unwind_Reason_Code (*_Unwind_Trace_Fn)
					(struct _Unwind_Context *, void *);

struct trace_arg {
	void **array;
	int cnt, size;
	_Unwind_VRS_Result (*unwind_vrs_get) (_Unwind_Context *,
			_Unwind_VRS_RegClass,
			_uw,
			_Unwind_VRS_DataRepresentation,
			void *);
};

struct trace_context {
	void *handle;
	_Unwind_Reason_Code (*unwind_backtrace) (_Unwind_Trace_Fn, void *);
	_Unwind_VRS_Result (*unwind_vrs_get) (_Unwind_Context *,
			_Unwind_VRS_RegClass,
			_uw,
			_Unwind_VRS_DataRepresentation,
			void *);
};

/*This function returns a malloced char* that you will have to free yourself.*/
char*  xmalloc_readlink(const char *path)
{
	enum { GROWBY = 80 }; /* how large we will grow strings by */

	char *buf = NULL, *bufold;
	int bufsize = 0, readsize = 0;

	do {
		bufsize += GROWBY;
		bufold = buf;
		buf = realloc(buf, bufsize);
		if (NULL == buf) {
			if (NULL != bufold)
				free(bufold);
			return NULL;
		}
		readsize = readlink(path, buf, bufsize);
		if (readsize == -1) {
			free(buf);
			return NULL;
		}
	} while (bufsize < readsize + 1);

	buf[readsize] = '\0';

	return buf;
}

static int32_t read_int32(char *p) {
	int32_t v = *(unsigned char *)p++;
	v = (v << 8) | *(unsigned char *)p++;
	v = (v << 8) | *(unsigned char *)p++;
	v = (v << 8) | *(unsigned char *)p++;
	return v;
}

int read2buf(const char *filename, void *buf, int buf_size)
{
	int fd;
	int size;

	if(NULL == filename || 0 != access(filename, R_OK))
		return -1;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return -1;

	size = read(fd, buf, buf_size);
	close(fd);
	if(size < 0)
		return -1;

	((char*)buf)[size > 0 ? size :0] = '\0';

	return size;
}

unsigned long fast_strtoul_10(char **endptr)
{
	unsigned char c;
	char *str = *endptr;
	unsigned long n = *str - '0';

	/* Need to stop on both ' ' and '\n' */
	while ((c = *++str) > ' ')
		n = n*10 + (c - '0');

	*endptr = str + 1; /* We skip trailing space! */
	return n;
}

static int find_insert_index(map_entry_t *head[], int item_num,
		map_entry_t *map_entry)
{
	int begin = 0;
	int end = item_num - 1;
	int idx = -1;

	while (begin <= end) {
		idx = (begin + end) / 2;
		if (head[idx]->start > map_entry->start)
			end = idx - 1;
		else
			begin = idx + 1;
	}
	if (head[idx]->start <= map_entry->start)
		idx++;
	return idx;
}

static int Bsearch(map_entry_t *head[], int item_num, unsigned long addr)
{
	int low = 0, high = item_num - 1, mid;
	if (head == NULL || item_num == 0)
		return INVALID_INDEX;

	while (low <= high) {
		mid = (low + high) / 2;
		if (head[mid]->start > addr)
			high = mid - 1;
		else if (head[mid]->end < addr)
			low = mid + 1;
		else if (head[mid]->start <= addr && addr <= head[mid]->end)
			return mid;
		else
			return INVALID_INDEX;
	}
	return INVALID_INDEX;
}


/*count: the number of space char to skip*/
char *skip_fields(char *str, int count)
{
	do {
		while (*str++ != ' ')
			continue;
		/* we found a space char, str points after it */
	} while (--count);
	return str;
}

char*  safe_strncpy(char *dst, const char *src, size_t size)
{
	if (!size)
		return dst;
	dst[--size] = '\0';
	return strncpy(dst, src, size);
}

int get_thread_name(int pid, char *pname)
{
	char filename[sizeof("/proc/%d/task/%d/stat") + sizeof(int)*3*2];
	char buf[BUFFER_SIZE] = {0};
	int length = 0; /* length of thread name */

	sprintf(filename, "/proc/%d/task/%d/stat", getpid(), pid);

	if(-1 == read2buf(filename, buf, BUFFER_SIZE))
		return -1;

	char *cp, *comm1;

	cp = strrchr(buf, ')'); /* split into "pid (cmd" and "<rest>" */
	if (NULL == cp)
		return -1;
	cp[0] = '\0';

	comm1 = strchr(buf, '(');
	if (NULL == comm1)
		return -1;

	length = cp - comm1;

	safe_strncpy(pname, comm1 + 1, min(length, BUFFER_SIZE));/*2: thread name*/

	return 0;
}

pid_t gettid(void){ return syscall(__NR_gettid); }


/* Return thread group id */
pid_t gettgid(pid_t pid)
{
	int fd = -1;
	char filename[sizeof("/proc/%d/task/%d/status") + sizeof(int) * 3 * 2];
	char line[BUFFER_SIZE] = {0}, *p = NULL;
	pid_t tgid = 0;

	sprintf(filename, "/proc/%d/task/%d/status", getpid(), pid);

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return 0;

	if (read(fd, line, BUFFER_SIZE-1) < 0) {
		close(fd);
		return 0;
	}

	close(fd);

	p = strstr(line, "Tgid");
	if (p == NULL)
		return 0;

	sscanf(p, "Tgid:\t%d\n", &tgid);

	return tgid;
}

ssize_t safe_getline(int fd, char *lineptr, size_t size)
{
	ssize_t result = 0;
	size_t cur_len = 0;

	if (lineptr == NULL || fd < 0 || size <= 0) {
		errno = EINVAL;
		return -1;
	}

	for(;;) {
		char i;
		if (read(fd, &i, 1) != 1) {
			result = -1;
			break;
		}
		if (i == EOF) {
			result = -1;
			break;
		}
		lineptr[cur_len] = i;
		cur_len++;
		if (i == '\n' || cur_len == size)
			break;
	}
	lineptr[cur_len] = '\0';
	result = cur_len ? cur_len : result;

	return result;
}

static size_t get_off(const char *file_name) {
	int fd = -1;
	char hdr[44];
	char buf[6]; /* multiple of 4, 6 and 8 */
	size_t len;
	size_t offset = 0;

	fd = open(file_name, O_RDONLY);
	if (fd < 0)
		return 0;

	if (read(fd, hdr, sizeof(hdr)) != sizeof(hdr))
		goto fail;

	size_t nttisstd = read_int32(hdr + 24);
	size_t nttime = read_int32(hdr + 32);

	if (lseek(fd, nttime * 4 + nttime + (nttisstd - 1) * 6, SEEK_CUR) < 0)
		goto fail;

	len = 6;
	if (read(fd, buf, len) != len)
		goto fail;

	offset = read_int32(buf);

	close(fd);
	return offset;
fail:
	close(fd);
	return 0;
}


static char *sys_get_localtime(time_t second, char *fmt_time)
{
	time_t sec_time, temp;
	int month_day[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

	int year = 1970, month, day, hour, min, sec;
	month = day = hour = min = sec = 0;

	if(fmt_time == NULL)
		return NULL;

	sec_time = second + get_off("/etc/localtime");

	for(;;) {
		temp = sec_time;
		if((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
			sec_time -= 31622400;
		else
			sec_time -= 31536000;
		if (sec_time < 0) {
			sec_time = temp;
			break;
		}
		year++;
	}

	if((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
		month_day[1] = 29;

	for(;;) {
		temp = sec_time;
		sec_time = sec_time - month_day[month] * 86400;
		if(sec_time < 0) {
			sec_time = temp;
			break;
		}
		month++;
	}

	month += 1; /* Start from 1 */
	day = sec_time / 86400 + 1; /* Start from 1 */
	hour = (sec_time % 86400) / 3600;
	min = (sec_time % 3600) / 60;
	sec = sec_time % 60;

	sprintf(fmt_time, "%04d-%02d-%02d %02d:%02d:%02d",
	year, month, day, hour, min, sec);

	return fmt_time;
}


static void printf_time()
{
	struct timespec real_time = {0, 0};
	char fmt_time[64] = {0};

	clock_gettime(CLOCK_REALTIME, &real_time);
	if (sys_get_localtime(real_time.tv_sec, fmt_time))
		printf("%s\n", fmt_time);
	else
		printf("%lu secs from 1970-01-01\n", real_time.tv_sec);

	return ;
}

map_entry_t * get_map_entry(map_entry_t *head[], int item_num, unsigned long addr)
{
	int idx = -1;

	idx = Bsearch(head, item_num, addr);

	if(idx == INVALID_INDEX)
		return NULL;

	return head[idx];
}

int valid_pc(map_entry_t *head[], int item_num, ptr_t addr)
{
	int idx = -1;

	idx = Bsearch(head, item_num, addr);

	if(idx == INVALID_INDEX)
		return 0;

	if (NULL != strchr(head[idx]->perms, 'x') && head[idx]->inode != 0)
		return 1;

	return 0;
}

static inline int valid_object(struct elf_image *ei)
{
	if (ei->size <= EI_VERSION)
		return 0;

	return (memcmp(ei->image, ELFMAG, SELFMAG) == 0 &&
		((uint8_t *)ei->image)[EI_CLASS] == ELF_CLASS &&
		((uint8_t *)ei->image)[EI_VERSION] != EV_NONE &&
		((uint8_t *)ei->image)[EI_VERSION] <= EV_CURRENT);
}

static Elf_W(Addr) get_load_offset(struct elf_image *ei,
			unsigned long map_base,
			unsigned long file_offset)
{
	Elf_W(Addr) offset = 0;
	Elf_W(Ehdr) *ehdr;
	Elf_W(Phdr) *phdr;
	int i;

	ehdr = ei->image;
	phdr = (Elf_W(Phdr) *)((char *) ei->image + ehdr->e_phoff);

	for (i = 0; i < ehdr->e_phnum; ++i) {
		if (phdr[i].p_type == PT_LOAD && phdr[i].p_offset == file_offset) {
			offset = map_base - phdr[i].p_vaddr;
			break;
		}
	}

	return offset;
}

/*
*
* Return a pointer point to section header table
*
*/
static Elf_W(Shdr) *section_table(struct elf_image *ei)
{
	Elf_W(Ehdr) *ehdr = ei->image;
	Elf_W(Off) soff;

	soff = ehdr->e_shoff;
	if (soff + ehdr->e_shnum * ehdr->e_shentsize > ei->size) {
		return NULL;
	}

	return (Elf_W(Shdr) *)((char *)ei->image + soff);
}

static char* string_table(struct elf_image *ei, int section)
{
	Elf_W (Ehdr) *ehdr = ei->image;
	Elf_W (Off) soff, str_soff;
	Elf_W (Shdr) *str_shdr;

	/* this offset is assumed to be OK */
	soff = ehdr->e_shoff;

	str_soff = soff + (section * ehdr->e_shentsize);
	if (str_soff + ehdr->e_shentsize > ei->size) {
		return NULL;
	}

	str_shdr = (Elf_W(Shdr) *)((char *) ei->image + str_soff);

	if (str_shdr->sh_offset + str_shdr->sh_size > ei->size) {
		return NULL;
	}

	return (char *)ei->image + str_shdr->sh_offset;
}

void arch_fp_step(pid_t pid,
		ptr_t curr_fp,
		ptr_t *next_pc,
		ptr_t *next_fp)
{
	*next_pc = 0;
	*next_fp = 0;

	if (gettgid(pid) == getpid()) {
		*next_pc = (ptr_t) *((ptr_t *)curr_fp);
		*next_fp = (ptr_t) *((ptr_t *)curr_fp - 1);
	} else {
		*next_pc = ptrace(PTRACE_PEEKDATA, pid, (ptr_t *)curr_fp, 0);
		*next_fp = ptrace(PTRACE_PEEKDATA, pid, (ptr_t *)curr_fp - 1, 0);
	}

	return;
}


int elf_map_image (struct elf_image *ei,
			const char *path)
{
	struct stat statbuf;
	int fd;

	fd = open (path, O_RDONLY);
	if (fd < 0)
		return -1;

	if (fstat(fd, &statbuf) < 0) {
		close (fd);
		return -1;
	}

	ei->size = statbuf.st_size;
	ei->image = mmap(NULL, ei->size, PROT_READ, MAP_PRIVATE, fd, 0);
	close (fd);
	if (ei->image == MAP_FAILED)
		return -1;

	if (!valid_object(ei)) {
		munmap(ei->image, ei->size);
		return -1;
	}

	return 0;
}

static void init(struct trace_context *lib_context)
{
	lib_context->handle = dlopen("libgcc_s.so.1", RTLD_LAZY);

	if (lib_context->handle == NULL)
		return;

	lib_context->unwind_backtrace = dlsym(lib_context->handle, "_Unwind_Backtrace");
	lib_context->unwind_vrs_get = dlsym(lib_context->handle, "_Unwind_VRS_Get");
	if (lib_context->unwind_vrs_get == NULL)
		lib_context->unwind_backtrace = NULL;
}


static inline _Unwind_Word
unwind_getgr (_Unwind_Context *context, int regno,
			_Unwind_VRS_Result (*unwind_vrs_get) (_Unwind_Context *,
				_Unwind_VRS_RegClass,
				_uw,
				_Unwind_VRS_DataRepresentation,
				void *))
{
	_uw val;
	unwind_vrs_get (context, _UVRSC_CORE, regno, _UVRSD_UINT32, &val);
	return val;
}


# define unwind_getip(context, vrs_get) \
	(unwind_getgr(context, 15, vrs_get) & ~(_Unwind_Word)1)


static _Unwind_Reason_Code
backtrace_helper (struct _Unwind_Context *ctx, void *a)
{
	struct trace_arg *arg = a;

	/* We are first called with address in the __backtrace function.
	   Skip it.  */
	if (arg->cnt != -1)
		arg->array[arg->cnt] = (void *) unwind_getip (ctx, arg->unwind_vrs_get);
	if (++arg->cnt == arg->size)
		return _URC_END_OF_STACK;
	return _URC_NO_REASON;
}

int backtrace_unwind(void **array, int size,
		pid_t pid, regs_t *regs, map_entry_t *head[], int item_num)
{
	map_entry_t *stack = NULL;
	ptr_t curr_pc = regs->pc;
	int cnt = 0;
	int self = 0;

	if (!array || !size)
		return 0;

	stack = get_map_entry(head, item_num, regs->sp);
	if (!stack) {
		return 0;
	}

	/* same thread group, can not ptrace */
	if (gettgid(pid) == getpid())
		self = 1;

	while(1) {
		if (valid_pc(head, item_num, curr_pc))
			array[cnt++] = (void *)curr_pc;

		if (cnt >= size)
			break;

		/* invalid sp */
		if (regs->sp < stack->start || regs->sp >= stack->end)
			break;

		if (self)
			curr_pc = *(ptr_t *)regs->sp;
		else
			curr_pc = ptrace(PTRACE_PEEKDATA, pid, (ptr_t *)regs->sp, 0);

		regs->sp += sizeof(ptr_t);
	}

	return cnt;
}


int backtrace_fp(void **array, int size,
		pid_t pid, regs_t *regs, map_entry_t *head[], int item_num)
{
	map_entry_t *stack = NULL;
	int cnt = 0;
	ptr_t next_pc = regs->pc;
	ptr_t next_fp = regs->fp;

	if (!array || !size)
		return 0;

	stack = get_map_entry(head, item_num, regs->sp);
	if (!stack) {
		return 0;
	}

	while(1) {
		if (valid_pc(head, item_num, next_pc))
			array[cnt++] = (void *)next_pc;

		if (cnt >= size)
			break;

		/* invalid sp */
		if (next_fp < stack->start || next_fp >= stack->end)
			break;

		arch_fp_step(pid, next_fp, &next_pc, &next_fp);

	}

	return cnt;
}

static void close_handler (struct trace_context *lib_context)
{
	lib_context->unwind_backtrace = NULL;
	lib_context->unwind_vrs_get = NULL;
	if (lib_context->handle != NULL) {
		dlclose(lib_context->handle);
		lib_context->handle = NULL;
	}
}

int internal_backtrace (void **array, int size)
{
	struct trace_context lib_context = {0};
	struct trace_arg arg = {
		.array = array,
		.size = size,
		.cnt = -1
	};

	init(&lib_context);

	if (lib_context.handle == NULL)
		return 0;

	if (lib_context.unwind_backtrace == NULL)
		return 0;

	arg.unwind_vrs_get = lib_context.unwind_vrs_get;

	if (size >= 1)
		lib_context.unwind_backtrace(backtrace_helper, &arg);

	if (arg.cnt > 1 && arg.array[arg.cnt - 1] == NULL)
		--arg.cnt;

	close_handler(&lib_context);

	return arg.cnt != -1 ? arg.cnt : 0;
}


#define backtrace(array, size)      \
	internal_backtrace(array, size)

int lookup_symbol(struct elf_image *ei,
			unsigned long map_start,
			unsigned long long map_offset,
			unsigned long ip,
			char *func_name,
			unsigned long *func_offset)
{
	Elf_W(Ehdr) *ehdr = ei->image;
	Elf_W(Sym) *sym, *symtab, *symtab_end;
	Elf_W(Shdr) *shdr;
	Elf_W(Addr) val, load_offset;
	size_t syment_size;
	char *strtab;
	unsigned int i = 0;

	if (!valid_object(ei))
		return -1;

	shdr = section_table(ei);
	if (!shdr)
		return -1;

	load_offset = get_load_offset(ei, map_start, map_offset);

	/* scanning section header table */
	for (i = 0; i < ehdr->e_shnum; ++i) {
		switch (shdr->sh_type) {
		case SHT_SYMTAB:
		case SHT_DYNSYM:
			symtab = (Elf_W(Sym) *)((char *)ei->image + shdr->sh_offset);
			symtab_end = (Elf_W(Sym) *)((char *)symtab + shdr->sh_size);
			syment_size = shdr->sh_entsize;     /* entry size */

			strtab = string_table(ei, shdr->sh_link);
			if (!strtab)
				break;

			for (sym = symtab; sym < symtab_end;
					sym = (Elf_W(Sym) *)((char *)sym + syment_size)) {
				if (ELF_W(ST_TYPE)(sym->st_info) == STT_FUNC &&
						sym->st_shndx != SHN_UNDEF) {
					/* val hold the function vaddr */
					if (arch_get_func_addr(sym->st_value, &val) < 0)
						continue;
					if (sym->st_shndx != SHN_ABS)
						val += load_offset;

					/* found the symbol */
					if ((Elf_W(Addr))ip >= (Elf_W(Addr))val &&
						(Elf_W(Addr))ip < val + sym->st_size) {
						if (strlen(strtab + sym->st_name) >= FUNCNAME_SIZE) {
							snprintf(func_name, FUNCNAME_SIZE, "%.63s", strtab + sym->st_name);
						} else {
							strncpy(func_name, strtab + sym->st_name, FUNCNAME_SIZE - 1);
							func_name[FUNCNAME_SIZE - 1] = '\0';
						}
						*func_offset = ip - val;
						return 0;
					}
				}
			}
			break;

		default:
			break;
		}
		shdr = (Elf_W(Shdr) *)(((char *) shdr) + ehdr->e_shentsize);
	}
	return -1;
}


int safe_backtrace_symbols(pid_t pid,
		void * const *array,
		int size,
		map_entry_t *head[], int item_num)
{
	int cnt;
	struct elf_image ei;
	symbol_info_t symbol_info;
	map_entry_t *cur;
	int ret;

	for (cnt = 0; cnt < size; cnt++) {
		cur = get_map_entry(head, item_num, (unsigned long)array[cnt]);
		if (!cur)
			continue;

		if (elf_map_image(&ei, cur->pathname) < 0)
			continue;

		symbol_info.addr = (unsigned long)array[cnt];
		strncpy(symbol_info.filename, cur->pathname, MAX_PATH);

		ret = lookup_symbol(&ei, cur->start, cur->offset,
				symbol_info.addr,
				symbol_info.func_name,
				&(symbol_info.offset));

		munmap(ei.image, ei.size);

		if (ret < 0) {
			(symbol_info.func_name)[0] = '\0';
		}

		printf("<%s> (<%s> + %p) [%p]\n",
				symbol_info.filename,
				symbol_info.func_name,
				(void *)(symbol_info.offset),
				(void *)(symbol_info.addr));
	}
	return 0;
}

static int map_entry_insert(map_entry_t *phead[], int *item_num,
		map_entry_t *map_entry)
{
	int idx = -1;
	int i;

	if (NULL == phead)
		return *item_num;

	if(*item_num == 0) {
		phead[0] = map_entry;
		*item_num += 1;
	} else {
		idx = find_insert_index(phead, *item_num, map_entry);

		i = *item_num;
		while (i > idx) {
			phead[i] = phead[i-1];
			i--;
		}
		phead[idx] = map_entry;
		*item_num += 1;
	}

	return *item_num;
}

int maps_read(int pid, map_entry_t *head[], int *item_num, map_entry_t *poll, int poll_size)
{
	int fd = -1, i = 0;
	char line[BUFFER_SIZE] = {0};
	map_entry_t *tmp = NULL;
	char filename[sizeof("/proc/%d/task/%d/maps") + sizeof(int)*3*2];

	if (!head)
		return -1;

	sprintf(filename, "/proc/%d/task/%d/maps", getpid(), pid);

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return -2;

	while (-1 != safe_getline(fd, line, BUFFER_SIZE)) {
		if (i == poll_size)
			break;

		tmp = &poll[i++];
		if (!tmp)
			break;

		sscanf(line, "%lx-%lx %s %llx %s %lu %s\n",
				&(tmp->start), &(tmp->end),
				tmp->perms, &(tmp->offset),
				tmp->dev, &(tmp->inode),
				tmp->pathname);

		map_entry_insert(head, item_num, tmp);
	}

	close(fd);

	return 0;
}

#define BACKTRACE_SIZE      50         /*max size of backtrace*/
#define BT_EXTBL    0x01
#define BT_UNWIND   0x02
#define BT_FP       0x04
#define BT_MASK     0x07
#define BT_TYPES    0x03

void dump_stack(pid_t pid, regs_t *regs, int flags)
{
	void *array[BACKTRACE_SIZE];
	char *prompt[BT_TYPES] = {0};
	size_t size[BT_TYPES] = {0};
	int cnt = 0;
	int item_num = 0;
#define ITEM_MAX_NUM 1024
	map_entry_t *head[ITEM_MAX_NUM];
	map_entry_t item_poll[ITEM_MAX_NUM];

	/* read the whole address space */
	memset(item_poll, 0, sizeof(item_poll));
	if (maps_read(pid, head, &item_num, item_poll, ITEM_MAX_NUM) < 0)
		return;

	if (flags & BT_EXTBL) {
		if (gettgid(pid) == getpid()) {
			prompt[cnt] = "extbl";
			memset(array, 0, sizeof(array));
			size[cnt] = backtrace(array, BACKTRACE_SIZE);
			printf("\n===========Call Trace(%s)==========\n", prompt[cnt]);
			printf("Obtained %zd stack frames:\n", size[cnt]);
			safe_backtrace_symbols(getpid(), array, size[cnt], head, item_num);
			cnt++;
		}
	}

	if (flags & BT_FP) {
		prompt[cnt] = "fp";
		memset(array, 0, sizeof(array));
		size[cnt] = backtrace_fp(array, BACKTRACE_SIZE, pid, regs, head, item_num);
		printf("\n===========Call Trace(%s)==========\n", prompt[cnt]);
		printf("Obtained %zd stack frames:\n", size[cnt]);
		safe_backtrace_symbols(pid, array, size[cnt], head, item_num);
		cnt++;
	}

	if (flags & BT_UNWIND) {
		prompt[cnt] = "unwind";
		memset(array, 0, sizeof(array));
		size[cnt] = backtrace_unwind(array, BACKTRACE_SIZE, pid, regs, head, item_num);
		printf("\n===========Call Trace(%s)==========\n", prompt[cnt]);
		printf("Obtained %zd stack frames:\n", size[cnt]);
		safe_backtrace_symbols(pid, array, size[cnt], head, item_num);
	}
	return;
}


static inline void print_except_header(int signo)
{
	pid_t pid;
	char thread_name[BUFFER_SIZE] = {0};

	pid = (pid_t)gettid();

	get_thread_name(pid, thread_name);

	printf("\n=============== Exception Occur =================\n");
	printf("Thread Tid: %d\n", pid);
	printf("Thread name:%s\n", thread_name);
	printf("Signal num: %d\n", signo);
	printf("Exception time: ");
	printf_time();
}
static void print_backtrace(struct ucontext_t *ct)
{
	regs_t regs;
	dump_stack(gettid(), &regs, BT_EXTBL | BT_UNWIND);
}

static void exception_handing(int signo,
		siginfo_t* info, void* ct)
{
	print_except_header(signo);
	print_backtrace((struct ucontext_t *)ct);

	return ;
}

int register_exception_signals(void)
{
	int i = 0;
	struct sigaction sigact;
	int signals[10] = {
		SIGHUP, SIGQUIT, SIGILL, SIGTRAP, SIGABRT,
		SIGBUS, SIGFPE, SIGSEGV, SIGPWR, SIGSYS };

	stack_t ss;
	ss.ss_sp = malloc(SIGSTKSZ);
	if (ss.ss_sp == NULL)
		return -1;

	ss.ss_flags = 0;
	ss.ss_size = SIGSTKSZ;
	if (sigaltstack (&ss, NULL) < 0) {
		free(ss.ss_sp);
		return -1;
	}

	sigemptyset(&sigact.sa_mask);
	memset(&sigact, 0, sizeof (struct sigaction));
	sigact.sa_flags = SA_ONESHOT | SA_SIGINFO;
	sigact.sa_sigaction = exception_handing;

	for (i = 0; i < 10; i++)
		sigaction(signals[i], &sigact, NULL);

	return 0;
}

