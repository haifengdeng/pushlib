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
#include "basic-main-outputs.hpp"
#include "basic-preview.hpp"
#include "push-engine.h"
#include "source_helper.hpp"
using namespace std;

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

static void SaveAudioDevice(const char *name, int channel, obs_data_t *parent,
	vector<OBSSource> &audioSources)
{
	obs_source_t *source = obs_get_output_source(channel);
	if (!source)
		return;

	audioSources.push_back(source);

	obs_data_t *data = obs_save_source(source);

	obs_data_set_obj(parent, name, data);

	obs_data_release(data);
	obs_source_release(source);
}

static obs_data_t *GenerateSaveData(obs_data_array_t *sceneOrder,
	obs_data_array_t *quickTransitionData, int transitionDuration,
	obs_data_array_t *transitions,
	OBSScene &scene, OBSSource &curProgramScene)
{
	obs_data_t *saveData = obs_data_create();

	vector<OBSSource> audioSources;
	audioSources.reserve(5);

	SaveAudioDevice(DESKTOP_AUDIO_1, 1, saveData, audioSources);
	SaveAudioDevice(DESKTOP_AUDIO_2, 2, saveData, audioSources);
	SaveAudioDevice(AUX_AUDIO_1, 3, saveData, audioSources);
	SaveAudioDevice(AUX_AUDIO_2, 4, saveData, audioSources);
	SaveAudioDevice(AUX_AUDIO_3, 5, saveData, audioSources);

	auto FilterAudioSources = [&](obs_source_t *source)
	{
		return find(begin(audioSources), end(audioSources), source) ==
			end(audioSources);
	};
	using FilterAudioSources_t = decltype(FilterAudioSources);

	obs_data_array_t *sourcesArray = obs_save_sources_filtered(
		[](void *data, obs_source_t *source)
	{
		return (*static_cast<FilterAudioSources_t*>(data))(source);
	}, static_cast<void*>(&FilterAudioSources));

	obs_source_t *transition = obs_get_output_source(0);
	obs_source_t *currentScene = obs_scene_get_source(scene);
	const char   *sceneName = obs_source_get_name(currentScene);
	const char   *programName = obs_source_get_name(curProgramScene);

	const char *sceneCollection = config_get_string(GetGlobalConfig(),
		"Basic", "SceneCollection");

	obs_data_set_string(saveData, "current_scene", sceneName);
	obs_data_set_string(saveData, "current_program_scene", programName);
	obs_data_set_array(saveData, "scene_order", sceneOrder);
	obs_data_set_string(saveData, "name", sceneCollection);
	obs_data_set_array(saveData, "sources", sourcesArray);
	obs_data_set_array(saveData, "quick_transitions", quickTransitionData);
	obs_data_set_array(saveData, "transitions", transitions);
	obs_data_array_release(sourcesArray);

	obs_data_set_string(saveData, "current_transition",
		obs_source_get_name(transition));
	obs_data_set_int(saveData, "transition_duration", transitionDuration);
	obs_source_release(transition);

	return saveData;
}


static void LoadAudioDevice(const char *name, int channel, obs_data_t *parent)
{
	obs_data_t *data = obs_data_get_obj(parent, name);
	if (!data)
		return;

	obs_source_t *source = obs_load_source(data);
	if (source) {
		obs_set_output_source(channel, source);
		obs_source_release(source);
	}

	obs_data_release(data);
}

static inline bool HasAudioDevices(const char *source_id)
{
	const char *output_id = source_id;
	obs_properties_t *props = obs_get_source_properties(output_id);
	size_t count = 0;

	if (!props)
		return false;

	obs_property_t *devices = obs_properties_get(props, "device_id");
	if (devices)
		count = obs_property_list_item_count(devices);

	obs_properties_destroy(props);

	return count != 0;
}


BroardcastBase::BroardcastBase()
{
}

BroardcastBase::~BroardcastBase()
{
	signalHandlers.clear();
	ClearSceneData();
	service = nullptr;
	outputHandler.reset();
	obs_display_remove_draw_callback(previewer->GetDisplay(),
		BroardcastBase::RenderMain, this);

	obs_enter_graphics();
	gs_vertexbuffer_destroy(box);
	gs_vertexbuffer_destroy(boxLeft);
	gs_vertexbuffer_destroy(boxTop);
	gs_vertexbuffer_destroy(boxRight);
	gs_vertexbuffer_destroy(boxBottom);
	gs_vertexbuffer_destroy(circle);
	obs_leave_graphics();
}

