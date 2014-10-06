/*
        KStack Sample Application - stdafx.h
		Copyright (C) 2009 aw.comyr.com
		Distributed under the ZLib license
		http://www.opensource.org/licenses/zlib-license.html
*/

#ifndef STDAFX_H
#define STDAFX_H

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif

#define DBGHELP_TRANSLATE_TCHAR

#include <windows.h>
#include <string>
#include <vector>
#include <iostream>
#include "ScopedData.h"

// which stack trace to get for a thread
enum TraceMode
{
	USER,
	KERNEL
};

#endif
