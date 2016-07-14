#include "jk-display.hpp"
#include <WinUser.h>
#include <Windows.h>

OBSJKDisplay::OBSJKDisplay(void * hwnd)
{
	surface = hwnd;
	display = NULL;
}

void OBSJKDisplay::CreateDisplay()
{
	if (display || NULL == surface)
		return;
	RECT rect;
	if(::GetWindowRect((HWND)surface, &rect)){
		gs_init_data info = {};
		info.cx = rect.right-rect.left;
		info.cy = rect.bottom-rect.top;
		info.format = GS_RGBA;
		info.zsformat = GS_ZS_NONE;
		info.window.hwnd = surface;

		display = obs_display_create(&info);
	}
}

void OBSJKDisplay::onResizeDisplay()
{
	CreateDisplay();
	RECT rect;
	if (::GetWindowRect((HWND)surface, &rect)&&display){
		int width = rect.right - rect.left;
		int height = rect.bottom - rect.top;
		obs_display_resize(display, width, height);
	}
}

bool OBSJKDisplay::getSize(int &width, int &height)
{
	RECT rect;
	if (::GetWindowRect((HWND)surface, &rect) && display){
		 width = rect.right - rect.left;
		 height = rect.bottom - rect.top;
		 return true;
	}
	return false;
}
