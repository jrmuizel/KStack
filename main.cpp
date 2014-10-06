/*
        KStack Sample Application - main.cpp
		Copyright (C) 2009 aw.comyr.com
		Distributed under the ZLib license
		http://www.opensource.org/licenses/zlib-license.html
*/

#include "stdafx.h"
#include "utility.h"
#include "SymbolHelper.h"
#include "ServiceController.h"
#include <algorithm>
#include <iterator>

int __cdecl wmain(int argc, WCHAR** argv)
{
	MessageBox(NULL, L"", L"", 0);
	WORD majorVersion = LOWORD(GetVersion());
	if((LOBYTE(majorVersion) > 5) || HIBYTE(majorVersion) > 1)
	{
		DisplayError(L"This application and it's driver is not supported under Server 2003 and above");
	}
	// begin validation of command line arguments
	bool install = false, uninstall = false;
	if(argc == 2 && !(
			(install = !_wcsicmp(argv[1], L"install")) || 
			(uninstall = !_wcsicmp(argv[1], L"uninstall"))
		)
	)
	{
		DisplayError(L"Invalid arguments.\nUsage is \nKStack <threadID> <processID>\nOR\nKStack [install|uninstall]\n");
		return 1;
	}
	if((argc < 3) && !(install || uninstall))
	{
		DisplayError(L"Invalid arguments.\nUsage is \nKStack <threadID> <processID>\nOR\nKStack [install|uninstall]\n");
		return 1;
	}
	if(install)
	{
		InstallDriver();
		return 0;
	}
	else if(uninstall)
	{
		UninstallDriver();
		return 0;
	}
	// begin parsing arguments
	WCHAR* endPtr = NULL;
	DWORD threadID = wcstoul(argv[1], &endPtr, 10);
	if(*endPtr != 0)
	{
		DisplayError(L"ThreadID input error.\nUsage KStack <threadID> <processID>");
		return 2;
	}
	endPtr = NULL;
	DWORD processID = wcstoul(argv[2], &endPtr, 10);
	if(*endPtr != 0)
	{
		DisplayError(L"ProcessID input error.\nUsage KStack <threadID> <processID>");
		return 3;
	}
	// enabling the debugging privilege so we can suspend and resume the thread
	SetProcessPrivilege(GetCurrentProcess(), SE_DEBUG_NAME, TRUE);
	// get the relevant handles
	ScopedData<> hThread(OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, threadID), &CloseHandle);
	if(!hThread)
	{
		DisplayError(L"Unable to open thread.", GetLastError());
		return 4;
	}
	// PROCESS_VM_READ is required for invading into the process
	// and for enumerating process modules
	ScopedData<> hProcess(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID), &CloseHandle);
	if(!hProcess)
	{
		DisplayError(L"Unable to open process.", GetLastError());
		return 5;
	}
	try
	{
		// this assumes that the driver has been installed
		// under the service name KStack
		ServiceController kStackDriver(L"KStack");
		CRITICAL_SECTION cSec;
		InitializeCriticalSection(&cSec);
		// make sure the critical section will be cleaned up
		ScopedData<PCRITICAL_SECTION, void> pCriticalSec(&cSec, &DeleteCriticalSection);
		std::vector<std::wstring> stackTrace;
		// initialize the symbol helper and get the stack trace
		SymbolHelper syms(*hProcess, NULL, TRUE, &cSec);
		syms.GetThreadStackTrace(*hThread, threadID, KERNEL, stackTrace);
		syms.GetThreadStackTrace(*hThread, threadID, USER, stackTrace);
		if(!stackTrace.empty())
		{
			std::wcout << L"Thread stack for thread " << threadID << L":\n";
			std::copy(stackTrace.begin(), stackTrace.end(), std::ostream_iterator<std::wstring, WCHAR>(std::wcout, L"\n"));
		}
		else
		{
			DisplayError(L"Unable to get stack trace\n");
		}
	}
	catch(const std::exception& ex)
	{
		std::cout << ex.what() << '\n';
	}
}
