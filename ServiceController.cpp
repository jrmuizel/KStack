/*
        KStack Sample Application - Utility.cpp
		Copyright (C) 2009 aw.comyr.com
		Distributed under the ZLib license
		http://www.opensource.org/licenses/zlib-license.html
*/

#include "stdafx.h"
#include "ServiceController.h"
#include "Utility.h"
#include <shlwapi.h>
#include <stdexcept>

ServiceController::ServiceController(LPCWSTR lpwszServiceName)
{
	// Open the manager
	ScopedData<SC_HANDLE> scManager(OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CREATE_SERVICE), &CloseServiceHandle);
	if(scManager)
	{
		// open the service
		scService.Init(OpenService(*scManager, lpwszServiceName, SERVICE_START | SERVICE_STOP), &CloseServiceHandle);
		if(scService)
		{
			// start it
			if(!StartService(*scService, 0, NULL))
			{
				// This can be returned on XP, calling StartService again will launch the driver
				if(GetLastError() == ERROR_ALREADY_EXISTS)
				{
					StartService(*scService, 0, NULL);
				}
			}			
		}
		else
		{
			throw std::runtime_error("Unable to find service with the given name. Verify it is installed and try again");
		}
	}
	else
	{
		throw std::runtime_error("Unable to open service manager");
	}
}

ServiceController::~ServiceController()
{
	// stop the service and unload the driver
	SERVICE_STATUS stat;
	if(!ControlService(*scService, SERVICE_CONTROL_STOP, &stat))
	{
		DisplayError(L"Unable to stop service/driver", GetLastError());
	}
}