void BroardcastBase::CreateFirstRunSources()
{
	bool hasDesktopAudio = HasAudioDevices(Engine()->OutputAudioSource());
	bool hasInputAudio = HasAudioDevices(Engine()->InputAudioSource());

	if (hasDesktopAudio)
		ResetAudioDevice(Engine()->OutputAudioSource(), "default",
		"DesktopDevice1_pingan", 1);
	if (hasInputAudio)
		ResetAudioDevice(Engine()->InputAudioSource(), "default",
		"AuxDevice1_input", 3);
}

void BroardcastBase::CreateDefaultScene(bool firstStart)
{
	scene = obs_scene_create("scene_pingan");

	if (firstStart)
		CreateFirstRunSources();
}

bool BroardcastBase::InitService()
{
	ProfileScope("OBSBasic::InitService");

	service = obs_service_create("rtmp_custom", "default_service", nullptr,
		nullptr);
	if (!service)
		return false;
	obs_service_release(service);

	return true;
}

obs_service_t *BroardcastBase::GetService()
{
	if (!service) {
		service = obs_service_create("rtmp_custom", NULL, NULL,
			nullptr);
		obs_service_release(service);
	}
	return service;
}

void BroardcastBase::updateServiceSetting(string server_, string key_)
{
	server_name = server_;
	key_name = key_;
	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "server", server_.c_str());
	obs_data_set_string(settings, "key", key_.c_str());

	obs_service_t * service_ = GetService();

	obs_service_update(service_, settings);
	obs_data_release(settings);
}

bool BroardcastBase::StreamingActive() const
{
	if (!outputHandler)
		return false;
	return outputHandler->StreamingActive();
}

bool BroardcastBase::Active() const
{
	if (!outputHandler)
		return false;
	return outputHandler->Active();
}


static const double scaled_vals[] =
{
	1.0,
	1.25,
	(1.0 / 0.75),
	1.5,
	(1.0 / 0.6),
	1.75,
	2.0,
	2.25,
	2.5,
	2.75,
	3.0,
	0.0
};

