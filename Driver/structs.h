/*
        KStack Sample Driver - structs.h
		Copyright (C) 2009 aw.comyr.com
		Distributed under the ZLib license
		http://www.opensource.org/licenses/zlib-license.html
*/

#ifndef KSTACK_STRUCTS_H
#define KSTACK_STRUCTS_H

#pragma once

/* Defines structures and constants used to communicate between app and driver */
#ifdef KSTACK_USER_APP
#include <winioctl.h> /* For the Ctl_Code macro */
#endif

typedef struct _ReadRequest
{
	void* address;
	unsigned long bytes;
} ReadRequest;

/* Input is a pointer to a ReadRequest structure, output is a buffer large enough to hold 'byte' bytes */
#define IOCTL_READ_MEMORY (ULONG)CTL_CODE(FILE_DEVICE_UNKNOWN, 0x00, METHOD_BUFFERED, FILE_ANY_ACCESS )

/* output buffer struct */
typedef struct _ThreadCtx
{
    PVOID eip;
    PVOID esp;
    PVOID ebp;
} ThreadCtx;

/* Input is a pointer to a HANDLE to the desired thread, output is a pointer to a ThreadCtx structure */
#define IOCTL_THREAD_CONTEXT (ULONG)CTL_CODE(FILE_DEVICE_UNKNOWN, 0x01, METHOD_BUFFERED, FILE_ANY_ACCESS )

#endif