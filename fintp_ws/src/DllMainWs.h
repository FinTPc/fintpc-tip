/*
* FinTP - Financial Transactions Processing Application
* Copyright (C) 2013 Business Information Systems (Allevo) S.R.L.
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>
* or contact Allevo at : 031281 Bucuresti, 23C Calea Vitan, Romania,
* phone +40212554577, office@allevo.ro <mailto:office@allevo.ro>, www.allevo.ro.
*/

#ifdef WIN32
	#ifdef _WSDLL
		#define ExportedWsObject __declspec( dllexport )
		//#define EXPIMP_TEMPLATE
	#else
		#define ExportedWsObject __declspec( dllimport )
		//#define EXPIMP_TEMPLATE extern
	#endif
#else
	#define ExportedWsObject 
	//#define EXPIMP_TEMPLATE
#endif

#ifndef DLLMAIN_WS_LIB
#define DLLMAIN_WS_LIB

#include <string>

namespace FinTPWS
{
	class ExportedWsObject VersionInfo
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

#endif // DLLMAIN_WS_LIB
