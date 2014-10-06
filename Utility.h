/*
        KStack Sample Application - Utility.h
		Copyright (C) 2009 aw.comyr.com
		Distributed under the ZLib license
		http://www.opensource.org/licenses/zlib-license.html
*/

#ifndef UTILITY_H
#define UTILITY_H

#include "stdafx.h"

// forward declaration of required types
typedef struct _CONTEXT CONTEXT;
typedef struct _tagSTACKFRAME64 STACKFRAME64;

// really basic info about a process module or a driver
struct ProcessModule
{
	PVOID base;
	std::wstring path;
	ProcessModule(PVOID base, const std::wstring& path) : base(base), path(path){}
};

// function from the SDK to enable privilege in the process token
BOOL SetProcessPrivilege(HANDLE hProcess, LPCTSTR lpszPrivilege, BOOL bEnablePrivilege);
// Gets the user or kernel modules (depending on 'mode') loaded into hProc
void GetLoadedModules(HANDLE hProc, TraceMode mode, std::vector<ProcessModule>& modules);
// returns the highest possible address for user mode
PVOID GetMaximumAppAddress();
// displays 'error' in a message box, with last error if it's specified
void DisplayError(LPCWSTR error, DWORD lastError = 0);
// Retrieves a handle to the KStack driver
HANDLE GetDriverHandle();
// Copies the KStack driver into the drivers folder, and creates the service
void InstallDriver();
// deletes the service and KStack.sys from the drivers folder
void UninstallDriver();

// simple getprocaddress wrapper
template<class Function>
bool FindFunctionInModule(HMODULE hMod, LPCSTR lpzName, Function& func)
{
	func = reinterpret_cast<Function>(GetProcAddress(hMod, lpzName));
	return !!func;
}

#endif
