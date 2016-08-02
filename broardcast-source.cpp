#include "stdafx.h"
#include <vector>
#include <memory>
#include "util\platform.h"
#include <util/bmem.h>
#include <util/dstr.h>
#include <util/platform.h>
#include <util/profiler.hpp>
#include <obs-config.h>
#include <obs.hpp>
#include "platform.hpp"
#include  <windows.h>
#include "BroardcastBase.hpp"
#include "BroardcastEngine.hpp"
#include "push-engine.h"
#include "source_helper.hpp"
using namespace std;

static std::string logSrcName = "log_source";
struct AddSourceData {
	obs_source_t *source;
	bool visible;
	obs_sceneitem_t  *item;//output
};

static void AddSource(void *_data, obs_scene_t *scene)
{
	AddSourceData *data = (AddSourceData *)_data;
	obs_sceneitem_t *sceneitem;

	sceneitem = obs_scene_add(scene, data->source);
	obs_sceneitem_set_visible(sceneitem, data->visible);
	data->item = sceneitem;
}

obs_source_t * BroardcastBase::GetSource(const char * srcName)
{
	auto it = nameArray.find(srcName);
	if (it != nameArray.end()){
		return it->second;
	}
	return NULL;
}

int  BroardcastBase::addNewSource(const char* srcName, int type,obs_data_t *settings)
{
	obs_source_t  * source = GetSource(srcName);
	if (source){
		blog(LOG_ERROR, "source %s has existed.", srcName);
		return -1;
	}

	std::string id;
	SourceTypeConvertString((InputSourceType)type, id);
	source = obs_source_create(id.c_str(), srcName, settings, NULL);

	if (source) {
		//add source to scene
		AddSourceData data;
		data.source = source;
		data.visible = true;
		data.item = NULL;
		obs_scene_atomic_update(scene, AddSource, &data);

		//add source to map
		struct SourceInfo info;
		info.name = srcName;

#if 0
		info.setting = obs_data_create();
		obs_data_t * settings = obs_source_get_settings(source);
		obs_data_apply(info.setting, settings);
		obs_data_release(settings);
#else
		info.setting = obs_source_get_settings(source);
#endif

		info.properties = obs_source_properties(source);
		info.source = source;
		info.type = (int) type;
		info.item = data.item;
		sourceArray[source] = info;
		nameArray[srcName] = source;
		return 0;
	}
	return -1;
}

void BroardcastBase::removeOBSSource(const char * srcName)
{
	obs_source_t * source = GetSource(srcName);
	removeOBSSource(source);
	nameArray.erase(srcName);
}
void BroardcastBase::removeOBSSource(obs_source_t *source)
{
	obs_sceneitem_t * item = sourceArray[source].item;
	obs_data_t *settings = sourceArray[source].setting;
	obs_data_release(settings);
	obs_sceneitem_remove(item);
	obs_source_release(source);
	sourceArray.erase(source);
}
void BroardcastBase::removeAllSource()
{
	
	while (sourceArray.begin() != sourceArray.end())
	{
		auto it = sourceArray.begin();
		removeOBSSource(it->first);
	}

	sourceArray.clear();
	nameArray.clear();
}

int BroardcastBase::setVisible(const char *srcName, bool isShow)
{
	obs_source_t * source = GetSource(srcName);

	if (!source){
		blog(LOG_ERROR, "can not find %s source.",srcName);
		return -1;
	}

	obs_sceneitem_set_visible(sourceArray[source].item, isShow);
}

static bool select_one(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	obs_sceneitem_t *selectedItem =
		reinterpret_cast<obs_sceneitem_t*>(param);
	obs_sceneitem_select(item, (selectedItem == item));

	UNUSED_PARAMETER(scene);
	return true;
}


int BroardcastBase::setSelection(const char* srcName, bool select)
{
	obs_source_t * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "can not find %s source.", srcName);
		return -1;
	}

	obs_scene_enum_items(scene, select_one, (obs_sceneitem_t*)sourceArray[source].item);
	return 0;
}

//功能：新建名为srcName，类型为type的源
int addNewSource(const char* srcName, InputSourceType type)
{
	return Engine_main()->addNewSource(srcName, type);
}

//logo
std::string  BroardcastBase::getLogo()
{
	return logoPath;
}

int BroardcastBase::setLogo(const char *imageFilePath)
{
	obs_source_t * source = GetSource(logSrcName.c_str());
	if (!source){
		obs_data_t *settings = obs_data_create();
	    obs_data_set_string(settings, "file", imageFilePath);
	    addNewSource(logSrcName.c_str(), IMAGE_FILE_SOURCE,settings);
		obs_data_release(settings);
	}
	else{
		obs_data_t * settings = sourceArray[source].setting;
        logoPath = imageFilePath;
		obs_source_update(source, settings);
	}
	
	return 0;
}

