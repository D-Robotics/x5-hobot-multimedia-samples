#include "RtspSvr.hh"
#include <sched.h>
#include <sys/prctl.h>
#include "utils/utils_log.h"
#include "utils/common_utils.h"
#include "rtsp_server_default_param.h"

CRtspServer* CRtspServer::instance = NULL;

static int const samplingFrequencyTable[16] =
{
	96000, 88200, 64000, 48000,
	44100, 32000, 24000, 22050,
	16000, 12000, 11025, 8000,
	7350, 0, 0, 0
};
static int StringCopyWithCheck(char *dst, const char *src, int dst_len){

	int src_len = strlen(src) + 1;
	if(src_len > dst_len){
		SC_LOGE("copy <%s> failed, name is too short, %d < %d.",
				src, dst_len, src_len);
		return -1;
	}
	strncpy(dst, src, dst_len);

	return 0;
}
static bool CheckAsyncProcessSmsIsComplete(cqueue *queue){
	int check_period_ms = 100;
	//等待1s 查询是否删除完毕
	for(int i = 0; i< 10; i++){
		if(i == 0){
			usleep(1 * 1000); //wait 1ms
		}else{
			usleep(check_period_ms * 1000); //wait 100ms
		}
		int is_empty = cqueue_is_empty(queue);
		if(is_empty){
			SC_LOGI("rtsp server sms async process consumed %d ms", check_period_ms * i + 1);
			return true;
		}
	}
	return false;
}

static int GetSamplingFrequencyIndex(int sampleate)
{
	int index = 0;
	unsigned int i = 0;
	for(i = 0; i < ARRAY_SIZE(samplingFrequencyTable); i++)
	{
		if(samplingFrequencyTable[i] == sampleate)
		{
			index = i;
			break;
		}
	}

	return index;
}

CRtspServer* CRtspServer::GetInstance()
{
	if(instance == NULL)
	{
		instance = new CRtspServer();
	}

	return instance;
}

CRtspServer::CRtspServer()
	:m_Stop(true),m_watchVariable(0),m_scheduler(NULL),m_env(NULL),
	m_rtspServer(NULL),m_pThread(0)
{

}

CRtspServer::~CRtspServer()
{
	if(m_env == NULL && m_scheduler == NULL)
	{
		Destory();
	}
}

void * CRtspServer::ThreadRtspServerProcImpl(void* arg)
{
	CRtspServer* cRtspServer = (CRtspServer*)arg;

	cRtspServer->ThreadRtspServer();

	return NULL;
}

void CRtspServer::ThreadRtspServer()
{
	/*Boolean reuseFirstSource = False;*/

	UserAuthenticationDatabase* authDB = NULL;
#ifdef ACCESS_CONTROL
	authDB = new UserAuthenticationDatabase;
	authDB->addUserRecord("admin", "admin123"); // replace these with real strings
#endif

	prctl(PR_SET_NAME, "rtsp server");

	do{
		portNumBits rtspServerPortNum = m_port;
		m_rtspServer = RTSPServer::createNew(*m_env, rtspServerPortNum, authDB);
		if (m_rtspServer == NULL) {
			SC_LOGE("m_rtspServer create error, port:%d", rtspServerPortNum);
			break;
		}
		SC_LOGI("CRtspServer Start At Port: %d", m_port);
		m_Stop = false;
		m_env->taskScheduler().doEventLoop(&m_watchVariable); // does not return
	}while(0);

#ifdef ACCESS_CONTROL
	delete authDB;
#endif
}

bool CRtspServer::Create(portNumBits port)
{
	m_port = port;

	/*SC_LOGE("CRtspServer creat port:%d\n", port);*/

	if(m_env != NULL && m_scheduler != NULL)
	{

		SC_LOGE("CRtspServer is already Create yet");
		return false;
	}
	cqueue_init(&m_sms_action_queue);
	m_scheduler = BasicTaskScheduler::createNew();
	m_env = BasicUsageEnvironment::createNew(*m_scheduler);
	m_process_sms = m_env->taskScheduler().createEventTrigger(AsyncProcessSms);
	if(m_process_sms == 0){
		SC_LOGE("CRtspServer user event create faield.");
		return false;
	}

	SC_LOGI("CRtspServer created, live555 user event id [%x]", m_process_sms);
	return true;
}

bool CRtspServer::Destory()
{
	if(!m_Stop)
	{
		Stop();
	}

	if(m_env == NULL && m_scheduler == NULL)
	{
		SC_LOGE("CRtspServer is already Destory yet, call Destory error!");
		return false;
	}

	m_env->reclaim(); m_env = NULL;
    delete m_scheduler; m_scheduler = NULL;

	return true;
}

