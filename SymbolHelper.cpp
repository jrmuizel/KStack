/*
        KStack Sample Application - SymbolHelper.cpp
		Copyright (C) 2009 aw.comyr.com
		Distributed under the ZLib license
		http://www.opensource.org/licenses/zlib-license.html
*/

#include "stdafx.h"
#include "SymbolHelper.h"
#include "Utility.h"
#include "CSectionLock.h"
#include "SuspendResumer.h"
#include <exception>
#include <algorithm>
#include <functional>
#include <sstream>
#define KSTACK_USER_APP
#include "Driver/structs.h"

// initializes the symbol helper
SymbolHelper::SymbolHelper(HANDLE hProc, LPCWSTR lpwzSymPath, BOOL bInvade, PCRITICAL_SECTION pCritSec)
	: hProc(hProc),
	moduleLoadFlags(LOAD_BOTH),
	lock(pCritSec)
{
	LPCSTR exceptionText = NULL;
	dbgHelp.Init(LoadLibrary(L"dbghelp_6_6.dll"), &FreeLibrary);
	if(dbgHelp)
	{
		SymInit symInitialize = NULL;
		if(FindFunctionInModule(*dbgHelp, "SymInitializeW", symInitialize))
		{
			// do the function discovery outside the critical section
			SymOptions symOpts = NULL;
			FindFunctionInModule(*dbgHelp, "SymSetOptions", symOpts);
			SymReg registerFunc = NULL;
			FindFunctionInModule(*dbgHelp, "SymRegisterCallbackW64", registerFunc);
			CSectionLock locker(lock);
			if(!symInitialize(hProc, lpwzSymPath, bInvade))
			{
				// if invading fails, initialize without it and we'll load
				// the modules into the sym table ourself
				if(!symInitialize(hProc, lpwzSymPath, FALSE))
				{
					exceptionText = "Failed to initialize the symbol handler";
					goto throw_exception;
				}
			}
			else moduleLoadFlags &= (~LOAD_USER);
			// set the options to be used by the symbol handler functions
			if(symOpts)
			{
				/* Set the options that: 
				   undecorate C++ function names
				   make symbols case insensitive
				   loads line info where available
				   finds nearest for address lookups
				   search public symbols if one cannot be found in the global table
				   don't provide any UI prompts
				   don't try and load symbols for a modules until a request is made
				   provide debug output in debug mode
				   */
				symOpts(SYMOPT_UNDNAME | SYMOPT_CASE_INSENSITIVE | SYMOPT_LOAD_LINES | 
						SYMOPT_OMAP_FIND_NEAREST | SYMOPT_AUTO_PUBLICS |
						SYMOPT_NO_PROMPTS | SYMOPT_DEFERRED_LOADS
#ifdef _DEBUG
						| SYMOPT_DEBUG
#endif
						); // symOpts
			}
			// finally register the notification callback
			if(registerFunc)
			{
				registerFunc(hProc, &CallbackProc, 0);
			}
		}
		else 
		{
			exceptionText = "Couldn't find SymInitializeW in dbghelp dll";
			goto throw_exception;
		}
	}
	else 
	{
		exceptionText = "Unable to find dbghelp_6_6.dll";
		goto throw_exception;
	}
	return;
throw_exception:
		throw std::runtime_error(exceptionText);
}

SymbolHelper::~SymbolHelper()
{
	// release any modules we may have loaded
	std::for_each(moduleHandles.begin(), moduleHandles.end(), &CloseHandle);
}