int BroardcastBase::setLogoGeometry(int x, int y, int width, int height)
{
	obs_source_t * source = GetSource(logSrcName.c_str());
	if (!source){
		blog(LOG_ERROR, "can not find logo.");
		return -1;
	}
	obs_sceneitem_t * item = sourceArray[source].item;
	logo_geometry.x = x;
	logo_geometry.y = y;
	logo_geometry.width = width;
	logo_geometry.height = height;
	vec2 pos = { x, y };
	obs_sceneitem_set_pos(item, &pos);
	vec2 size = { width, height };

	obs_bounds_type boundsType = obs_sceneitem_get_bounds_type(item);
	if (boundsType != OBS_BOUNDS_NONE) {
		obs_sceneitem_set_bounds(item, &size);
	}
	else {
		int base_width = obs_source_get_width(source);
		int base_height = obs_source_get_height(source);
		size.x = width / float(base_width);
		size.y = height / float(base_height);
		obs_sceneitem_set_scale(item, &size);
	}
	return 0;
}
int BroardcastBase::getLogoGeometry(int& x, int& y, int& width, int& height)
{
	x = logo_geometry.x;
	y = logo_geometry.y;
	width = logo_geometry.width;
	height = logo_geometry.height;
	return 0;
}
int BroardcastBase::removeLogo()
{
	removeOBSSource(logSrcName.c_str());
	logoPath.erase();
	return 0;
}


std::string getLogo()
{
	return Engine_main()->getLogo();
}

int setLogo(const char *imageFilePath)
{
	return Engine_main()->setLogo(imageFilePath);
}
int setLogoGeometry(int x, int y, int width, int height)
{
	return Engine_main()->setLogoGeometry(x, y, width, height);
}
int getLogoGeometry(int& x, int& y, int& width, int& height)
{
	return Engine_main()->getLogoGeometry(x, y, width, height);
}
int removeLogo()
{
	return Engine_main()->removeLogo();
}

#define VIDEO_DEVICE_ID "video_device_id"
#define RES_TYPE          "res_type"
#define RESOLUTION        "resolution"
#define FRAME_INTERVAL    "frame_interval"
#define VIDEO_FORMAT      "video_format"
#define LAST_VIDEO_DEV_ID "last_video_device_id"
#define LAST_RESOLUTION   "last_resolution"
#define BUFFERING_VAL     "buffering"
#define FLIP_IMAGE        "flip_vertically"
#define AUDIO_OUTPUT_MODE "audio_output_mode"
#define USE_CUSTOM_AUDIO  "use_custom_audio_device"
#define AUDIO_DEVICE_ID   "audio_device_id"
#define COLOR_SPACE       "color_space"
#define COLOR_RANGE       "color_range"
#define DEACTIVATE_WNS    "deactivate_when_not_showing"

enum ResType {
	ResType_Preferred,
	ResType_Custom
};

std::string BroardcastBase::enumVideoDevices(const char * srcName, size_t idx)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return "";
	}

	obs_properties_t * properties = sourceArray[source].properties;
	obs_property_t *property = obs_properties_get(properties, VIDEO_DEVICE_ID);
	if (property) {
		size_t   count = obs_property_list_item_count(property);
		if (idx >= count)
			return "";
		else
			return obs_property_list_item_name(property, idx);
	}
	return "";
}

int BroardcastBase::setVideoDefault(const char *srcName)
{
	std::string device_id = enumVideoDevices(srcName, 0);
	return setVideoDevice(srcName, device_id.c_str());
}

bool BroardcastBase::getVideoDefault(const char *srcName)
{
	std::string device_id = enumVideoDevices(srcName, 0);
	if (device_id.empty())
		return false;
	else
		return true;
}

int BroardcastBase::setVideoDevice(const char* srcName, const char *deviceName)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return -1;
	}

	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;

	obs_property_t *property = obs_properties_get(properties, VIDEO_DEVICE_ID);
	if (property) {
		size_t           count = obs_property_list_item_count(property);
		for (int i = 0; i < count; i++){
			string devname = obs_property_list_item_name(property, i);
			if (strcmp(devname.c_str(), deviceName) == 0){
				string devId = obs_property_list_item_string(property, i);
				obs_data_set_string(settings, VIDEO_DEVICE_ID, devId.c_str());
				obs_property_modified(property, settings);
				obs_source_update(source, settings);
				return 0;
			}
		}
		return -1;
	}
	return -1;
}

