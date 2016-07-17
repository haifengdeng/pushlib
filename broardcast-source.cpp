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