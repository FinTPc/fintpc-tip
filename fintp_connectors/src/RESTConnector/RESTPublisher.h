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

#ifndef RESTPUBLISHER_H
#define RESTPUBLISHER_H

#include "MQ/MqWatcher.h"
#include "REST/RESTHelper.h"
#include "../Endpoint.h"
#include "../Message.h"
#include "../FTPBackupLoader.h"

using namespace std;

class RESTPublisher : public Endpoint
{
	protected :

		// methods for controlling the endpoint 
		void internalStart();
		void internalStop();

	public:
	
		// constructor
		RESTPublisher();
	
		//destructor
		~RESTPublisher();

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
		
		bool moreMessages() const { return false; }
		pthread_t getWatcherThreadId() { return m_Watcher.getThreadId(); }

	private :

		MqWatcher m_Watcher;
		string m_WatchQueue;

		string m_RESTChannel, m_ResourcePath;	
		string m_SSLCertificateFile, m_SSLCertificatePhrase;
		string m_ClientCertificate, m_ClientCertificatePassword;
		IPSHelper m_CurrentHelper;

		TransportHelper *m_RespliesHelper;
		string m_RepliesQueue;
		string m_RepliesQueueManager;
		string m_RepliesTransportURI;
		
		string m_TransformFile;
		FinTPMessage::Metadata m_Metadata;
		
		FTPBackupLoader m_BackupLoader;
		AbstractWatcher::DeepNotificationPool m_BackupPool;
		string m_BackupURI;
};

#endif //RESTPUBLISHER_H
