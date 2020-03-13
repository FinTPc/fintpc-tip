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

#ifndef DBFETCHER_H
#define DBFETCHER_H

#include "DB/DbWatcher.h"
#include "Database.h"
#include "DatabaseProvider.h"

#include "../Endpoint.h"

// Database to Mq

class ExportedTestObject DbFetcher : public Endpoint
{
	protected :

		// methods for controlling the endpoint 
		void internalStart();
		void internalStop();

	public:
	
		// constructor
		DbFetcher();
	
		//destructor
		~DbFetcher();

		// called before start. allows the endpoint to prepare environment
		void Init();
		
		// methods for transactional execution
		/// <summary>Make an engagement whether it can/can't process the data</summary>
		/// <returns>True if it can process the data</returns>
		
		//Connect to database
		string Prepare();

		/// <summary>Commits the work</summary>
		// Delete processed data, disconnect from database
		void Commit();

		/// <summary>Aborts the work and rolls back the data</summary>
		// Delete processed data, disconnect from database
		void Abort();
		
		/// <summary>Rollback</summary>
		// Disconnect from database
		void Rollback();

		/// <summary>Processes the data</summary>
		/// <returns>True if everything worked fine</returns>
		//Select data, convert to XML format, passed them to be processed
		void Process( const string& correlationId );	
		
		static void NotificationCallback( const AbstractWatcher::NotificationObject* notification );	
		
		static DbFetcher* m_Me;
		pthread_t getWatcherThreadId() { return m_Watcher.getThreadId(); }

	private :
		
		DbWatcher m_Watcher;
		string m_CurrentRowId;
		int m_UncommitedTrns, m_MaxUncommitedTrns;
		XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument *m_CurrentMessage;
		XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument *m_SavedMessage;
		ManagedBuffer *m_CurrentMessageStr;
		bool m_NotificationTypeXML, m_Rollback;
		bool m_DatabaseToXmlTrimm;

		string m_DatabaseProvider;
		string m_DatabaseName; 			//Connector Database Name
		string m_UserName;				//Connector User
		string m_UserPassword;			//Connector Password
		string m_TableName;				//Connector Table
		
		//Connector table Stored Procedures
		string m_SPmarkforprocess;		//mark an unprocessed record as the currently processed recod
		string m_SPselectforprocess;	//select the current record
		string m_SPmarkcommit;			//mark current processed record commited
		string m_SPmarkabort;			//mark current processed record aborted
		string m_SPWatcher;				//look for available records
		
		Database *m_CurrentDatabase;
		DatabaseProviderFactory *m_CurrentProvider;
};

#endif
