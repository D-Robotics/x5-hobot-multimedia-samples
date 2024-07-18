#ifndef __RTSP_SVR_HH__
#define __RTSP_SVR_HH__

#include <pthread.h>
#include "BasicUsageEnvironment.hh"
#include "RTSPServer.hh"
#include "H264VideoLiveServerMediaSubsession.hh"
#include "H265VideoLiveServerMediaSubsession.hh"
#include "LPCMAudioLiveServerMediaSubsession.hh"
#include "PCMAAudioLiveServerMediaSubsession.hh"
#include "utils/cqueue.h"

struct SmsParam{
	int actionType; //0: 删除， 1:添加
	char streamName[128];
	bool audioEnable;
	int audioType;
	int audioSampleRate;
	int audioBitPerSample;
	int audioChannels;
	bool videoEnable;
	int videoType;
	int videoFrameRate;
	char shmId[32];
	char shmName[32];
	int streamBufSize;
	int frameRate;
	int suggest_buffer_item_count;
	int suggest_buffer_region_size;
};

class CRtspServer
{
public:
	static CRtspServer* GetInstance();

	bool Create(portNumBits port);
	bool Destory();
	bool Start();
	bool Stop();
	bool Restart();
	bool DynamicAddSms(const char* streamName,
		bool audioEnable, int audioType, int audioSampleRate, int audioBitPerSample,
		int audioChannels, bool videoEnable, int videoType, int videoFrameRate,
		char *shmId, char *shmName, int streamBufSize, int frameRate,
		int suggest_buffer_region_size, int suggest_buffer_item_count);
	bool DynamicDelSms(const char* streamName);
	bool DynamicProcessSmsCommonProcess(int actionType, const char*streamName,
		bool audioEnable, int audioType, int audioSampleRate, int audioBitPerSample,
		int audioChannels, bool videoEnable, int videoType, int videoFrameRate,
		char *shmId, char *shmName, int streamBufSize, int frameRate,
		int suggest_buffer_region_size, int suggest_buffer_item_count);
	/*H264VideoLiveServerMediaSubsession* m_h264_subsession;*/
	/*PCMAAudioLiveServerMediaSubsession* m_PCMA_subsession;*/

protected:
	CRtspServer();
	~CRtspServer();

	static void* ThreadRtspServerProcImpl(void* arg);
	void ThreadRtspServer();

	static void AsyncProcessSms(void *param);
	static void DynamicDelSmsInternal(CRtspServer *rtsp_server, struct SmsParam *sms_param);
	static void DynamicAddSmsInternal(CRtspServer *rtsp_server, struct SmsParam *sms_param);

private:
	static  CRtspServer* instance;
	bool 	m_Stop;
	char 	m_watchVariable;
	TaskScheduler* 		m_scheduler;
	UsageEnvironment* 	m_env;
	RTSPServer* 		m_rtspServer;
	pthread_t 			m_pThread;
	portNumBits			m_port;
	cqueue m_sms_action_queue;

	EventTriggerId m_process_sms;
	/*char				m_streamName[128];*/
	/*bool 				m_audioEnable;*/
	/*int 				m_audioType;*/
	/*int 				m_audioSampleRate;*/
	/*int 				m_audioBitPerSample;*/
	/*int 				m_audioChannels;*/
	/*bool 				m_videoEnable;*/
	/*int 				m_videoType;*/
	/*int 				m_videoFrameRate;*/
};

#endif
