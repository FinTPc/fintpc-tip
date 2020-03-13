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

#ifndef MQPUBLISHER_H
#define MQPUBLISHER_H

#include "MQ/MqWatcher.h"
#include "TransportHelper.h"
#include "XPathHelper.h"
#include "../Endpoint.h"
#include "../Message.h"
#include <boost/shared_ptr.hpp>

#ifndef NO_DB
	#include "Database.h"
	#include "DatabaseProvider.h"
#endif

#include "BatchManager/Storages/BatchZipArchiveStorage.h"
#include "Swift/SAAFilter.h"
#include "Swift/SwiftFormatFilter.h"

using namespace std;

class MqPublisher : public Endpoint
{
	protected :

		// methods for controlling the endpoint 
		void internalStart();
		void internalStop();

	public:
	
		// constructor
		MqPublisher();
	
		//destructor
		~MqPublisher();

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
		
		bool moreMessages() const { return !m_IsLast; }
		pthread_t getWatcherThreadId() { return m_Watcher->getThreadId(); }

		//static const int MAX_BATCH_BACKOUTCOUNT;
		static string XPathCallback( const string& itemNamespace );
	private :

		AbstractWatcher* m_Watcher;
		
		string m_WatchQueue;
		string m_WatchQueueManager;
		string m_WatchTransportURI;
		bool m_WMQBatchFilter;
		
		string m_AppQueue, m_AppQmgr, m_AppTransportURI;
		string m_SSLKeyRepos, m_SSLCypherSpec, m_SSLPeerName;
		string m_CertificateFileName, m_CertificatePasswd;
		string m_RepliesQueue;	
		
		bool m_NotificationTypeXML;

		//string m_CurrentGroupId;
		//string m_CurrentMessageId;
		//string m_MessageFormat;
		//unsigned long m_CurrentMessageLength;
		
		TransportHelper* m_CurrentHelper;
		AbstractFilter* m_SAAFilter;
		string m_ParamFileXslt, m_StrictSwiftFormat, m_TransformFile;
		
		FinTPMessage::Metadata m_Metadata;
		static MqPublisher* m_Me;

		// ID member
		BatchManager< BatchZipArchiveStorage > m_BatchManagerID;
		string m_LastIdImageInZip;

		bool m_IsIDsEnabled ;
		bool m_IsCurrentBatchID;
		int m_CurrentSequence;

#ifndef NO_DB
		//ManagedBuffer* GetIDImage( string groupId, string correlationId, string& imgRef  );
		void GetIDImage( const string& groupId, const string& correlationId, string& imgRef, ManagedBuffer* outputSignedBuffer );

		Database *m_CurrentDatabase;
		DatabaseProviderFactory *m_CurrentProvider;
#endif 

		string m_DatabaseProvider;		// Conector Database Provider
		string m_DatabaseName; 			//Connector Database Name
		string m_UserName;				//Connector Database User
		string m_UserPassword;			//Connector Database Password

		string m_IDCertificateFileName, m_IDCertificatePasswd;
		string m_LAUKey;

		static const string CHQ_NAMESPACE ;
		static const string PRN_NAMESPACE ;
		static const string BLX_NAMESPACE ;
};

#endif //MQPUBLISHER_H
