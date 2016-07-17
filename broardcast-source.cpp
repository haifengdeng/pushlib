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
};

static void AddSource(void *_data, obs_scene_t *scene)
{
	AddSourceData *data = (AddSourceData *)_data;
	obs_sceneitem_t *sceneitem;

	sceneitem = obs_scene_add(scene, data->source);
	obs_sceneitem_set_visible(sceneitem, data->visible);
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
		obs_scene_atomic_update(scene, AddSource, &data);
		obs_source_release(source);

		//add source to map
		struct SourceInfo info;
		info.name = srcName;
		info.setting = obs_source_get_settings(source);
		info.properties = obs_source_properties(source);
		info.source = source;
		info.type = (int) type;

		sourceArray[source] = info;
		return 0;
	}
	return -1;
}


//功能：新建名为srcName，类型为type的源
int addNewSource(const char* srcName, InputSourceType type)
{
	return Engine_main()->addNewSource(srcName, type);
}