bool SymbolHelper::AddressToSymbol(PVOID address, std::wstring& name)
{
	bool success = true;
	// initialize the output buffer with the address
	std::wostringstream outputBuffer;
	outputBuffer << std::hex << address << L" : ";

	TraceMode tMode = static_cast<TraceMode>(address > GetMaximumAppAddress());
	// load the modules if required
	LoadAllModules(tMode);
	// setup the variables and structures required
	BYTE arr[sizeof(SYMBOL_INFO) + ((MAX_SYM_NAME - 1) * sizeof(WCHAR))] = {0};
	PSYMBOL_INFO inf = reinterpret_cast<PSYMBOL_INFO>(arr);
	inf->SizeOfStruct = sizeof(SYMBOL_INFO);
	inf->MaxNameLen = MAX_SYM_NAME;
	
	DWORD64 disp = 0;
	ULONG_PTR symAddress = reinterpret_cast<ULONG_PTR>(address);
	IMAGEHLP_MODULE64 modInf = {sizeof(modInf), 0};

	// find the entrypoints needed
	SymAtAddr symFromAddr = NULL;
	FindFunctionInModule(*dbgHelp, "SymFromAddrW", symFromAddr);
	SymModuleInfo symModInf = NULL;
	FindFunctionInModule(*dbgHelp, "SymGetModuleInfoW64", symModInf);

	// acquire the lock and call the functions
	CSectionLock locker(lock);
	if(symModInf && symModInf(hProc, symAddress, &modInf))
	{
		outputBuffer << modInf.ModuleName;
		if(symFromAddr && symFromAddr(hProc, symAddress, &disp, inf))
		{
			outputBuffer << L"!" << inf->Name;
			if(disp)
			{
				outputBuffer << L"+0x" << std::hex << disp;
			}
		}
		else
		{
			if(modInf.BaseOfImage)
			{
				outputBuffer << L"+0x" << std::hex << symAddress - modInf.BaseOfImage;
			}
		}
		name = outputBuffer.str();
	}
	else
	{
		success = false;
	}
	return success;
}

// helper struct for LoadAllModules
struct SymbolLoader
{
	SymLoadMod loader;
	HANDLE hProc;

	SymbolLoader(SymLoadMod loader, HANDLE hProc)
		: loader(loader),
		hProc(hProc)
	{}

	HANDLE operator()(const ProcessModule& mod) const
	{
		// open the module file
		HANDLE hFile = CreateFile(mod.path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);
		if(hFile != INVALID_HANDLE_VALUE)
		{
			ULONG_PTR baseAddr = reinterpret_cast<ULONG_PTR>(mod.base);
			// register the module with the symbol handler
			// locking the critical section isn't required here as it happens in the callling function
			DWORD64 where = loader
			(
				hProc, hFile, mod.path.c_str(),	NULL,
				baseAddr, GetFileSize(hFile, NULL), NULL, 0
			);
#ifdef _DEBUG
			if(!where)
			{
				std::wostringstream str;
				str << L"SymLoadModuleExW failed for " << mod.path << L": Error " << GetLastError() << L'\n';
				OutputDebugString(str.str().c_str());
			}
#endif
		}
		// add hFile to the collection
		return hFile;
	}
};

// loads all user or kernel modules depending on the TraceMode
void SymbolHelper::LoadAllModules(TraceMode userOrKernel)
{
	const bool isKernel = (userOrKernel == KERNEL);
	// check whether we've already loaded the modules or not
	if(((isKernel) && (moduleLoadFlags & LOAD_KERNEL)) ||
	   ((!isKernel) && (moduleLoadFlags & LOAD_USER))
	)
	{
		std::vector<ProcessModule> modules;
		// get the modules
		GetLoadedModules(hProc, userOrKernel, modules);
		const size_t currentModuleHandlesSize = moduleHandles.size();
		moduleHandles.resize(currentModuleHandlesSize + modules.size());
		// find the loader function, then load the modules we found
		SymLoadMod loader;
		if(FindFunctionInModule(*dbgHelp, "SymLoadModuleExW", loader))
		{
			// lock in preparation for the loads
			CSectionLock locker(lock);
			std::transform(
				modules.begin(),
				modules.end(),
				moduleHandles.begin() + currentModuleHandlesSize,
				SymbolLoader(loader, hProc)
				);
			// update the load flags
			moduleLoadFlags &= (~(isKernel ? LOAD_KERNEL : LOAD_USER));
		}
		else
		{
			DisplayError(L"Unable to find SymLoadModuleExW in dbghelp_6_6.dll", GetLastError());
		}
	}
}

