#ifdef WIN32
	#ifdef _RR_DLL
		#define ExportedRRObject __declspec( dllexport )
		#define RREXPIMP_TEMPLATE
	#else
		#define ExportedRRObject __declspec( dllimport )
		#define RREXPIMP_TEMPLATE extern
	#endif
#else
	#define ExportedRRObject 
	#define RREXPIMP_TEMPLATE
#endif

#ifndef DLLMAIN_RR_LIB
#define DLLMAIN_RR_LIB

#include <string>
using namespace std;

#include <Trace.h>
using namespace FinTP;

namespace FinTPRR
{
	class ExportedRRObject VersionInfo
	{
		private : 
			VersionInfo(){};
			
		public :
			static std::string Name();
			static std::string Id();
			///\return Repository url 
			static std::string Url();
			static std::string BuildDate();
	}; 
}

#endif // DLLMAIN_RR_LIB