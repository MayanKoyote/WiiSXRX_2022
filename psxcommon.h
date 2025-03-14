/***************************************************************************
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.           *
 ***************************************************************************/

/*
* This file contains common definitions and includes for all parts of the
* emulator core.
*/

#ifndef __PSXCOMMON_H__
#define __PSXCOMMON_H__

//#include "config.h"

/* System includes */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/param.h>
#include <zlib.h>
//#include <glib.h>

/* Define types */
/*typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef intptr_t sptr;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef uintptr_t uptr;
*/
/* Local includes */
#include "system.h"
#include "coredebug.h"

/* Ryan TODO WTF is this? */
#if defined (__LINUX__) || defined (__MACOSX__) || defined(HW_RVL) || defined(HW_DOL)
#define strnicmp strncasecmp
#endif
#define __inline inline

/* Enables NLS/internationalization if active */
#ifdef ENABLE_NLS

#include <libintl.h>

#undef _
#define _(String) gettext(String)
#ifdef gettext_noop
#  define N_(String) gettext_noop (String)
#else
#  define N_(String) (String)
#endif

#else

#define _(msgid) msgid
#define N_(msgid) msgid

#endif

extern int Log;
void __Log(char *fmt, ...);

#define CYCLE_MULT_DEFAULT 175

typedef struct {
	char Gpu[256];
	char Spu[256];
	char Cdr[256];
	char Pad1[256];
	char Pad2[256];
	char Net[256];
	char Mcd1[256];
	char Mcd2[256];
	char Bios[256];
	char BiosDir[MAXPATHLEN];
	char PluginsDir[MAXPATHLEN];
	char PatchesDir[MAXPATHLEN];
	long Xa;
	long Sio;
	long Mdec;
	long PsxAuto;
	long PsxType;		/* NTSC or PAL */
	long Cdda;
	long AsyncCD;
	long HLE;
	long Cpu;
	long Dbg;
	long PsxOut;
	long SpuIrq;
	long RCntFix;
	long UseNet;
	long VSyncWA;
	long pR3000Fix;
	bool icache_emulation;
	bool DisableStalls;
	bool PreciseExceptions;
	int cycle_multiplier; // 100 for 1.0
	int cycle_multiplier_override;
	int gpu_timing_override;
	s8 GpuListWalking;
	s8 FractionalFramerate; // ~49.75 and ~59.81 instead of 50 and 60
	struct {
		bool cdr_read_timing;
		bool gpu_slow_list_walking;
		bool gpu_busy_hack;
		bool gpu_centering;
		bool fractional_Framerate;
		// variable for see if game has special correction (dwActFixes) autoFix
		unsigned long dwActFixes;
		// variable for see if game uses hacks from Lightrec
		unsigned long lightrec_hacks;
	} hacks;
} PcsxConfig;

extern PcsxConfig Config;

extern char LoadCdBios;
extern int StatesC;
extern int cdOpenCase;
extern int NetOpened;

#define gzfreeze(ptr, size) \
	if (Mode == 1) gzwrite(f, ptr, size); \
	if (Mode == 0) gzread(f, ptr, size);

#define gzfreezel(ptr) gzfreeze(ptr, sizeof(ptr))

//#define BIAS	1
#define BIAS	2
#define PSXCLK	33868800	/* 33.8688 Mhz */

#define PS_SPU_FREQ     44100
#define WII_SPU_FREQ    48000
#define SINC (((u32)1 << 16) * PS_SPU_FREQ / (WII_SPU_FREQ))

enum {
	BIOS_USER_DEFINED,
	BIOS_HLE
};	/* BIOS Types */

enum {
	PSX_TYPE_NTSC,
	PSX_TYPE_PAL
};	/* PSX Type */

// To improve execution efficiency, use built-in instructions lhbrx, lwbrx, sthbrx, stwbrx to accelerate byte swapping
#define LOAD_SWAP16p(ptr) ({u16 __ret, *__ptr=(ptr); __asm__ ("lhbrx %0, 0, %1" : "=r" (__ret) : "r" (__ptr)); __ret;})
#define LOAD_SWAP32p(ptr) ({u32 __ret, *__ptr=(ptr); __asm__ ("lwbrx %0, 0, %1" : "=r" (__ret) : "r" (__ptr)); __ret;})
#define STORE_SWAP16p(ptr,val) ({u16 __val=(val), *__ptr=(ptr); __asm__ ("sthbrx %0, 0, %1" : : "r" (__val), "r" (__ptr) : "memory");})
#define STORE_SWAP32p(ptr,val) ({u32 __val=(val), *__ptr=(ptr); __asm__ ("stwbrx %0, 0, %1" : : "r" (__val), "r" (__ptr) : "memory");})
#define STORE_SWAP32p2(ptr,val) ({u32 __val=(val); __asm__ ("stwbrx %0, 0, %1" : : "r" (__val), "r" (ptr) : "memory");})

#define FIK0(fid)    (FK0[fid])
#define FIK1(fid)    (FK1[fid])

typedef struct {
    f32 IK0;
    f32 IK1;
    u32 vblockp;
    u32 range;
    u32 decpAddr;
    u32 tmpAddr[4];
} typPcmDecode;

#endif /* __PSXCOMMON_H__ */
