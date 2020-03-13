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

#ifndef EVENTSWATCHERMAIN_H
#define EVENTSWATCHERMAIN_H

#ifdef TESTDLL_EXPORT
#define ExportedTestObject __declspec( dllexport )
#else //TESTDLL_EXPORT

#ifdef TESTDLL_IMPORT
#define ExportedTestObject __declspec( dllimport )

#else //TESTDLL_IMPORT
#define ExportedTestObject 
#endif //TESTDLL_IMPORT

#endif //TESTDLL_EXPORT

#include <string>
using namespace std; 

#include "Trace.h"
using namespace FinTP;

namespace FinTPEventsWatcher
{
	class VersionInfo
	{
		private : 
			VersionInfo(){};
			
		public :
			static string Name();
			static string Id();
			///\return Repository url 
			static string Url();
			static string BuildDate();
	}; 
}


#endif // EVENTSWATCHERMAIN_H