std::string BroardcastBase::getVideoDevice(const char* srcName)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return "";
	}
	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t * settings = sourceArray[source].setting;
	string device_id = obs_data_get_string(settings, VIDEO_DEVICE_ID);

	obs_property_t *property = obs_properties_get(properties, VIDEO_DEVICE_ID);
	if (property) {
		size_t           count = obs_property_list_item_count(property);
		for (int i = 0; i < count; i++){
			string id = obs_property_list_item_string(property, i);
			if (strcmp(id.c_str(), device_id.c_str()) == 0){
				string devname = obs_property_list_item_name(property, i);
				return devname;
			}
		}
		return "";
	}
	return "";
}

std::string enumVideoDevices(const char *srcName, size_t idx)
{
	return Engine_main()->enumVideoDevices(srcName, idx);
}
int setVideoDefault(const char *srcName)
{
	return Engine_main()->setVideoDefault(srcName);
}
bool getVideoDefault(const char *srcName)
{
	return Engine_main()->getVideoDefault(srcName);
}
int setVideoDevice(const char* srcName, const char *deviceName)
{
	return Engine_main()->setVideoDevice(srcName, deviceName);
}
std::string getVideoDevice(const char* srcName)
{
	return Engine_main()->getVideoDevice(srcName);
}


void BroardcastBase::setResolutionType2Custom(const char *srcName)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return;
	}

	obs_data_t * settings = sourceArray[source].setting;
	int res_type = obs_data_get_int(settings, RES_TYPE);
	if (res_type != ResType_Custom){
		obs_property_t *property = obs_properties_get(sourceArray[source].properties, RES_TYPE);
		obs_data_set_int(settings, RES_TYPE, ResType_Custom);
		obs_property_modified(property, settings);
		return;
	}
}

//枚举、设置视频设备支持的分辨率
std::string BroardcastBase::enumResolutions(const char* srcName, size_t idx)
{
	setResolutionType2Custom(srcName);
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return "";
	}

	obs_properties_t * properties = sourceArray[source].properties;
	obs_property_t *property =obs_properties_get(properties, RESOLUTION);
	if(property) {
		size_t   count = obs_property_list_item_count(property);
		if (idx >= count)
			return "";
		else
			return obs_property_list_item_name(property, idx);
	}
	return "";
}

int BroardcastBase::setVideoResolution(const char* srcName, const char *resolution)
{
	setResolutionType2Custom(srcName);
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return -1;
	}

	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;
	obs_property_t *property = obs_properties_get(properties, RESOLUTION);

	if (property) {
		obs_data_set_string(settings, RESOLUTION, resolution);
		if (!obs_property_modified(property, settings)){
			return  -1;
		}
		else
			return 0;
	}
	return  -1;
}
std::string BroardcastBase::getVideoResolution(const char* srcName)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return "";
	}

	obs_data_t *settings = sourceArray[source].setting;

	return obs_data_get_string(settings, RESOLUTION);
}


//枚举、设置视频设备支持的帧率
std::string BroardcastBase::enumFPSs(const char* srcName, size_t idx)
{
	setResolutionType2Custom(srcName);

	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return "";
	}

	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;
	obs_property_t *property = obs_properties_get(properties, FRAME_INTERVAL);

	if (property) {
		size_t count = obs_property_list_item_count(property);
		if (idx >= count)
			return "";
		else
			return obs_property_list_item_name(property, idx);
	}
	return "";
}

int BroardcastBase::setVideoFPS(const char* srcName, const char * strfps)
{
	setResolutionType2Custom(srcName);
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not  existed.", srcName);
		return -1;
	}
	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;
	obs_property_t *property = obs_properties_get(properties, FRAME_INTERVAL);

	if (property) {
		size_t count = obs_property_list_item_count(property);
		for (int i = 0; i < count; i++){
			string name = obs_property_list_item_name(property, i);
			if (strcmp(name.c_str(), strfps) == 0){
				int fps = obs_property_list_item_int(property, i);
				obs_data_set_int(settings, FRAME_INTERVAL, fps);
				if (!obs_property_modified(property, settings)){
					return  -1;
				}
				else
					return 0;
			}
		}
	}
	return -1;
}

std::string BroardcastBase::getVideoFPS(const char* srcName)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return  "";
	}

	obs_data_t *settings = sourceArray[source].setting;
	obs_properties_t * properties = sourceArray[source].properties;
	obs_property_t *property = obs_properties_get(properties, FRAME_INTERVAL);

	int fps= obs_data_get_int(settings, FRAME_INTERVAL);
	if (property) {
		size_t count = obs_property_list_item_count(property);
		for (int i = 0; i < count; i++){
			int interval = obs_property_list_item_int(property, i);
			if (interval == fps){
				return obs_property_list_item_name(property, i);
			}
		}
	}
	return "";
}


