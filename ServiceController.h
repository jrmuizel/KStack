/*
        KStack Sample Application - ServiceController.h
		Copyright (C) 2009 aw.comyr.com
		Distributed under the ZLib license
		http://www.opensource.org/licenses/zlib-license.html
*/

#ifndef SERVICE_MANAGER_H
#define SERVICE_MANAGER_H

#pragma once

#include "stdafx.h"

// RAII wrapper around starting and stopping a service
struct ServiceController
{
private:
	ScopedData<SC_HANDLE> scService;
	
public:
	ServiceController(LPCWSTR lpwszServiceName);
	~ServiceController();
};

#endif
