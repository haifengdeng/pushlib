#pragma once

#include <obs.hpp>
#include <util/lexer.h>
#include <util/profiler.h>
#include <util/util.hpp>
#include <util/platform.h>
#include <string>
#include <memory>
#include <vector>

#include "BroardcastBase.hpp"

std::string CurrentTimeString();
std::string CurrentDateTimeString();
std::string GenerateTimeDateFilename(const char *extension, bool noSpace = false);
std::string GenerateSpecifiedFilename(const char *extension, bool noSpace,
	const char *format);

struct BaseLexer {
	lexer lex;
public:
	inline BaseLexer() { lexer_init(&lex); }
	inline ~BaseLexer() { lexer_free(&lex); }
	operator lexer*() { return &lex; }
};

class BroardcastEngine
{
private:
	std::string                    locale;
	TextLookup                     textLookup;
	OBSContext                     obsContext;

	ConfigFile                     globalConfig;
	BroardcastBase *               bcBase = nullptr;



	bool InitGlobalConfigDefaults();
	bool InitGlobalConfig();
	bool InitLocale();
public:
	BroardcastEngine();
	~BroardcastEngine();

	void EngineInit();
	bool OBSInit();

	inline const char *GetLocale() const
	{
		return locale.c_str();
	}

	inline lookup_t *GetTextLookup() const { return textLookup; }

	inline const char *GetString(const char *lookupVal) const
	{
		return textLookup.GetString(lookupVal);
	}

	static BroardcastEngine *instance();
	config_t * mainConfig(){ return bcBase->Config(); }
	inline config_t *GlobalConfig() const { return globalConfig; }
	BroardcastBase * getMain() { return bcBase; }
	const char *GetRenderModule() const;
	std::string GetVersionString() const;
	const char *InputAudioSource() const;
	const char *OutputAudioSource() const;

	const char *GetCurrentLog() const;

};

int GetConfigPath(char *path, size_t size, const char *name);
char *GetConfigPathPtr(const char *name);

int GetProgramDataPath(char *path, size_t size, const char *name);
char *GetProgramDataPathPtr(const char *name);

bool GetFileSafeName(const char *name, std::string &file);
bool GetClosestUnusedFileName(std::string &path, const char *extension);

inline BroardcastEngine *  Engine(){ return BroardcastEngine::instance(); }
inline BroardcastBase   *  Engine_main(){ return Engine()->getMain(); }
inline config_t *GetGlobalConfig() { return Engine()->GlobalConfig(); }
std::vector<std::pair<std::string, std::string>> GetLocaleNames();

inline const char *Str(const char *lookup) { return Engine()->GetString(lookup); }