bool CRtspServer::Start()
{

	if(m_env == NULL || m_scheduler == NULL)
	{
		SC_LOGE("CRtspServer is not Create yet, call Create first!");
		return false;
	}

	if(!m_Stop)
	{
		SC_LOGE("CRtspServer is already start, call Start error!");
		return false;
	}

	m_watchVariable = 0;
	if(pthread_create(&m_pThread,NULL, ThreadRtspServerProcImpl, this))
	{
		SC_LOGE("ThreadRtspServerProcImpl false!\n");
		return false;
	}

	// 等待线程执行完成，代表rtsp server已经启动完成
	while(m_Stop) usleep(5);

	return true;
}

bool CRtspServer::Stop()
{
	if(m_Stop)
	{
		SC_LOGE("CRtspServer is stop, call stop error!");
		return false;
	}

	m_watchVariable = 1;
	::pthread_join(m_pThread, 0);
	m_pThread = 0;
	Medium::close(m_rtspServer); m_rtspServer = NULL;
	m_Stop = true;

	return true;
}

bool CRtspServer::Restart()
{
	bool ret = false;
	ret = Stop();
	if(!ret)
	{
		return ret;
	}
	ret = Start();

	return ret;
}

bool CRtspServer::DynamicAddSms(const char* streamName,
	bool audioEnable, int audioType, int audioSampleRate, int audioBitPerSample,
	int audioChannels, bool videoEnable, int videoType, int videoFrameRate,
	char *shmId, char *shmName, int streamBufSize, int frameRate,
	int suggest_buffer_region_size, int suggest_buffer_item_count)
{
	SC_LOGI("Add sms <%s>.", streamName);
	bool is_ok = DynamicProcessSmsCommonProcess(1, streamName,
		audioEnable, audioType, audioSampleRate, audioBitPerSample,
		audioChannels, videoEnable, videoType, videoFrameRate,
		shmId, shmName, streamBufSize, frameRate,
		suggest_buffer_region_size, suggest_buffer_item_count);
	if(!is_ok){
		SC_LOGE("Add sms <%s> failed.", streamName);
		return false;
	}
	SC_LOGI("Add sms <%s> sucess.", streamName);
	return true;
}

bool CRtspServer::DynamicDelSms(const char* streamName)
{
	SC_LOGI("Del sms <%s>.", streamName);
	ServerMediaSession* sms = m_rtspServer->lookupServerMediaSession(streamName);
	Boolean const smsExists = (sms != NULL);

	if (smsExists) {
		bool is_ok = DynamicProcessSmsCommonProcess(0, streamName,
			false, 0, 0, 0,
			0, false, 0, 0,
			NULL, NULL, 0, 0,
			0, 0);
		if(!is_ok){
			SC_LOGE("Del sms <%s> failed.", streamName);
			return false;
		}
	}else{
		SC_LOGW("Del sms <%s> failed, not found.", streamName);
		return false;
	}

	SC_LOGI("Del sms <%s> sucess.", streamName);
	return true;
}

void CRtspServer::DynamicAddSmsInternal(CRtspServer *rtsp_server, struct SmsParam *sms_param){

	Boolean reuseFirstSource = False;
	OutPacketBuffer::maxSize = 4*1024*1024; // 此处的配置客户根据码流的分辨率和bitrate调整，避免内存浪费

	ServerMediaSession* sms = NULL;
	if(sms_param->videoEnable && sms_param->videoType == RTSPSRV_VIDEO_TYPE_H264)
	{
		sms = ServerMediaSession::createNew(*(rtsp_server->m_env),
		sms_param->streamName, sms_param->streamName, "H.264 video elementary stream", True);

		sms->addSubsession(H264VideoLiveServerMediaSubsession::createNew(*(rtsp_server->m_env),
		 reuseFirstSource, sms_param->shmId, sms_param->shmName,
		 sms_param->streamBufSize, sms_param->frameRate,
		 sms_param->suggest_buffer_region_size, sms_param->suggest_buffer_item_count));
	}else if(sms_param->videoEnable && sms_param->videoType == RTSPSRV_VIDEO_TYPE_H265){
		sms = ServerMediaSession::createNew(*(rtsp_server->m_env),
		 sms_param->streamName, sms_param->streamName, "H.265 video elementary stream", True);
		sms->addSubsession(H265VideoLiveServerMediaSubsession::createNew(*(rtsp_server->m_env),
		 reuseFirstSource, sms_param->shmId, sms_param->shmName,
		 sms_param->streamBufSize, sms_param->frameRate,
		 sms_param->suggest_buffer_region_size, sms_param->suggest_buffer_item_count));
	}else{
		SC_LOGE("Stream <%s> recv unsupport video type :%d.", sms_param->streamName, sms_param->videoType);
		return;
	}

	if(sms_param->audioEnable)
	{
		int index = GetSamplingFrequencyIndex(sms_param->audioSampleRate);
		if(sms_param->audioType == RTSPSRV_AUDIO_TYPE_LPCM)
		{
			sms->addSubsession(LPCMAudioLiveServerMediaSubsession::createNew(*(rtsp_server->m_env),
			 reuseFirstSource, sms_param->audioBitPerSample, index, sms_param->audioChannels));
		}
		else if(sms_param->audioType == RTSPSRV_AUDIO_TYPE_PCMA)
		{
			sms->addSubsession(PCMAAudioLiveServerMediaSubsession::createNew(*(rtsp_server->m_env),
			 reuseFirstSource, sms_param->audioBitPerSample, index, sms_param->audioChannels));
		}else{
			SC_LOGE("Stream <%s> recv unsupport audio type :%d.", sms_param->streamName, sms_param->audioType);
			return;
		}
	}

	rtsp_server->m_rtspServer->addServerMediaSession(sms);

	char* url = rtsp_server->m_rtspServer->rtspURL(sms);
	SC_LOGI("Play <%s> stream using the URL %s", sms_param->streamName, url);
	delete[] url;

	return;

}
void CRtspServer::DynamicDelSmsInternal(CRtspServer *rtsp_server, struct SmsParam *sms_param){
	SC_LOGI("start delete sms:[%s].", sms_param->streamName);
	ServerMediaSession* sms = rtsp_server->m_rtspServer->lookupServerMediaSession(sms_param->streamName);
	if(sms != NULL){
		rtsp_server->m_rtspServer->deleteServerMediaSession(sms);
	}else{
		SC_LOGE("delete sms:[%s] faild: not found.", sms_param->streamName);
	}
}

