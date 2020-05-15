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

#ifndef RESTFETCHER_H
#define RESTFETCHER_H

#include "REST/RESTWatcher.h"
#include "../FTPBackupLoader.h"
#include "../Endpoint.h"

using namespace std;

class RESTFetcher : public Endpoint
{
	protected :

		enum ProcessSteps
		{
			MESSAGE = 1,
			REPLY  = 2,
			RESPONSE = 3
		};

		// methods for controlling the endpoint 
		void internalStart();
		void internalStop();
		bool requiresReply();
		bool requiresSecurityCheck();

	public:
	
		// constructor
		RESTFetcher();
	
		//destructor
		~RESTFetcher();

		// called before start. allows the endpoint to prepare environment
		void Init();
		
		// methods for transactional execution
		/// <summary>Make an engagement whether it can process the data</summary>
		string Prepare();

		/// <summary>Commits the work</summary>
		void Commit();

		/// <summary>Aborts the work and rolls back the data</summary>
		void Abort();
		
		/// <summary>Rollback</summary>
		void Rollback();

		/// <summary>Processes the data</summary>
		void Process( const string& correlationId );	
		
		bool moreMessages() const;

		pthread_t getWatcherThreadId() { return 0; }
		
	private :

		static const string m_ProcessStepName[3];
		static const unsigned int MAX_IDLETIME;
		
		FTPBackupLoader m_BackupLoader;
		AbstractBackupLoader::DeepNotificationPool m_BackupPool;

		ManagedBuffer* m_CurrentMessage;
		string m_CurrentMessageId;

		IPSHelper m_ReplyHelper;
		IPSHelper m_RequestHelper;

		string m_ReplyXsltFile;

		string m_RESTChannel, m_ResourcePath;
		string m_ReplyResourcePath;
		string m_ConfirmationPath;
		string m_BackupURI;

		string m_CertificateSSL, m_CertificateSSLPhrase, m_ClientBIC;
		string m_ClientCertificate;
		string m_ReplyCertificate;
		
		bool m_IsLast;
		ProcessSteps m_ProcessStep;

		bool m_IPEnabled;
		int m_IdleTime;
		bool isConfirmationRequired( string& messageType  ) const;

		//int m_RequestSequence;
};

#endif //RESTFETCHER_H
