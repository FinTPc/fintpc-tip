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

#ifndef DUMPCONTEXT_H
#define DUMPCONTEXT_H

#include "DllMainLog.h"
#include "Collections.h"
#include <map>

using namespace std;

//EXPIMP_TEMPLATE template class ExportedObject std::map< string, string >;
namespace FinTP
{
	class ExportedLogObject DumpContext
	{
		public:
		
			virtual ~DumpContext();
			
			static void Clear();
			static void Dump();
			static void Add( const string& name, const string& value );

			static void Terminate() { if ( m_Instance != NULL ) { delete m_Instance; m_Instance = NULL; } }
			
		private :

			DumpContext();
			
			static DumpContext* getInstance();
			static DumpContext* m_Instance;
			
			map< string, string > m_Context;
	};
}

#endif // DUMPCONTEXT_H