//枚举、设置视频设备支持的Video formats
std::string BroardcastBase::enumVideoFormats(const char* srcName, size_t idx)
{
	setResolutionType2Custom(srcName);
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return "";
	}

	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;

	obs_property_t *property = obs_properties_get(properties, VIDEO_FORMAT);

	if (property) {
			size_t  count = obs_property_list_item_count(property);
			if (idx >= count)
				return "";
			else
				return obs_property_list_item_name(property, idx);
	}
	return "";
}

int BroardcastBase::setVideoFormat(const char* srcName,const char * format)
{
	setResolutionType2Custom(srcName);
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return -1;
	}
	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;
	obs_property_t *property = obs_properties_get(properties, VIDEO_FORMAT);

	if (property) {
		size_t count = obs_property_list_item_count(property);
		for (int i = 0; i < count; i++){
			string name = obs_property_list_item_name(property, i);
			if (strcmp(name.c_str(), format) == 0){
				int fmt = obs_property_list_item_int(property, i);
				obs_data_set_int(settings, VIDEO_FORMAT, fmt);
				if (!obs_property_modified(property, settings)){
					return  -1;
				}
				else
					return 0;
			}
		}
	}
	return -1;
}


std::string BroardcastBase::getVideoFormat(const char* srcName)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return  "";
	}

	obs_data_t *settings = sourceArray[source].setting;
	obs_properties_t * properties = sourceArray[source].properties;
	obs_property_t *property = obs_properties_get(properties, VIDEO_FORMAT);

	int fmt = obs_data_get_int(settings, VIDEO_FORMAT);
	if (property) {
		size_t count = obs_property_list_item_count(property);
		for (int i = 0; i < count; i++){
			int fmt_i = obs_property_list_item_int(property, i);
			if (fmt_i == fmt){
				return obs_property_list_item_name(property, i);
			}
		}
	}
	return "";
}

//枚举、设置视频设备支持的Color Space
std::string BroardcastBase::enumColorSpaces(const char* srcName, size_t idx)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return "";
	}

	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;

	obs_property_t *property = obs_properties_get(properties, COLOR_SPACE);

	if (property) {
		size_t  count = obs_property_list_item_count(property);
		if (idx >= count)
			return "";
		else
			return obs_property_list_item_name(property, idx);
	}
	return "";
}

int BroardcastBase::setVideoColorSpace(const char* srcName, const char *colorspace)
{

	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return -1;
	}
	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;
	obs_property_t *property = obs_properties_get(properties, COLOR_SPACE);

	if (property) {
		size_t count = obs_property_list_item_count(property);
		for (int i = 0; i < count; i++){
			string name = obs_property_list_item_name(property, i);
			if (strcmp(name.c_str(), colorspace) == 0){
				string _space = obs_property_list_item_string(property, i);
				obs_data_set_string(settings, COLOR_SPACE, _space.c_str());
				obs_property_modified(property, settings);
				obs_source_update(source, settings);
				return 0;
			}
		}
	}
	return -1;
}

std::string BroardcastBase::getVideoColorSpace(const char* srcName)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return  "";
	}

	obs_data_t *settings = sourceArray[source].setting;
	obs_properties_t * properties = sourceArray[source].properties;
	obs_property_t *property = obs_properties_get(properties, COLOR_SPACE);

	string _space = obs_data_get_string(settings, COLOR_SPACE);
	if (property) {
		size_t count = obs_property_list_item_count(property);
		for (int i = 0; i < count; i++){
			string space_i = obs_property_list_item_string(property, i);
			if (strcmp(_space.c_str(),space_i.c_str())==0){
				return obs_property_list_item_name(property, i);
			}
		}
	}
	return "";
}

