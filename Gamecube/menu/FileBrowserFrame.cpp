/**
 * WiiSX - FileBrowserFrame.cpp
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

#include <math.h>
#include <cstdlib>
#include <strings.h>
#include "MenuContext.h"
#include "FileBrowserFrame.h"
#include "../libgui/Button.h"
#include "../libgui/resources.h"
#include "../libgui/MessageBox.h"
#include "../libgui/FocusManager.h"
#include "../libgui/CursorManager.h"

#include "../../psxcommon.h"

#include "../../HidController/KernelHID.h"

extern "C" {
#include "../fileBrowser/fileBrowser.h"
#include "../fileBrowser/fileBrowser-libfat.h"
#include "../fileBrowser/fileBrowser-DVD.h"
#include "../fileBrowser/fileBrowser-CARD.h"
#include "../fileBrowser/fileBrowser-SMB.h"
#include "../../database.h"
extern long ISOgetTN(unsigned char *buffer);
extern char debugInfo[256];
}

extern "C" char * JoinString(char *s1, char *s2);

void Func_PrevPage();
void Func_NextPage();
void Func_ReturnFromFileBrowserFrame();
void Func_Select1();
void Func_Select2();
void Func_Select3();
void Func_Select4();
void Func_Select5();
void Func_Select6();
void Func_Select7();
void Func_Select8();
void Func_Select9();
void Func_Select10();

#define NUM_FRAME_BUTTONS 12
#define NUM_FILE_SLOTS 10
#define FRAME_BUTTONS fileBrowserFrameButtons
#define FRAME_STRINGS fileBrowserFrameStrings

static char FRAME_STRINGS[3][5] =
	{ "Prev",
	  "Next",
	  ""};


struct ButtonInfo
{
	menu::Button	*button;
	int				buttonStyle;
	char*			buttonString;
	float			x;
	float			y;
	float			width;
	float			height;
	int				focusUp;
	int				focusDown;
	int				focusLeft;
	int				focusRight;
	ButtonFunc		clickedFunc;
	ButtonFunc		returnFunc;
} FRAME_BUTTONS[NUM_FRAME_BUTTONS] =
{ //	button	buttonStyle		buttonString		x		y		width	height	Up	Dwn	Lft	Rt	clickFunc		returnFunc
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[0],	 35.0,	220.0,	 70.0,	40.0,	-1,	-1,	-1,	 2,	Func_PrevPage,	Func_ReturnFromFileBrowserFrame }, // Prev
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[1],	535.0,	220.0,	 70.0,	40.0,	-1,	-1,	 2,	-1,	Func_NextPage,	Func_ReturnFromFileBrowserFrame }, // Next
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[2],	120.0,	 40.0,	400.0,	35.0,	11,	 3,	 0,	 1,	Func_Select1,	Func_ReturnFromFileBrowserFrame }, // File Button 1
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[2],	120.0,	 80.0,	400.0,	35.0,	 2,	 4,	 0,	 1,	Func_Select2,	Func_ReturnFromFileBrowserFrame }, // File Button 2
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[2],	120.0,	120.0,	400.0,	35.0,	 3,	 5,	 0,	 1,	Func_Select3,	Func_ReturnFromFileBrowserFrame }, // File Button 3
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[2],	120.0,	160.0,	400.0,	35.0,	 4,	 6,	 0,	 1,	Func_Select4,	Func_ReturnFromFileBrowserFrame }, // File Button 4
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[2],	120.0,	200.0,	400.0,	35.0,	 5,	 7,	 0,	 1,	Func_Select5,	Func_ReturnFromFileBrowserFrame }, // File Button 5
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[2],	120.0,	240.0,	400.0,	35.0,	 6,	 8,	 0,	 1,	Func_Select6,	Func_ReturnFromFileBrowserFrame }, // File Button 6
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[2],	120.0,	280.0,	400.0,	35.0,	 7,	 9,	 0,	 1,	Func_Select7,	Func_ReturnFromFileBrowserFrame }, // File Button 7
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[2],	120.0,	320.0,	400.0,	35.0,	 8,	10,	 0,	 1,	Func_Select8,	Func_ReturnFromFileBrowserFrame }, // File Button 8
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[2],	120.0,	360.0,	400.0,	35.0,	 9,	11,	 0,	 1,	Func_Select9,	Func_ReturnFromFileBrowserFrame }, // File Button 9
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[2],	120.0,	400.0,	400.0,	35.0,	10,	 2,	 0,	 1,	Func_Select10,	Func_ReturnFromFileBrowserFrame }, // File Button 10
};

FileBrowserFrame::FileBrowserFrame()
{
	for (int i = 0; i < NUM_FRAME_BUTTONS; i++)
		FRAME_BUTTONS[i].button = new menu::Button(FRAME_BUTTONS[i].buttonStyle, &FRAME_BUTTONS[i].buttonString,
										FRAME_BUTTONS[i].x, FRAME_BUTTONS[i].y,
										FRAME_BUTTONS[i].width, FRAME_BUTTONS[i].height);

	for (int i = 0; i < NUM_FRAME_BUTTONS; i++)
	{
		if (FRAME_BUTTONS[i].focusUp != -1) FRAME_BUTTONS[i].button->setNextFocus(menu::Focus::DIRECTION_UP, FRAME_BUTTONS[FRAME_BUTTONS[i].focusUp].button);
		if (FRAME_BUTTONS[i].focusDown != -1) FRAME_BUTTONS[i].button->setNextFocus(menu::Focus::DIRECTION_DOWN, FRAME_BUTTONS[FRAME_BUTTONS[i].focusDown].button);
		if (FRAME_BUTTONS[i].focusLeft != -1) FRAME_BUTTONS[i].button->setNextFocus(menu::Focus::DIRECTION_LEFT, FRAME_BUTTONS[FRAME_BUTTONS[i].focusLeft].button);
		if (FRAME_BUTTONS[i].focusRight != -1) FRAME_BUTTONS[i].button->setNextFocus(menu::Focus::DIRECTION_RIGHT, FRAME_BUTTONS[FRAME_BUTTONS[i].focusRight].button);
		FRAME_BUTTONS[i].button->setActive(true);
		if (FRAME_BUTTONS[i].clickedFunc) FRAME_BUTTONS[i].button->setClicked(FRAME_BUTTONS[i].clickedFunc);
		if (FRAME_BUTTONS[i].returnFunc) FRAME_BUTTONS[i].button->setReturn(FRAME_BUTTONS[i].returnFunc);
		add(FRAME_BUTTONS[i].button);
		menu::Cursor::getInstance().addComponent(this, FRAME_BUTTONS[i].button, FRAME_BUTTONS[i].x,
												FRAME_BUTTONS[i].x+FRAME_BUTTONS[i].width, FRAME_BUTTONS[i].y,
												FRAME_BUTTONS[i].y+FRAME_BUTTONS[i].height);
	}

	for (int i = 2; i < NUM_FRAME_BUTTONS; i++)
	{
		FRAME_BUTTONS[i].button->setLabelMode(menu::Button::LABEL_SCROLLONFOCUS);
		FRAME_BUTTONS[i].button->setLabelScissor(6);
	}

	setDefaultFocus(FRAME_BUTTONS[2].button);
	setBackFunc(Func_ReturnFromFileBrowserFrame);
	setEnabled(true);

}

FileBrowserFrame::~FileBrowserFrame()
{
	for (int i = 0; i < NUM_FRAME_BUTTONS; i++)
	{
		menu::Cursor::getInstance().removeComponent(this, FRAME_BUTTONS[i].button);
		delete FRAME_BUTTONS[i].button;
	}

}

int fileBrowserMode = FileBrowserFrame::FILEBROWSER_LOADISO;

void FileBrowserFrame::activateSubmenu(int submenu)
{
	fileBrowserMode = submenu;
}

static fileBrowser_file* dir_entries;
static int				num_entries;
static int				current_page;
static int				max_page;

void fileBrowserFrame_OpenDirectory(fileBrowser_file* dir);
void fileBrowserFrame_Error(fileBrowser_file* dir, int error_code);
void fileBrowserFrame_FillPage();
void fileBrowserFrame_LoadFile(int i);
static int dir_comparator(const void* _x, const void* _y);

void FileBrowserFrame::drawChildren(menu::Graphics &gfx)
{
	if(isVisible())
	{
#ifdef HW_RVL
		WPADData* wiiPad = menu::Input::getInstance().getWpad();
		PADStatus* hidGcPad = (PADStatus*)(HID_MEM2_PAD_BUFF); //PadBuff
#endif
		for (int i=0; i<4; i++)
		{
			u16 currentButtonsGC = PAD_ButtonsHeld(i);
			if (currentButtonsGC ^ previousButtonsGC[i])
			{
				u16 currentButtonsDownGC = (currentButtonsGC ^ previousButtonsGC[i]) & currentButtonsGC;
				previousButtonsGC[i] = currentButtonsGC;
				if (currentButtonsDownGC & PAD_TRIGGER_Z)
				{
					// Change sort method
					fileSortMode ^= 1;
					// Resort the list
					qsort(dir_entries, num_entries, sizeof(fileBrowser_file), dir_comparator);
					current_page = 0;
					fileBrowserFrame_FillPage();
					menu::Focus::getInstance().clearPrimaryFocus();
					break;
				}
				else if (currentButtonsDownGC & PAD_TRIGGER_R)
				{
					//move to next set & return
					current_page = (current_page + 1) % max_page;
					fileBrowserFrame_FillPage();
					menu::Focus::getInstance().clearPrimaryFocus();
					break;
				}
				else if (currentButtonsDownGC & PAD_TRIGGER_L)
				{
					//move to the previous set & return
					current_page = (max_page + current_page - 1) % max_page;
					fileBrowserFrame_FillPage();
					menu::Focus::getInstance().clearPrimaryFocus();
					break;
				}
			}
#ifdef HW_RVL
			else if (wiiPad[i].btns_h ^ previousButtonsWii[i])
			{
				u32 currentButtonsDownWii = (wiiPad[i].btns_h ^ previousButtonsWii[i]) & wiiPad[i].btns_h;
				previousButtonsWii[i] = wiiPad[i].btns_h;
				if (wiiPad[i].exp.type == WPAD_EXP_CLASSIC)
				{
					if (currentButtonsDownWii & WPAD_CLASSIC_BUTTON_ZR)
					{
						// Change sort method
						fileSortMode ^= 1;
						// Resort the list
						qsort(dir_entries, num_entries, sizeof(fileBrowser_file), dir_comparator);
						current_page = 0;
						fileBrowserFrame_FillPage();
						menu::Focus::getInstance().clearPrimaryFocus();
						break;
					}
					else if (currentButtonsDownWii & WPAD_CLASSIC_BUTTON_FULL_R)
					{
						//move to next set & return
						current_page = (current_page + 1) % max_page;
						fileBrowserFrame_FillPage();
						menu::Focus::getInstance().clearPrimaryFocus();
						break;
					}
					else if (currentButtonsDownWii & WPAD_CLASSIC_BUTTON_FULL_L)
					{
						//move to the previous set & return
						current_page = (max_page + current_page - 1) % max_page;
						fileBrowserFrame_FillPage();
						menu::Focus::getInstance().clearPrimaryFocus();
						break;
					}
				}
				else
				{
					if (currentButtonsDownWii & WPAD_BUTTON_1)
					{
						// Change sort method
						fileSortMode ^= 1;
						// Resort the list
						qsort(dir_entries, num_entries, sizeof(fileBrowser_file), dir_comparator);
						current_page = 0;
						fileBrowserFrame_FillPage();
						menu::Focus::getInstance().clearPrimaryFocus();
						break;
					}
					else if (currentButtonsDownWii & WPAD_BUTTON_PLUS)
					{
						//move to next set & return
						current_page = (current_page + 1) % max_page;
						fileBrowserFrame_FillPage();
						menu::Focus::getInstance().clearPrimaryFocus();
						break;
					}
					else if (currentButtonsDownWii & WPAD_BUTTON_MINUS)
					{
						//move to the previous set & return
						current_page = (max_page + current_page - 1) % max_page;
						fileBrowserFrame_FillPage();
						menu::Focus::getInstance().clearPrimaryFocus();
						break;
					}
				}
			}
			else if (hidControllerConnected && (hidGcPad[i].button ^ previousButtonsGCHid[i]))
			{
				previousButtonsGCHid[i] = hidGcPad[i].button;
				if (hidGcPad[i].button & PAD_TRIGGER_R1)
				{
					// Change sort method
					fileSortMode ^= 1;
					// Resort the list
					qsort(dir_entries, num_entries, sizeof(fileBrowser_file), dir_comparator);
					current_page = 0;
					fileBrowserFrame_FillPage();
					menu::Focus::getInstance().clearPrimaryFocus();
					break;
				}
				else if (hidGcPad[i].button & PAD_TRIGGER_R)
				{
					//move to next set & return
					current_page = (current_page + 1) % max_page;
					fileBrowserFrame_FillPage();
					menu::Focus::getInstance().clearPrimaryFocus();
					break;
				}
				else if (hidGcPad[i].button & PAD_TRIGGER_L)
				{
					//move to the previous set & return
					current_page = (max_page + current_page - 1) % max_page;
					fileBrowserFrame_FillPage();
					menu::Focus::getInstance().clearPrimaryFocus();
					break;
				}
			}
#endif //HW_RVL
		}

		//Draw buttons
		menu::ComponentList::const_iterator iteration;
		for (iteration = componentList.begin(); iteration != componentList.end(); ++iteration)
		{
			(*iteration)->draw(gfx);
		}
	}
}

void Func_PrevPage()
{
	if(current_page > 0) current_page -= 1;
	fileBrowserFrame_FillPage();
}

void Func_NextPage()
{
	if(current_page+1 < max_page) current_page +=1;
	fileBrowserFrame_FillPage();
}

extern MenuContext *pMenuContext;

void Func_ReturnFromFileBrowserFrame()
{
	pMenuContext->setActiveFrame(MenuContext::FRAME_MAIN);
}

void Func_Select1() { fileBrowserFrame_LoadFile((current_page*NUM_FILE_SLOTS) + 0); }

void Func_Select2() { fileBrowserFrame_LoadFile((current_page*NUM_FILE_SLOTS) + 1); }

void Func_Select3() { fileBrowserFrame_LoadFile((current_page*NUM_FILE_SLOTS) + 2); }

void Func_Select4() { fileBrowserFrame_LoadFile((current_page*NUM_FILE_SLOTS) + 3); }

void Func_Select5() { fileBrowserFrame_LoadFile((current_page*NUM_FILE_SLOTS) + 4); }

void Func_Select6() { fileBrowserFrame_LoadFile((current_page*NUM_FILE_SLOTS) + 5); }

void Func_Select7() { fileBrowserFrame_LoadFile((current_page*NUM_FILE_SLOTS) + 6); }

void Func_Select8() { fileBrowserFrame_LoadFile((current_page*NUM_FILE_SLOTS) + 7); }

void Func_Select9() { fileBrowserFrame_LoadFile((current_page*NUM_FILE_SLOTS) + 8); }

void Func_Select10() { fileBrowserFrame_LoadFile((current_page*NUM_FILE_SLOTS) + 9); }


char* filenameFromAbsPath(char* absPath)
{
	char* filename = absPath;
	// Here we want to extract from the absolute path
	//   just the filename
	// First we move the pointer all the way to the end
	//   of the the string so we can work our way back
	while( *filename ) ++filename;
	// Now, just move it back to the last '/' or the start
	//   of the string
	while( filename != absPath && (*(filename-1) != '\\' && *(filename-1) != '/'))
		--filename;
	return filename;
}

int loadISO(fileBrowser_file*);
int loadISOSwap(fileBrowser_file*);

static int dir_comparator(const void* _x, const void* _y){
	const fileBrowser_file* x = (const fileBrowser_file*)_x;
	const fileBrowser_file* y = (const fileBrowser_file*)_y;
	int xIsDir = x->attr & FILE_BROWSER_ATTR_DIR;
	int yIsDir = y->attr & FILE_BROWSER_ATTR_DIR;
	// Directories go on top, otherwise alphabetical
	if(fileSortMode && xIsDir != yIsDir)
		return yIsDir - xIsDir;
	else
		return strcasecmp(x->name, y->name);
}

void fileBrowserFrame_OpenDirectory(fileBrowser_file* dir)
{
	// Free the old menu stuff
//	if(menu_items){  free(menu_items);  menu_items  = NULL; }
	if(dir_entries){ free(dir_entries); dir_entries = NULL; }

	// Read the directories and return on error
	num_entries = isoFile_readDir(dir, &dir_entries);
	if(num_entries <= 0)
	{
		if(dir_entries) { free(dir_entries); dir_entries = NULL; }
		fileBrowserFrame_Error(dir, num_entries);
		return;
	}

	// Sort the listing
	qsort(dir_entries, num_entries, sizeof(fileBrowser_file), dir_comparator);

	current_page = 0;
	max_page = (int)ceil((float)num_entries/NUM_FILE_SLOTS);
	fileBrowserFrame_FillPage();
}

void fileBrowserFrame_Error(fileBrowser_file* dir, int error_code)
{
	char feedback_string[256];
	//disable all buttons
	for (int i = 0; i < NUM_FRAME_BUTTONS; i++)
		FRAME_BUTTONS[i].button->setActive(false);
	for (int i = 0; i<NUM_FILE_SLOTS; i++)
		FRAME_BUTTONS[i+2].buttonString = FRAME_STRINGS[2];
	if(error_code == SMB_NETINITERR) {
  	sprintf(feedback_string,"Network not yet initialized");
	}
	else if(error_code == SMB_SMBCFGERR) {
  	sprintf(feedback_string,"SMB not configured");
	}
	else if(error_code == SMB_SMBERR) {
  	sprintf(feedback_string,"SMB failed to connect");
	}
	//set first entry to read 'error' and return to main menu
	else if(dir->name)
	  sprintf(feedback_string,"Error opening directory \"%s\"",&dir->name[0]);
	else
	  strcpy(feedback_string,"An error occured");
/*	FRAME_BUTTONS[2].buttonString = feedback_string;
	FRAME_BUTTONS[2].button->setClicked(Func_ReturnFromFileBrowserFrame);
	FRAME_BUTTONS[2].button->setActive(true);
	for (int i = 1; i<NUM_FILE_SLOTS; i++)
		FRAME_BUTTONS[i+2].buttonString = FRAME_STRINGS[2];
	FRAME_BUTTONS[2].button->setNextFocus(menu::Focus::DIRECTION_UP, NULL);
	FRAME_BUTTONS[2].button->setNextFocus(menu::Focus::DIRECTION_DOWN, NULL);
	pMenuContext->getFrame(MenuContext::FRAME_FILEBROWSER)->setDefaultFocus(FRAME_BUTTONS[2].button);
	menu::Focus::getInstance().clearPrimaryFocus();*/
	menu::MessageBox::getInstance().setMessage(feedback_string);
	pMenuContext->setActiveFrame(MenuContext::FRAME_MAIN);
}

