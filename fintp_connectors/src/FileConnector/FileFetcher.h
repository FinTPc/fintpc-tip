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

#ifndef FILEFETCHER_H
#define FILEFETCHER_H

#include "FS/FsWatcher.h"
#include "../Endpoint.h"

#include <fstream>

using namespace std;

class FileFetcher : public Endpoint
{
	protected :

		// methods for controlling the endpoint 
		void internalStart();
		void internalStop();
		void FinalMove( string& finalPath );

	public:
	
		// constructor
		FileFetcher();
	
		//destructor
		~FileFetcher();
	
		// called before start. allows the endpoint to prepare environment
		void Init();
		
		// methods for transactional execution
		/// <summary>Make an engagement whether it can/can't process the data</summary>
		/// <returns> CurrentFileName if it can process the data</returns>
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

		static void NotificationCallback( const AbstractWatcher::NotificationObject* notification );
		
		bool moreMessages() const;
		pthread_t getWatcherThreadId() { return m_Watcher.getThreadId(); }
		
	private :

		static int m_PrevBatchItem;
		BatchItem m_LastBatchItem;
		
		FsWatcher m_Watcher;
		
		string m_CurrentFilename;
		string m_CurrentBatchId;
		ifstream m_CurrentFile;
		unsigned long m_CurrentFileSize;
		string m_ReconSource;
		
		string m_WatchPath;
		string m_SuccessPath;
		string m_ErrorPath;

		string m_BatchXsltFile;

#ifdef USING_REGULATIONS
		string m_RepliesPath;
		string m_Regulations;
		string m_ParamFilePattern;
		string m_ParamFileXslt;
		string m_ReplyServiceName;
		string m_RejectLAUKey;
#endif// USING_REGULATIONS
	
		static FileFetcher* m_Me;

	public :
	
		string getWatchPath() const { return m_WatchPath; }
		string Serialize( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument *doc, const string& xsltFilename, const string& paramFilename, const long paramFileSize, const string& HMAC = "" );
};

#endif
