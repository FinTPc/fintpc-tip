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

#ifndef ROUTINGENGINEMAIN_H
#define ROUTINGENGINEMAIN_H

#ifdef TESTDLL_EXPORT
#define ExportedTestObject __declspec( dllexport )
#else //TESTDLL_EXPORT

#ifdef TESTDLL_IMPORT
#define ExportedTestObject __declspec( dllimport )

#else //TESTDLL_IMPORT
#define ExportedTestObject 
#endif //TESTDLL_IMPORT

#endif //TESTDLL_EXPORT

#if ( defined( WIN32 ) && ( _MSC_VER <= 1400 ) )
	#ifdef _HAS_ITERATOR_DEBUGGING
		#undef _HAS_ITERATOR_DEBUGGING
	#endif
	#define _HAS_ITERATOR_DEBUGGING 0
#endif

#include <string>
#include <vector>
#include <deque>
#include <list>
using namespace std;
#include "Trace.h"
using namespace FinTP;

namespace FinTPRoutingEngine
{
	class VersionInfo
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

#endif // ROUTINGENGINEMAIN_H
