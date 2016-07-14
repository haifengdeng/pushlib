#pragma once

#include <obs.hpp>
#include <windows.h>

class OBSJKDisplay {
	void *surface;
	OBSDisplay display;

	void CreateDisplay();

	void onResizeDisplay();
public:
	OBSJKDisplay(void * hwnd);
	inline obs_display_t *GetDisplay() const {return display;}
};