bool BroardcastBase::InitBasicConfigDefaults()
{
	vector<MonitorInfo> monitors;
	GetMonitors(monitors);

	if (!monitors.size()) {
		blog(LOG_ERROR, "There appears to be no monitors.  Er, this "
			"technically shouldn't be possible.");
		return false;
	}

	uint32_t cx = monitors[0].cx;
	uint32_t cy = monitors[0].cy;

	/* ----------------------------------------------------- */
	/* move over mixer values in advanced if older config */
	if (config_has_user_value(basicConfig, "AdvOut", "RecTrackIndex") &&
		!config_has_user_value(basicConfig, "AdvOut", "RecTracks")) {

		uint64_t track = config_get_uint(basicConfig, "AdvOut",
			"RecTrackIndex");
		track = 1ULL << (track - 1);
		config_set_uint(basicConfig, "AdvOut", "RecTracks", track);
		config_remove_value(basicConfig, "AdvOut", "RecTrackIndex");
		config_save_safe(basicConfig, "tmp", nullptr);
	}

	/* ----------------------------------------------------- */

	config_set_default_string(basicConfig, "Output", "Mode", "Simple");

	config_set_default_string(basicConfig, "SimpleOutput", "FilePath",
		GetDefaultVideoSavePath().c_str());
	config_set_default_string(basicConfig, "SimpleOutput", "RecFormat",
		"flv");
	config_set_default_uint(basicConfig, "SimpleOutput", "VBitrate",
		2500);
	config_set_default_string(basicConfig, "SimpleOutput", "StreamEncoder",
		SIMPLE_ENCODER_X264);
	config_set_default_uint(basicConfig, "SimpleOutput", "ABitrate", 160);
	config_set_default_bool(basicConfig, "SimpleOutput", "UseAdvanced",
		false);
	config_set_default_bool(basicConfig, "SimpleOutput", "EnforceBitrate",
		true);
	config_set_default_string(basicConfig, "SimpleOutput", "Preset",
		"veryfast");
	config_set_default_string(basicConfig, "SimpleOutput", "RecQuality",
		"Stream");
	config_set_default_string(basicConfig, "SimpleOutput", "RecEncoder",
		SIMPLE_ENCODER_X264);

	config_set_default_bool(basicConfig, "AdvOut", "ApplyServiceSettings",
		true);
	config_set_default_bool(basicConfig, "AdvOut", "UseRescale", false);
	config_set_default_uint(basicConfig, "AdvOut", "TrackIndex", 1);
	config_set_default_string(basicConfig, "AdvOut", "Encoder", "obs_x264");

	config_set_default_string(basicConfig, "AdvOut", "RecType", "Standard");

	config_set_default_string(basicConfig, "AdvOut", "RecFilePath",
		GetDefaultVideoSavePath().c_str());
	config_set_default_string(basicConfig, "AdvOut", "RecFormat", "flv");
	config_set_default_bool(basicConfig, "AdvOut", "RecUseRescale",
		false);
	config_set_default_uint(basicConfig, "AdvOut", "RecTracks", (1 << 0));
	config_set_default_string(basicConfig, "AdvOut", "RecEncoder",
		"none");

	config_set_default_bool(basicConfig, "AdvOut", "FFOutputToFile",
		true);
	config_set_default_string(basicConfig, "AdvOut", "FFFilePath",
		GetDefaultVideoSavePath().c_str());
	config_set_default_string(basicConfig, "AdvOut", "FFExtension", "mp4");
	config_set_default_uint(basicConfig, "AdvOut", "FFVBitrate", 2500);
	config_set_default_bool(basicConfig, "AdvOut", "FFUseRescale",
		false);
	config_set_default_uint(basicConfig, "AdvOut", "FFABitrate", 160);
	config_set_default_uint(basicConfig, "AdvOut", "FFAudioTrack", 1);

	config_set_default_uint(basicConfig, "AdvOut", "Track1Bitrate", 160);
	config_set_default_uint(basicConfig, "AdvOut", "Track2Bitrate", 160);
	config_set_default_uint(basicConfig, "AdvOut", "Track3Bitrate", 160);
	config_set_default_uint(basicConfig, "AdvOut", "Track4Bitrate", 160);

	config_set_default_uint(basicConfig, "Video", "BaseCX", cx);
	config_set_default_uint(basicConfig, "Video", "BaseCY", cy);

	config_set_default_string(basicConfig, "Output", "FilenameFormatting",
		"%CCYY-%MM-%DD %hh-%mm-%ss");

	config_set_default_bool(basicConfig, "Output", "DelayEnable", false);
	config_set_default_uint(basicConfig, "Output", "DelaySec", 20);
	config_set_default_bool(basicConfig, "Output", "DelayPreserve", true);

	config_set_default_bool(basicConfig, "Output", "Reconnect", true);
	config_set_default_uint(basicConfig, "Output", "RetryDelay", 10);
	config_set_default_uint(basicConfig, "Output", "MaxRetries", 20);

	int i = 0;
	uint32_t scale_cx = cx;
	uint32_t scale_cy = cy;

	/* use a default scaled resolution that has a pixel count no higher
	* than 1280x720 */
	while (((scale_cx * scale_cy) > (1280 * 720)) && scaled_vals[i] > 0.0) {
		double scale = scaled_vals[i++];
		scale_cx = uint32_t(double(cx) / scale);
		scale_cy = uint32_t(double(cy) / scale);
	}

	config_set_default_uint(basicConfig, "Video", "OutputCX", scale_cx);
	config_set_default_uint(basicConfig, "Video", "OutputCY", scale_cy);

	config_set_default_uint(basicConfig, "Video", "FPSType", 0);
	config_set_default_string(basicConfig, "Video", "FPSCommon", "30");
	config_set_default_uint(basicConfig, "Video", "FPSInt", 30);
	config_set_default_uint(basicConfig, "Video", "FPSNum", 30);
	config_set_default_uint(basicConfig, "Video", "FPSDen", 1);
	config_set_default_string(basicConfig, "Video", "ScaleType", "bicubic");
	config_set_default_string(basicConfig, "Video", "ColorFormat", "NV12");
	config_set_default_string(basicConfig, "Video", "ColorSpace", "601");
	config_set_default_string(basicConfig, "Video", "ColorRange",
		"Partial");

	config_set_default_uint(basicConfig, "Audio", "SampleRate", 44100);
	config_set_default_string(basicConfig, "Audio", "ChannelSetup",
		"Stereo");

	return true;
}

