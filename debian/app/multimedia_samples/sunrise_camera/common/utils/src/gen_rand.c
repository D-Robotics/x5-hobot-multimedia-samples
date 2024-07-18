#include <string.h>
#include <time.h>
#ifdef WIN32
#include <windows.h>
#elif __APPLE__
#include <sys/types.h>
#include <sys/sysctl.h>
#else
#include <sys/sysinfo.h>
#endif

#include "gen_rand.h"
#include "sha256.h"

#ifndef WIN32
#define UINT64	unsigned long long
#endif

static unsigned char randpool[32];

void init_rand(unsigned char *randevent, unsigned int randlen)
{
	sha256_mac(randevent, randlen, randpool);
}

void init_rand_ex()
{
#ifdef WIN32
	struct stRandInfo
	{
		time_t	tmNow;
		DWORD	dwNowTick;
		DWORD	dwProcID;
		DWORD	dwThreadID;
		MEMORYSTATUS	stMem;
	};

	struct stRandInfo info;
	info.tmNow = time(NULL);
	info.dwNowTick = GetTickCount();
	info.dwProcID = GetCurrentProcessId();
	info.dwThreadID = GetCurrentThreadId();
	//
	info.stMem.dwLength = sizeof(MEMORYSTATUS);
	GlobalMemoryStatus(&info.stMem);

	init_rand((unsigned char*)&info,sizeof(info));

#elif __APPLE__

	struct stRandInfo
	{
		time_t tmNow;
		struct timeval uptimer;
		struct loadavg load;
		unsigned long memsize;
		unsigned long usermem;
	};

	struct stRandInfo rand_info;
	rand_info.tmNow = time(NULL);
	int mib[2];
	size_t len;

	//???????????????
	mib[0] = CTL_KERN;
	mib[1] = KERN_BOOTTIME;
	len = sizeof(struct timeval);
	sysctl(mib,2,&rand_info.uptimer,&len,NULL,0);

	mib[0] = CTL_VM;
	mib[1] = VM_LOADAVG;
	len = sizeof(struct loadavg);
	sysctl(mib,2,&rand_info.load,&len,NULL,0);

	mib[0] = CTL_HW;
	mib[1] = HW_MEMSIZE;
	len = sizeof(unsigned long);
	sysctl(mib,2,&rand_info.memsize,&len,NULL,0);

	mib[0] = CTL_HW;
	mib[1] = HW_USERMEM;
	len = sizeof(unsigned long);
	sysctl(mib,2,&rand_info.usermem,&len,NULL,0);

	init_rand((unsigned char*)&rand_info,sizeof(rand_info));

#else

	struct stRandInfo
	{
		time_t tmNow;
		struct sysinfo info;
	};

	struct stRandInfo rand_info;
	rand_info.tmNow = time(NULL);
	sysinfo(&rand_info.info);

	init_rand((unsigned char*)&rand_info,sizeof(rand_info));
#endif
}

void gen_rand(unsigned char* buf, unsigned int buflen)
{
	static UINT64 randcnt = 0;

	UINT64 randcnt2;

	unsigned int n;
	unsigned char tmp[32 + sizeof(randcnt2)];
	unsigned char mac[32];

	while(buflen != 0)
	{
		randcnt2 = ++randcnt;

		memcpy(tmp,randpool,32);
		memcpy(tmp+32,&randcnt2,sizeof(randcnt2));

		sha256_mac(tmp,sizeof(tmp),mac);

		n = (buflen < 32)?buflen:32;
		memcpy(buf,mac,n);
		buf += n;
		buflen -= n;
	}
}