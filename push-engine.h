#pragma once

#ifdef PINGANPUBLISH_EXPORTS
#define PUSH_ENGINE_EXPORT __declspec(dllexport)
#else
#define PUSH_ENGINE_EXPORT
#endif
#include <vector>
#include <string>

enum  InputSourceType{
	TEXT_SOURCE,
	IMAGE_FILE_SOURCE,
	MEDIA_FILE_SOURCE,
	MONITOR_CAPTURE_SOURCE,
	WINDOW_CAPTURE_SOURCE,
	VIDEO_CAPTURE_SOURCE,
	AUDIO_INPUT_SOURCE,
	AUDIO_OUTPUT_SOURCE,
	NONE_SOURCE,
};

PUSH_ENGINE_EXPORT int BroardcastEngine_Init();

PUSH_ENGINE_EXPORT int BroardcastAddDefaultAVSoruce();
PUSH_ENGINE_EXPORT int stopStreaming();
PUSH_ENGINE_EXPORT int startStreaming();

PUSH_ENGINE_EXPORT int GetDeviceVectorFromSourceType(int type);
PUSH_ENGINE_EXPORT void setRenderWindow(void* Window);
PUSH_ENGINE_EXPORT int addNewSource(const char* srcName, InputSourceType type);