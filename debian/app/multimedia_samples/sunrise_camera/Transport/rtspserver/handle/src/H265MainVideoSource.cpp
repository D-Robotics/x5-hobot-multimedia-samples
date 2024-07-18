#include "GroupsockHelper.hh"
#include "H265MainVideoSource.hh"
#include "utils/utils_log.h"
#include "utils/stream_define.h"
#include "utils/stream_manager.h"
#include "utils/nalu_utils.h"

#include "communicate/sdk_communicate.h"
#include "communicate/sdk_common_struct.h"
#include "communicate/sdk_common_cmd.h"

H265MainVideoSource* H265MainVideoSource::createNew(UsageEnvironment& env,
	char *shmId, char *shmName, int streamBufSize, int frameRate,
	int buffer_region_size, int buffer_item_count, bool is_dumy,
	unsigned preferredFrameSize,
	unsigned playTimePerFrame)
{
	H265MainVideoSource* source = new H265MainVideoSource(env, shmId, shmName, streamBufSize, frameRate,
		buffer_region_size, buffer_item_count, is_dumy, preferredFrameSize, playTimePerFrame);
	return source;
}
H265MainVideoSource::H265MainVideoSource(UsageEnvironment& env,
	char *shmId, char *shmName, int streamBufSize, int frameRate,
	int buffer_region_size, int buffer_item_count,
	bool is_dumy,
	unsigned preferredFrameSize,
	unsigned playTimePerFrame)
	: FramedSource(env), fPreferredFrameSize(preferredFrameSize), fPlayTimePerFrame(playTimePerFrame), fLastPlayTime(0)
{
	fIsDummy = is_dumy;

	int ret = shm_stream_is_already_create(shmId, shmName, STREAM_MAX_USER);
	if(ret != 0){
		SC_LOGW("shm_id: %s, shm_name: %s is already created.", shmId, shmName);
	}

	fPresentationTime.tv_sec = 0;
	fPresentationTime.tv_usec = 0;
	SC_LOGI("video source created for shm_id: %s, shm_name: %s, is dummy %d", shmId, shmName, is_dumy);

	fShmSource = shm_stream_create(shmId, shmName, STREAM_MAX_USER,
		buffer_item_count, buffer_region_size,
		SHM_STREAM_READ, SHM_STREAM_MALLOC);

	strncpy(fShmName, shmName, sizeof(fShmName) - 1);
	strncpy(fShmId, shmId, sizeof(fShmId) - 1);
	fBufferRegionSize = buffer_region_size;
	fBufferItemCount = buffer_item_count;
	fPts = 0;
	fNaluLen = 0;

	SC_LOGI("video_stream_create => shm_id: %s, shm_name: %s, STREAM_MAX_USER: %d, framerate: %d, stream_buf_size: %d, region size:%d, item count %d",
		 shmId, shmName, STREAM_MAX_USER, frameRate, streamBufSize, buffer_region_size, buffer_item_count);
}

H265MainVideoSource::~H265MainVideoSource()
{
	SC_LOGI("video source deleted shm_id: %s, shm_name: %s, is dummy %d", fShmId, fShmName, fIsDummy);
	if(fShmSource != NULL)
	{
		shm_stream_destory(fShmSource);
		fShmSource = NULL;
	}else{
		SC_LOGW("shm is null.");
	}
}

void H265MainVideoSource::sync()
{
	if(fShmSource != NULL){
		shm_stream_sync(fShmSource);
	}else{
		SC_LOGW("shm is null, but do sync.");
	}
}

void H265MainVideoSource::idr()
{
	T_SDK_FORCE_I_FARME idr;
	idr.channel = 0;
	idr.num = 1;

	SDK_Cmd_Impl(SDK_CMD_VPP_VENC_FORCE_IDR, &idr);
}

void H265MainVideoSource::doGetNextFrame() {
	//do read from memory
	incomingDataHandler(this);
}

void H265MainVideoSource::incomingDataHandler(H265MainVideoSource* source) {
	if (!source->isCurrentlyAwaitingData())
	{
		source->doStopGettingFrames(); // we're not ready for the data yet
		return;
	}
	source->incomingDataHandler1();
}


