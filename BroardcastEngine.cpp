#include "stdafx.h"
#include <string>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <wchar.h>
#include <chrono>
#include <ratio>
#include <sstream>
#include <algorithm>
#include <mutex>
#include <util/bmem.h>
#include <util/dstr.h>
#include <util/platform.h>
#include <util/profiler.hpp>
#include <obs-config.h>
#include <obs.hpp>
#include "BroardcastEngine.hpp"
#include "platform.hpp"
#include <fstream>
#include <curl/curl.h>
#include "push-engine.h"
#include <windows.h>

using namespace std;

static string currentLogFile;
static string lastLogFile;

static bool portable_mode = false;
bool opt_start_streaming = false;
bool opt_start_recording = false;
string opt_starting_collection;
string opt_starting_profile;
string opt_starting_scene;

string CurrentTimeString()
{
	using namespace std::chrono;

	struct tm  tstruct;
	char       buf[80];

	auto tp = system_clock::now();
	auto now = system_clock::to_time_t(tp);
	tstruct = *localtime(&now);

	size_t written = strftime(buf, sizeof(buf), "%X", &tstruct);
	if (ratio_less<system_clock::period, seconds::period>::value &&
		written && (sizeof(buf) - written) > 5) {
		auto tp_secs =
			time_point_cast<seconds>(tp);
		auto millis =
			duration_cast<milliseconds>(tp - tp_secs).count();

		snprintf(buf + written, sizeof(buf) - written, ".%03u",
			static_cast<unsigned>(millis));
	}

	return buf;
}

string CurrentDateTimeString()
{
	time_t     now = time(0);
	struct tm  tstruct;
	char       buf[80];
	tstruct = *localtime(&now);
	strftime(buf, sizeof(buf), "%Y-%m-%d, %X", &tstruct);
	return buf;
}

static inline void LogString(fstream &logFile, const char *timeString,
	char *str)
{
	logFile << timeString << str << endl;
}

static inline void LogStringChunk(fstream &logFile, char *str)
{
	char *nextLine = str;
	string timeString = CurrentTimeString();
	timeString += ": ";

	while (*nextLine) {
		char *nextLine = strchr(str, '\n');
		if (!nextLine)
			break;

		if (nextLine != str && nextLine[-1] == '\r') {
			nextLine[-1] = 0;
		}
		else {
			nextLine[0] = 0;
		}

		LogString(logFile, timeString.c_str(), str);
		nextLine++;
		str = nextLine;
	}

	LogString(logFile, timeString.c_str(), str);
}

#define MAX_REPEATED_LINES 30
#define MAX_CHAR_VARIATION (255 * 3)

static inline int sum_chars(const char *str)
{
	int val = 0;
	for (; *str != 0; str++)
		val += *str;

	return val;
}

static inline bool too_many_repeated_entries(fstream &logFile, const char *msg,
	const char *output_str)
{
	static mutex log_mutex;
	static const char *last_msg_ptr = nullptr;
	static int last_char_sum = 0;
	static char cmp_str[4096];
	static int rep_count = 0;

	int new_sum = sum_chars(output_str);

	lock_guard<mutex> guard(log_mutex);

	if (last_msg_ptr == msg) {
		int diff = std::abs(new_sum - last_char_sum);
		if (diff < MAX_CHAR_VARIATION) {
			return (rep_count++ >= MAX_REPEATED_LINES);
		}
	}

	if (rep_count > MAX_REPEATED_LINES) {
		logFile << CurrentTimeString() <<
			": Last log entry repeated for " <<
			to_string(rep_count - MAX_REPEATED_LINES) <<
			" more lines" << endl;
	}

	last_msg_ptr = msg;
	strcpy(cmp_str, output_str);
	last_char_sum = new_sum;
	rep_count = 0;

	return false;
}

static void do_log(int log_level, const char *msg, va_list args, void *param)
{
	fstream &logFile = *static_cast<fstream*>(param);
	char str[4096];

#ifndef _WIN32
	va_list args2;
	va_copy(args2, args);
#endif

	vsnprintf(str, 4095, msg, args);

#ifdef _WIN32
	OutputDebugStringA(str);
	OutputDebugStringA("\n");
#else
	def_log_handler(log_level, msg, args2, nullptr);
#endif

	if (too_many_repeated_entries(logFile, msg, str))
		return;

	if (log_level <= LOG_INFO)
		LogStringChunk(logFile, str);

#if defined(_WIN32) && defined(OBS_DEBUGBREAK_ON_ERROR)
	if (log_level <= LOG_ERROR && IsDebuggerPresent())
		__debugbreak();
#endif
}

