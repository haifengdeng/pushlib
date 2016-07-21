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

//鼠标事件的类型,注：鼠标事件是用来拖拉、缩放源的渲染窗口的大小
enum  CCMouseButton {
	PREVIEW_MOUSE_LEFT,
	PREVIEW_MOUSE_RIGHT,
};

//源在渲染区域的层级移动和窗口变换
enum class CCSourceActiton {
	//移动层级
	ORDER_MOVE_UP,
	ORDER_MOVE_DOWN,
	ORDER_MOVE_TOP,
	ORDER_MOVE_BOTTOM,
	//窗口变换
	RESET_TRANSFORM,
	ROTATE_90,
	ROTATE_180,
	ROTATE_270,
	FLIP_HORIZONTAL,
	FLIP_VERTICAL,
	FIT_TO_SCREEN,
	STRETCH_TO_SCREEN,
	CENTER_TO_SCREEN,
};

// 鼠标事件+键盘辅助键，Ctrl,Shift键+鼠标点击
enum CCKey {
	KEY_NONE = 0x00,
	KEY_SHIFT = 0x02,
	KEY_CONTROL = 0x04,
	KEY_ALT = 0x08,
};

struct CCMouseEvent {
	CCMouseButton button;
	CCKey         modifiers;
	//
	int           x;
	int           y;
};

PUSH_ENGINE_EXPORT int BroardcastEngine_Init();

PUSH_ENGINE_EXPORT int stopStreaming();
PUSH_ENGINE_EXPORT int startStreaming(std::string server_, std::string key_);
PUSH_ENGINE_EXPORT int startRecording();

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
PUSH_ENGINE_EXPORT std::string enumFPSs(const char* srcName, size_t idx);
PUSH_ENGINE_EXPORT int setVideoFPS(const char* srcName,const char *strfps);
PUSH_ENGINE_EXPORT std::string getVideoFPS(const char* srcName);
//枚举、设置视频设备支持的Video formats
PUSH_ENGINE_EXPORT std::string enumVideoFormats(const char* srcName, size_t idx);
PUSH_ENGINE_EXPORT int setVideoFormat(const char* srcName, const char* format);
PUSH_ENGINE_EXPORT std::string getVideoFormat(const char* srcName);
//枚举、设置视频设备支持的Color Space
PUSH_ENGINE_EXPORT std::string enumColorSpaces(const char* srcName, size_t idx);
PUSH_ENGINE_EXPORT int setVideoColorSpace(const char* srcName, const char *colorspace);
PUSH_ENGINE_EXPORT std::string getVideoColorSpace(const char* srcName);
//枚举、设置视频设备支持的Color Range
PUSH_ENGINE_EXPORT std::string enumColorRanges(const char* srcName, size_t idx);
PUSH_ENGINE_EXPORT int setVideoColorRange(const char* srcName, const char *colorrange);
PUSH_ENGINE_EXPORT std::string getVideoColorRange(const char* srcName);

//枚举、设置音频输入设备
PUSH_ENGINE_EXPORT std::string enumAudioInDevices(const char* srcName, size_t idx);
PUSH_ENGINE_EXPORT int setAudioInputDevice(const char* srcName, const char *deviceName);
PUSH_ENGINE_EXPORT std::string getAudioInputDevice(const char* srcName);

//枚举、设置音频输出设备
PUSH_ENGINE_EXPORT std::string enumAudioOutDevices(const char* srcName, size_t idx);
PUSH_ENGINE_EXPORT int setAudioOutputDevice(const char* srcName, const char *deviceName);
PUSH_ENGINE_EXPORT std::string getAudioOutputDevice(const char* srcName);


//获取，设置音频输入,输出设备的音量
//PUSH_ENGINE_EXPORT bool hasAudioProperty(const char* srcName);
//PUSH_ENGINE_EXPORT int getAudioVolume(const char* srcName);
//PUSH_ENGINE_EXPORT int setAudioVolume(const char* srcName, int vol);

//设置源在渲染窗口中的鼠标事件，改变源的渲染窗口的大小
PUSH_ENGINE_EXPORT void mousePressEvent(CCMouseEvent *event);
PUSH_ENGINE_EXPORT void mouseReleaseEvent(CCMouseEvent *event);
PUSH_ENGINE_EXPORT void mouseMoveEvent(CCMouseEvent *event);