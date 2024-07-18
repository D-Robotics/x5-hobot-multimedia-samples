#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/prctl.h>

#include "mthread.h"

#define THREAD_SIGNAL SIGUSR2

/************************** Threads Functionality ****************************/

/** Structure representing an OS independant thread */
typedef struct
{
	pthread_t thread;
	tprThreadFunction   prThreadFunction;
} tsThreadPrivate;


/** Signal handler to receive THREAD_SIGNAL.
 *  This is just used to interrupt system calls such as recv() and sleep().
 */
static void thread_signal_handler(int sig)
{
	pthread_t self_id = pthread_self(); // 获取当前线程ID
	printf("Signal %d received in thread %lu\n", sig, self_id);
}
teThreadStatus mThreadStartHighPriority(tprThreadFunction prThreadFunction, tsThread *psThreadInfo, teThreadDetachState eDetachState)
{
	psThreadInfo->eState = E_THREAD_STOPPED;
	psThreadInfo->eThreadDetachState = eDetachState;

	static int iFirstTime = 1;
	if (iFirstTime)
	{
		/* Set up sigmask to receive configured signal in the main thread.
		* All created threads also get this signal mask, so all threads
		* get the signal. But we can use pthread_signal to direct it at one.
		*/
		struct sigaction sa;
		sa.sa_handler = thread_signal_handler;
		sa.sa_flags = 0;
		sigemptyset(&sa.sa_mask);

		if (sigaction(THREAD_SIGNAL, &sa, NULL) == -1)
		{
			printf("sigaction:%s\n", strerror(errno));
		}
		else
		{
			printf("Signal action registered\n\r");
			iFirstTime = 0;
		}
	}

	pthread_attr_t attr;
    struct sched_param param;

    pthread_attr_init(&attr);

    // 设置调度策略为 SCHED_FIFO
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);

    // 设置优先级为 1
    param.sched_priority = 1;
    pthread_attr_setschedparam(&attr, &param);
	errno = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
	if(errno != 0)
	{
		perror("setinherit failed\n");
		return E_THREAD_ERROR_FAILED;
	}
	psThreadInfo->eState = E_THREAD_RUNNING;
	if (pthread_create(&psThreadInfo->pThread_Id, &attr, prThreadFunction, psThreadInfo))
	{
		printf("Could not start thread:%s\n", strerror(errno));
		return E_THREAD_ERROR_FAILED;
	}

	if (eDetachState == E_THREAD_DETACHED)
	{
		printf("Detach Thread %p\n", psThreadInfo);
		if (pthread_detach(psThreadInfo->pThread_Id))
		{
			printf("pthread_detach():%s\n", strerror(errno));
			return E_THREAD_ERROR_FAILED;
		}
	}
	printf("Create Thread %p\n", psThreadInfo);
	return  E_THREAD_OK;
}

// 启动线程
teThreadStatus mThreadStart(tprThreadFunction prThreadFunction, tsThread *psThreadInfo, teThreadDetachState eDetachState)
{

	psThreadInfo->eState = E_THREAD_STOPPED;
	psThreadInfo->eThreadDetachState = eDetachState;

	static int iFirstTime = 1;
	if (iFirstTime)
	{
		/* Set up sigmask to receive configured signal in the main thread.
		* All created threads also get this signal mask, so all threads
		* get the signal. But we can use pthread_signal to direct it at one.
		*/
		struct sigaction sa;
		sa.sa_handler = thread_signal_handler;
		sa.sa_flags = 0;
		sigemptyset(&sa.sa_mask);

		if (sigaction(THREAD_SIGNAL, &sa, NULL) == -1)
		{
			printf("sigaction:%s\n", strerror(errno));
		}
		else
		{
			printf("Signal action registered\n\r");
			iFirstTime = 0;
		}
	}
	//要在 create之前设置：如果在函数最后设置 状态，会导致如下bug
	//线程启动完成并且函数运行起来 while 时 发现状态是STOPed 就直接退出了
	psThreadInfo->eState = E_THREAD_RUNNING;
	if (pthread_create(&psThreadInfo->pThread_Id, NULL, prThreadFunction, psThreadInfo))
	{
		printf("Could not start thread:%s\n", strerror(errno));
		return E_THREAD_ERROR_FAILED;
	}

	if (eDetachState == E_THREAD_DETACHED)
	{
		printf("Detach Thread %p\n", psThreadInfo);
		if (pthread_detach(psThreadInfo->pThread_Id))
		{
			printf("pthread_detach():%s\n", strerror(errno));
			return E_THREAD_ERROR_FAILED;
		}
	}
	printf("Create Thread %p\n", psThreadInfo);
	return  E_THREAD_OK;
}
const char *mThreadStateString(int state){
	switch (state)
	{
	case E_THREAD_STOPPED:
		return "stoped";
	case E_THREAD_RUNNING:
		return "stoped";
	case E_THREAD_STOPPING:
		return "stoped";
	default:
		return "unknown";
	}

	return "unknown";
}