//枚举、设置视频设备支持的Color Range
std::string BroardcastBase::enumColorRanges(const char* srcName, size_t idx)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return "";
	}

	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;

	obs_property_t *property = obs_properties_get(properties, COLOR_RANGE);

	if (property) {
		size_t  count = obs_property_list_item_count(property);
		if (idx >= count)
			return "";
		else
			return obs_property_list_item_name(property, idx);
	}
	return "";
}
int BroardcastBase::setVideoColorRange(const char* srcName, const char *colorrange)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return -1;
	}
	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;
	obs_property_t *property = obs_properties_get(properties, COLOR_RANGE);

	if (property) {
		size_t count = obs_property_list_item_count(property);
		for (int i = 0; i < count; i++){
			string name = obs_property_list_item_name(property, i);
			if (strcmp(name.c_str(), colorrange) == 0){
				string _range = obs_property_list_item_string(property, i);
				obs_data_set_string(settings, COLOR_RANGE, _range.c_str());
				obs_property_modified(property, settings);
				obs_source_update(source, settings);
				return 0;
			}
		}
	}
	return -1;
}
std::string BroardcastBase::getVideoColorRange(const char* srcName)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return  "";
	}

	obs_data_t *settings = sourceArray[source].setting;
	obs_properties_t * properties = sourceArray[source].properties;
	obs_property_t *property = obs_properties_get(properties, COLOR_RANGE);

	string _range = obs_data_get_string(settings, COLOR_RANGE);
	if (property) {
		size_t count = obs_property_list_item_count(property);
		for (int i = 0; i < count; i++){
			string range_i = obs_property_list_item_string(property, i);
			if (strcmp(_range.c_str(), range_i.c_str()) == 0){
				return obs_property_list_item_name(property, i);
			}
		}
	}
	return "";
}



//枚举、设置视频设备支持的分辨率
std::string enumResolutions(const char* srcName, size_t idx)
{
	return Engine_main()->enumResolutions(srcName, idx);
}
int setVideoResolution(const char* srcName, const char *resolution)
{
	return Engine_main()->setVideoResolution(srcName, resolution);
}
std::string getVideoResolution(const char* srcName)
{
	return Engine_main()->getVideoResolution(srcName);
}

//枚举、设置视频设备支持的帧率
std::string enumFPSs(const char* srcName, size_t idx)
{
	return Engine_main()->enumFPSs(srcName, idx);
}
int setVideoFPS(const char* srcName, const char * strfps)
{
	return Engine_main()->setVideoFPS(srcName, strfps);
}
std::string getVideoFPS(const char* srcName)
{
	return Engine_main()->getVideoFPS(srcName);
}
//枚举、设置视频设备支持的Video formats
std::string enumVideoFormats(const char* srcName, size_t idx)
{
	return Engine_main()->enumVideoFormats(srcName, idx);
}
int setVideoFormat(const char* srcName, const char * format)
{
	return Engine_main()->setVideoFormat(srcName, format);
}
std::string getVideoFormat(const char* srcName)
{
	return Engine_main()->getVideoFormat(srcName);
}
//枚举、设置视频设备支持的Color Space
std::string enumColorSpaces(const char* srcName, size_t idx)
{
	return Engine_main()->enumColorSpaces(srcName, idx);
}
int setVideoColorSpace(const char* srcName, const char *colorspace)
{
	return Engine_main()->setVideoColorSpace(srcName, colorspace);
}
std::string getVideoColorSpace(const char* srcName)
{
	return Engine_main()->getVideoColorSpace(srcName);
}
//枚举、设置视频设备支持的Color Range
std::string enumColorRanges(const char* srcName, size_t idx)
{
	return Engine_main()->enumColorRanges(srcName, idx);
}
int setVideoColorRange(const char* srcName, const char *colorrange)
{
	return Engine_main()->setVideoColorRange(srcName, colorrange);
}
std::string getVideoColorRange(const char* srcName)
{
	return Engine_main()->getVideoColorRange(srcName);
}


#define OPT_DEVICE_ID         "device_id"
#define OPT_USE_DEVICE_TIMING "use_device_timing"

//枚举、设置音频输入设备
std::string BroardcastBase::enumAudioInDevices(const char* srcName, size_t idx)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return "";
	}

	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;

	obs_property_t *property = obs_properties_get(properties, OPT_DEVICE_ID);

	if (property) {
		size_t  count = obs_property_list_item_count(property);
		if (idx >= count)
			return "";
		else
			return obs_property_list_item_name(property, idx);
	}
	return "";
}
int BroardcastBase::setAudioInputDevice(const char* srcName, const char *deviceName)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return -1;
	}
	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;
	obs_property_t *property = obs_properties_get(properties, OPT_DEVICE_ID);

	if (property) {
		size_t count = obs_property_list_item_count(property);
		for (int i = 0; i < count; i++){
			string name = obs_property_list_item_name(property, i);
			if (strcmp(name.c_str(), deviceName) == 0){
				string _device = obs_property_list_item_string(property, i);
				obs_data_set_string(settings, OPT_DEVICE_ID, _device.c_str());
				obs_source_update(source, settings);
				return 0;
			}
		}
	}
	return -1;
}