BOOL SymbolHelper::GetThreadContextAndTranslate(HANDLE hThread, DWORD threadId, TraceMode mode, ContextArgs& args) const
{
	BOOL bSuccess = FALSE;
	
	if(mode == USER)
	{
		args.ctx.ContextFlags = CONTEXT_CONTROL;
		bSuccess = GetThreadContext(hThread, &(args.ctx));
	}
	else
	{
		HANDLE hDriver = GetDriverHandle();
		if(hDriver != INVALID_HANDLE_VALUE)
		{
			ThreadCtx thCtx;
			DWORD bytesCopied = 0;
			// call the driver in a synchronous manner
			bSuccess = DeviceIoControl(hDriver, IOCTL_THREAD_CONTEXT, &threadId, sizeof(threadId), &thCtx, sizeof(thCtx), &bytesCopied, NULL);
			if(bSuccess)
			{
				// convert the threadctx to a normal context
				ThreadCtxToCONTEXT(thCtx, args.ctx);
			}
		}
		else
		{
			DisplayError(L"Couldn't open a handle to the driver", GetLastError());
		}
	}
	if(bSuccess)
	{
		ContextToStackFrame(args.ctx, args.stack);
		// use the image flag defined in the exe header as the machine type.
		// we can statically link the ImageNtHeader function
		// since it exists in the base version of dbghelp available
		// on Win2000/XP
		CSectionLock locker(lock);
		PIMAGE_NT_HEADERS header = ImageNtHeader(GetModuleHandle(NULL));
		args.machType = header->FileHeader.Machine;
	}
	return bSuccess;
}

bool SymbolHelper::GetThreadStackTrace(HANDLE hThread, DWORD threadID, TraceMode mode, std::vector<std::wstring>& output)
{
	bool success = false;
	// find the functions required
	SymWalkStack stackWalk = NULL;
	FindFunctionInModule(*dbgHelp, "StackWalk64", stackWalk);
	SymFuncTable funcAccess = NULL;
	FindFunctionInModule(*dbgHelp, "SymFunctionTableAccess64", funcAccess);
	SymModBase modBase = NULL;
	FindFunctionInModule(*dbgHelp, "SymGetModuleBase64", modBase);
	if(stackWalk && funcAccess && modBase)
	{
		// suspend the thread so we can reliable information
		ThreadSuspendResumer suspender(hThread);
		// get the thread context info and the initial stackframe64
		ContextArgs args = {0};
		if(GetThreadContextAndTranslate(hThread, threadID, mode, args))
		{
			// ensure the modules are loaded so we can get symbol info
			LoadAllModules(mode);
			CSectionLock locker(lock);
			// only use the custom function for kernel addresses
			PREAD_PROCESS_MEMORY_ROUTINE64 readMemFunc = (reinterpret_cast<PVOID>(args.stack.AddrPC.Offset) > GetMaximumAppAddress()) ? &ReadMemory : NULL;
			// start getting the call stack, the first frame is the initial state of the 
			// StackFrame64 struct
			do
			{
				std::wstring symName;
				if(AddressToSymbol(reinterpret_cast<PVOID>(args.stack.AddrPC.Offset), symName))
				{
					output.push_back(symName);
				}
			}
			while(stackWalk(args.machType, // machine type
							hProc, // process
							hThread, // thread
							&(args.stack), // stackframe
							&(args.ctx), // context
							readMemFunc, // our custom function
							funcAccess, // built-in function for function tables
							modBase, // built-in function for module bases
							NULL // we won't have any 16-bit addresses to translate
							)
						);
			success = true;
		}
	}
	else
	{
		DisplayError(L"Unable to find StackWalk64, SymFunctionTableAccess64 or SymGetModuleBase64 in dbghelp_6_6.dll", GetLastError());
	}
	return success;
}

// static or free functions start from here
//
void SymbolHelper::ThreadCtxToCONTEXT(const ThreadCtx& threadCtx, CONTEXT& ctx)
{
	ctx.ContextFlags = CONTEXT_CONTROL;

#ifndef _WIN64

	// win32 conversion
	ctx.Eip = reinterpret_cast<DWORD>(threadCtx.eip);
	ctx.Esp = reinterpret_cast<DWORD>(threadCtx.esp);
	ctx.Ebp = reinterpret_cast<DWORD>(threadCtx.ebp);

#elif (defined(_IA64_) || defined(_M_IA64_))

	// itanium conversion
	ctx.StIIP = reinterpret_cast<DWORD64>(threadCtx.eip);
	ctx.IntSp = reinterpret_cast<DWORD64>(threadCtx.esp);
	ctx.RsBSP = reinterpret_cast<DWORD64>(threadCtx.ebp);
		
#elif (defined(_AMD64_) || defined(_M_AMD64))

	// amd64 conversion
	ctx.Rip = reinterpret_cast<DWORD64>(threadCtx.eip);
	ctx.Rbp = reinterpret_cast<DWORD64>(threadCtx.ebp);
	ctx.Rsp = reinterpret_cast<DWORD64>(threadCtx.esp);

#elif
#error "Unknown Platform for context structure"
#endif
}

