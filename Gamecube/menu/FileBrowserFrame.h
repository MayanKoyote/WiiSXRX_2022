/**
 * WiiSX - FileBrowserFrame.h
 * Copyright (C) 2009, 2010 sepp256
 *
 * WiiSX homepage: http://www.emulatemii.com
 * email address: sepp256@gmail.com
 *
 *
 * This program is free software; you can redistribute it and/
 * or modify it under the terms of the GNU General Public Li-
 * cence as published by the Free Software Foundation; either
 * version 2 of the Licence, or any later version.
 *
 * This program is distributed in the hope that it will be use-
 * ful, but WITHOUT ANY WARRANTY; without even the implied war-
 * ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public Licence for more details.
 *
**/

#ifndef FILEBROWSERFRAME_H
#define FILEBROWSERFRAME_H

#include "../libgui/Frame.h"
#include "MenuTypes.h"

class FileBrowserFrame : public menu::Frame
{
public:
	FileBrowserFrame();
	~FileBrowserFrame();
	void drawChildren(menu::Graphics& gfx);
	void activateSubmenu(int submenu);

	enum FilebrowserMode
	{
		FILEBROWSER_LOADISO=1,
		FILEBROWSER_SWAPCD,

	};

private:
	u16 previousButtonsGC[4];
	u32 previousButtonsWii[4];
	u32 previousButtonsGCHid[4];

};

void fileBrowserFrame_AutoBootFile();

#endif