#define DEFAULT_LANG "en-US"

bool BroardcastEngine::InitGlobalConfigDefaults()
{
	config_set_default_string(globalConfig, "General", "Language",
		DEFAULT_LANG);
	config_set_default_uint(globalConfig, "General", "MaxLogs", 10);
	config_set_default_string(globalConfig, "General", "ProcessPriority",
		"Normal");

#if _WIN32
	config_set_default_string(globalConfig, "Video", "Renderer",
		"Direct3D 11");
#else
	config_set_default_string(globalConfig, "Video", "Renderer", "OpenGL");
#endif

	config_set_default_bool(globalConfig, "BasicWindow", "PreviewEnabled",
		true);
	config_set_default_bool(globalConfig, "BasicWindow",
		"PreviewProgramMode", false);
	config_set_default_bool(globalConfig, "BasicWindow",
		"SceneDuplicationMode", true);
	config_set_default_bool(globalConfig, "BasicWindow",
		"SwapScenesMode", true);
	config_set_default_bool(globalConfig, "BasicWindow",
		"SnappingEnabled", true);
	config_set_default_bool(globalConfig, "BasicWindow",
		"ScreenSnapping", true);
	config_set_default_bool(globalConfig, "BasicWindow",
		"SourceSnapping", true);
	config_set_default_bool(globalConfig, "BasicWindow",
		"CenterSnapping", false);
	config_set_default_double(globalConfig, "BasicWindow",
		"SnapDistance", 10.0);
	config_set_default_bool(globalConfig, "BasicWindow",
		"RecordWhenStreaming", false);
	config_set_default_bool(globalConfig, "BasicWindow",
		"KeepRecordingWhenStreamStops", false);
	config_set_default_bool(globalConfig, "BasicWindow",
		"ShowTransitions", true);
	config_set_default_bool(globalConfig, "BasicWindow",
		"ShowListboxToolbars", true);
	config_set_default_bool(globalConfig, "BasicWindow",
		"ShowStatusBar", true);
	return true;
}

static bool do_mkdir(const char *path)
{
	if (os_mkdirs(path) == MKDIR_ERROR) {
		blog(LOG_ERROR,  "Failed to create directory %s", path);
		return false;
	}

	return true;
}