// initializes a stackframe64 from a context struct
void ContextToStackFrame(const CONTEXT& ctx, STACKFRAME64& sf)
{
	sf.AddrFrame.Mode = sf.AddrPC.Mode = sf.AddrStack.Mode = sf.AddrBStore.Mode = AddrModeFlat;
	sf.Virtual = TRUE;

#ifndef _WIN64

	sf.AddrPC.Offset = ctx.Eip;
	sf.AddrStack.Offset = ctx.Esp;
	sf.AddrFrame.Offset = ctx.Ebp;
	sf.AddrBStore = sf.AddrFrame;

#elif (defined(_IA64_) || defined(_M_IA64_))

	sf.AddrPC.Offset = ctx.StIIP;
	sf.AddrStack.Offset = ctx.IntSp;
	sf.AddrBStore.Offset = ctx.RsBSP;
	sf.AddrFrame = sf.AddrBStore;
		
#elif (defined(_AMD64_) || defined(_M_AMD64))

	sf.AddrPC.Offset = ctx.Rip;
	sf.AddrFrame.Offset = ctx.Rbp;
	sf.AddrStack.Offset = ctx.Rsp;
	sf.AddrBStore = sf.AddrFrame;

#elif
#error "Unknown Platform for context structure"
#endif
}

// callback that fires when events occur in the symbol handler
BOOL CALLBACK SymbolHelper::CallbackProc(HANDLE hProcess, ULONG ActionCode, ULONG64 CallbackData, ULONG64 UserContext)
{
	BOOL bRet = FALSE;
	switch(ActionCode)
	{
		// I've never seen this called but the docs say to implement it
		// if you use a custom ReadMemory function for StackWalk64
		// if it does get called, we just forward to the read memory routine
		case CBA_READ_MEMORY:
		{
			IMAGEHLP_CBA_READ_MEMORY* pMem = reinterpret_cast<IMAGEHLP_CBA_READ_MEMORY*>(CallbackData);
			bRet = ReadMemory(hProcess, pMem->addr, pMem->buf, pMem->bytes, pMem->bytesread);
		}
		break;
#ifdef _DEBUG
		case CBA_DEBUG_INFO:
		{
			std::wostringstream str;
			str << L"Callback Debug: " << reinterpret_cast<LPCWSTR>(CallbackData) << L'\n';
			OutputDebugString(str.str().c_str());
			bRet = TRUE;
		}
		break;
		default:
		{
			std::wostringstream str;
			str << L"Callback: called with code of " << ActionCode << L'\n';
			OutputDebugString(str.str().c_str());
		}
#else
		default: __assume(0); // stop code generation for default case in release mode
#endif
		break;
	}
	UNREFERENCED_PARAMETER(UserContext);
	return bRet;
}

BOOL CALLBACK SymbolHelper::ReadMemory(HANDLE hProcess, DWORD64 lpBaseAddress, PVOID lpBuffer, DWORD nSize, LPDWORD lpNumberOfBytesRead)
{
    PVOID maxUserAddress = GetMaximumAppAddress();
    PVOID base = reinterpret_cast<PVOID>(lpBaseAddress);
    const bool isUserAddress = (base <= maxUserAddress);
    // if the address is a user one, just use ReadProcessMemory
    if(isUserAddress)
    {
        *lpNumberOfBytesRead = 0;
    }
    // otherwise use the driver to read the kernel menory
    else
    {
        HANDLE hDriver = GetDriverHandle();
        if(hDriver != INVALID_HANDLE_VALUE)
        {
            ReadRequest req = {base, nSize};
            // use the input parameters as the output buffer, size, and bytes read
            DeviceIoControl(hDriver, IOCTL_READ_MEMORY, &req, sizeof(req), lpBuffer, nSize, lpNumberOfBytesRead, NULL);
        }
        else
        {
            DisplayError(L"Couldn't open an handle to the driver", GetLastError());
        }
    }
    return TRUE;
}