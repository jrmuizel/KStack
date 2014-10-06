/*
        KStack Sample Application - Utility.cpp
		Copyright (C) 2009 aw.comyr.com
		Distributed under the ZLib license
		http://www.opensource.org/licenses/zlib-license.html
*/

#include "stdafx.h"
#include <dbghelp.h>
#include <psapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <cctype>
#include <sstream>
#include <iostream>
#include <functional>
#include <algorithm>
#include "Utility.h"

// slighly modified function from MSDN
// http://msdn.microsoft.com/en-us/library/aa446619(VS.85).aspx
BOOL SetProcessPrivilege(HANDLE hProcess, LPCTSTR lpszPrivilege, BOOL bEnablePrivilege)
{
	HANDLE hToken = NULL;
	TOKEN_PRIVILEGES tp;
	LUID luid;

	if(!OpenProcessToken(hProcess, TOKEN_ADJUST_PRIVILEGES, &hToken))
	{
		return FALSE;
	}
	// make sure token gets closed
	ScopedData<> hProcToken(hToken, &CloseHandle);
	if ( !LookupPrivilegeValue( 
		NULL,            // lookup privilege on local system
		lpszPrivilege,   // privilege to lookup 
		&luid ) )        // receives LUID of privilege
	{
		return FALSE; 
	}

	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	if (bEnablePrivilege)
	{
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	}
	else
	{
		tp.Privileges[0].Attributes = 0;
	}

	// Enable the privilege or disable all privileges.

	if ( !AdjustTokenPrivileges(
		hToken, 
		FALSE, 
		&tp, 
		sizeof(TOKEN_PRIVILEGES), 
		(PTOKEN_PRIVILEGES) NULL, 
		(PDWORD) NULL) )
	{ 
		return FALSE; 
	} 

	return GetLastError() != ERROR_NOT_ALL_ASSIGNED;
}

// returns the path to the windows directory
std::wstring GetWindowsDirectoryString()
{
	std::wstring winDir(GetSystemWindowsDirectory(NULL, 0), 0);
	GetSystemWindowsDirectory(&winDir[0], static_cast<UINT>(winDir.size()));
	// remove the terminating null
	winDir.resize(winDir.size() - 1);
	// lowercase it
	//std::transform(winDir.begin(), winDir.end(), winDir.begin(), std::ptr_fun(towlower));
	return winDir;
}

// converts a NT Path to a more familiar (and user mode friendly) path
// we can pass to createfile
std::wstring NTPathToDosPath(LPCWSTR ntPath)
{
	std::wstring dosPath;
	static const WCHAR backSlash = L'\\';
	dosPath.reserve(MAX_PATH);
	// if it's a relative path
	// add on the path to the drivers directory
	if(PathIsRelativeW(ntPath))
	{
		static const WCHAR driverString[] = L"drivers/";
		WCHAR buf[MAX_PATH] = {0};
		GetSystemDirectory(buf, MAX_PATH - (ARRAYSIZE(driverString) + 1));
		PathAppend(buf, driverString);
		dosPath = buf;
		dosPath += ntPath;
	}
	else
	{
		// otherwise check for various prefixes
		static const WCHAR* sysRoot = L"\\systemroot\\";
		const std::wstring& windir = GetWindowsDirectoryString();
		// find the first slash on the windir path 
		const WCHAR* startOfWindirPath = PathSkipRoot(windir.c_str()) - 1;
		static const WCHAR* dosDevSymLink = L"\\??\\";
		// lowercase the input string
		std::transform(ntPath, ntPath + wcslen(ntPath) + 1, std::back_inserter(dosPath), std::ptr_fun(towlower));
		std::wstring::size_type pos = 0;
		// if the prefix is systemroot
		if((pos = dosPath.find(sysRoot)) != std::wstring::npos)
		{
			// replace the first and last slashes with percent signs
			dosPath[0] = dosPath[11] = L'%';
			// add in a backslash after the last percent
			dosPath.insert(12, 1, backSlash);
			// expand the systemroot environment string
			std::vector<WCHAR> temp(ExpandEnvironmentStrings(dosPath.c_str(), NULL, 0));
			ExpandEnvironmentStrings(dosPath.c_str(), &temp[0], temp.size());
			temp.pop_back(); // remove the terminating NULL
			dosPath.assign(temp.begin(), temp.end());
		}
		// if the prefix is the dos device symlink, just remove it
		else if((pos = dosPath.find(dosDevSymLink)) != std::wstring::npos)
		{
			dosPath.erase(0, 4);
		}
		// if the prefix is the start of the windows path
		// add on the drive
		else if((pos = dosPath.find(startOfWindirPath)) != std::wstring::npos)
		{
			// insert the drive and the colon
			dosPath.insert(0, windir, 0, 2);
		}
		// else leave it alone
		else __assume(0);
	}
	return dosPath;
}

void GetUserModulePaths(HANDLE hProc, HMODULE* modules, DWORD count, std::vector<ProcessModule>& procModules)
{
	for(DWORD i = 0; i < count; ++i)
	{
		WCHAR modPath[MAX_PATH] = {0};
		GetModuleFileNameEx(hProc, modules[i], modPath, MAX_PATH);
		std::wstring truePath = modPath;
		// certain processes have exes that are loaded with a relative path
		// (smss and csrss) seem to be the main culprits
		if(modPath[0] == L'\\')
		{
			truePath = NTPathToDosPath(truePath.c_str());
		}
		procModules.push_back(ProcessModule(modules[i], truePath));
	}
}

