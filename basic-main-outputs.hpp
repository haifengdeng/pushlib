#pragma once
#include "BroardcastBase.hpp"

enum StartPublishushResultType
{
	StartPublish_Success,			//推流成功
	StartPublish_No_Heart_Live,		//未建立心跳
	StartPublish_Server_Error,		//服务器错误
};

//停止推流结果类型
enum StopPublishResultType
{
	StopPublish_SUCCESS = 0,				//停止推流成功
	StopPublish_BAD_PATH = -1,				//错误的路径
	StopPublish_CONNECT_FAILED = -2,		//连接服务器失败
	StopPublish_INVALID_STREAM = -3,		//无效的媒体流
	StopPublish_ERROR = -4,					//错误
	StopPublish_DISCONNECTED = -5,			//连接已断开
	StopPublish_Heart_Live_Timeout = -6		//心跳超时
};

#define SIMPLE_ENCODER_X264                    "x264"
#define SIMPLE_ENCODER_X264_LOWCPU             "x264_lowcpu"
#define SIMPLE_ENCODER_QSV                     "qsv"
#define SIMPLE_ENCODER_NVENC                   "nvenc"

class OutputObserver{
public:
	virtual void OnStartPublish(StartPublishushResultType result) = 0;
	//功能：停止推流结果返回、推流中止结果返回
	//		stopPublish可触发此函数，但不唯一，如推流过程中因其它因素中断，
	//		也会通过此函数返回
	//参数：返回类型
	virtual void OnStopPublish(StopPublishResultType reault) = 0;
};

struct BasicOutputHandler {
	OBSOutput              fileOutput;
	OBSOutput              streamOutput;
	bool                   streamingActive = false;
	bool                   recordingActive = false;
	bool                   delayActive = false;
	config_t *             config;
	BroardcastBase*        main;

	OBSSignal              startRecording;
	OBSSignal              stopRecording;
	OBSSignal              startStreaming;
	OBSSignal              stopStreaming;
	OBSSignal              streamDelayStarting;
	OBSSignal              streamStopping;
	OBSSignal              recordStopping;

	inline BasicOutputHandler(BroardcastBase* main_) : main(main_){
		config = main->Config();
	}

	virtual ~BasicOutputHandler() {};

	virtual bool StartStreaming(obs_service_t *service) = 0;
	virtual bool StartRecording() = 0;
	virtual void StopStreaming() = 0;
	virtual void ForceStopStreaming() = 0;
	virtual void StopRecording() = 0;
	virtual bool StreamingActive() const = 0;
	virtual bool RecordingActive() const = 0;

	virtual void Update() = 0;

	inline bool Active() const
	{
		return streamingActive || recordingActive || delayActive;
	}
};

BasicOutputHandler *CreateSimpleOutputHandler(BroardcastBase *main_);
BasicOutputHandler *CreateAdvancedOutputHandler(BroardcastBase *main_);