void fileBrowserFrame_FillPage()
{
	//Restore next focus directions for top item in list
	FRAME_BUTTONS[2].button->setNextFocus(menu::Focus::DIRECTION_UP, FRAME_BUTTONS[FRAME_BUTTONS[2].focusUp].button);
	FRAME_BUTTONS[2].button->setNextFocus(menu::Focus::DIRECTION_DOWN, FRAME_BUTTONS[FRAME_BUTTONS[2].focusDown].button);

	//disable all buttons
	for (int i = 0; i < NUM_FRAME_BUTTONS; i++)
	{
		FRAME_BUTTONS[i].button->setActive(false);
		FRAME_BUTTONS[i].button->setLabelColor((GXColor) {255,255,255,255});
	}
	//set entries according to page
	for (int i = 0; i < NUM_FILE_SLOTS; i++)
	{
		if ((current_page*NUM_FILE_SLOTS) + i < num_entries)
		{
			FRAME_BUTTONS[i+2].buttonString = filenameFromAbsPath(dir_entries[i+(current_page*NUM_FILE_SLOTS)].name);
			FRAME_BUTTONS[i+2].button->setClicked(FRAME_BUTTONS[i+2].clickedFunc);
			FRAME_BUTTONS[i+2].button->setActive(true);
			if(dir_entries[i+(current_page*NUM_FILE_SLOTS)].attr & FILE_BROWSER_ATTR_DIR)
				FRAME_BUTTONS[i+2].button->setLabelColor((GXColor) {254,213,95,255});
		}
		else
			FRAME_BUTTONS[i+2].buttonString = FRAME_STRINGS[2];
	}
	if (!FRAME_BUTTONS[3].button->getActive())
	{ //NULL out up/down focus if there's only one item in list
		FRAME_BUTTONS[2].button->setNextFocus(menu::Focus::DIRECTION_UP, NULL);
		FRAME_BUTTONS[2].button->setNextFocus(menu::Focus::DIRECTION_DOWN, NULL);
	}
	//activate next/prev buttons
	if (current_page > 0) FRAME_BUTTONS[0].button->setActive(true);
	if (current_page+1 < max_page) FRAME_BUTTONS[1].button->setActive(true);
	//set default focus past "." and ".." entries
	int default_index = 0;
	while ( (!strcmp(filenameFromAbsPath(dir_entries[default_index+(current_page*NUM_FILE_SLOTS)].name),".") ||
			 !strcmp(filenameFromAbsPath(dir_entries[default_index+(current_page*NUM_FILE_SLOTS)].name),"..")) &&
			 num_entries > default_index+1 ) default_index++;
	pMenuContext->getFrame(MenuContext::FRAME_FILEBROWSER)->setDefaultFocus(FRAME_BUTTONS[default_index+2].button);
}