void CRtspServer::AsyncProcessSms(void *param){
	CRtspServer *rtsp_server = (CRtspServer *)param;
	if(rtsp_server == NULL){
		SC_LOGE("rtsp server is null.");
		return;
	}

	cqueue* c_queue = &rtsp_server->m_sms_action_queue;
	SC_LOGI("AsyncProcessSms start.");
	while(1){
		//触发一次 会把之前得剩余操作都完成
		if(cqueue_is_empty(c_queue)){
			break;
		}
		void *queue_node = cqueue_dequeue(c_queue);
		if(queue_node == NULL){
			SC_LOGE("smsparam is null.");
			continue;
		}

		SmsParam *sms_param = (SmsParam *)queue_node;
		if(sms_param->actionType == 0){
			DynamicDelSmsInternal(rtsp_server, sms_param);
		}else if(sms_param->actionType == 1){
			DynamicAddSmsInternal(rtsp_server, sms_param);
		}else{
			SC_LOGE("unsupport action type %d, so ignore it .", sms_param->actionType);
		}

		//必须释放
		free(queue_node);
	}
	SC_LOGI("AsyncProcessSms end.");
}

bool CRtspServer::DynamicProcessSmsCommonProcess(int actionType, const char*streamName,
	bool audioEnable, int audioType, int audioSampleRate, int audioBitPerSample,
	int audioChannels, bool videoEnable, int videoType, int videoFrameRate,
	char *shmId, char *shmName, int streamBufSize, int frameRate,
	int suggest_buffer_region_size, int suggest_buffer_item_count){

	//异步得方式：删除sms
	SmsParam *sms_param = (SmsParam *)malloc(sizeof(SmsParam));
	if(sms_param == NULL){
		SC_LOGE("Stop <%s> stream failed, malloc failed, so ignore it.");
		return false;
	}

	int is_ok = 0;
	if(streamName != NULL){
		is_ok |= StringCopyWithCheck(sms_param->streamName, streamName, sizeof(sms_param->streamName));
	}
	if(shmId != NULL){
		is_ok |= StringCopyWithCheck(sms_param->shmId, shmId, sizeof(sms_param->shmId));
	}
	if(shmName != NULL){
		is_ok |= StringCopyWithCheck(sms_param->shmName, shmName, sizeof(sms_param->shmName));
	}
	if(is_ok != 0){
		free(sms_param);
		SC_LOGE("string copy is failed:array is too short");
		return false;
	}
	sms_param->actionType = actionType;
	sms_param->audioEnable = audioEnable;
	sms_param->audioType = audioType;
	sms_param->audioSampleRate = audioSampleRate;
	sms_param->audioBitPerSample = audioBitPerSample;
	sms_param->audioChannels = audioChannels;
	sms_param->videoEnable = videoEnable;
	sms_param->videoType = videoType;
	sms_param->videoFrameRate = videoFrameRate;
	sms_param->streamBufSize = streamBufSize;
	sms_param->frameRate = frameRate;
	sms_param->suggest_buffer_item_count = suggest_buffer_item_count;
	sms_param->suggest_buffer_region_size = suggest_buffer_region_size;

	int ret = cqueue_enqueue(&m_sms_action_queue, sms_param);
	if(ret != 0){
		free(sms_param);
		SC_LOGE("enqueue faild: %d.", ret);
		return false;
	}
	m_env->taskScheduler().triggerEvent(m_process_sms, this);

	//check for debug
	bool is_completed = CheckAsyncProcessSmsIsComplete(&m_sms_action_queue);
	if(!is_completed){
		SC_LOGW("rtsp server sms async process del sms not completed.");
	}
	return true;
}