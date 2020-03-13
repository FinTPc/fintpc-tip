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

#ifndef FILECONNECTOR_H
#define FILECONNECTOR_H

#include <pthread.h>

#include <string>
#include <deque>

using namespace std;

#include "Endpoint.h"
#include "Service/NTServiceBase.h"
#include "AppSettings.h"

#if defined( WIN32 ) && defined( WIN32_SERVICE )
class Connector : public CNTService
#else
class Connector
#endif
{
	public :
	
		//constructor
		Connector( const string& mainProgramName = "Connector", const string& startupFolder = "" );
	
		//destructor
		~Connector();
	
		//members		
		void Start( const string& recoveryMessageId = "" );
		void Stop( void );

		bool isRunning() const { return m_Running; }

#if defined( WIN32_SERVICE )
		virtual void Run( DWORD, LPTSTR * );
#endif
		
		void setMainProgramName( const string& mainProgramName ) { m_FullProgramName = mainProgramName; }
		string getMainProgramName() const { return m_FullProgramName; }
	
		// endpoints accessors
		const Endpoint* getFetcher() const { return m_Fetcher; }
		const Endpoint* getPublisher() const { return m_Publisher; }
		
		AppSettings GlobalSettings;

	private :

#if defined( WIN32_SERVICE )
		HANDLE	m_hStop;
#endif

		string m_FullProgramName;
		static bool m_ShouldStop;

		static Endpoint *m_Fetcher;
		static Endpoint *m_Publisher;
		
		bool m_Running;		
};

#endif