extern BOOL hasLoadedISO;
extern char CdromId[10];
extern char CdromLabel[33];
extern signed char autoSaveLoaded;
void Func_SetPlayGame();
void Func_PlayGame();
extern "C" {
void newCD(fileBrowser_file *file);
}

// For special game correction
extern unsigned long dwActFixes;

// Display bios name
extern char biosFileName[16];

static void CheckGameR3000AutoFix(void)
{
    Config.pR3000Fix = 0;
    if (Config.Cpu != DYNACORE_DYNAREC_OLD)
    {
        return;
    }

    int autoFixR3000Len = 8;
    char autoFixR3000JR[autoFixR3000Len][10] = {
         "SLES00037" // Alone in the Dark - Jack is Back
        ,"SLPS00141"
        ,"SLUS00239"

        ,"SLUS00590" // Need for Speed - V-Rally
        ,"SLES00250"
        ,"SLPS01149"
        ,"SLPS91099"
        ,"SLPS91430"
    };

    int i;
    for (i = 0; i < autoFixR3000Len; i++)
    {
        if (strcmp(CdromId, autoFixR3000JR[i]) == 0) {
            Config.pR3000Fix = 1;
            break;
        }
    }

    autoFixR3000Len = 2;
    char autoFixR3000LW[autoFixR3000Len][10] = {
         "SLUS01005" // Supercross 2000
        ,"SLES02373"
    };
    for (i = 0; i < autoFixR3000Len; i++)
    {
        if (strcmp(CdromId, autoFixR3000LW[i]) == 0) {
            Config.pR3000Fix = 2;
            break;
        }
    }

    autoFixR3000Len = 2;
    char autoFixR3000SRA[autoFixR3000Len][10] = {
         // Hot Wheels Turbo Racing
         "SLUS00964" // for NTSC-U,
        ,"SLES02198" // for PAL
    };
    for (i = 0; i < autoFixR3000Len; i++)
    {
        if (strcmp(CdromId, autoFixR3000SRA[i]) == 0) {
            Config.pR3000Fix = 3;
            break;
        }
    }

    autoFixR3000Len = 2;
    char autoFixR3000SLL[autoFixR3000Len][10] = {
         // Blast chamber
         "SLUS00219" // for NTSC,
        ,"SLES00476" // for PAL
    };
    for (i = 0; i < autoFixR3000Len; i++)
    {
        if (strcmp(CdromId, autoFixR3000SLL[i]) == 0) {
            Config.pR3000Fix = 4;
            break;
        }
    }
}

