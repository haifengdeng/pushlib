#pragma once

#include "obs.hpp"
#include "util\util.hpp"
#include "BroardcastBase.hpp"
class BroardcastEngine
{
public:
	BroardcastEngine();
	~BroardcastEngine();

	ConfigFile                     globalConfig;
	BroardcastBase *               bcBase;

	static BroardcastEngine *instance();
	config_t * mainConfig(){ return bcBase->Config(); }
	inline config_t *GlobalConfig() const { return globalConfig; }
	BroardcastBase * getMain() { return bcBase; }
};

int GetConfigPath(char *path, size_t size, const char *name);
inline BroardcastEngine *  Engine(){ return BroardcastEngine::instance(); }
inline BroardcastBase   *  Engine_main(){ return Engine()->getMain(); }
inline config_t *GetGlobalConfig() { return Engine()->GlobalConfig(); }