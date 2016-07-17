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

int  BroardcastBase::addNewSource(const char* srcName, int type)
{
	obs_source_t  * source = obs_get_source_by_name(srcName);
	if (source){
		blog(LOG_ERROR, "source %s has existed.", srcName);
		obs_source_release(source);
		return -1;
	}

	std::string id;
	SourceTypeConvertString((InputSourceType)type, id);
	source = obs_source_create(id.c_str(), srcName, NULL, NULL);

	if (source) {
		//add source to scene
		AddSourceData data;
		data.source = source;
		data.visible = true;
		data.item = NULL;
		obs_scene_atomic_update(scene, AddSource, &data);
		obs_source_release(source);

		//add source to map
		struct SourceInfo info;
		info.name = srcName;
		info.setting = obs_source_get_settings(source);
		info.properties = obs_source_properties(source);
		info.source = source;
		info.type = (int) type;
		info.item = data.item;
		sourceArray[source] = info;
		return 0;
	}
	return -1;
}

int BroardcastBase::setVisible(const char *srcName, bool isShow)
{
	obs_source_t * source = obs_get_source_by_name(srcName);

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
	obs_source_t * source = obs_get_source_by_name(srcName);

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
	string name = "logo_source";
	addNewSource(name.c_str(), IMAGE_FILE_SOURCE);
	obs_source_t * source = obs_get_source_by_name(name.c_str());

	if (!source){
		blog(LOG_ERROR, "can not create logo source.");
		return -1;
	}
	logoPath = imageFilePath;
	obs_data_t * settings = sourceArray[source].setting;
	obs_data_set_string(settings, "file", imageFilePath);
	obs_source_update(source, settings);
}

