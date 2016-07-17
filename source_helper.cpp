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

#include "push-engine.h"
#include "source_helper.hpp"

bool SourceTypeConvertString(InputSourceType type, std::string & id)
{
	switch (type)
	{
	case 	TEXT_SOURCE:
		id = "text_ft2_source";
		break;
	case	IMAGE_FILE_SOURCE:
		id = "image_source";
		break;
	case	MEDIA_FILE_SOURCE:
		id = "ffmpeg_source";
		break;
	case	MONITOR_CAPTURE_SOURCE:
		id = "monitor_capture";
		break;
	case	WINDOW_CAPTURE_SOURCE:
		id = "window_capture";
		break;
	case	VIDEO_CAPTURE_SOURCE:
		id = "dshow_input";
		break;
	case	AUDIO_INPUT_SOURCE:
		id = "wasapi_input_capture";
		break;
	case	AUDIO_OUTPUT_SOURCE:
		id = "wasapi_output_capture";
		break;
	default:
		return false;
	}
	return true;
}