bool BroardcastBase::InitBasicConfig()
{
	ProfileScope("OBSBasic::InitBasicConfig");

	char configPath[512];

	int ret = GetProfilePath(configPath, sizeof(configPath), "");
	if (ret <= 0) {
		blog(LOG_ERROR, "Failed to get profile path");
		return false;
	}

	if (os_mkdir(configPath) == MKDIR_ERROR) {
		blog(LOG_ERROR, "Failed to create profile path");
		return false;
	}

	ret = GetProfilePath(configPath, sizeof(configPath), "basic.ini");
	if (ret <= 0) {
		blog(LOG_ERROR, "Failed to get base.ini path");
		return false;
	}

	int code = basicConfig.Open(configPath, CONFIG_OPEN_ALWAYS);
	if (code != CONFIG_SUCCESS) {
		blog(LOG_ERROR, "Failed to open basic.ini: %d", code);
		return false;
	}

	if (config_get_string(basicConfig, "General", "Name") == nullptr) {
		const char *curName = config_get_string(Engine()->GlobalConfig(),
			"Basic", "Profile");

		config_set_string(basicConfig, "General", "Name", curName);
		basicConfig.SaveSafe("tmp");
	}

	return InitBasicConfigDefaults();
}

void BroardcastBase::InitPrimitives()
{
	ProfileScope("OBSBasic::InitPrimitives");

	obs_enter_graphics();

	gs_render_start(true);
	gs_vertex2f(0.0f, 0.0f);
	gs_vertex2f(0.0f, 1.0f);
	gs_vertex2f(1.0f, 1.0f);
	gs_vertex2f(1.0f, 0.0f);
	gs_vertex2f(0.0f, 0.0f);
	box = gs_render_save();

	gs_render_start(true);
	gs_vertex2f(0.0f, 0.0f);
	gs_vertex2f(0.0f, 1.0f);
	boxLeft = gs_render_save();

	gs_render_start(true);
	gs_vertex2f(0.0f, 0.0f);
	gs_vertex2f(1.0f, 0.0f);
	boxTop = gs_render_save();

	gs_render_start(true);
	gs_vertex2f(1.0f, 0.0f);
	gs_vertex2f(1.0f, 1.0f);
	boxRight = gs_render_save();

	gs_render_start(true);
	gs_vertex2f(0.0f, 1.0f);
	gs_vertex2f(1.0f, 1.0f);
	boxBottom = gs_render_save();

	gs_render_start(true);
	for (int i = 0; i <= 360; i += (360 / 20)) {
		float pos = RAD(float(i));
		gs_vertex2f(cosf(pos), sinf(pos));
	}
	circle = gs_render_save();

	obs_leave_graphics();
}

void BroardcastBase::SceneReordered(void *data, calldata_t *params)
{
	obs_scene_t *scene = (obs_scene_t*)calldata_ptr(params, "scene");
}

void BroardcastBase::SceneItemAdded(void *data, calldata_t *params)
{
	obs_sceneitem_t *item = (obs_sceneitem_t*)calldata_ptr(params, "item");
}

void BroardcastBase::SceneItemRemoved(void *data, calldata_t *params)
{
	obs_sceneitem_t *item = (obs_sceneitem_t*)calldata_ptr(params, "item");
}

void BroardcastBase::SceneItemSelected(void *data, calldata_t *params)
{

	obs_scene_t     *scene = (obs_scene_t*)calldata_ptr(params, "scene");
	obs_sceneitem_t *item = (obs_sceneitem_t*)calldata_ptr(params, "item");
}

void BroardcastBase::SceneItemDeselected(void *data, calldata_t *params)
{
	obs_scene_t     *scene = (obs_scene_t*)calldata_ptr(params, "scene");
	obs_sceneitem_t *item = (obs_sceneitem_t*)calldata_ptr(params, "item");
}

void BroardcastBase::SourceLoaded(void *data, obs_source_t *source)
{
	BroardcastBase *base = static_cast<BroardcastBase*>(data);

	if (obs_scene_from_source(source) != NULL){
		return;
	}
}

void BroardcastBase::SourceRemoved(void *data, calldata_t *params)
{
	obs_source_t *source = (obs_source_t*)calldata_ptr(params, "source");

	if (obs_scene_from_source(source) != NULL){
		return;
	}
}

void BroardcastBase::SourceActivated(void *data, calldata_t *params)
{
	obs_source_t *source = (obs_source_t*)calldata_ptr(params, "source");
	uint32_t     flags = obs_source_get_output_flags(source);

	if (flags & OBS_SOURCE_AUDIO){
		return;
	}
}

void BroardcastBase::SourceDeactivated(void *data, calldata_t *params)
{
	obs_source_t *source = (obs_source_t*)calldata_ptr(params, "source");
	uint32_t     flags = obs_source_get_output_flags(source);

	if (flags & OBS_SOURCE_AUDIO){
		return;
	}
}

