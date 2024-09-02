#ifndef MTHREAD_H_
#define MTHREAD_H_


typedef enum
{
    E_THREAD_OK,
    E_THREAD_ERROR_FAILED,
    E_THREAD_ERROR_TIMEOUT,
    E_THREAD_ERROR_NO_MEM,
} teThreadStatus;

typedef enum
{
    E_THREAD_JOINABLE,  /**< Thread is created so that it can be waited on and joined */
    E_THREAD_DETACHED,  /**< Thread is created detached so all resources are automatically free'd at exit. */
} teThreadDetachState;


typedef struct
{
    volatile enum
    {
        E_THREAD_STOPPED,
        E_THREAD_RUNNING,
        E_THREAD_STOPPING,
    } eState;
    teThreadDetachState eThreadDetachState;
    pthread_t pThread_Id;
	char pThread_Name[128];
    void *pvThreadData;
} tsThread;

typedef void *(*tprThreadFunction)(void *psThreadInfoVoid);

teThreadStatus mThreadStart(tprThreadFunction prThreadFunction, tsThread *psThreadInfo, teThreadDetachState eDetachState);
teThreadStatus mThreadStartHighPriority(tprThreadFunction prThreadFunction, tsThread *psThreadInfo, teThreadDetachState eDetachState);

teThreadStatus mThreadStop(tsThread *psThreadInfo);
teThreadStatus mThreadFinish(tsThread *psThreadInfo);
teThreadStatus mThreadYield(void);
void mThreadSetName(tsThread *psThreadInfo, const char *name);
void mThreadSetNameWidthIndex(tsThread *psThreadInfo, const char *name, int index);

#endif // MTHREAD_H_