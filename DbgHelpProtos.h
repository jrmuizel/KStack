/*
        KStack Sample Application - DbgHelpProtos.h
		Copyright (C) 2009 aw.comyr.com
		Distributed under the ZLib license
		http://www.opensource.org/licenses/zlib-license.html
*/

#ifndef DBGHELPPROTOS_H
#define DBGHELPPROTOS_H

#pragma once

#include "stdafx.h"
#include <dbghelp.h>

// SymLoadModuleExW
typedef DWORD64 (WINAPI*SymLoadMod)(HANDLE, HANDLE, PCWSTR, PCWSTR, DWORD64, DWORD, PMODLOAD_DATA, DWORD);
// SymFromAddrW
typedef BOOL (WINAPI*SymAtAddr)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFOW);
// SymGetModuleBase64W
typedef BOOL (WINAPI*SymModuleInfo)(HANDLE, DWORD64, PIMAGEHLP_MODULE64);
// SymInitializeW
typedef BOOL (WINAPI*SymInit)(HANDLE, LPCWSTR, BOOL);
// SymCleanup
typedef BOOL(WINAPI*SymClose)(HANDLE);
// StackWalk64
typedef BOOL (WINAPI*SymWalkStack)(UINT, HANDLE, HANDLE, STACKFRAME64*, CONTEXT*, 
		PREAD_PROCESS_MEMORY_ROUTINE64, PFUNCTION_TABLE_ACCESS_ROUTINE64,
		PGET_MODULE_BASE_ROUTINE64, PTRANSLATE_ADDRESS_ROUTINE64);
// SymFunctionTableAccess64
typedef PVOID (WINAPI*SymFuncTable)(HANDLE, DWORD64);
// SymGetModuleBaseW64
typedef DWORD64 (WINAPI*SymModBase)(HANDLE, DWORD64);
// SymRegisterCallbackProc64
typedef BOOL (WINAPI*SymReg)(HANDLE, PSYMBOL_REGISTERED_CALLBACK64, ULONG64);
// SymGetModuleInfo64
typedef BOOL (WINAPI*SymModuleInfo)(HANDLE, DWORD64, PIMAGEHLP_MODULE64);
// SymSetOptions
typedef DWORD (WINAPI*SymOptions)(DWORD);

#endif
