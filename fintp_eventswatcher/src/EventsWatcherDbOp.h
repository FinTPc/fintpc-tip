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

#ifndef EVENTSWATCHERDBOP_H
#define EVENTSWATCHERDBOP_H

#include "DatabaseProvider.h"
#include <string>

using namespace std;
using namespace FinTP;

class EventsWatcherDbOp
{
	private :
	
		// default .ctor is private ( no instance required )
		EventsWatcherDbOp();
		static Database *m_DataDatabase;
		static Database *m_ConfigDatabase;
		
		static DatabaseProviderFactory *m_DatabaseProvider;
		
		static Database* getData();
		static Database* getConfig();
		
		static ConnectionString m_ConfigConnectionString;
		static ConnectionString m_DataConnectionString;
		
	public:

		// destructor
		~EventsWatcherDbOp();
		static void Terminate();
		
		// static methods
		static void SetConfigDataSection( const NameValueCollection& dataSection );
		static void SetConfigCfgSection( const NameValueCollection& cfgSection );
		
		// static methods
		static DataSet* ReadServiceStatus();
		static void InsertPerformanceInfo( long serviceId, long sessionId, const string& timestamp,
			long minTT, long maxTT, long meanTT, long sequenceNo, long ioIdentifier, long commitedNo );
			
		static void InsertEvent( const long serviceId, const string& correlationId, const string& sessionId, 
			const string& type, const string& machine, const string& date, const string& messageBuffer, 
			const string& event_class = "", const string& additionalInfo = "", const string& innerException = "" );
		static void InsertEvent( const string& dadbuffer, const string& messageBuffer );
			
		static void UpdateServiceState( const long serviceId, const long newState, const string& sessionId );
		static void UpdateServiceVersion( const string& serviceName, const string& name, const string& version, const string& machine, const string& hash );
};

#endif // EVENTSWATCHERDBOP_H
