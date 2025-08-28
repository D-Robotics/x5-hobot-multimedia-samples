 const DecoderRecv = {
	kInitDecoderReq : 0,      // 初始化解码器
	kUninitDecoderReq : 1,    // 反初始化解码器
	kOpenDecoderReq : 2,      // 打开解码器
	kCloseDecoderReq : 3,     // 关闭解码器
	kFeedDataReq : 4,         // 输入数据
	kStartDecodingReq : 5,    // 开始解码
	kPauseDecodingReq : 6,    // 暂停解码
	kSeekToReq : 7            // 跳转到指定位置
  };

   const DecoderSend = {
	kInitDecoderRsp : 0,       // 初始化解码器响应
	kUninitDecoderRsp : 1,     // 反初始化解码器响应
	kOpenDecoderRsp : 2,       // 打开解码器响应
	kCloseDecoderRsp : 3,      // 关闭解码器响应
	kVideoFrame : 4,           // 视频帧数据
	kAudioFrame : 5,           // 音频帧数据
	kStartDecodingRsp : 6,     // 开始解码响应
	kPauseDecodingRsp : 7,     // 暂停解码响应
	kDecodeFinished : 8,       // 解码完成事件
	kRequestData : 9,          // 需要更多数据事件
	kSeekToRsp : 10            // 跳转响应
  }
