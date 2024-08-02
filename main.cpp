#include "LoadedModules.h"
#include <TraceLoggingProvider.h>
#include <evntrace.h>

/* 
 I developed this driver as a quick solution for a book project.  It provided the correct results.
	 However, I am not a kernel programming expert, I'm still learning, so please be aware that the code below may contain errors.
*/

/*
This driver enumerates loaded modules every 10 minutes using a recurring timer.

It logs information about each module using Event Tracing for Windows (ETW).
*/


#pragma warning(disable : 4996) // ExAllocatePoolWithTag
#pragma warning(disable : 4100) // Unreferenced parameter


// {6F303849-16C8-4985-9B43-F0225A970C3D}
TRACELOGGING_DEFINE_PROVIDER( g_Provider , "ModEnumTimer" , (0x6f303849 , 0x16c8 , 0x4985 , 0x9b , 0x43 , 0xf0 , 0x22 , 0x5a , 0x97 , 0xc , 0x3d ));
#define DEVICE_NAME L"\\Device\\ModEnumTimer"


void DriverUnload( _In_ PDRIVER_OBJECT DriverObject );

// Global timer and dpc
PKTIMER g_Timer;
PKDPC g_Dpc;



VOID WorkerRoutine( _In_ PDEVICE_OBJECT DeviceObject , _In_opt_ PVOID Context ) {
	

	// Disables the execution of normal kernel APCs (PASSIVE_LEVEL)
	KeEnterCriticalRegion( );

	// Acquire the resource for shared access
	ExAcquireResourceSharedLite( PsLoadedModuleResource , TRUE ); // ExAcquireResourceSharedLite

	
	// Safe iteration over the PsLoadedModuleList
	for ( auto Entry = ( _KLDR_DATA_TABLE_ENTRY *)PsLoadedModuleList->InLoadOrderLinks.Flink; 
		  Entry != PsLoadedModuleList; 
		  Entry = ( _KLDR_DATA_TABLE_ENTRY * ) Entry->InLoadOrderLinks.Flink ) 
	{
		TraceLoggingWrite( g_Provider ,
						   "Loaded Modules Enumeration" ,
						   TraceLoggingLevel( TRACE_LEVEL_INFORMATION ),
						   TraceLoggingWideString( Entry->FullDllName.Buffer , "FullDllName" ),
						   TraceLoggingPointer(Entry->DllBase, "DllBase" ),
						   TraceLoggingHexUInt64(Entry->SizeOfImage, "SizeOfImage" )


		);
	}



	// Release the shared ressource
	ExReleaseResourceLite( PsLoadedModuleResource );

	// re enable normal kernel APCs
	KeLeaveCriticalRegion( );
	
	
	if( Context ) 
		IoFreeWorkItem( reinterpret_cast< PIO_WORKITEM >( Context ) );

	return;
}

VOID DpcRoutine( _In_ struct _KDPC *Dpc , _In_opt_ PVOID DeferredContext , _In_opt_ PVOID SystemArgument1 , _In_opt_ PVOID SystemArgument2 ) {
	

	if ( DeferredContext == nullptr ) return;

	// Create a Work Item
	PIO_WORKITEM WorkItem = IoAllocateWorkItem( reinterpret_cast< PDEVICE_OBJECT >( DeferredContext ) );

	if ( WorkItem == nullptr ) return;

	// To safely enumerate loaded modules at PASSIVE_LEVEL
	IoQueueWorkItem( WorkItem , WorkerRoutine , NormalWorkQueue , WorkItem ); 


	return;

}

extern "C" NTSTATUS DriverEntry( PDRIVER_OBJECT DriverObject , PUNICODE_STRING RegisteryPath ) {

	TraceLoggingRegister( g_Provider );

	NTSTATUS Status { STATUS_SUCCESS };
	UNICODE_STRING DeviceName;
	PDEVICE_OBJECT DeviceObject;


	DriverObject->DriverUnload = DriverUnload;

	RtlInitUnicodeString( &DeviceName , DEVICE_NAME );



	do {
		// Device Creation
		Status = IoCreateDevice( DriverObject , 0 , &DeviceName , FILE_DEVICE_UNKNOWN , FILE_DEVICE_SECURE_OPEN , FALSE , &DeviceObject );

		if ( !NT_SUCCESS( Status ) ) break;


		// Dpc and Timer Creation
		g_Dpc = reinterpret_cast< PKDPC >( ExAllocatePoolWithTag( NonPagedPoolNx , sizeof( KDPC ) , 'FloF' ) );
		g_Timer = reinterpret_cast< PKTIMER >( ExAllocatePoolWithTag( NonPagedPoolNx , sizeof( KTIMER ) , 'FloF' ) );


		if ( g_Timer == nullptr || g_Dpc == nullptr ) {
			Status = STATUS_INSUFFICIENT_RESOURCES;
			IoDeleteDevice( DeviceObject );
			break;
		}

		// Dpc initialization
		KeInitializeThreadedDpc( g_Dpc , DpcRoutine , DeviceObject );

		// Timer initialization
		KeInitializeTimer( g_Timer );


		// Defining recurring timer 
		LARGE_INTEGER DueTime;

		DueTime.QuadPart = -6000000000LL; // 10 minutes in miliseconds (10000000 * 60 * 10)
		LONG Period = 600000L; // 10 minutes in miliseconds (10 * 60 * 1000)


		BOOLEAN AlreadyInserted = KeSetTimerEx( g_Timer , DueTime , Period , g_Dpc );

		// Handle already inserted timer
		if ( AlreadyInserted ) {

			ExFreePool( g_Timer );
			ExFreePool( g_Dpc );

			IoDeleteDevice( DeviceObject );

			Status = STATUS_UNSUCCESSFUL;
		}


	}
	while ( false );


	return Status;
}


void DriverUnload( _In_ PDRIVER_OBJECT DriverObject ) {

	// Cancel the timer 
	KeCancelTimer( g_Timer );

	// Free allocated ressources
	ExFreePool( g_Timer );
	ExFreePool( g_Dpc );

	// Delete the device
	IoDeleteDevice( DriverObject->DeviceObject );

	// Unregister ETW
	TraceLoggingUnregister( g_Provider );

	return;
}
