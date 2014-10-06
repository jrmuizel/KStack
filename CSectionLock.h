/*
        KStack Sample Application - CSectionLock.h
		Copyright (C) 2009 aw.comyr.com
		Distributed under the ZLib license
		http://www.opensource.org/licenses/zlib-license.html
*/

#include "stdafx.h"

struct CSectionLock
{
	inline CSectionLock(PCRITICAL_SECTION pCritSec) : pCritSec(pCritSec)
	{
		EnterCriticalSection(pCritSec);
	}
	inline ~CSectionLock()
	{
		LeaveCriticalSection(pCritSec);
	}

private:
	PCRITICAL_SECTION pCritSec;
};