void BroardcastBase::SourceRenamed(void *data, calldata_t *params)
{
	const char *newName = calldata_string(params, "new_name");
	const char *prevName = calldata_string(params, "prev_name");
}

void BroardcastBase::DrawBackdrop(float cx, float cy)
{
	if (!box)
		return;

	gs_effect_t    *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t    *color = gs_effect_get_param_by_name(solid, "color");
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	vec4 colorVal;
	vec4_set(&colorVal, 0.0f, 0.0f, 0.0f, 1.0f);
	gs_effect_set_vec4(color, &colorVal);

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);
	gs_matrix_push();
	gs_matrix_identity();
	gs_matrix_scale3f(float(cx), float(cy), 1.0f);

	gs_load_vertexbuffer(box);
	gs_draw(GS_TRISTRIP, 0, 0);

	gs_matrix_pop();
	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_load_vertexbuffer(nullptr);
}

void BroardcastBase::RenderMain(void *data, uint32_t cx, uint32_t cy)
{
	BroardcastBase *window = static_cast<BroardcastBase*>(data);
	obs_video_info ovi;

	obs_get_video_info(&ovi);

	window->previewCX = int(window->previewScale * float(ovi.base_width));
	window->previewCY = int(window->previewScale * float(ovi.base_height));

	gs_viewport_push();
	gs_projection_push();

	/* --------------------------------------- */

	gs_ortho(0.0f, float(ovi.base_width), 0.0f, float(ovi.base_height),
		-100.0f, 100.0f);
	gs_set_viewport(window->previewX, window->previewY,
		window->previewCX, window->previewCY);

	window->DrawBackdrop(float(ovi.base_width), float(ovi.base_height));

	if (window->IsPreviewProgramMode()) {
		OBSScene scene = window->GetCurrentScene();
		obs_source_t *source = obs_scene_get_source(scene);
		if (source)
			obs_source_video_render(source);
	}
	else {
		obs_render_main_view();
	}
	gs_load_vertexbuffer(nullptr);

	/* --------------------------------------- */
	int width = 0, height = 0;
	window->GetPreviewerSize(width,height);
	float right = float(width) - window->previewX;
	float bottom = float(height) - window->previewY;

	gs_ortho(-window->previewX, right,
		-window->previewY, bottom,
		-100.0f, 100.0f);
	gs_reset_viewport();

	window->DrawPreviewerSceneEditing();

	/* --------------------------------------- */

	gs_projection_pop();
	gs_viewport_pop();

	UNUSED_PARAMETER(cx);
	UNUSED_PARAMETER(cy);
}


void BroardcastBase::InitOBSCallbacks()
{
	ProfileScope("OBSBasic::InitOBSCallbacks");

	signalHandlers.reserve(signalHandlers.size() + 6);
	signalHandlers.emplace_back(obs_get_signal_handler(), "source_remove",
		BroardcastBase::SourceRemoved, this);
	signalHandlers.emplace_back(obs_get_signal_handler(), "source_activate",
		BroardcastBase::SourceActivated, this);
	signalHandlers.emplace_back(obs_get_signal_handler(), "source_deactivate",
		BroardcastBase::SourceDeactivated, this);
	signalHandlers.emplace_back(obs_get_signal_handler(), "source_rename",
		BroardcastBase::SourceRenamed, this);
}


#ifdef _WIN32
#define IS_WIN32 1
#else
#define IS_WIN32 0
#endif

static inline int AttemptToResetVideo(struct obs_video_info *ovi)
{
	return obs_reset_video(ovi);
}

static inline enum obs_scale_type GetScaleType(ConfigFile &basicConfig)
{
	const char *scaleTypeStr = config_get_string(basicConfig,
		"Video", "ScaleType");

	if (astrcmpi(scaleTypeStr, "bilinear") == 0)
		return OBS_SCALE_BILINEAR;
	else if (astrcmpi(scaleTypeStr, "lanczos") == 0)
		return OBS_SCALE_LANCZOS;
	else
		return OBS_SCALE_BICUBIC;
}

