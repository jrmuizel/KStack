/*
        KStack Sample Application - SymbolHelper.h
		Copyright (C) 2009 aw.comyr.com
		Distributed under the ZLib license
		http://www.opensource.org/licenses/zlib-license.html
*/

#ifndef SYMBOL_HELPER_H
#define SYMBOL_HELPER_H

#pragma once

#include "stdafx.h"
#include "DbgHelpProtos.h"

// forward declaration of kernel thread context structure
typedef struct _ThreadCtx ThreadCtx;

// helper class for symbol lookups
class SymbolHelper
{
public:
	SymbolHelper(HANDLE hProc, LPCWSTR lpwzSymPath, BOOL bInvade, PCRITICAL_SECTION pCritSec);
	~SymbolHelper();

	// returns the module, symbol, and offset pAddress
	bool AddressToSymbol(PVOID pAddress, std::wstring& name);
	// returns a stack trace for the 'mode' part of the thread
	bool GetThreadStackTrace(HANDLE hThread, DWORD threadID, TraceMode mode, std::vector<std::wstring>& output);

private:
	// args for GetThreadContext / StackWalk64
	struct ContextArgs
	{
		CONTEXT ctx;
		STACKFRAME64 stack;
		DWORD machType;
	};

	typedef std::vector<HANDLE> ModuleCollection;

	// state of the moduleHandles collection
	enum ModLoadState
	{
		BOTH_LOADED = 0,
		LOAD_USER,
		LOAD_KERNEL,
		LOAD_BOTH
	};

	// the process being investigated
	HANDLE hProc;
	// loaded modules for symbol lookups
	ModuleCollection moduleHandles;
	// state tracking whether we need to load user or kernel flags
	// it is defined as a DWORD so we can perform bit manipulations on it
	DWORD moduleLoadFlags;
	// lock acquired and released to serialize access to dbghelp functions
	PCRITICAL_SECTION lock;
	// handle to the dynamically loaded dbghelp.dll
	// this is required since the built-in version on 2000/XP doesn't 
	// export the 64-bit function versions
	ScopedData<HMODULE> dbgHelp;

	// function to read memory on behalf of StackWalk64
	static BOOL CALLBACK ReadMemory(HANDLE hProcess, DWORD64 lpBaseAddress, PVOID lpBuffer, DWORD nSize, LPDWORD lpNumberOfBytesRead);
	// dbghelp callback, apparently required when providing a read memory function
	static BOOL CALLBACK CallbackProc(HANDLE hProcess, ULONG ActionCode, ULONG64 CallbackData, ULONG64 UserContext);

	// loads all modules in the addresses region
	void LoadAllModules(TraceMode userOrKernel);

	// gets the 'mode' context for the thread, and translates it into the first stackframe
	BOOL GetThreadContextAndTranslate(HANDLE hThread, DWORD threadId, TraceMode mode, ContextArgs& args) const;

	// converts a kernel thread context to a CONTEXT
	static void ThreadCtxToCONTEXT(const ThreadCtx& threadCtx, CONTEXT& ctx);
};

// convert a context to an initial stackframe
void ContextToStackFrame(const CONTEXT& ctx, STACKFRAME64& sf);

#endif
