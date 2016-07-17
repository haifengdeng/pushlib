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

PUSH_ENGINE_EXPORT void setRenderWindow(void* Window);
PUSH_ENGINE_EXPORT int addNewSource(const char* srcName, InputSourceType type);

PUSH_ENGINE_EXPORT std::string getLogo();
PUSH_ENGINE_EXPORT int setLogo(const char *imageFilePath);
PUSH_ENGINE_EXPORT int setLogoGeometry(int x, int y, int width, int height);
PUSH_ENGINE_EXPORT int getLogoGeometry(int& x, int& y, int& width, int& height);
PUSH_ENGINE_EXPORT int removeLogo();

//枚举、设置视频设备
PUSH_ENGINE_EXPORT std::string enumVideoDevices(const char *srcName,size_t idx);
PUSH_ENGINE_EXPORT int setVideoDefault(const char *srcName);
PUSH_ENGINE_EXPORT bool getVideoDefault(const char *srcName);
PUSH_ENGINE_EXPORT int setVideoDevice(const char* srcName, const char *deviceName);
PUSH_ENGINE_EXPORT std::string getVideoDevice(const char* srcName);

//枚举、设置视频设备支持的分辨率
PUSH_ENGINE_EXPORT std::string enumResolutions(const char* srcName, size_t idx);
PUSH_ENGINE_EXPORT int setVideoResolution(const char* srcName, const char *resolution);
PUSH_ENGINE_EXPORT std::string getVideoResolution(const char* srcName);

//枚举、设置视频设备支持的帧率
PUSH_ENGINE_EXPORT int enumFPSs(const char* srcName, size_t idx);
PUSH_ENGINE_EXPORT int setVideoFPS(const char* srcName, const char *fps);
PUSH_ENGINE_EXPORT int getVideoFPS(const char* srcName);
//枚举、设置视频设备支持的Video formats
PUSH_ENGINE_EXPORT int enumVideoFormats(const char* srcName, size_t idx);
PUSH_ENGINE_EXPORT int setVideoFormat(const char* srcName, int format);
PUSH_ENGINE_EXPORT int getVideoFormat(const char* srcName);
//枚举、设置视频设备支持的Color Space
PUSH_ENGINE_EXPORT std::string enumColorSpaces(const char* srcName, size_t idx);
PUSH_ENGINE_EXPORT int setVideoColorSpace(const char* srcName, const char *colorspace);
PUSH_ENGINE_EXPORT std::string getVideoColorSpace(const char* srcName);
//枚举、设置视频设备支持的Color Range
PUSH_ENGINE_EXPORT std::string enumColorRanges(const char* srcName, size_t idx);
PUSH_ENGINE_EXPORT int setVideoColorRange(const char* srcName, const char *colorrange);
PUSH_ENGINE_EXPORT std::string getVideoColorRange(const char* srcName);