static inline enum video_format GetVideoFormatFromName(const char *name)
{
	if (astrcmpi(name, "I420") == 0)
		return VIDEO_FORMAT_I420;
	else if (astrcmpi(name, "NV12") == 0)
		return VIDEO_FORMAT_NV12;
	else if (astrcmpi(name, "I444") == 0)
		return VIDEO_FORMAT_I444;
#if 0 //currently unsupported
	else if (astrcmpi(name, "YVYU") == 0)
		return VIDEO_FORMAT_YVYU;
	else if (astrcmpi(name, "YUY2") == 0)
		return VIDEO_FORMAT_YUY2;
	else if (astrcmpi(name, "UYVY") == 0)
		return VIDEO_FORMAT_UYVY;
#endif
	else
		return VIDEO_FORMAT_RGBA;
}

void BroardcastBase::GetFPSCommon(uint32_t &num, uint32_t &den) const
{
	const char *val = config_get_string(basicConfig, "Video", "FPSCommon");

	if (strcmp(val, "10") == 0) {
		num = 10;
		den = 1;
	}
	else if (strcmp(val, "20") == 0) {
		num = 20;
		den = 1;
	}
	else if (strcmp(val, "25") == 0) {
		num = 25;
		den = 1;
	}
	else if (strcmp(val, "29.97") == 0) {
		num = 30000;
		den = 1001;
	}
	else if (strcmp(val, "48") == 0) {
		num = 48;
		den = 1;
	}
	else if (strcmp(val, "59.94") == 0) {
		num = 60000;
		den = 1001;
	}
	else if (strcmp(val, "60") == 0) {
		num = 60;
		den = 1;
	}
	else {
		num = 30;
		den = 1;
	}
}

void BroardcastBase::GetFPSInteger(uint32_t &num, uint32_t &den) const
{
	num = (uint32_t)config_get_uint(basicConfig, "Video", "FPSInt");
	den = 1;
}

void BroardcastBase::GetFPSFraction(uint32_t &num, uint32_t &den) const
{
	num = (uint32_t)config_get_uint(basicConfig, "Video", "FPSNum");
	den = (uint32_t)config_get_uint(basicConfig, "Video", "FPSDen");
}

void BroardcastBase::GetFPSNanoseconds(uint32_t &num, uint32_t &den) const
{
	num = 1000000000;
	den = (uint32_t)config_get_uint(basicConfig, "Video", "FPSNS");
}

void BroardcastBase::GetConfigFPS(uint32_t &num, uint32_t &den) const
{
	uint32_t type = config_get_uint(basicConfig, "Video", "FPSType");

	if (type == 1) //"Integer"
		GetFPSInteger(num, den);
	else if (type == 2) //"Fraction"
		GetFPSFraction(num, den);
	else if (false) //"Nanoseconds", currently not implemented
		GetFPSNanoseconds(num, den);
	else
		GetFPSCommon(num, den);
}

int BroardcastBase::ResetVideo()
{
	ProfileScope("OBSBasic::ResetVideo");

	struct obs_video_info ovi;
	int ret;

	GetConfigFPS(ovi.fps_num, ovi.fps_den);

	const char *colorFormat = config_get_string(basicConfig, "Video",
		"ColorFormat");
	const char *colorSpace = config_get_string(basicConfig, "Video",
		"ColorSpace");
	const char *colorRange = config_get_string(basicConfig, "Video",
		"ColorRange");

	ovi.graphics_module = Engine()->GetRenderModule();
	ovi.base_width = (uint32_t)config_get_uint(basicConfig,
		"Video", "BaseCX");
	ovi.base_height = (uint32_t)config_get_uint(basicConfig,
		"Video", "BaseCY");
	ovi.output_width = (uint32_t)config_get_uint(basicConfig,
		"Video", "OutputCX");
	ovi.output_height = (uint32_t)config_get_uint(basicConfig,
		"Video", "OutputCY");
	ovi.output_format = GetVideoFormatFromName(colorFormat);
	ovi.colorspace = astrcmpi(colorSpace, "601") == 0 ?
	VIDEO_CS_601 : VIDEO_CS_709;
	ovi.range = astrcmpi(colorRange, "Full") == 0 ?
	VIDEO_RANGE_FULL : VIDEO_RANGE_PARTIAL;
	ovi.adapter = 0;
	ovi.gpu_conversion = true;
	ovi.scale_type = GetScaleType(basicConfig);

	if (ovi.base_width == 0 || ovi.base_height == 0) {
		ovi.base_width = 1920;
		ovi.base_height = 1080;
		config_set_uint(basicConfig, "Video", "BaseCX", 1920);
		config_set_uint(basicConfig, "Video", "BaseCY", 1080);
	}

	if (ovi.output_width == 0 || ovi.output_height == 0) {
		ovi.output_width = ovi.base_width;
		ovi.output_height = ovi.base_height;
		config_set_uint(basicConfig, "Video", "OutputCX",
			ovi.base_width);
		config_set_uint(basicConfig, "Video", "OutputCY",
			ovi.base_height);
	}

	ret = AttemptToResetVideo(&ovi);
	if (IS_WIN32 && ret != OBS_VIDEO_SUCCESS) {
		/* Try OpenGL if DirectX fails on windows */
		if (astrcmpi(ovi.graphics_module, DL_OPENGL) != 0) {
			blog(LOG_WARNING, "Failed to initialize obs video (%d) "
				"with graphics_module='%s', retrying "
				"with graphics_module='%s'",
				ret, ovi.graphics_module,
				DL_OPENGL);
			ovi.graphics_module = DL_OPENGL;
			ret = AttemptToResetVideo(&ovi);
		}
	}
	else if (ret == OBS_VIDEO_SUCCESS) {
		//TODO
		blog(LOG_INFO, "Reset video ok.");
	}

	return ret;
}