int BroardcastBase::setLogoGeometry(int x, int y, int width, int height)
{
	obs_source_t * source = obs_get_source_by_name(logoPath.c_str());
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
	obs_sceneitem_set_bounds(item, &size);
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
	obs_source_t * source = obs_get_source_by_name(logoPath.c_str());
	if (!source){
		blog(LOG_ERROR, "can not find logo.");
		return -1;
	}
	obs_sceneitem_t * item = sourceArray[source].item;
	obs_sceneitem_remove(item);
	obs_source_remove(source);
	sourceArray.erase(source);
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
	return Engine_main()->getLogoGeometry(x, y, width, height);
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
	obs_source_t  * source = obs_get_source_by_name(srcName);
	if (source){
		blog(LOG_ERROR, "source %s has existed.", srcName);
		obs_source_release(source);
		return "";
	}
	obs_properties_t * properties = sourceArray[source].properties;
	obs_property_t *property = obs_properties_first(properties);
	bool hasNoProperties = !property;

	while (property) {
		const char        *name = obs_property_name(property);
		obs_property_type type = obs_property_get_type(property);

		if (!obs_property_visible(property))
			goto End;

		if (strcmp(VIDEO_DEVICE_ID, name) == 0 && type == OBS_PROPERTY_LIST){
			const char       *name = obs_property_name(property);
			obs_combo_type   type = obs_property_list_type(property);
			obs_combo_format format = obs_property_list_format(property);
			size_t           count = obs_property_list_item_count(property);
			if (idx > count)
				return "";
			else
				return obs_property_list_item_name(property, idx);
		}
	End:
		obs_property_next(&property);
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
	obs_source_t  * source = obs_get_source_by_name(srcName);
	if (source){
		blog(LOG_ERROR, "source %s has existed.", srcName);
		obs_source_release(source);
		return -1;
	}
	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;

	obs_property_t *property = obs_properties_first(properties);
	bool hasNoProperties = !property;

	while (property) {
		const char        *name = obs_property_name(property);
		obs_property_type type = obs_property_get_type(property);

		if (!obs_property_visible(property))
			goto End;

		if (strcmp(VIDEO_DEVICE_ID, name) == 0 && type == OBS_PROPERTY_LIST){
			const char       *name = obs_property_name(property);
			obs_combo_type   type = obs_property_list_type(property);
			obs_combo_format format = obs_property_list_format(property);
			size_t           count = obs_property_list_item_count(property);
			obs_data_set_string(settings,VIDEO_DEVICE_ID, srcName);
			obs_property_modified(property, settings);
			return 0;
		}
	End:
		obs_property_next(&property);
	}
	return -1;
}

std::string BroardcastBase::getVideoDevice(const char* srcName)
{
	obs_source_t  * source = obs_get_source_by_name(srcName);
	if (source){
		blog(LOG_ERROR, "source %s has existed.", srcName);
		obs_source_release(source);
		return "";
	}
	obs_data_t * settings = sourceArray[source].setting;
	return obs_data_get_string(settings, VIDEO_DEVICE_ID);
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
	obs_source_t  * source = obs_get_source_by_name(srcName);
	if (source){
		blog(LOG_ERROR, "source %s has existed.", srcName);
		obs_source_release(source);
		return;
	}

	obs_data_t * settings = sourceArray[source].setting;
	int res_type = obs_data_get_int(settings, RES_TYPE);
	if (res_type != ResType_Custom){
		obs_property_t *property = obs_properties_first(sourceArray[source].properties);
		bool hasNoProperties = !property;

		while (property) {
			const char        *name = obs_property_name(property);
			obs_property_type type = obs_property_get_type(property);

			if (!obs_property_visible(property))
				goto End;

			if (strcmp(RES_TYPE, name) == 0 && type == OBS_PROPERTY_LIST){
				const char       *name = obs_property_name(property);
				obs_combo_type   type = obs_property_list_type(property);
				obs_combo_format format = obs_property_list_format(property);
				size_t           count = obs_property_list_item_count(property);
				obs_data_set_int(settings, RES_TYPE, ResType_Custom);
				obs_property_modified(property, settings);
				return;
			}
		End:
			obs_property_next(&property);
		}
	}
}

//枚举、设置视频设备支持的分辨率
std::string BroardcastBase::enumResolutions(const char* srcName, size_t idx)
{
	setResolutionType2Custom(srcName);
	obs_source_t  * source = obs_get_source_by_name(srcName);
	if (source){
		blog(LOG_ERROR, "source %s has existed.", srcName);
		obs_source_release(source);
		return "";
	}

	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;

	obs_property_t *property = obs_properties_first(properties);
	bool hasNoProperties = !property;

	while (property) {
		const char        *name = obs_property_name(property);
		obs_property_type type = obs_property_get_type(property);

		if (!obs_property_visible(property))
			goto End;

		if (strcmp(RESOLUTION, name) == 0 && type == OBS_PROPERTY_EDITABLE_LIST){
			obs_combo_format format = obs_property_list_format(property);
			size_t           count = obs_property_list_item_count(property);
			if (idx > count)
				return "";
			else
				return obs_property_list_item_name(property, idx);
		}
	End:
		obs_property_next(&property);
	}
	return "";
}
int BroardcastBase::setVideoResolution(const char* srcName, const char *resolution)
{
	setResolutionType2Custom(srcName);
	obs_source_t  * source = obs_get_source_by_name(srcName);
	if (source){
		blog(LOG_ERROR, "source %s has existed.", srcName);
		obs_source_release(source);
		return -1;
	}

	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;

	obs_property_t *property = obs_properties_first(properties);
	bool hasNoProperties = !property;

	while (property) {
		const char        *name = obs_property_name(property);
		obs_property_type type = obs_property_get_type(property);

		if (!obs_property_visible(property))
			goto End;

		if (strcmp(RESOLUTION, name) == 0 && type == OBS_PROPERTY_EDITABLE_LIST){
			obs_combo_format format = obs_property_list_format(property);
			size_t           count = obs_property_list_item_count(property);
			obs_data_set_string(settings, RESOLUTION, resolution);
			if (!obs_property_modified(property, settings)){
				return  -1;
			}
			else
				return 0;
		}
	End:
		obs_property_next(&property);
	}
	return  -1;
}
std::string BroardcastBase::getVideoResolution(const char* srcName)
{
	obs_source_t  * source = obs_get_source_by_name(srcName);
	if (source){
		blog(LOG_ERROR, "source %s has existed.", srcName);
		obs_source_release(source);
		return "";
	}

	obs_data_t *settings = sourceArray[source].setting;

	return obs_data_get_string(settings, RESOLUTION);
}

//枚举、设置视频设备支持的帧率
int BroardcastBase::enumFPSs(const char* srcName, size_t idx)
{
	setResolutionType2Custom(srcName);
	obs_source_t  * source = obs_get_source_by_name(srcName);
	if (source){
		blog(LOG_ERROR, "source %s has existed.", srcName);
		obs_source_release(source);
		return -1;
	}

	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;

	obs_property_t *property = obs_properties_first(properties);
	bool hasNoProperties = !property;

	while (property) {
		const char        *name = obs_property_name(property);
		obs_property_type type = obs_property_get_type(property);

		if (!obs_property_visible(property))
			goto End;

		if (strcmp(FRAME_INTERVAL, name) == 0 && type == OBS_COMBO_TYPE_LIST){
			obs_combo_format format = obs_property_list_format(property);
			size_t           count = obs_property_list_item_count(property);
			if (idx > count)
				return -1;
			else
				return obs_property_list_item_int(property, idx);
		}
	End:
		obs_property_next(&property);
	}
	return -1;
}
int BroardcastBase::setVideoFPS(const char* srcName, int fps)
{
	setResolutionType2Custom(srcName);
	obs_source_t  * source = obs_get_source_by_name(srcName);
	if (source){
		blog(LOG_ERROR, "source %s has existed.", srcName);
		obs_source_release(source);
		return -1;
	}

	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;

	obs_property_t *property = obs_properties_first(properties);
	bool hasNoProperties = !property;

	while (property) {
		const char        *name = obs_property_name(property);
		obs_property_type type = obs_property_get_type(property);

		if (!obs_property_visible(property))
			goto End;

		if (strcmp(FRAME_INTERVAL, name) == 0 && type == OBS_COMBO_TYPE_LIST){
			obs_combo_format format = obs_property_list_format(property);
			size_t           count = obs_property_list_item_count(property);
			obs_data_set_int(settings, FRAME_INTERVAL, fps);
			if (!obs_property_modified(property, settings)){
				return  -1;
			}
			else
				return 0;
		}
	End:
		obs_property_next(&property);
	}
	return -1;
}
int BroardcastBase::getVideoFPS(const char* srcName)
{
	obs_source_t  * source = obs_get_source_by_name(srcName);
	if (source){
		blog(LOG_ERROR, "source %s has existed.", srcName);
		obs_source_release(source);
		return -1;
	}

	obs_data_t *settings = sourceArray[source].setting;

	return obs_data_get_int(settings, FRAME_INTERVAL);
}
//枚举、设置视频设备支持的Video formats
int BroardcastBase::enumVideoFormats(const char* srcName, size_t idx)
{
	setResolutionType2Custom(srcName);
	obs_source_t  * source = obs_get_source_by_name(srcName);
	if (source){
		blog(LOG_ERROR, "source %s has existed.", srcName);
		obs_source_release(source);
		return -1;
	}

	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;

	obs_property_t *property = obs_properties_first(properties);
	bool hasNoProperties = !property;

	while (property) {
		const char        *name = obs_property_name(property);
		obs_property_type type = obs_property_get_type(property);

		if (!obs_property_visible(property))
			goto End;

		if (strcmp(VIDEO_FORMAT, name) == 0 && type == OBS_COMBO_TYPE_LIST){
			obs_combo_format format = obs_property_list_format(property);
			size_t           count = obs_property_list_item_count(property);
			if (idx > count)
				return -1;
			else
				return obs_property_list_item_int(property, idx);
		}
	End:
		obs_property_next(&property);
	}
	return -1;
}
int BroardcastBase::setVideoFormat(const char* srcName,int format)
{
	setResolutionType2Custom(srcName);
	obs_source_t  * source = obs_get_source_by_name(srcName);
	if (source){
		blog(LOG_ERROR, "source %s has existed.", srcName);
		obs_source_release(source);
		return -1;
	}

	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;

	obs_property_t *property = obs_properties_first(properties);
	bool hasNoProperties = !property;

	while (property) {
		const char        *name = obs_property_name(property);
		obs_property_type type = obs_property_get_type(property);

		if (!obs_property_visible(property))
			goto End;

		if (strcmp(VIDEO_FORMAT, name) == 0 && type == OBS_COMBO_TYPE_LIST){
			obs_combo_format format = obs_property_list_format(property);
			size_t           count = obs_property_list_item_count(property);
			obs_data_set_int(settings, VIDEO_FORMAT, format);
			if (!obs_property_modified(property, settings)){
				return  -1;
			}
			else
				return 0;
		}
	End:
		obs_property_next(&property);
	}
	return -1;
}
int BroardcastBase::getVideoFormat(const char* srcName)
{
	obs_source_t  * source = obs_get_source_by_name(srcName);
	if (source){
		blog(LOG_ERROR, "source %s has existed.", srcName);
		obs_source_release(source);
		return -1;
	}

	obs_data_t *settings = sourceArray[source].setting;

	return obs_data_get_int(settings, VIDEO_FORMAT);
}
//枚举、设置视频设备支持的Color Space
std::string BroardcastBase::enumColorSpaces(const char* srcName, size_t idx)
{
	obs_source_t  * source = obs_get_source_by_name(srcName);
	if (source){
		blog(LOG_ERROR, "source %s has existed.", srcName);
		obs_source_release(source);
		return "";
	}

	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;

	obs_property_t *property = obs_properties_first(properties);
	bool hasNoProperties = !property;

	while (property) {
		const char        *name = obs_property_name(property);
		obs_property_type type = obs_property_get_type(property);

		if (!obs_property_visible(property))
			goto End;

		if (strcmp(COLOR_SPACE, name) == 0 && type == OBS_COMBO_TYPE_LIST){
			obs_combo_format format = obs_property_list_format(property);
			size_t           count = obs_property_list_item_count(property);
			if (idx > count)
				return "";
			else
				return obs_property_list_item_name(property, idx);
		}
	End:
		obs_property_next(&property);
	}
	return "";
}
int BroardcastBase::setVideoColorSpace(const char* srcName, const char *colorspace)
{
	obs_source_t  * source = obs_get_source_by_name(srcName);
	if (source){
		blog(LOG_ERROR, "source %s has existed.", srcName);
		obs_source_release(source);
		return -1;
	}

	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;

	obs_property_t *property = obs_properties_first(properties);
	bool hasNoProperties = !property;

	while (property) {
		const char        *name = obs_property_name(property);
		obs_property_type type = obs_property_get_type(property);

		if (!obs_property_visible(property))
			goto End;

		if (strcmp(COLOR_SPACE, name) == 0 && type == OBS_COMBO_TYPE_LIST){
			obs_combo_format format = obs_property_list_format(property);
			size_t           count = obs_property_list_item_count(property);
			obs_data_set_string(settings, COLOR_SPACE, colorspace);
			if (!obs_property_modified(property, settings)){
				return  -1;
			}
			else
				return 0;
		}
	End:
		obs_property_next(&property);
	}
	return  -1;
}
std::string BroardcastBase::getVideoColorSpace(const char* srcName)
{
	obs_source_t  * source = obs_get_source_by_name(srcName);
	if (source){
		blog(LOG_ERROR, "source %s has existed.", srcName);
		obs_source_release(source);
		return "";
	}

	obs_data_t *settings = sourceArray[source].setting;

	return obs_data_get_string(settings, COLOR_SPACE);
}
//枚举、设置视频设备支持的Color Range
std::string BroardcastBase::enumColorRanges(const char* srcName, size_t idx)
{
	obs_source_t  * source = obs_get_source_by_name(srcName);
	if (source){
		blog(LOG_ERROR, "source %s has existed.", srcName);
		obs_source_release(source);
		return "";
	}

	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;

	obs_property_t *property = obs_properties_first(properties);
	bool hasNoProperties = !property;

	while (property) {
		const char        *name = obs_property_name(property);
		obs_property_type type = obs_property_get_type(property);

		if (!obs_property_visible(property))
			goto End;

		if (strcmp(COLOR_RANGE, name) == 0 && type == OBS_COMBO_TYPE_LIST){
			obs_combo_format format = obs_property_list_format(property);
			size_t           count = obs_property_list_item_count(property);
			if (idx > count)
				return "";
			else
				return obs_property_list_item_name(property, idx);
		}
	End:
		obs_property_next(&property);
	}
	return "";
}
int BroardcastBase::setVideoColorRange(const char* srcName, const char *colorrange)
{
	obs_source_t  * source = obs_get_source_by_name(srcName);
	if (source){
		blog(LOG_ERROR, "source %s has existed.", srcName);
		obs_source_release(source);
		return -1;
	}

	obs_properties_t * properties = sourceArray[source].properties;
	obs_data_t *settings = sourceArray[source].setting;

	obs_property_t *property = obs_properties_first(properties);
	bool hasNoProperties = !property;

	while (property) {
		const char        *name = obs_property_name(property);
		obs_property_type type = obs_property_get_type(property);

		if (!obs_property_visible(property))
			goto End;

		if (strcmp(COLOR_RANGE, name) == 0 && type == OBS_COMBO_TYPE_LIST){
			obs_combo_format format = obs_property_list_format(property);
			size_t           count = obs_property_list_item_count(property);
			obs_data_set_string(settings, COLOR_RANGE, colorrange);
			if (!obs_property_modified(property, settings)){
				return  -1;
			}
			else
				return 0;
		}
	End:
		obs_property_next(&property);
	}
	return  -1;
}
std::string BroardcastBase::getVideoColorRange(const char* srcName)
{
	obs_source_t  * source = obs_get_source_by_name(srcName);
	if (source){
		blog(LOG_ERROR, "source %s has existed.", srcName);
		obs_source_release(source);
		return "";
	}

	obs_data_t *settings = sourceArray[source].setting;

	return obs_data_get_string(settings, COLOR_RANGE);
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
int enumFPSs(const char* srcName, size_t idx)
{
	return Engine_main()->enumFPSs(srcName, idx);
}
int setVideoFPS(const char* srcName, int fps)
{
	return Engine_main()->setVideoFPS(srcName, fps);
}
int getVideoFPS(const char* srcName)
{
	return Engine_main()->getVideoFPS(srcName);
}
//枚举、设置视频设备支持的Video formats
int enumVideoFormats(const char* srcName, size_t idx)
{
	return Engine_main()->enumVideoFormats(srcName, idx);
}
int setVideoFormat(const char* srcName, int format)
{
	return Engine_main()->setVideoFormat(srcName, format);
}
int getVideoFormat(const char* srcName)
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