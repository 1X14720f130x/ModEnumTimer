#pragma once
#include <ntifs.h>



typedef struct _KLDR_DATA_TABLE_ENTRY {
	_LIST_ENTRY InLoadOrderLinks;
	PVOID Reserved0;
	unsigned long Reserved1;
	PVOID GpValue;
	PVOID Reserved2;
	PVOID DllBase;
	PVOID Reserved3;
	unsigned long SizeOfImage;
	UNICODE_STRING FullDllName;
}_KLDR_DATA_TABLE_ENTRY;


extern "C" _KLDR_DATA_TABLE_ENTRY* PsLoadedModuleList;
extern "C" ERESOURCE* PsLoadedModuleResource;