bool BroardcastBase::ResetAudio()
{
	ProfileScope("OBSBasic::ResetAudio");

	struct obs_audio_info ai;
	ai.samples_per_sec = config_get_uint(basicConfig, "Audio",
		"SampleRate");

	const char *channelSetupStr = config_get_string(basicConfig,
		"Audio", "ChannelSetup");

	if (strcmp(channelSetupStr, "Mono") == 0)
		ai.speakers = SPEAKERS_MONO;
	else
		ai.speakers = SPEAKERS_STEREO;

	return obs_reset_audio(&ai);
}

void BroardcastBase::ResetAudioDevice(const char *sourceId, const char *deviceId,
	const char *deviceDesc, int channel)
{
	obs_source_t *source;
	obs_data_t *settings;
	bool same = false;

	source = obs_get_output_source(channel);
	if (source) {
		settings = obs_source_get_settings(source);
		const char *curId = obs_data_get_string(settings, "device_id");

		same = (strcmp(curId, deviceId) == 0);

		obs_data_release(settings);
		obs_source_release(source);
	}

	if (!same)
		obs_set_output_source(channel, nullptr);

	if (!same && strcmp(deviceId, "disabled") != 0) {
		obs_data_t *settings = obs_data_create();
		obs_data_set_string(settings, "device_id", deviceId);
		source = obs_source_create(sourceId, deviceDesc, settings,
			nullptr);
		obs_data_release(settings);

		obs_set_output_source(channel, source);
		obs_source_release(source);
	}
}

void BroardcastBase::ClearSceneData()
{
	obs_set_output_source(0, nullptr);
	obs_set_output_source(1, nullptr);
	obs_set_output_source(2, nullptr);
	obs_set_output_source(3, nullptr);
	obs_set_output_source(4, nullptr);
	obs_set_output_source(5, nullptr);

	auto cb = [](void *unused, obs_source_t *source)
	{
		obs_source_remove(source);
		UNUSED_PARAMETER(unused);
		return true;
	};

	obs_enum_sources(cb, nullptr);

	blog(LOG_INFO, "All scene data cleared");
	blog(LOG_INFO, "------------------------------------------------");
}

void BroardcastBase::StartStreaming()
{
	if (!outputHandler->StartStreaming(service)) {
		blog(LOG_INFO, "starting streaming");
	}

	bool recordWhenStreaming = config_get_bool(GetGlobalConfig(),
		"BasicWindow", "RecordWhenStreaming");
}

void BroardcastBase::StopStreaming()
{

	if (outputHandler->StreamingActive())
		outputHandler->StopStreaming();

	if (!outputHandler->Active()) {
		blog(LOG_INFO, "stoping streaming");
	}
}

void BroardcastBase::StartRecording()
{
	if (outputHandler->RecordingActive())
		return;
	outputHandler->StartRecording();
}
void BroardcastBase::StopRecording()
{

	if (outputHandler->RecordingActive())
		outputHandler->StopRecording();
}

config_t *BroardcastBase::Config() const
{
	return basicConfig;
}

bool BroardcastBase::GetPreviewerSize(int &width, int &height)
{ 
	return previewer->getSize(width, height);

}
void BroardcastBase::DrawPreviewerSceneEditing()
{ 
	return previewer->DrawSceneEditing();
}

OBSScene BroardcastBase::GetCurrentScene()
{
	return scene;
}

int BroardcastBase::devicePixelRatio()
{
	return 1;
}

