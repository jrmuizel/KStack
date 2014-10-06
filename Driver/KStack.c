/*
        KStack Sample Driver - KStack.c
		Copyright (C) 2009 aw.comyr.com
		Distributed under the ZLib license
		http://www.opensource.org/licenses/zlib-license.html
*/

#include <ntifs.h>
#include "structs.h"
#include "private_structs.h"

/* Allow our code sections to be paged out when not needed */
#pragma code_seg("PAGE")

NTSTATUS ReadMemory(ReadRequest* request, ULONG inputLen, PVOID outBuffer, ULONG outLen, ULONG* bytesCopied)
{
    NTSTATUS stat = STATUS_SUCCESS;
    *bytesCopied = 0; /* initialize */
	/* Validate params */
    if(request && outBuffer && (inputLen >= sizeof(*request)))
    {
        /* ensure we don't read more than we can or more then we have to */
        const ULONG bytesToRead = min(request->bytes, outLen);
        /* Get the delimiting pointers */
        UCHAR* startOfRange = (UCHAR*)request->address;
        UCHAR* endOfRange = startOfRange + (bytesToRead - 1);
        /* Check the range is valid */
        if(MmIsAddressValid(startOfRange) && MmIsAddressValid(endOfRange))
        {
            RtlCopyMemory(outBuffer, request->address, bytesToRead);
            *bytesCopied = bytesToRead;
        }
        else
        {
            /* assume pointers were invalid */
            stat = STATUS_ACCESS_VIOLATION;
        }
    }
    else
    {
        /* we were passed one or more invalid parameters */
        stat = STATUS_INVALID_PARAMETER;
    }
    return stat;
}

NTSTATUS GetThreadContext(ULONG* pThreadID, ULONG inSize, ThreadCtx* ctx, ULONG outSize, ULONG* bytesCopied)
{
    NTSTATUS stat = STATUS_SUCCESS;
	*bytesCopied = 0;
    /* validate parameters */
    if(pThreadID && ctx && (inSize >= sizeof(*pThreadID)) && (outSize >= sizeof(*ctx)))
    {
        PETHREAD pThread = NULL;
        /* grab a pointer to the ETHREAD */
        if(NT_SUCCESS(stat = PsLookupThreadByThreadId((HANDLE)*pThreadID, &pThread)))
        {
            /* bend it to our superior version */
            KTHREAD_XP* kThread = (KTHREAD_XP*)pThread;
            /* Do the hustle as outlined above */
            UINT_PTR* kernelStack = (UINT_PTR*)kThread->KernelStack;
            PVOID threadESP = (PVOID)(kernelStack + 3);
            ctx->esp = threadESP;
            if(MmIsAddressValid(threadESP))
            {
                ctx->ebp = *(PVOID*)threadESP;
                ctx->eip = *(PVOID*)(kernelStack + 2);
				*bytesCopied = sizeof(*ctx);
            }
            else
            {
                stat = STATUS_INVALID_PARAMETER;
            }
            /* finally release our hold on the thread */
            ObDereferenceObject(pThread);
        }
    }
    else
    {
        stat = STATUS_INFO_LENGTH_MISMATCH;
    }
    return stat;
}

NTSTATUS KStack_DevIOCTL(PDEVICE_OBJECT pDevice, PIRP pIrp)
{
    PIO_STACK_LOCATION iosp = IoGetCurrentIrpStackLocation (pIrp);
    /* Parse the request details out of the relevant buffers.
     * The below isn't a typo. With the ioctl mode we're using, the input buffer
     * is also the output buffer.
     */
    PVOID inBuffer  = pIrp->AssociatedIrp.SystemBuffer;
    PVOID outBuffer = pIrp->AssociatedIrp.SystemBuffer;
    ULONG inLength  = iosp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLength = iosp->Parameters.DeviceIoControl.OutputBufferLength;
    ULONG ioctl     = iosp->Parameters.DeviceIoControl.IoControlCode;     
    PIO_STATUS_BLOCK ioStatus = &(pIrp->IoStatus);

    /* call the relevant function if any */
    switch(ioctl)
    {
        case IOCTL_READ_MEMORY: 
        {
            ioStatus->Status = ReadMemory((ReadRequest*)inBuffer, inLength, outBuffer, outLength, &(ioStatus->Information));
        }
        break;
        case IOCTL_THREAD_CONTEXT:
        {
            ioStatus->Status = GetThreadContext((ULONG*)inBuffer, inLength, (ThreadCtx*)outBuffer, outLength, &(ioStatus->Information));
        }
        break;
        default:
        {
            ioStatus->Status = STATUS_NOT_SUPPORTED;
            ioStatus->Information = 0;
        }
    }
    /* Tell the IO Manager that we're finished with the packet
     * and not to boost the thread that made the request
     */
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
    return ioStatus->Status;
}

