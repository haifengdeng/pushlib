#pragma once

#include "obs.hpp"
#include <obs.hpp>
#include <util/lexer.h>
#include <util/profiler.h>
#include <util/util.hpp>
#include <util/platform.h>
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <util/threading.h>

#define DESKTOP_AUDIO_1 Str("DesktopAudioDevice1")
#define DESKTOP_AUDIO_2 Str("DesktopAudioDevice2")
#define AUX_AUDIO_1     Str("AuxAudioDevice1")
#define AUX_AUDIO_2     Str("AuxAudioDevice2")
#define AUX_AUDIO_3     Str("AuxAudioDevice3")

#define SIMPLE_ENCODER_X264                    "x264"
#define SIMPLE_ENCODER_X264_LOWCPU             "x264_lowcpu"
#define SIMPLE_ENCODER_QSV                     "qsv"
#define SIMPLE_ENCODER_NVENC                   "nvenc"

#define PREVIEW_EDGE_SIZE 10

struct BasicOutputHandler;
class OBSBasicPreview;
struct Geometry
{
	int x;
	int y;
	int width;
	int height;
	Geometry(){
		x = y = width = height = 0;
	}
};

class BroardcastBase
{
public:
	ConfigFile    basicConfig;

	std::vector<OBSSignal> signalHandlers;

	bool loaded = false;
	long disableSaving = 1;
	bool projectChanged = false;
	bool previewEnabled = true;

	os_cpu_usage_info_t *cpuUsageInfo = nullptr;
	OBSService service;
	std::unique_ptr<BasicOutputHandler> outputHandler;

	OBSScene scene;
	OBSBasicPreview *previewer = nullptr;

	gs_vertbuffer_t *box = nullptr;
	gs_vertbuffer_t *boxLeft = nullptr;
	gs_vertbuffer_t *boxTop = nullptr;
	gs_vertbuffer_t *boxRight = nullptr;
	gs_vertbuffer_t *boxBottom = nullptr;
	gs_vertbuffer_t *circle = nullptr;


	int           previewX = 0, previewY = 0;
	int           previewCX = 0, previewCY = 0;
	float         previewScale = 0.0f;

	void          DrawBackdrop(float cx, float cy);

	void          SetupEncoders();

	void          CreateFirstRunSources();
	void          CreateDefaultScene(bool firstStart);
	void          ClearSceneData();
	void          Load(const char *file);
	bool          InitService();
	bool          InitBasicConfigDefaults();
	bool          InitBasicConfig();
	void          InitOBSCallbacks();
	
	void          InitPrimitives();

	//transition
	obs_source_t  *fadeTransition = nullptr;
    void          InitDefaultTransitions();
	void          InitTransition(obs_source_t *transition);
	void          TransitionStopped();
	void          TransitionToScene(obs_scene_t *scene, bool force);
	void          TransitionToScene(obs_source_t *source, bool force);
	void          SetTransition(obs_source_t *transition);
	void          SetCurrentScene(obs_source_t *scene, bool force);
	void          SetCurrentScene(obs_scene_t *scene, bool force);
	void          InitOBSTransition();
	//

	volatile bool previewProgramMode = false;
	inline bool IsPreviewProgramMode() const
	{
		return os_atomic_load_bool(&previewProgramMode);
	}

	void GetFPSCommon(uint32_t &num, uint32_t &den) const;
	void GetFPSInteger(uint32_t &num, uint32_t &den) const;
	void GetFPSFraction(uint32_t &num, uint32_t &den) const;
	void GetFPSNanoseconds(uint32_t &num, uint32_t &den) const;
	void GetConfigFPS(uint32_t &num, uint32_t &den) const;

private:
	/* OBS Callbacks */
	static void SceneReordered(void *data, calldata_t *params);
	static void SceneItemAdded(void *data, calldata_t *params);
	static void SceneItemRemoved(void *data, calldata_t *params);
	static void SceneItemSelected(void *data, calldata_t *params);
	static void SceneItemDeselected(void *data, calldata_t *params);
	static void SourceLoaded(void *data, obs_source_t *source);
	static void SourceRemoved(void *data, calldata_t *params);
	static void SourceActivated(void *data, calldata_t *params);
	static void SourceDeactivated(void *data, calldata_t *params);
	static void SourceRenamed(void *data, calldata_t *params);
	static void RenderMain(void *data, uint32_t cx, uint32_t cy);

public:
	BroardcastBase();
	~BroardcastBase();

	void OBSInit();
	bool StreamingActive() const;
	bool Active() const;

	int  ResetVideo();
	bool ResetAudio();

	void ResetOutputs();

	void ResetAudioDevice(const char *sourceId, const char *deviceId,
		const char *deviceDesc, int channel);

	config_t *     Config() const;
	OBSScene GetCurrentScene();
	int devicePixelRatio();
	bool GetPreviewerSize(int &width, int &height);
	void DrawPreviewerSceneEditing();

	void EnablePreviewDisplay(bool enable);

	//set main preview window
	void setRenderWindow(void* Window);

	//source
	int  addNewSource(const char* srcName, int type);
	int setVisible(const char *srcName, bool isShow);
	int setSelection(const char* srcName, bool select);
	struct SourceInfo{
		int             type;
		obs_source_t    *source;
		std::string     name;
		obs_data_t      *setting;
		obs_properties_t *properties;
		obs_sceneitem_t  *item;
	};
	std::map<obs_source_t *, struct SourceInfo>  sourceArray;

	//streaming & recording
	void StartStreaming();
	void StopStreaming();
	void StartRecording();
	void StopRecording();
	//service 
	obs_service_t * GetService();
	void updateServiceSetting(std::string server_, std::string key_);
	std::string server_name;
	std::string key_name;

	//logo
	std::string     logoPath;
	Geometry   logo_geometry;
	std::string getLogo();
	int setLogo(const char *imageFilePath);
	int setLogoGeometry(int x, int y, int width, int height);
	int getLogoGeometry(int& x, int& y, int& width, int& height);
	int removeLogo();
};

int GetProfilePath(char *path, size_t size, const char *file);