void BroardcastBase::ResetOutputs()
{
	ProfileScope("OBSBasic::ResetOutputs");

	const char *mode = config_get_string(basicConfig, "Output", "Mode");
	bool advOut = astrcmpi(mode, "Advanced") == 0;

	if (!outputHandler || !outputHandler->Active()) {
		outputHandler.reset();
		outputHandler.reset(advOut ?
			CreateAdvancedOutputHandler(this) :
			CreateSimpleOutputHandler(this));
	}
	else {
		outputHandler->Update();
	}
}

void BroardcastBase::EnablePreviewDisplay(bool enable)
{
	obs_display_set_enabled(previewer->GetDisplay(), enable);
}

void BroardcastBase::setRenderWindow(void* Window)
{
	if (previewer)
		return;
	previewer = new OBSBasicPreview(Window);
	obs_display_add_draw_callback(previewer->GetDisplay(),
		BroardcastBase::RenderMain, this);

	struct obs_video_info ovi;
	if (obs_get_video_info(&ovi)){
		//TODO;
		//ResizePreview(ovi.base_width, ovi.base_height);
	}
	EnablePreviewDisplay(true);
}

void BroardcastBase::OBSInit()
{
	ProfileScope("OBSBasic::OBSInit");
	int ret;

	if (!InitBasicConfig())
		throw "Failed to load basic.ini";

	if (!ResetAudio())
		throw "Failed to initialize audio";

	ret = ResetVideo();

	switch (ret) {
	case OBS_VIDEO_MODULE_NOT_FOUND:
		throw "Failed to initialize video:  Graphics module not found";
	case OBS_VIDEO_NOT_SUPPORTED:
		throw "Failed to initialize video:  Required graphics API "
			"functionality not found on these drivers or "
			"unavailable on this equipment";
	case OBS_VIDEO_INVALID_PARAM:
		throw "Failed to initialize video:  Invalid parameters";
	default:
		if (ret != OBS_VIDEO_SUCCESS)
			throw "Failed to initialize video:  Unspecified error";
	}

	InitOBSCallbacks();
	obs_load_all_modules();
	ResetOutputs();
	if (!InitService())
		throw "Failed to initialize service";
	InitPrimitives();

	InitOBSTransition();
	CreateDefaultScene(false);
	SetCurrentScene(scene, true);
}

int GetDeviceVectorFromSourceType(int type)
{
	if (type != WINDOW_CAPTURE_SOURCE &&
		type != VIDEO_CAPTURE_SOURCE &&
		type != AUDIO_INPUT_SOURCE &&
		type != AUDIO_OUTPUT_SOURCE)
		return false;
	string id;
	SourceTypeConvertString((InputSourceType)type, id);
	string tempName = id + "tempName";
	vector<string>  DeviceArray;
	obs_source_t *source = obs_get_source_by_name(tempName.c_str());
	if (!source){
		source = obs_source_create(id.c_str(), tempName.c_str(), NULL, nullptr);
		if (!source)
			return false;
	}
	using properties_delete_t = decltype(&obs_properties_destroy);
	using properties_t =
		std::unique_ptr<obs_properties_t, properties_delete_t>;
	properties_t  properties(nullptr, obs_properties_destroy);
	properties.reset(obs_source_properties(source));

	obs_property_t *property = obs_properties_first(properties.get());
	bool hasNoProperties = !property;

	while (property) {
		const char        *name = obs_property_name(property);
		obs_property_type type = obs_property_get_type(property);

		if (!obs_property_visible(property))
			goto End;

		if (type == OBS_PROPERTY_LIST){
			const char       *name = obs_property_name(property);
			obs_combo_type   type = obs_property_list_type(property);
			obs_combo_format format = obs_property_list_format(property);
			size_t           count = obs_property_list_item_count(property);
			for (int i = 0; i < count; i++){
				const char *name_item = obs_property_list_item_name(property, i);
				blog(LOG_INFO, "%s: %s %d/%d %s", id.c_str(), name, i, count, name_item);
			}
		}

End:
		obs_property_next(&property);
	}
	return true;
}

void setRenderWindow(void* Window)
{
	Engine_main()->setRenderWindow(Window);
}


//功能：开始推流
//回调：OnStartPublish
int startStreaming(string server_,string key_)
{
	Engine_main()->updateServiceSetting(server_, key_);
	Engine_main()->StartStreaming();
	return 0;
}
//功能：停止推流
//回调：OnStopPublish
int stopStreaming()
{
	Engine_main()->StopStreaming();
	return 0;
}