/* SimpleOK is called when either CreateFile or CloseHandle are called for our driver.
 * All it does is signal that the processing of the I/O Request Packet was successful,
 * tells the IO manager we're finished with the packet and finally returns success.
 */
NTSTATUS KStack_SimpleOK(PDEVICE_OBJECT pDevice, PIRP pIrp)
{
    /* set IRP success */
    pIrp->IoStatus.Status = STATUS_SUCCESS;
    pIrp->IoStatus.Information = 0;
	/* tell the manager we've finished with it 
	 * and not to boost priority of the thread that created the request */
    IoCompleteRequest(pIrp, IO_NO_INCREMENT );
    return STATUS_SUCCESS;
}

/* these values never change and are required in both Entry and Unload funcs
 * unfortunately they can't be made const
 */
static UNICODE_STRING deviceName = RTL_CONSTANT_STRING(L"\\Device\\KStack");
static UNICODE_STRING deviceLink = RTL_CONSTANT_STRING(L"\\DosDevices\\KStack");

void KStack_Unload(PDRIVER_OBJECT DriverObject)
{
    KdPrint(("KStack: Unloading\n"));

    /* Delete the symbolic link */
    IoDeleteSymbolicLink(&deviceName);
    KdPrint(("KStack: Deleted the symbolic link\n"));

    /* Delete the device object */
    IoDeleteDevice(DriverObject->DeviceObject);
    KdPrint(("KStack: Deleted the device\n"));
}

/* Allow the entry function to be discarded after it has run */
#pragma code_seg("INIT")

NTSTATUS DriverEntry(PDRIVER_OBJECT pDrvObj, PUNICODE_STRING pRegPath)
{
    /* first create the device */
    PDEVICE_OBJECT interfaceDevice = NULL;
    NTSTATUS status = IoCreateDevice(pDrvObj, 0, &deviceName, FILE_DEVICE_UNKNOWN,
                                     FILE_READ_ONLY_DEVICE | FILE_DEVICE_SECURE_OPEN,
                                     FALSE, &interfaceDevice);
    if(NT_SUCCESS(status))
    {
        status = IoCreateSymbolicLink (&deviceLink, &deviceName);
        /* KdPrint is a macro that is a equivalent to a debug mode printf()
         * It evaluates to nothing in optimized builds
         * Use DbgPrint to print regardless of debug/release mode
         */
        KdPrint(("KStack: IoCreateSymbolicLink returned 0x%x\n", status));
        /*
         * Fill in the handlers for those requests we're interested in.
         * Set create and close to a simple success function
         * And the others to specialized functions
         */
        pDrvObj->MajorFunction[IRP_MJ_CREATE] =
        pDrvObj->MajorFunction[IRP_MJ_CLOSE] = KStack_SimpleOK;
        pDrvObj->MajorFunction[IRP_MJ_DEVICE_CONTROL] = KStack_DevIOCTL;
        pDrvObj->DriverUnload  = KStack_Unload;
    }
    /* If something went wrong, delete what we created */
    if (!NT_SUCCESS(status))
    {
       IoDeleteSymbolicLink(&deviceLink);
       if(interfaceDevice ) 
       {
           IoDeleteDevice( interfaceDevice );
       }
    }
    KdPrint(("KStack: DriverEntry returning 0x%x\n", status));
    return status;
}
