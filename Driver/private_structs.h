/*
        KStack Sample Driver - private_structs.h
		Copyright (C) 2009 aw.comyr.com
		Distributed under the ZLib license
		http://www.opensource.org/licenses/zlib-license.html
*/

#ifndef KSTACK_PRIVATE_STRUCTS_H
#define KSTACK_PRIVATE_STRUCTS_H

#pragma once

#include <ntddk.h>

/* first few members of the ethread / kthread structure are all we need
   note: this structure does change between OS releases, this layout works on 2000 and XP
*/
typedef struct _KTHREAD_2000_XP
{
    DISPATCHER_HEADER Header;
    LIST_ENTRY MutantListHead;
    PVOID InitialStack; /* Bottom of the stack memory region */
    PVOID StackLimit; /* top of the stack memory region */
    PVOID TEB;
    PVOID TlsArray;
    PVOID KernelStack; /* Current stack top */
    /* ... */
} KTHREAD_XP;

#endif