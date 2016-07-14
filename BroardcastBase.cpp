#include "stdafx.h"
#include "BroardcastBase.hpp"
#include "BroardcastEngine.hpp"
#include "util\platform.h"

int GetProfilePath(char *path, size_t size, const char *file)
{
	char profiles_path[512];
	const char *profile = config_get_string(BroardcastEngine::instance()->GlobalConfig(),
		"Basic", "ProfileDir");
	int ret;

	if (!profile)
		return -1;
	if (!path)
		return -1;
	if (!file)
		file = "";

	ret = GetConfigPath(profiles_path, 512, "obs-studio/basic/profiles");
	if (ret <= 0)
		return ret;

	if (!*file)
		return snprintf(path, size, "%s/%s", profiles_path, profile);

	return snprintf(path, size, "%s/%s/%s", profiles_path, profile, file);
}

BroardcastBase::BroardcastBase()
{
}


BroardcastBase::~BroardcastBase()
{
}

obs_service_t *BroardcastBase::GetService()
{
	if (!service) {
		service = obs_service_create("rtmp_common", NULL, NULL,
			nullptr);
		obs_service_release(service);
	}
	return service;
}

config_t *BroardcastBase::Config() const
{
	return basicConfig;
}

OBSScene BroardcastBase::GetCurrentScene()
{
	return scene;
}

int BroardcastBase::devicePixelRatio()
{
	return 1;
}

void BroardcastBase::OBSInit()
{

}