std::string BroardcastBase::getAudioInputDevice(const char* srcName)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return  "";
	}

	obs_data_t *settings = sourceArray[source].setting;
	obs_properties_t * properties = sourceArray[source].properties;
	obs_property_t *property = obs_properties_get(properties, OPT_DEVICE_ID);

	string _device = obs_data_get_string(settings, OPT_DEVICE_ID);
	if (property) {
		size_t count = obs_property_list_item_count(property);
		for (int i = 0; i < count; i++){
			string device_i = obs_property_list_item_string(property, i);
			if (strcmp(_device.c_str(), device_i.c_str()) == 0){
				return obs_property_list_item_name(property, i);
			}
		}
	}
	return "";
}

//枚举、设置音频输出设备
std::string BroardcastBase::enumAudioOutDevices(const char* srcName, size_t idx)
{
	return enumAudioInDevices(srcName,idx);
}
int BroardcastBase::setAudioOutputDevice(const char* srcName, const char *deviceName)
{
	return setAudioInputDevice(srcName, deviceName);
}
std::string BroardcastBase::getAudioOutputDevice(const char* srcName)
{
	return getAudioInputDevice(srcName);
}


std::string enumAudioInDevices(const char* srcName, size_t idx)
{
	return Engine_main()->enumAudioInDevices(srcName, idx);
}
int setAudioInputDevice(const char* srcName, const char *deviceName)
{
	return Engine_main()->setAudioInputDevice(srcName, deviceName);
}
std::string getAudioInputDevice(const char* srcName)
{
	return Engine_main()->getAudioInputDevice(srcName);
}

std::string enumAudioOutDevices(const char* srcName, size_t idx)
{
	return Engine_main()->enumAudioOutDevices(srcName, idx);
}
int setAudioOutputDevice(const char* srcName, const char *deviceName)
{
	return Engine_main()->setAudioOutputDevice(srcName, deviceName);
}
std::string getAudioOutputDevice(const char* srcName)
{
	return Engine_main()->getAudioOutputDevice(srcName);
}


//获取，设置音频输入,输出设备的音量
int BroardcastBase::getAudioVolume(const char* srcName)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return  -1;
	}

	float val = obs_source_get_volume(source);
	return (int)(val * 100.0f);
}
int BroardcastBase::setAudioVolume(const char* srcName, int vol)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return  -1;
	}
	float val = float(vol) / 100.0f;
	obs_source_set_volume(source, val);
	return 0;
}

int getAudioVolume(const char* srcName)
{
	return Engine_main()->getAudioVolume(srcName);
}
int setAudioVolume(const char* srcName, int vol)
{
	return Engine_main()->setAudioVolume(srcName, vol);
}


enum obs_prop_format {
	OBS_FORMAT_INVALID,
	OBS_FORMAT_INT,
	OBS_FORMAT_FLOAT,
	OBS_FORMAT_STRING
};



std::string BroardcastBase::enumSource(const char *srcName, 
	const char *proper_str,
	size_t idx)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return "";
	}

	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;

	obs_property_t *property = obs_properties_get(properties, proper_str);

	if (property) {
		size_t  count = obs_property_list_item_count(property);
		if (idx >= count)
			return "";
		else
			return obs_property_list_item_name(property, idx);
	}
	return "";
}
int BroardcastBase::setCurrentSource(const char* srcName,
	   const char *proper_str, 
	   int prop_fmt,
	   const char *srcString)
{
	obs_prop_format prop_format = (obs_prop_format)prop_fmt;
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return -1;
	}
	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;
	obs_property_t *property = obs_properties_get(properties, proper_str);

	if (property) {
		size_t count = obs_property_list_item_count(property);
		for (int i = 0; i < count; i++){
			string name = obs_property_list_item_name(property, i);
			if (strcmp(name.c_str(), srcString) == 0){
				if (OBS_FORMAT_INT == prop_format){
					int  _id = obs_property_list_item_int(property, i);
					obs_data_set_int(settings, proper_str, _id);
					obs_property_modified(property, settings);
					obs_source_update(source, settings);
				}
				else if (OBS_FORMAT_STRING == prop_format){
					string _device = obs_property_list_item_string(property, i);
					obs_data_set_string(settings, proper_str, _device.c_str());
					obs_property_modified(property, settings);
					obs_source_update(source, settings);
				}
				else{
					return -1;
				}
				return 0;
			}
		}
	}
	return -1;

}

