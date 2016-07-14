#pragma once

#include "obs.hpp"
#include "util\util.hpp"

class BroardcastBase
{
public:
	BroardcastBase();
	~BroardcastBase();

	ConfigFile    basicConfig;
	OBSService service;

	gs_vertbuffer_t *box = nullptr;
	gs_vertbuffer_t *boxLeft = nullptr;
	gs_vertbuffer_t *boxTop = nullptr;
	gs_vertbuffer_t *boxRight = nullptr;
	gs_vertbuffer_t *boxBottom = nullptr;
	gs_vertbuffer_t *circle = nullptr;

	int           previewX = 0, previewY = 0;
	int           previewCX = 0, previewCY = 0;
	float         previewScale = 0.0f;

	OBSScene scene;
	obs_service_t *BroardcastBase::GetService();
	config_t *     Config() const;
	OBSScene GetCurrentScene();
	int devicePixelRatio();
};

int GetProfilePath(char *path, size_t size, const char *file);