void fileBrowserFrame_LoadFile(int i)
{
	char feedback_string[256] = "Failed to load ISO";
	if(dir_entries[i].attr & FILE_BROWSER_ATTR_DIR){
		// Here we are 'recursing' into a subdirectory
		// We have to do a little dance here to avoid a dangling pointer
		fileBrowser_file* dir = (fileBrowser_file*)malloc( sizeof(fileBrowser_file) );
		memcpy(dir, dir_entries+i, sizeof(fileBrowser_file));
		fileBrowserFrame_OpenDirectory(dir);
		free(dir);
		menu::Focus::getInstance().clearPrimaryFocus();
	} else if (fileBrowserMode == FileBrowserFrame::FILEBROWSER_LOADISO) {
		// We must select this file
		int ret = loadISO( &dir_entries[i] );

		if(!ret){	// If the read succeeded.
			if(Autoboot){
				// Switches for painting textured quads as 2 triangles (small glitches, but better shading!)
                // This function has been automatically started in soft.c and dwActFixes have been determined in gpu code, so need to set it here
                Config.hacks.dwActFixes |= AUTO_FIX_QUADS_AS_2TRIANGLES;

                CheckGameR3000AutoFix(); // call to check pR3000A autoFix (Supercross 2000, etc.)

				// FIXME: The MessageBox is a hacky way to fix input not responding.
				// No time to improve this...
				menu::MessageBox::getInstance().setMessage("Autobooting game...");
				Func_SetPlayGame();
				Func_PlayGame();
				pMenuContext->setActiveFrame(MenuContext::FRAME_MAIN);
				Autoboot = false;
				return;
			}

			strcpy(feedback_string, "Loaded ");
			strcat(feedback_string, filenameFromAbsPath(dir_entries[i].name));

			char RomInfo[512] = "";
			char buffer [50];
			strcat(RomInfo,feedback_string);
			sprintf(buffer,"\nCD-ROM Label: %s\n",CdromLabel);
			strcat(RomInfo,buffer);
			sprintf(buffer,"CD-ROM ID: %s\n", CdromId);
			strcat(RomInfo,buffer);
            if (check_unsatisfied_libcrypt())
            {
                sprintf(buffer, "LibCrypt protected game with missing SBI\n");
                strcat(RomInfo,buffer);
            }
            if (Config.hacks.gpu_slow_list_walking)
            {
                sprintf(buffer, "GpuSlowListWalking auto fixed\n");
                strcat(RomInfo,buffer);
            }
            if (Config.cycle_multiplier_override)
            {
                sprintf(buffer, "CycleMultiplierOverride fixed\n");
                strcat(RomInfo,buffer);
            }
            if (Config.hacks.gpu_busy_hack)
            {
                sprintf(buffer, "GPU 'Fake Busy States' hacked\n");
                strcat(RomInfo,buffer);
            }
            if (Config.hacks.dwActFixes)
            {
                sprintf(buffer, "Special game auto fixed %08x\n", Config.hacks.dwActFixes);
                strcat(RomInfo,buffer);
            }
            if (Config.hacks.lightrec_hacks)
            {
                sprintf(buffer, "Applied Lightrec hacks\n");
                strcat(RomInfo,buffer);
            }
            // Switches for painting textured quads as 2 triangles (small glitches, but better shading!)
            // This function has been automatically started in soft.c and dwActFixes have been determined in gpu code, so need to set it here
            Config.hacks.dwActFixes |= AUTO_FIX_QUADS_AS_2TRIANGLES;

            // auto recJR => psxJR for some game
            CheckGameR3000AutoFix();
            if (Config.pR3000Fix)
            {
                sprintf(buffer, "pR3000 auto fixed\n");
                strcat(RomInfo,buffer);
            }

			sprintf(buffer,"ISO Size: %u Mb\n",isoFile.size/1024/1024);
			strcat(RomInfo,buffer);
			sprintf(buffer,"Country: %s\n",forceNTSC == FORCENTSC_ENABLE ? "FORCE NTSC" : ((!Config.PsxType) ? "NTSC":"PAL"));
			strcat(RomInfo,buffer);
			sprintf(buffer,"BIOS: %s\n",(Config.HLE==BIOS_USER_DEFINED) ? "PSX":"HLE");
			strcat(RomInfo,buffer);
			sprintf(buffer, "BIOS Name: %s\n", biosFileName);
			strcat(RomInfo, buffer);
			unsigned char tracks[2];
            ISOgetTN(&tracks[0]);
            sprintf(buffer,"Number of tracks %u\n", tracks[1]);
			strcat(RomInfo,buffer);

			switch (autoSaveLoaded)
			{
			case NATIVESAVEDEVICE_NONE:
				break;
			case NATIVESAVEDEVICE_SD:
				strcat(RomInfo,"\nFound & loaded save from SD card\n");
				break;
			case NATIVESAVEDEVICE_USB:
				strcat(RomInfo,"\nFound & loaded save from USB device\n");
				break;
			case NATIVESAVEDEVICE_CARDA:
				strcat(RomInfo,"\nFound & loaded save from memcard in slot A\n");
				break;
			case NATIVESAVEDEVICE_CARDB:
				strcat(RomInfo,"\nFound & loaded save from memcard in slot B\n");
				break;
			}
			autoSaveLoaded = NATIVESAVEDEVICE_NONE;

			menu::MessageBox::getInstance().setMessage(RomInfo);
		}
		else		// If not.
		{
/*  		switch(ret) {
    		case ROM_CACHE_ERROR_READ:
			    strcpy(feedback_string,"A read error occured");
			    break;
			  case ROM_CACHE_INVALID_ROM:
			   strcpy(feedback_string,"Invalid ROM type");
			    break;
			  default:
			    strcpy(feedback_string,"An error has occured");
			    break;
		  }
*/
			menu::MessageBox::getInstance().setMessage(feedback_string);
		}

/*		//disable all buttons
		for (int i = 0; i < NUM_FRAME_BUTTONS; i++)
			FRAME_BUTTONS[i].button->setActive(false);
		//set first entry to report 'success'/'error' and return to main menu
		FRAME_BUTTONS[2].buttonString = feedback_string;
		FRAME_BUTTONS[2].button->setClicked(Func_ReturnFromFileBrowserFrame);
		FRAME_BUTTONS[2].button->setActive(true);
		FRAME_BUTTONS[2].button->setNextFocus(menu::Focus::DIRECTION_UP, NULL);
		FRAME_BUTTONS[2].button->setNextFocus(menu::Focus::DIRECTION_DOWN, NULL);
		for (int i = 1; i<NUM_FILE_SLOTS; i++)
			FRAME_BUTTONS[i+2].buttonString = FRAME_STRINGS[2];
		pMenuContext->getFrame(MenuContext::FRAME_FILEBROWSER)->setDefaultFocus(FRAME_BUTTONS[2].button);
		menu::Focus::getInstance().clearPrimaryFocus();*/

		pMenuContext->setActiveFrame(MenuContext::FRAME_MAIN);
		//if(hasLoadedISO) Func_SetPlayGame();
		Func_SetPlayGame(); //hasLoadedISO will be set to False if SysInit() fails
	}
	else if (fileBrowserMode == FileBrowserFrame::FILEBROWSER_SWAPCD) {
		//TODO: Properly implement this
		int ret = loadISOSwap( &dir_entries[i] );
		if(!ret) {
		  menu::MessageBox::getInstance().setMessage("Swapped CD");
	  } else {
  	  menu::MessageBox::getInstance().setMessage("Error swapping CD");
	  }
	  pMenuContext->setActiveFrame(MenuContext::FRAME_MAIN);
	}
}

void fileBrowserFrame_AutoBootFile()
{
	int i;
	for(i = 0; i < num_entries - 1; i++)
		if(strcasestr(dir_entries[i].name, AutobootROM) != NULL)
			break;
	fileBrowserFrame_LoadFile(i);
}