std::string BroardcastBase::getCurrentSource(const char* srcName, 
	const char *proper_str, 
	int prop_fmt)
{
	obs_prop_format prop_format = (obs_prop_format)prop_fmt;
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return  "";
	}

	obs_data_t *settings = sourceArray[source].setting;
	obs_properties_t * properties = sourceArray[source].properties;
	obs_property_t *property = obs_properties_get(properties, proper_str);

	if (property) {		
		size_t count = obs_property_list_item_count(property);
		for (int i = 0; i < count; i++){
			if (OBS_FORMAT_INT == prop_format){
				int  _id = obs_data_get_int(settings, proper_str);
				int _id_i = obs_property_list_item_int(property, i);
				if (_id == _id_i){
					return obs_property_list_item_name(property, i);
				}
			}
			else if (OBS_FORMAT_STRING == prop_format){
				string _device = obs_data_get_string(settings, proper_str);
				string device_i = obs_property_list_item_string(property, i);
				if (strcmp(_device.c_str(), device_i.c_str()) == 0){
					return obs_property_list_item_name(property, i);
				}
			}
		}
	}
	return "";
}

//枚举、设置桌面屏幕
std::string BroardcastBase::enumMonitors(const char* srcName, size_t idx)
{
	return enumSource(srcName,"monitor", idx);
}
int BroardcastBase::setMonitor(const char* srcName, const char *monitor)
{
	return setCurrentSource(srcName, "monitor", OBS_FORMAT_INT, monitor);
}
std::string BroardcastBase::getMonitor(const char* srcName)
{
	return getCurrentSource(srcName,"monitor", OBS_FORMAT_INT);
}

//枚举、设置应用程序窗口
std::string BroardcastBase::enumAppWindows(const char* srcName, size_t idx)
{
	return enumSource(srcName, "window", idx);
}
int BroardcastBase::setAppWindow(const char* srcName, const char *window)
{
	return setCurrentSource(srcName, "window", OBS_FORMAT_STRING, window);
}
std::string BroardcastBase::getAppWindow(const char* srcName)
{
	return getCurrentSource(srcName, "window", OBS_FORMAT_STRING);
}

#define SETTING_CAPTURE_WINDOW   "window"
//枚举、设置游戏窗口
std::string BroardcastBase::enumGameWindows(const char* srcName, size_t idx)
{
	return enumSource(srcName, SETTING_CAPTURE_WINDOW, idx);
}
int BroardcastBase::setGameWindow(const char* srcName, const char *game)
{
	return setCurrentSource(srcName, SETTING_CAPTURE_WINDOW, OBS_FORMAT_STRING,game);
}
std::string BroardcastBase::getGameWindow(const char* srcName)
{
	return getCurrentSource(srcName, SETTING_CAPTURE_WINDOW, OBS_FORMAT_STRING);
}

//枚举、设置桌面屏幕
std::string enumMonitors(const char* srcName, size_t idx)
{
	return Engine_main()->enumMonitors(srcName, idx);
}
int setMonitor(const char* srcName, const char *monitor)
{
	return Engine_main()->setMonitor(srcName, monitor);
}

std::string getMonitor(const char* srcName)
{
	return Engine_main()->getMonitor(srcName);
}

//枚举、设置应用程序窗口
std::string enumAppWindows(const char* srcName, size_t idx)
{
	return Engine_main()->enumAppWindows(srcName, idx);
}
int setAppWindow(const char* srcName, const char *window)
{
	return Engine_main()->setAppWindow(srcName, window);
}
std::string getAppWindow(const char* srcName)
{
	return Engine_main()->getAppWindow(srcName);
}

//枚举、设置游戏窗口
std::string enumGameWindows(const char* srcName, size_t idx)
{
	return Engine_main()->enumGameWindows(srcName, idx);
}

int setGameWindow(const char* srcName, const char *game)
{
	return Engine_main()->setGameWindow(srcName, game);
}
std::string getGameWindow(const char* srcName)
{
	return Engine_main()->getGameWindow(srcName);
}


enum pa_data_type{
	pa_data_bool,
	pa_data_string
};
int BroardcastBase::setCurrentSetting(const char* srcName, const char *setting_item, int setting_type, const void * data)
{
	pa_data_type type = (pa_data_type)setting_type;
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return -1;
	}
	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;
	obs_property_t *property = obs_properties_get(properties, setting_item);
	if (type == pa_data_bool){
		obs_data_set_bool(settings, setting_item, (bool)data);
	}
	else if (type == pa_data_string){
		obs_data_set_string(settings, setting_item, (const char *)data);
	}
	else{
		return -1;
	}
	obs_property_modified(property, settings);
	obs_source_update(source, settings);
	return 0;
}

std::string BroardcastBase::getCurrentSetting(const char* srcName, const char *setting_item)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return "";
	}
	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;
	obs_property_t *property = obs_properties_get(properties, setting_item);

	return	obs_data_get_string(settings, setting_item);
}
bool BroardcastBase::getCurrentSettingBool(const char* srcName, const char *setting_item)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return "";
	}
	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;
	obs_property_t *property = obs_properties_get(properties, setting_item);

	return	obs_data_get_bool(settings, setting_item);
}

