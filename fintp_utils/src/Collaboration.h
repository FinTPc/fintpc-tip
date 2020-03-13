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

#ifndef COLLABORATION_H
#define COLLABORATION_H

#include "DllMainUtils.h"
#include <pthread.h>

namespace FinTP
{
	class ExportedUtilsObject Collaboration
	{
	#if defined( TESTDLL_EXPORT ) || defined ( TESTDLL_IMPORT )
		friend class CollaborationTest;
	#endif
		private :

			static unsigned int m_Static;
			static unsigned int m_Contor; 

			static pthread_once_t KeysCreate;
			static pthread_key_t CollaborationKey;

			static void CreateKeys();
			static void DeleteCollaborationIds( void* data );
			
			// one instance ( to create thread specific keys )
			static Collaboration m_Instance;
			Collaboration();
			

		public:
		
			~Collaboration();
			
			static std::string GenerateGuid();
			static std::string EmptyGuid();
			static std::string GenerateMsgID();
			static void setGuidSeed( const unsigned int value );
			
	};
}

#endif // COLLABORATION_H
