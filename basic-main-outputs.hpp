#pragma once
#include "BroardcastBase.hpp"

enum StartPublishushResultType
{
	StartPublish_Success,			//�����ɹ�
	StartPublish_No_Heart_Live,		//δ��������
	StartPublish_Server_Error,		//����������
};

//ֹͣ�����������
enum StopPublishResultType
{
	StopPublish_SUCCESS = 0,				//ֹͣ�����ɹ�
	StopPublish_BAD_PATH = -1,				//�����·��
	StopPublish_CONNECT_FAILED = -2,		//���ӷ�����ʧ��
	StopPublish_INVALID_STREAM = -3,		//��Ч��ý����
	StopPublish_ERROR = -4,					//����
	StopPublish_DISCONNECTED = -5,			//�����ѶϿ�
	StopPublish_Heart_Live_Timeout = -6		//������ʱ
};

#define SIMPLE_ENCODER_X264                    "x264"
#define SIMPLE_ENCODER_X264_LOWCPU             "x264_lowcpu"
#define SIMPLE_ENCODER_QSV                     "qsv"
#define SIMPLE_ENCODER_NVENC                   "nvenc"

class OutputObserver{
public:
	virtual void OnStartPublish(StartPublishushResultType result) = 0;
	//���ܣ�ֹͣ����������ء�������ֹ�������
	//		stopPublish�ɴ����˺���������Ψһ�������������������������жϣ�
	//		Ҳ��ͨ���˺�������
	//��������������
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