//设置文本源的内容为字符串
int BroardcastBase::setTextFromString(const char* srcName, const char *text)
{
	return setCurrentSetting(srcName, "text", pa_data_string, text);
}
//设置文本源的内容为文件内容(UTF-8/UTF-16)
int BroardcastBase::setTextFromFile(const char* srcName, const char *file)
{
	setCurrentSetting(srcName, "text_file", pa_data_string, (const void *)file);
	setCurrentSetting(srcName, "from_file", pa_data_bool, (const void *)true);
	return 0;
}

std::string BroardcastBase::getTextString(const char* srcName)
{
	return getCurrentSetting(srcName, "text");
}

std::string BroardcastBase::getTextFile(const char* srcName)
{
	return getCurrentSetting(srcName, "text_file");
}

//文本源的字体族，大小，线性渐变颜色
std::string BroardcastBase::getTextFontFace(const char* srcName)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return "";
	}
	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;
	obs_data_t *obj = obs_data_get_obj(settings, "font");
	std::string fontface = obs_data_get_string(obj, "face");
	obs_data_release(obj);
	return fontface;
}
int BroardcastBase::setTextFontFace(const char* srcName, const char *face)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return -1;
	}
	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;
	obs_data_t *obj = obs_data_get_obj(settings, "font");
	obs_data_set_string(obj, "face",face);
	obs_data_release(obj);
	obs_source_update(source, settings);
	return 0;
}
int BroardcastBase::getTextFontSize(const char* srcName)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return -1;
	}
	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;
	obs_data_t *obj = obs_data_get_obj(settings, "font");
	int size = obs_data_get_int(obj, "size");
	obs_data_release(obj);
	return size;
}
int BroardcastBase::setTextFontSize(const char* srcName, int size)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return -1;
	}
	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;
	obs_data_t *obj = obs_data_get_obj(settings, "font");
	obs_data_set_int(obj, "size", size);
	obs_data_release(obj);
	obs_source_update(source, settings);
	return 0;
}
//颜色字符串是argb，如#ffff00ff
int BroardcastBase::setTextFontColor(const char* srcName, unsigned int color1, unsigned int color2)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return -1;
	}
	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;
	obs_data_set_int(settings, "color1", color1);
	obs_data_set_int(settings, "color2", color2);
	obs_source_update(source, settings);
	return 0;
}

unsigned int BroardcastBase::getTextFontColor1(const char* srcName)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return -1;
	}
	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;
	unsigned int color1 = obs_data_get_int(settings, "color1");
	return color1;
}

unsigned int  BroardcastBase::getTextFontColor2(const char* srcName)
{
	obs_source_t  * source = GetSource(srcName);
	if (!source){
		blog(LOG_ERROR, "source %s has not existed.", srcName);
		return -1;
	}
	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;
	unsigned int color2 = obs_data_get_int(settings, "color2");
	return color2;
}


//设置文本源的内容为字符串
int setTextFromString(const char* srcName, const char *text)
{
	return Engine_main()->setTextFromString(srcName, text);
}
//设置文本源的内容为文件内容(UTF-8/UTF-16)
int setTextFromFile(const char* srcName, const char *file)
{
	return Engine_main()->setTextFromFile(srcName, file);
}

std::string getTextString(const char* srcName)
{
	return Engine_main()->getTextString(srcName);
}
std::string getTextFile(const char* srcName)
{
	return Engine_main()->getTextFile(srcName);
}
//文本源的字体族，大小，线性渐变颜色
std::string getTextFontFace(const char* srcName)
{
	return Engine_main()->getTextFontFace(srcName);
}
int setTextFontFace(const char* srcName, const char *face)
{
	return Engine_main()->setTextFontFace(srcName,face);
}
int getTextFontSize(const char* srcName)
{
	return Engine_main()->getTextFontSize(srcName);
}
int setTextFontSize(const char* srcName, int size)
{
	return Engine_main()->setTextFontSize(srcName,size);
}
//颜色字符串是argb，如#ffff00ff
int setTextFontColor(const char* srcName, unsigned int color1, unsigned int color2)
{
	return Engine_main()->setTextFontColor(srcName,color1,color2);
}
unsigned int getTextFontColor1(const char* srcName)
{
	return Engine_main()->getTextFontColor1(srcName);
}
unsigned int getTextFontColor2(const char* srcName)
{
	return Engine_main()->getTextFontColor2(srcName);
}