void GetKernelModulePaths(PVOID* modules, DWORD count, std::vector<ProcessModule>& procModules)
{
	for(DWORD i = 0; i < count; ++i)
	{
		// for each driver get its filename and convert it into a "normal" windows path
		WCHAR modPath[MAX_PATH] = {0};
		if(GetDeviceDriverFileName(modules[i], modPath, MAX_PATH))
		{
			std::wstring dosPath = NTPathToDosPath(modPath);
			procModules.push_back(ProcessModule(modules[i], dosPath));
		}
	}
}

// returns the modules loaded into 'mode' of 'hProc'
void GetLoadedModules(HANDLE hProc, TraceMode mode, std::vector<ProcessModule>& procModules)
{
	HMODULE modules[1024];
	DWORD modCount = 0;
	if(mode == USER)
	{
		EnumProcessModules(hProc, modules, sizeof(modules), &modCount);
	}
	else
	{
		EnumDeviceDrivers(reinterpret_cast<LPVOID*>(modules), sizeof(modules), &modCount);
	}
	modCount /= sizeof(*modules);
	procModules.reserve(modCount);
	if(modCount)
	{
		if(mode == USER)
		{
			GetUserModulePaths(hProc, modules, modCount, procModules);
		}
		else
		{
			GetKernelModulePaths(reinterpret_cast<LPVOID*>(modules), modCount, procModules);
		}
	}
}

// returns maximum user mode address
PVOID GetMaximumAppAddress()
{
	static SYSTEM_INFO sysInfo = {0};
	if(!sysInfo.lpMaximumApplicationAddress)
	{
		GetSystemInfo(&sysInfo);
	}
	return sysInfo.lpMaximumApplicationAddress;
}

// prints 'error' to stdout, with last error if it's specified
void DisplayError(LPCWSTR error, DWORD lastError)
{
	std::wostringstream stream;
	stream << error;
	if(lastError)
	{
		// straight from the SDK
		LPWSTR lpMsgBuf = NULL;
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			lastError,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			reinterpret_cast<LPWSTR>(&lpMsgBuf),
			0, NULL);
		stream << L"\nLast error: " << lpMsgBuf;
		LocalFree(lpMsgBuf);
	}
	std::wcerr << stream.str().c_str() << std::endl;
}

// Retrieves a handle to the KStack driver
HANDLE GetDriverHandle()
{
	// handle will be cleaned up when the global static data is destructed
	static ScopedData<> driver(CreateFile(L"\\\\.\\Global\\KStack", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL), &CloseHandle);
	return *driver;
}

// Get the path to the drivers directory
std::wstring GetDriversDirectory()
{
	WCHAR driversPath[MAX_PATH] = {0};
	std::wstring systemPath = GetWindowsDirectoryString();
	PathCombine(driversPath, systemPath.c_str(), L"drivers\\");
	return std::wstring(driversPath);
}

// Copies the KStack driver into the drivers folder, and creates the service
void InstallDriver()
{
	// build the path to the KStack driver that came with the exe
	WCHAR sysPath[MAX_PATH] = {0};
	GetModuleFileName(NULL, sysPath, MAX_PATH - 1);
	PathRemoveFileSpec(sysPath);
	PathAppend(sysPath, L"KStack.sys");
	// ensure it exists
	if(PathFileExists(sysPath))
	{
		// get the path to the driver directory and copy it there
		std::wstring driverPath = GetDriversDirectory();
		driverPath += L"KStack.sys";
		if(CopyFile(sysPath, driverPath.c_str(), FALSE))
		{
			// open the service manager and create the service
			ScopedData<SC_HANDLE> scManager(OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CREATE_SERVICE), &CloseServiceHandle);
			if(scManager)
			{
				SC_HANDLE scService = CreateService(
							*scManager,
							L"KStack",
							L"KStack",
							0,
							SERVICE_KERNEL_DRIVER,
							SERVICE_DEMAND_START,
							SERVICE_ERROR_IGNORE,
							driverPath.c_str(),
							NULL, 
							NULL,
							NULL,
							NULL,
							NULL);
				if(scService)
				{
					DisplayError(L"Driver installed");
					CloseServiceHandle(scService);
				}
				else
				{
					// if we didn't create the service, delete the copied file
					DisplayError(L"Couldn't install service", GetLastError());
					DeleteFile(driverPath.c_str());
				}
			}
			else DisplayError(L"Couldn't open service manager", GetLastError());
		}
		else DisplayError(L"Couldn't copy KStack driver to the drivers directory");
	}
	else DisplayError(L"Couldn't find KStack.sys, ensure it is in the same directory as this executable");
}

// stop and deletes the service and KStack.sys from the drivers folder
void UninstallDriver()
{
	ScopedData<SC_HANDLE> scManager(OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CONNECT), &CloseServiceHandle);
	if(scManager)
	{
		ScopedData<SC_HANDLE> kstack(OpenService(*scManager, L"KStack", SERVICE_STOP | DELETE), &CloseServiceHandle);
		if(kstack)
		{
			SERVICE_STATUS stat = {0};
			ControlService(*kstack, SERVICE_CONTROL_STOP, &stat);
			DeleteService(*kstack);
			std::wstring driverPath = GetDriversDirectory();
			driverPath += L"KStack.sys";
			DeleteFile(driverPath.c_str());
			DisplayError(L"Driver uninstalled");
		}
		else DisplayError(L"Couldn't open KStack service", GetLastError());
	}
	else DisplayError(L"Couldn't open Service Manager", GetLastError());
}