// 在主线程调用，用来结束线程
teThreadStatus mThreadStop(tsThread *psThreadInfo)
{
	if (psThreadInfo->eState == E_THREAD_STOPPED) {
		printf("Stopping Thread %s, found thread is not runing, so return.\n", psThreadInfo->pThread_Name);
		return  E_THREAD_OK; // 有可能是重复调用退出线程，也有可能是调用退出一个没有启动的线程
	}
	printf("Stopping Thread %s\n", psThreadInfo->pThread_Name);

	psThreadInfo->eState = E_THREAD_STOPPING; // 如果线程中存在死循环，这里就是退出标志

	if (0 != psThreadInfo->pThread_Id)
	{

		/* Send signal to the thread to kick it out of any system call it was in */
		pthread_kill(psThreadInfo->pThread_Id, THREAD_SIGNAL);
		printf("Signaled Thread %p\n", psThreadInfo);

		if (psThreadInfo->eThreadDetachState == E_THREAD_JOINABLE)
		{
			/* Thread is joinable */
			if (pthread_join(psThreadInfo->pThread_Id, NULL))
			{
				printf("Could not join thread:%s\n", strerror(errno));
				return E_THREAD_ERROR_FAILED;
			}
		}
		else
		{
			printf("Cannot join detached thread %p\n", psThreadInfo);
			return E_THREAD_ERROR_FAILED;
		}
	}

	{
		int wait_count = 0;
		while(psThreadInfo->eState == E_THREAD_STOPPING){
			usleep(10 * 1000);
			if(psThreadInfo->eState != E_THREAD_STOPPING){
				break;
			}
			if(wait_count % 100 == 0){
				printf("waited [%d] second for thread [%s:%lu] complete stop, current state %s.\n",
				wait_count/100, psThreadInfo->pThread_Name, psThreadInfo->pThread_Id,
				mThreadStateString(psThreadInfo->eState));
			}
			wait_count++;
		}
	}

	printf("Stopped Thread %s\n", psThreadInfo->pThread_Name);
	psThreadInfo->eState = E_THREAD_STOPPED;
	psThreadInfo->pThread_Id = -1;
	return  E_THREAD_OK;
}

// 在线程结束时调用
teThreadStatus mThreadFinish(tsThread *psThreadInfo)
{
	psThreadInfo->eState = E_THREAD_STOPPED;

	printf("Finish Thread %s\n", psThreadInfo->pThread_Name);

	/* Cleanup function is called when pthread quits */
	pthread_exit(NULL);
	return E_THREAD_OK; /* Control won't get here */
}

// 重新调度CPU，让本线程进入到线程队尾，然其他线程运行
teThreadStatus mThreadYield(void)
{
	sched_yield();
	return E_THREAD_OK;
}

// 设置线程名称

void mThreadSetName(tsThread *psThreadInfo, const char *name)
{
	pthread_t tid = pthread_self();

	if (NULL != name)
	{
		sprintf(psThreadInfo->pThread_Name, "%s", (char *)name);
		prctl(PR_SET_NAME, psThreadInfo->pThread_Name);
	}
	else
	{
		sprintf(psThreadInfo->pThread_Name, "p%zu", tid);
		prctl(PR_SET_NAME, psThreadInfo->pThread_Name);
	}
	printf("Thread %s start running\n", name);
}

