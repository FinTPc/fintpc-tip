#include "DllMainRoutingRegulations.h"

#include <string>
using namespace std; 

namespace FinTPRR
{
	string VersionInfo::Name()
	{
		return "FinTP RR lib (SEPA core 2.2,2.3 - T0)"; 
	}
	
	string VersionInfo::Id()
	{
		return "";
	}

	string VersionInfo::Url()
	{
		return "";
	}
	
	string VersionInfo::BuildDate()
	{
		return __TIME__;
	}
}
