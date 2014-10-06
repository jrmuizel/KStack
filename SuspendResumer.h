/*
        KStack Sample Application - SuspendResumer.h
		Copyright (C) 2009 aw.comyr.com
		Distributed under the ZLib license
		http://www.opensource.org/licenses/zlib-license.html
*/

#ifndef SUSPEND_RESUMER_H
#define SUSPEND_RESUMER_H

#pragma once

#include "stdafx.h"

// RAII wrapper around suspending and resuming the thread
// so the thread will be resumed if an exception is thrown
// from the method
struct ThreadSuspendResumer
{
	HANDLE hThread;
	inline ThreadSuspendResumer(HANDLE hThread) : hThread(hThread)
	{
		SuspendThread(hThread);
	}
	inline ~ThreadSuspendResumer()
	{
		ResumeThread(hThread);
	}
};

#endif