/*extern int hdr_len;*/
/*extern unsigned char *hdr_data;*/
/*int isFirstFrame = 0;*/
void H265MainVideoSource::incomingDataHandler1()
{
	fFrameSize = 0;
	frame_info info;
	unsigned int length;
	unsigned char* data = NULL;

	time_statistics_at_beginning_of_loop(&fTimeStatistics);
	if (shm_stream_front(fShmSource, &info, &data, &length) == 0)
	{
		NALU_t nalu;
		fFrameSize = length;
		if (fFrameSize > fMaxSize)
		{
			fNumTruncatedBytes = fFrameSize - fMaxSize;
			fFrameSize = fMaxSize;
		}
		else
		{
			fNumTruncatedBytes = 0;
		}
		int ret = get_annexb_nalu(data + fNaluLen, fFrameSize - fNaluLen, &nalu, 1);
		if (ret > 0) fNaluLen += nalu.len + nalu.startcodeprefix_len;    //记录nalu偏移总长

		#if 0
		static FILE *enc_data_file = NULL;
		if(enc_data_file == NULL){
			if(nalu.nal_unit_type == 32){
					char enc_file_name [100];
					sprintf(enc_file_name, "/tmp/front_rtsp_%s.h265", fShmSource->name);

					enc_data_file = fopen(enc_file_name, "wb");
					if(enc_data_file == NULL){
						SC_LOGE("open file %s failed.", (char *)enc_file_name);
					}
			}else{
				printf("ignore nalu type [%d], before idr.\n", nalu.nal_unit_type);
			}
		}
		if(enc_data_file != NULL){
			size_t elementsWritten = fwrite((unsigned char*)data,
				1, length, enc_data_file);
			if (elementsWritten != length) {
				SC_LOGE("write websocker file failed, return %d.", elementsWritten);
			}
		}
		#endif

		//只发送sps pps i p nalu, 其他抛弃
		if ( nalu.nal_unit_type == 1 || nalu.nal_unit_type == 32
			|| nalu.nal_unit_type == 33 || nalu.nal_unit_type == 34 || nalu.nal_unit_type == 19)
		{
			fFrameSize = nalu.len;
			//在使用 nalu 内存前做如下检测
			//码流数据在大压力的情况下，可能会被覆盖，所以此处判断web 发送的数据范围是否合法
			//数据被覆盖后，发送出去也没问题，但是要保证程序不会奔溃
			ret = nalu_is_beyond_source_data_range(nalu.buf, nalu.len, data, length, fShmId);
			if((ret != 0) || (nalu.len > fMaxSize)){
				SC_LOGE("shm_id: %s, shm_name: %s data range is error, so ignore this pkt. nalu len:%d dst max len:%d", fShmId, fShmName, nalu.len, fMaxSize);
				fNaluLen = 0;
				shm_stream_post(fShmSource);
				fDurationInMicroseconds = 1000 * 1;
				nextTask() = envir().taskScheduler().scheduleDelayedTask(fDurationInMicroseconds,
					(TaskFunc*)incomingDataHandler, this);
				return;
			}
			memcpy(fTo, nalu.buf, nalu.len);

			if (fPresentationTime.tv_sec == 0 && fPresentationTime.tv_usec == 0)
			{
				// This is the first frame, so use the current time:
				gettimeofday(&fPresentationTime, NULL);
				fPts = info.pts;
			}
			else if (nalu.nal_unit_type == 1)
			{
				unsigned uSeconds = fPresentationTime.tv_usec + (info.pts  - fPts);
				fPresentationTime.tv_sec += uSeconds / 1000000;
				fPresentationTime.tv_usec = uSeconds % 1000000;
				fPts = info.pts;
				gettimeofday(&fPresentationTime, NULL);
			}

			if (nalu.nal_unit_type == 1 || nalu.nal_unit_type == 19)
			{
				fNaluLen = 0;
				fDurationInMicroseconds = 1000 * 1; //1ms

				int remains = shm_stream_remains(fShmSource);
				if(remains > fBufferItemCount / 3 * 2){
					SC_LOGI("shm_id: %s, shm_name: %s, length:%d fFrameSize:%d remains:%d",
						fShmId, fShmName, length, fFrameSize, remains);
				}
				if(remains >= 3){
					fDurationInMicroseconds = 0;
				}
				//该帧发送完毕，包括sps pps等nalu拆分完毕，可以释放
				shm_stream_post(fShmSource);
			}else{
				//do noting, 拆包
			}

			nextTask() = envir().taskScheduler().scheduleDelayedTask(fDurationInMicroseconds, (TaskFunc*)FramedSource::afterGetting, this);
		}
		else
		{
			SC_LOGI("shm_id: %s, shm_name: %s, other nal_unit_type %d\n", fShmId, fShmName, nalu.nal_unit_type);
			fNaluLen = 0;
			shm_stream_post(fShmSource);
			fDurationInMicroseconds = 1000 * 2;
			nextTask() = envir().taskScheduler().scheduleDelayedTask(fDurationInMicroseconds,
				(TaskFunc*)incomingDataHandler, this);
		}
	}
	else
	{
		nextTask() = envir().taskScheduler().scheduleDelayedTask(10 * 1000,
			(TaskFunc*)incomingDataHandler, this);
	}

	time_statistics_at_ending_of_loop(&fTimeStatistics);
	time_statistics_info_show(&fTimeStatistics, "rtsp-h265", false);
}
