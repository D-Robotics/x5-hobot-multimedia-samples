// PullerCmd.js
 var LiveStreamPullerRecv = {
    CreateReq: 1,
    StartReq: 2,
    StopReq: 3,
    DestroyReq: 4,
};

 var LiveStreamPullerSent = {
	//function return
	CreateRsp: 100,
    StartRsp: 101,
	StopRsp: 102,
    DestroyRsp: 103,

	//data and error
	FrameInfo:196,
	FrameData: 104,
    Error: 105,
};
