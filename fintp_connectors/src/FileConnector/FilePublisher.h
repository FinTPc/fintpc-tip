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

#ifndef FILEPUBLISHER_H
#define FILEPUBLISHER_H

#include "../Endpoint.h"
#include "../Message.h"
#include "MQ/MqWatcher.h"

#include <fstream>

class FilePublisher : public Endpoint
{
	protected :

		// methods for controlling the endpoint 
		void internalStart();
		void internalStop();

		void internalCleanup();

	public:
	
		// constructor
		FilePublisher();
	
		//destructor
		~FilePublisher();

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

		string Serialize( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument *doc, const string& xsltFilename, const string& filename, const unsigned long fileSize = 0, const string& HMAC = "" );

		bool moreMessages() const;
		pthread_t getWatcherThreadId() { return m_Watcher->getThreadId(); }

		static string XPathCallback( const string& itemNamespace );
		
	private:

		void WriteMessage( const char* message, const streamsize length );
		string HMAC_ShaGen( const string& inData, const string& privateKey );

		void MoveFile( const string& destinationFile );
		void ProcessFile( string& messageOutput,  string feedback = "" );

		AbstractWatcher* m_Watcher;
		
		string m_WatchQueue;
		string m_WatchQueueManager;
		string m_WatchTransportURI;
		bool m_WMQBatchFilter;
		bool m_StrictSwiftFormat;
		bool m_CommitTransport;

		/**
		 * Used to run Publisher in aggregate mode. 
		 * Aggregate mode: Many messages in a output file but not as business batch
		 */
		bool m_MoreMessages;
		bool m_AggregateMessages;
		
		//string m_CurrentMessageId;		
		//string m_CurrentGroupId;
		//unsigned long m_CurrentMessageLength;

		// used by the "XMLFile" batch manager to generate a unique name ony for the first message 
		bool m_First;
		
		static string m_DestinationPath;
		static string m_ReplyDestinationPath;
		static string m_ReplyPattern;
		static string m_Pattern;
		static string m_TransformFile;
		static string m_ReplyFeedback;
		static string m_TempDestinationPath;
			
		string m_MessageId;

		// set only for xmlfile batchmanager type ( unique to each batch )
		string m_BatchId;

		string m_DestFileName;
		string m_ReplyDestFileName;
		string m_RenameFilePattern;

		boost::posix_time::time_duration m_TimeSinceEpoch;
	
		ofstream m_DestFile;
		
		FinTPMessage::Metadata m_Metadata;

		static FilePublisher* m_Me;

		int m_MaxMessagesPerFile;
		int m_CurrentMessageIndex;

		string m_LastFilename;

#ifdef USING_REGULATIONS
		string m_ParamFileXslt;
		string m_LAUCertFile;
#endif // USING_REGULATIONS

		string m_BlobLocator;				// simplified xpath to locate the blob in the payload ( ex. /root/IDIMAGINE )
		string m_BlobFilePattern;			// folder+filename containing {xpats}s ( ex : c:\blobs\{/root/BATCHID}_{/root/IMAGEREF}.tiff )
		string SaveBlobToFile( const string& xmlData );

		string RenameFile( const string& oldFilename, const string& newFilename );
};

#endif
