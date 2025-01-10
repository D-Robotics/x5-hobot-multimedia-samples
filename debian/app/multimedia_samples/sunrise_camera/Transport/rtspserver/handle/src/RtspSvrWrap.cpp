// Copyright (c) 2024，D-Robotics.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifdef __cplusplus
extern "C"{
#endif
#include "RtspSvrWrap.hh"
#ifdef __cplusplus
}
#endif

#include "RtspSvr.hh"

static  CRtspServer* instance = NULL;
void* rtspsvr_wrap_instance()
{
	if(instance == NULL)
	{
		instance = CRtspServer::GetInstance();
	}
	return (void*)instance;
}

int rtspsvr_wrap_prepare(void* instance, unsigned short port)
{
	bool result = ((CRtspServer*)instance)->Create(port);
	if(result)
		return 0;
	else
		return -1;
}

int rtspsvr_wrap_reclaim(void* instance)
{
	bool result = ((CRtspServer*)instance)->Destory();
	if(result)
		return 0;
	else
		return -1;
}

int rtspsvr_wrap_start(void* instance)
{
	bool result = ((CRtspServer*)instance)->Start();
	if(result)
		return 0;
	else
		return -1;
}

int rtspsvr_wrap_stop(void* instance)
{
	bool result = ((CRtspServer*)instance)->Stop();
	if(result)
		return 0;
	else
		return -1;
}

int rtspsvr_wrap_restart(void* instance)
{
	bool result = ((CRtspServer*)instance)->Restart();
	if(result)
		return 0;
	else
		return -1;
}

int rtspsvr_wrap_add_sms(void* instance, const char* streamName,
	int audioEnable, int audioType, int audioSampleRate, int audioBitPerSample,
	int audioChannels, int videoEnable, int videoType, int videoFrameRate,
	char *shmId, char *shmName, int streamBufSize, int frameRate,
    int suggest_buffer_region_size, int suggest_buffer_item_count)
{
	bool result = ((CRtspServer*)instance)->DynamicAddSms(streamName,
		audioEnable, audioType, audioSampleRate, audioBitPerSample, audioChannels,
		videoEnable, videoType, videoFrameRate, shmId, shmName, streamBufSize, frameRate,
		suggest_buffer_region_size, suggest_buffer_item_count);
	if(result)
		return 0;
	else
		return -1;
}

int rtspsvr_wrap_del_sms(void* instance, const char* streamName)
{
	bool result = ((CRtspServer*)instance)->DynamicDelSms(streamName);
	if(result)
		return 0;
	else
		return -1;
}

