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

#ifndef MQFETCHER_H
#define MQFETCHER_H

#include "MQ/MqWatcher.h"
#include "TransportHelper.h"
#include "../Endpoint.h"
#include "Swift/SAAFilter.h"
#include "Swift/SwiftFormatFilter.h"

#ifndef NO_DB
	#include "Database.h"
	#include "DatabaseProvider.h"
#endif

using namespace std;

class MqFetcher : public Endpoint
{
	protected :

		// methods for controlling the endpoint 
		void internalStart();
		void internalStop();

	public:
	
		// constructor
		MqFetcher();
	
		//destructor
		~MqFetcher();

		// called before start. allows the endpoint to prepare environment
		void Init();
		
		// methods for transactional execution
		/// <summary>Make an engagement whether it can/can't process the data</summary>
		/// <returns>True if it can process the data</returns>
		string Prepare();

		/// <summary>Commits the work</summary>
		void Commit();

		/// <summary>Aborts the work and rolls back the data</summary>
		void Abort();
		
		/// <summary>Rollback</summary>
		void Rollback();

		/// <summary>Processes the data</summary>
		/// <returns>True if everything worked fine</returns>
		void Process( const string& correlationId );	
		
		bool moreMessages() const;

		pthread_t getWatcherThreadId() { return m_Watcher.getThreadId(); }
		
	private :
		/// <summary>Geting image reference for ID message</summary>
		/// <returns>Image reference</returns>
		string GetIDImageReference( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument *doc );

		MqWatcher m_Watcher;
		
		string m_WatchQueue;
		string m_WatchQueueManager;
		string m_TransportURI;

		string m_StrictSwiftFormat;
		SwiftFormatFilter* m_SAASingleFilter;
		SAAFilter* m_SAAGroupFilter;

		string m_SSLKeyRepos, m_SSLCypherSpec, m_SSLPeerName;
		bool m_IsSigned;
		string m_CertificateFileName, m_CertificatePasswd;
		string m_RepliesQueue;

		string m_ReplyXsltFile;

		string m_CurrentMessageId;		
		string m_CurrentGroupId;
		unsigned long m_CurrentMessageLength;

		int m_CurrentSequence; // sequence of the currently processing message in batch
		
		//if BatchXml processing ( config ) it is the BatchXsltFile specified in config
		string m_BatchXsltFile;
		
		TransportHelper* m_CurrentHelper;

#ifndef NO_DB
		Database *m_CurrentDatabase;
		DatabaseProviderFactory *m_CurrentProvider;
#endif

		string m_DatabaseProvider;
		string m_DatabaseName; 			//Connector Database Name
		string m_UserName;				//Connector User
		string m_UserPassword;			//Connector Password

		string m_IDCertificateFileName, m_IDCertificatePasswd;
		string m_LAUKey;

		bool m_IsCurrentMessageID ;
		bool m_IsIDsEnabled ;
		string m_firstBatchId;
		string m_PreviousImageRef;
};

#endif //MQFETCHER_H