static bool MakeUserDirs()
{
	char path[512];

	if (GetConfigPath(path, sizeof(path), "obs-studio/basic") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	if (GetConfigPath(path, sizeof(path), "obs-studio/logs") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	if (GetConfigPath(path, sizeof(path), "obs-studio/plugin_config") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	return true;
}

static bool MakeUserProfileDirs()
{
	char path[512];

	if (GetConfigPath(path, sizeof(path), "obs-studio/basic/profiles") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	if (GetConfigPath(path, sizeof(path), "obs-studio/basic/scenes") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	return true;
}

static string GetProfileDirFromName(const char *name)
{
	string outputPath;
	os_glob_t *glob;
	char path[512];

	if (GetConfigPath(path, sizeof(path), "obs-studio/basic/profiles") <= 0)
		return outputPath;

	strcat(path, "/*.*");

	if (os_glob(path, 0, &glob) != 0)
		return outputPath;

	for (size_t i = 0; i < glob->gl_pathc; i++) {
		struct os_globent ent = glob->gl_pathv[i];
		if (!ent.directory)
			continue;

		strcpy(path, ent.path);
		strcat(path, "/basic.ini");

		ConfigFile config;
		if (config.Open(path, CONFIG_OPEN_EXISTING) != 0)
			continue;

		const char *curName = config_get_string(config, "General",
			"Name");
		if (astrcmpi(curName, name) == 0) {
			outputPath = ent.path;
			break;
		}
	}

	os_globfree(glob);

	if (!outputPath.empty()) {
		replace(outputPath.begin(), outputPath.end(), '\\', '/');
		const char *start = strrchr(outputPath.c_str(), '/');
		if (start)
			outputPath.erase(0, start - outputPath.c_str() + 1);
	}

	return outputPath;
}

static string GetSceneCollectionFileFromName(const char *name)
{
	string outputPath;
	os_glob_t *glob;
	char path[512];

	if (GetConfigPath(path, sizeof(path), "obs-studio/basic/scenes") <= 0)
		return outputPath;

	strcat(path, "/*.json");

	if (os_glob(path, 0, &glob) != 0)
		return outputPath;

	for (size_t i = 0; i < glob->gl_pathc; i++) {
		struct os_globent ent = glob->gl_pathv[i];
		if (ent.directory)
			continue;

		obs_data_t *data =
			obs_data_create_from_json_file_safe(ent.path, "bak");
		const char *curName = obs_data_get_string(data, "name");

		if (astrcmpi(name, curName) == 0) {
			outputPath = ent.path;
			obs_data_release(data);
			break;
		}

		obs_data_release(data);
	}

	os_globfree(glob);

	if (!outputPath.empty()) {
		outputPath.resize(outputPath.size() - 5);
		replace(outputPath.begin(), outputPath.end(), '\\', '/');
		const char *start = strrchr(outputPath.c_str(), '/');
		if (start)
			outputPath.erase(0, start - outputPath.c_str() + 1);
	}

	return outputPath;
}

bool BroardcastEngine::InitGlobalConfig()
{
	char path[512];

	int len = GetConfigPath(path, sizeof(path),
		"obs-studio/global.ini");
	if (len <= 0) {
		return false;
	}

	int errorcode = globalConfig.Open(path, CONFIG_OPEN_ALWAYS);
	if (errorcode != CONFIG_SUCCESS) {
		blog(LOG_ERROR, "Failed to open global.ini: %d", errorcode);
		return false;
	}

	if (!opt_starting_collection.empty()) {
		string path = GetSceneCollectionFileFromName(
			opt_starting_collection.c_str());
		if (!path.empty()) {
			config_set_string(globalConfig,
				"Basic", "SceneCollection",
				opt_starting_collection.c_str());
			config_set_string(globalConfig,
				"Basic", "SceneCollectionFile",
				path.c_str());
		}
	}

	if (!opt_starting_profile.empty()) {
		string path = GetProfileDirFromName(
			opt_starting_profile.c_str());
		if (!path.empty()) {
			config_set_string(globalConfig, "Basic", "Profile",
				opt_starting_profile.c_str());
			config_set_string(globalConfig, "Basic", "ProfileDir",
				path.c_str());
		}
	}

	return InitGlobalConfigDefaults();
}

bool BroardcastEngine::InitLocale()
{
	const char *lang = config_get_string(globalConfig, "General",
		"Language");

	locale = lang;

	locale = DEFAULT_LANG;
	return true;
	string englishPath;
	if (!GetDataFilePath("locale/" DEFAULT_LANG ".ini", englishPath)) {
		blog(LOG_ERROR, "Failed to find locale/" DEFAULT_LANG ".ini");
		return false;
	}

	textLookup = text_lookup_create(englishPath.c_str());
	if (!textLookup) {
		blog(LOG_ERROR, "Failed to create locale from file '%s'",
			englishPath.c_str());
		return false;
	}

	bool userLocale = config_has_user_value(globalConfig, "General",
		"Language");
	bool defaultLang = astrcmpi(lang, DEFAULT_LANG) == 0;

	if (userLocale && defaultLang)
		return true;

	if (!userLocale && defaultLang) {
		for (auto &locale_ : GetPreferredLocales()) {
			if (locale_ == lang)
				return true;

			stringstream file;
			file << "locale/" << locale_ << ".ini";

			string path;
			if (!GetDataFilePath(file.str().c_str(), path))
				continue;

			if (!text_lookup_add(textLookup, path.c_str()))
				continue;

			blog(LOG_INFO, "Using preferred locale '%s'",
				locale_.c_str());
			locale = locale_;
			return true;
		}

		return true;
	}

	stringstream file;
	file << "locale/" << lang << ".ini";

	string path;
	if (GetDataFilePath(file.str().c_str(), path)) {
		if (!text_lookup_add(textLookup, path.c_str()))
			blog(LOG_ERROR, "Failed to add locale file '%s'",
			path.c_str());
	}
	else {
		blog(LOG_ERROR, "Could not find locale file '%s'",
			file.str().c_str());
	}

	return true;
}


static BroardcastEngine* self = NULL;
BroardcastEngine* BroardcastEngine::instance()
{
	return self;
}

BroardcastEngine::BroardcastEngine()
{

}

BroardcastEngine::~BroardcastEngine()
{
	delete bcBase;
	obs_shutdown();
}

static void move_basic_to_profiles(void)
{
	char path[512];
	char new_path[512];
	os_glob_t *glob;

	/* if not first time use */
	if (GetConfigPath(path, 512, "obs-studio/basic") <= 0)
		return;
	if (!os_file_exists(path))
		return;

	/* if the profiles directory doesn't already exist */
	if (GetConfigPath(new_path, 512, "obs-studio/basic/profiles") <= 0)
		return;
	if (os_file_exists(new_path))
		return;

	if (os_mkdir(new_path) == MKDIR_ERROR)
		return;

	strcat(new_path, "/");
	strcat(new_path, Str("Untitled"));
	if (os_mkdir(new_path) == MKDIR_ERROR)
		return;

	strcat(path, "/*.*");
	if (os_glob(path, 0, &glob) != 0)
		return;

	strcpy(path, new_path);

	for (size_t i = 0; i < glob->gl_pathc; i++) {
		struct os_globent ent = glob->gl_pathv[i];
		char *file;

		if (ent.directory)
			continue;

		file = strrchr(ent.path, '/');
		if (!file++)
			continue;

		if (astrcmpi(file, "scenes.json") == 0)
			continue;

		strcpy(new_path, path);
		strcat(new_path, "/");
		strcat(new_path, file);
		os_rename(ent.path, new_path);
	}

	os_globfree(glob);
}

static void move_basic_to_scene_collections(void)
{
	char path[512];
	char new_path[512];

	if (GetConfigPath(path, 512, "obs-studio/basic") <= 0)
		return;
	if (!os_file_exists(path))
		return;

	if (GetConfigPath(new_path, 512, "obs-studio/basic/scenes") <= 0)
		return;
	if (os_file_exists(new_path))
		return;

	if (os_mkdir(new_path) == MKDIR_ERROR)
		return;

	strcat(path, "/scenes.json");
	strcat(new_path, "/");
	strcat(new_path, Str("Untitled"));
	strcat(new_path, ".json");

	os_rename(path, new_path);
}

void BroardcastEngine::EngineInit()
{

	if (!InitApplicationBundle())
		throw "Failed to initialize application bundle";
	if (!MakeUserDirs())
		throw "Failed to create required user directories";
	if (!InitGlobalConfig())
		throw "Failed to initialize global config";
	if (!InitLocale())
		throw "Failed to load locale";

	config_set_default_string(globalConfig, "Basic", "Profile",
		Str("Untitled"));
	config_set_default_string(globalConfig, "Basic", "ProfileDir",
		Str("Untitled"));
	config_set_default_string(globalConfig, "Basic", "SceneCollection",
		Str("Untitled"));
	config_set_default_string(globalConfig, "Basic", "SceneCollectionFile",
		Str("Untitled"));


	move_basic_to_profiles();
	move_basic_to_scene_collections();

	if (!MakeUserProfileDirs())
		throw "Failed to create profile directories";
}

const char *BroardcastEngine::GetRenderModule() const
{
	const char *renderer = config_get_string(globalConfig, "Video",
		"Renderer");

	return (astrcmpi(renderer, "Direct3D 11") == 0) ?
	DL_D3D11 : DL_OPENGL;
}

static bool StartupOBS(const char *locale)
{
	char path[512];

	if (GetConfigPath(path, sizeof(path), "obs-studio/plugin_config") <= 0)
		return false;

	return obs_startup(locale, path, NULL);
}

bool BroardcastEngine::OBSInit()
{
	if (!StartupOBS(locale.c_str()))
		return false;

	bcBase = new BroardcastBase();

	bcBase->OBSInit();

	return true;
}

#ifdef __APPLE__
#define INPUT_AUDIO_SOURCE  "coreaudio_input_capture"
#define OUTPUT_AUDIO_SOURCE "coreaudio_output_capture"
#elif _WIN32
#define INPUT_AUDIO_SOURCE  "wasapi_input_capture"
#define OUTPUT_AUDIO_SOURCE "wasapi_output_capture"
#else
#define INPUT_AUDIO_SOURCE  "pulse_input_capture"
#define OUTPUT_AUDIO_SOURCE "pulse_output_capture"
#endif

const char *BroardcastEngine::InputAudioSource() const
{
	return INPUT_AUDIO_SOURCE;
}

const char *BroardcastEngine::OutputAudioSource() const
{
	return OUTPUT_AUDIO_SOURCE;
}


const char *BroardcastEngine::GetCurrentLog() const
{
	return currentLogFile.c_str();
}

string GenerateTimeDateFilename(const char *extension, bool noSpace)
{
	time_t    now = time(0);
	char      file[256] = {};
	struct tm *cur_time;

	cur_time = localtime(&now);
	snprintf(file, sizeof(file), "%d-%02d-%02d%c%02d-%02d-%02d.%s",
		cur_time->tm_year + 1900,
		cur_time->tm_mon + 1,
		cur_time->tm_mday,
		noSpace ? '_' : ' ',
		cur_time->tm_hour,
		cur_time->tm_min,
		cur_time->tm_sec,
		extension);

	return string(file);
}

string GenerateSpecifiedFilename(const char *extension, bool noSpace,
	const char *format)
{
	time_t now = time(0);
	struct tm *cur_time;
	cur_time = localtime(&now);

	const size_t spec_count = 23;
	const char *spec[][2] = {
		{ "%CCYY", "%Y" },
		{ "%YY", "%y" },
		{ "%MM", "%m" },
		{ "%DD", "%d" },
		{ "%hh", "%H" },
		{ "%mm", "%M" },
		{ "%ss", "%S" },
		{ "%%", "%%" },

		{ "%a", "" },
		{ "%A", "" },
		{ "%b", "" },
		{ "%B", "" },
		{ "%d", "" },
		{ "%H", "" },
		{ "%I", "" },
		{ "%m", "" },
		{ "%M", "" },
		{ "%p", "" },
		{ "%S", "" },
		{ "%y", "" },
		{ "%Y", "" },
		{ "%z", "" },
		{ "%Z", "" },
	};

	char convert[128] = {};
	string sf = format;
	string c;
	size_t pos = 0, len;

	while (pos < sf.length()) {
		len = 0;
		for (size_t i = 0; i < spec_count && len == 0; i++) {

			if (sf.find(spec[i][0], pos) == pos) {
				if (strlen(spec[i][1]))
					strftime(convert, sizeof(convert),
					spec[i][1], cur_time);
				else
					strftime(convert, sizeof(convert),
					spec[i][0], cur_time);

				len = strlen(spec[i][0]);

				c = convert;
				if (c.length() && c.find_first_not_of(' ') !=
					std::string::npos)
					sf.replace(pos, len, convert);
			}
		}

		if (len)
			pos += strlen(convert);
		else if (!len && sf.at(pos) == '%')
			sf.erase(pos, 1);
		else
			pos++;
	}

	if (noSpace)
		replace(sf.begin(), sf.end(), ' ', '_');

	sf += '.';
	sf += extension;

	return (sf.length() < 256) ? sf : sf.substr(0, 255);
}

vector<pair<string, string>> GetLocaleNames()
{
	string path;
	if (!GetDataFilePath("locale.ini", path))
		throw "Could not find locale.ini path";

	ConfigFile ini;
	if (ini.Open(path.c_str(), CONFIG_OPEN_EXISTING) != 0)
		throw "Could not open locale.ini";

	size_t sections = config_num_sections(ini);

	vector<pair<string, string>> names;
	names.reserve(sections);
	for (size_t i = 0; i < sections; i++) {
		const char *tag = config_get_section(ini, i);
		const char *name = config_get_string(ini, tag, "Name");
		names.emplace_back(tag, name);
	}

	return names;
}

static void create_log_file(fstream &logFile)
{
	stringstream dst;

	currentLogFile = GenerateTimeDateFilename("txt");
	dst << "obs-studio/logs/" << currentLogFile.c_str();

	BPtr<char> path(GetConfigPathPtr(dst.str().c_str()));
	logFile.open(path,
		ios_base::in | ios_base::out | ios_base::trunc);

	if (logFile.is_open()) {
		base_set_log_handler(do_log, &logFile);
	}
	else {
		blog(LOG_ERROR, "Failed to open log file");
	}
}

static int start_engine()
{
	int ret = -1;
	static fstream logFile;
	if (self) return -1;

	self = new BroardcastEngine();
	try {
		self->EngineInit();
		create_log_file(logFile);

		if (!self->OBSInit())
			return 0;

		return 1;
	}
	catch (const char *error) {
		blog(LOG_ERROR, "%s", error);
	}

	return ret;
}


#ifdef __APPLE__
#define BASE_PATH ".."
#else
#define BASE_PATH "../.."
#endif

#define CONFIG_PATH BASE_PATH "/config"

#ifndef OBS_UNIX_STRUCTURE
#define OBS_UNIX_STRUCTURE 0
#endif

int GetConfigPath(char *path, size_t size, const char *name)
{
	if (!OBS_UNIX_STRUCTURE && portable_mode) {
		if (name && *name) {
			return snprintf(path, size, CONFIG_PATH "/%s", name);
		}
		else {
			return snprintf(path, size, CONFIG_PATH);
		}
	}
	else {
		return os_get_config_path(path, size, name);
	}
}

char *GetConfigPathPtr(const char *name)
{
	if (!OBS_UNIX_STRUCTURE && portable_mode) {
		char path[512];

		if (snprintf(path, sizeof(path), CONFIG_PATH "/%s", name) > 0) {
			return bstrdup(path);
		}
		else {
			return NULL;
		}
	}
	else {
		return os_get_config_path_ptr(name);
	}
}

int GetProgramDataPath(char *path, size_t size, const char *name)
{
	return os_get_program_data_path(path, size, name);
}

char *GetProgramDataPathPtr(const char *name)
{
	return os_get_program_data_path_ptr(name);
}

bool GetFileSafeName(const char *name, std::string &file)
{
	size_t base_len = strlen(name);
	size_t len = os_utf8_to_wcs(name, base_len, nullptr, 0);
	std::wstring wfile;

	if (!len)
		return false;

	wfile.resize(len);
	os_utf8_to_wcs(name, base_len, &wfile[0], len);

	for (size_t i = wfile.size(); i > 0; i--) {
		size_t im1 = i - 1;

		if (iswspace(wfile[im1])) {
			wfile[im1] = '_';
		}
		else if (wfile[im1] != '_' && !iswalnum(wfile[im1])) {
			wfile.erase(im1, 1);
		}
	}

	if (wfile.size() == 0)
		wfile = L"characters_only";

	len = os_wcs_to_utf8(wfile.c_str(), wfile.size(), nullptr, 0);
	if (!len)
		return false;

	file.resize(len);
	os_wcs_to_utf8(wfile.c_str(), wfile.size(), &file[0], len);
	return true;
}

bool GetClosestUnusedFileName(std::string &path, const char *extension)
{
	size_t len = path.size();
	if (extension) {
		path += ".";
		path += extension;
	}

	if (!os_file_exists(path.c_str()))
		return true;

	int index = 1;

	do {
		path.resize(len);
		path += std::to_string(++index);
		if (extension) {
			path += ".";
			path += extension;
		}
	} while (os_file_exists(path.c_str()));

	return true;
}

bool WindowPositionValid(int x, int y)
{
	vector<MonitorInfo> monitors;
	GetMonitors(monitors);

	for (auto &monitor : monitors) {
		int br_x = monitor.x + monitor.cx;
		int br_y = monitor.y + monitor.cy;

		if (x >= monitor.x && x < br_x &&
			y >= monitor.y && y < br_y)
			return true;
	}

	return false;
}

static bool update_ffmpeg_output(ConfigFile &config)
{
	if (config_has_user_value(config, "AdvOut", "FFOutputToFile"))
		return false;

	const char *url = config_get_string(config, "AdvOut", "FFURL");
	if (!url)
		return false;

	bool isActualURL = strstr(url, "://") != nullptr;
	if (isActualURL)
		return false;

	string urlStr = url;
	string extension;

	for (size_t i = urlStr.length(); i > 0; i--) {
		size_t idx = i - 1;

		if (urlStr[idx] == '.') {
			extension = &urlStr[i];
		}

		if (urlStr[idx] == '\\' || urlStr[idx] == '/') {
			urlStr[idx] = 0;
			break;
		}
	}

	if (urlStr.empty() || extension.empty())
		return false;

	config_remove_value(config, "AdvOut", "FFURL");
	config_set_string(config, "AdvOut", "FFFilePath", urlStr.c_str());
	config_set_string(config, "AdvOut", "FFExtension", extension.c_str());
	config_set_bool(config, "AdvOut", "FFOutputToFile", true);
	return true;
}

static bool move_reconnect_settings(ConfigFile &config, const char *sec)
{
	bool changed = false;

	if (config_has_user_value(config, sec, "Reconnect")) {
		bool reconnect = config_get_bool(config, sec, "Reconnect");
		config_set_bool(config, "Output", "Reconnect", reconnect);
		changed = true;
	}
	if (config_has_user_value(config, sec, "RetryDelay")) {
		int delay = (int)config_get_uint(config, sec, "RetryDelay");
		config_set_uint(config, "Output", "RetryDelay", delay);
		changed = true;
	}
	if (config_has_user_value(config, sec, "MaxRetries")) {
		int retries = (int)config_get_uint(config, sec, "MaxRetries");
		config_set_uint(config, "Output", "MaxRetries", retries);
		changed = true;
	}

	return changed;
}

static bool update_reconnect(ConfigFile &config)
{
	if (!config_has_user_value(config, "Output", "Mode"))
		return false;

	const char *mode = config_get_string(config, "Output", "Mode");
	if (!mode)
		return false;

	const char *section = (strcmp(mode, "Advanced") == 0) ?
		"AdvOut" : "SimpleOutput";

	if (move_reconnect_settings(config, section)) {
		config_remove_value(config, "SimpleOutput", "Reconnect");
		config_remove_value(config, "SimpleOutput", "RetryDelay");
		config_remove_value(config, "SimpleOutput", "MaxRetries");
		config_remove_value(config, "AdvOut", "Reconnect");
		config_remove_value(config, "AdvOut", "RetryDelay");
		config_remove_value(config, "AdvOut", "MaxRetries");
		return true;
	}

	return false;
}

static void convert_x264_settings(obs_data_t *data)
{
	bool use_bufsize = obs_data_get_bool(data, "use_bufsize");

	if (use_bufsize) {
		int buffer_size = (int)obs_data_get_int(data, "buffer_size");
		if (buffer_size == 0)
			obs_data_set_string(data, "rate_control", "CRF");
	}
}

static void convert_14_2_encoder_setting(const char *encoder, const char *file)
{
	obs_data_t *data = obs_data_create_from_json_file_safe(file, "bak");
	obs_data_item_t *cbr_item = obs_data_item_byname(data, "cbr");
	obs_data_item_t *rc_item = obs_data_item_byname(data, "rate_control");
	bool modified = false;
	bool cbr = true;

	if (cbr_item) {
		cbr = obs_data_item_get_bool(cbr_item);
		obs_data_item_unset_user_value(cbr_item);

		obs_data_set_string(data, "rate_control", cbr ? "CBR" : "VBR");

		modified = true;
	}

	if (!rc_item && astrcmpi(encoder, "obs_x264") == 0) {
		if (!cbr_item)
			obs_data_set_string(data, "rate_control", "CBR");
		else if (!cbr)
			convert_x264_settings(data);

		modified = true;
	}

	if (modified)
		obs_data_save_json_safe(data, file, "tmp", "bak");

	obs_data_item_release(&rc_item);
	obs_data_item_release(&cbr_item);
	obs_data_release(data);
}

int BroardcastEngine_Init()
{
	curl_global_init(CURL_GLOBAL_ALL);
	int ret = start_engine();
	return ret;
}

int BroardcastEngine_unInit()
{
	delete self;
	self = NULL;
	blog(LOG_INFO, "Number of memory leaks: %ld", bnum_allocs());
	base_set_log_handler(nullptr, nullptr);
	return 0;
}
