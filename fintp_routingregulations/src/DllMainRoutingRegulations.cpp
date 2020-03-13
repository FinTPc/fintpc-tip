#include "DllMainRoutingRegulations.h"

#ifdef WIN32
#include <windows.h>

#ifndef NETDLL
BOOL WINAPI DllMain ( HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved )
{ 
	return true;
} 
#endif //NETDLL 

#endif