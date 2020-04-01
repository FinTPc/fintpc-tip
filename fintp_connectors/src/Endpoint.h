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

#ifndef FTPENDPOINT_H
#define FTPENDPOINT_H

#ifdef WIN32
	#ifdef CHECK_MEMLEAKS
		//#define _CRTDBG_MAP_ALLOC
		#include <stdlib.h>
		#include <crtdbg.h>
	#endif
#endif

#include "ConnectorMain.h"

#include "InstrumentedObject.h"
#include "FilterChain.h"
#include "Management/Management.h"
#include "Transactions/AbstractStatePersistence.h"
#include "BatchManager/BatchManager.h"
#include "AppSettings.h"
#include "TimeUtil.h"
#include "AbstractWatcher.h"
#include "AppExceptions.h"

class ExportedTestObject EndpointConfig
{
	private :
		EndpointConfig() {};

	public :
		
		enum ConfigDirection
		{
			AppToWMQ,
			WMQToApp, 
			AppToMQ,
			MQToApp,
			Common
		};

		/** 
		 * Contains config file entries for  connectors
		 */
		enum ConfigSettings
		{
			// Common settings
			/**
			 * Config name : <b>BatchManagerType</b>
			 * Batch manager type. Fetchers use this to split incoming messages, publishers to combine<Note>Accepted values : XMLfile, Flatfile</Note>
			 */
			BATCHMGRTYPE,
			/**
			 * Config name : <b>BatchXsltFile</b>
			 * XSLT applied by dequeue to get an element from the batch<Note>Used by XMLfile batch managers</Note>
			 */
			BATCHMGRXSLT,
			/**
			* Config name : <b>ReplyBatchManagerType</b>
			* Reply Batch manager type. Publisher use this to split messages, publishers to combine<Note>Accepted values : XMLfile, Flatfile</Note>
			*/
			REPLYBATCHMGRTYPE,
			/**
			* Config name : <b>ReplyBatchXsltFile</b>
			* XSLT applied by dequeue to get an element from the batch<Note>Used by XMLfile batch managers</Note>
			*/
			REPLYBATCHMGRXSLT,
			/**
			 * Config name : <b>BatchManagerXPath</b>
			 * XPath applied by enqueue to get insertion point in batch<Note>Used by XMLFile batch managers</Note>
			 */
			BATCHMGRXPATH,
			/**
			 * Config name : <b>BatchManagerTemplate</b>
			 * Template applied by dequeue to get an element from the batch<Note>Used by Flatfile batch managers</Note>
			 */
			BATCHMGRTMPL,
			/**
			 * Config name : <b>BatchManagerFixedMQMessageSize</b>
			 * Used to split one MQ messages into multiple fixed size messages
			 */
			BATCHMGRFIXEDMQMSGSIZE,
			/**
			 * Config name : <b>ReplyOptions</b>
			 * Used by publishers to send a reply back to FinTP server
			 */
			RPLOPTIONS,
			/**
			 * Config name : <b>ReconSource</b>
			 * Used by ReconS Connector to detect if Ledger or Statement
			 */
			RECONSOURCE,
			/**
			 * Config name : <b>HashXSLT</b>
			 * Used by Connector to calculate a hash on a string that contains important fields from a message
			 */
			HASHXSLT,
			/**
			 * Config name : <b>Regulations</b>
			 * Used by Connector to enforce validation of a specific Regulation
			 */
			REGULATIONS,
			
			// DB settings
			/**
			 * Config name : <b>DatabaseProvider</b>
			 * Database provider <Note>Accepted values : Oracle, DB2, Fox</Note>
			 */
			DBPROVIDER,
			/**
			 * Config name : <b>DatabaseName</b>
			 * Database name ( name of the database to connect to )
			 */
			DBNAME,
			/**
			 * Config name : <b>UserName</b>
			 * Username used to connect to the database
			 */
			DBUSER,
			/**
			 * Config name : <b>UserPassword</b>
			 * Password used to connect to the database
			 */
			DBPASS,
			/**
			 * Config name : <b>TableName</b>
			 * Name of the table used for fetching data ( DBFetcher ) or upload data ( DBPublisher )
			 */
			TABLENAME,
			/**
			 * Config name : <b>RepliesTableName</b>
			 * Name of the table used by DBPublisher to upload replies
			 */
			REPLIESTABLE,
			/**
			 * Config name : <b>DadOptions</b>
			 * Dad options can be : "WithParams", "WithValues", "NoDad"
			 */
			DADOPTIONS,
			/**
			 * Config name : <b>DadFileName</b>
			 * Dad file used to map publisher data to table format <Note>Used only with DB2 and IFX</Note>
			 */
			DADFILE,
			/**
			 * Config name : <b>AckDadFileName</b>
			 * Ack dad file used to map ack data to table format <Note>Used only with IFX</Note>
			 */
		    ACKDADFILE,
			/**
			 * Config name : <b>SPinsertXmlData</b>
			 * Stored procedure used to upload data
			 */
			SPINSERT,
			/**
			 * <note>Deprecated - should not be used</note>
			 * Config name : <b>SPselectforprocess</b>
			 * Stored procedure used to fetch mark data from the database
			 */
			SPSELECT,
			/**
			 * <note>Deprecated - should not be used</note>
			 * Config name : <b>SPmarkforprocess</b>
			 * Stored procedure used to mark data for processing
			 */
			SPMARK,
			/**
			 * Config name : <b>SPmarkcommit</b>
			 * Stored procedure used to mark data as processed
			 */
			SPMARKCOMMIT,
			/**
			 * Config name : <b>SPmarkabort</b>
			 * Stored procedure used to mark data as aborted ( unable to process )
			 */
			SPMARKABORT,
			/**
			 * Config name : <b>SPWatcher</b>
			 * Stored procedure used to scan target table for new records
			 */
			SPWATCHER,
			/**
			 * Config name : <b>MaxUncommitedTrns</b>
			 * Maximum number of uncommited transactions for db fetcher
			 */
			UNCMTMAX,
			/**
			 * Config name : <b>NotificationType</b>
			 * Notification type expected from watcher <Note>Accepted values : <U>XML</U>, char</Note>
			 */
			NOTIFTYPE,
			/**
			 * Config name : <b>DateFormat</b>
			 * Date Format for extract from database default is DD.MM.YYYY
			 */
		    DATEFORMAT,
			/**
			 * Config name : <b>TimestampFormat</b>
			 * Timestamp format for extract from database , default is DD.MM.YYYY HH:MI
			 */
		    TIMESTAMPFORMAT,
			/**
			*Config name : <b>DatabaseToXmlTrimming</b>
			*Trimming database fields when messages are getting from BO
			*/
			DBTOXMLTRIMM,

			// MQ settings
			/**
			 * Config name : <b>AppQueue</b>
			 * Queue watched for new messages to upload ( MQPublisher ) or used to fetch data ( MQFetcher )
			 */
			APPQUEUE,
			/**
			 * Config name : <b>RepliesQueue</b>
			 * Queue used to upload replies ( MQPublisher )
			 */
			RPLYQUEUE,
			/**
			 * Config name : <b>BackupQueue</b>
			 * Queue used by fetcher to save a copy of the received message, and by publisher to save a copy of the sent message
			 */
			BAKQUEUE,
			/**
			 * Config name : <b>QueueManager</b>
			 * Name of the WMQ queue manager used to put/get messages
			 */
			WMQQMGR,
			/**
			 * Config name : <b>MQURI</b>
			 * URI of the WMQ broker used to put/get messages
			 */
			MQURI,
			/**
			 * Config name : <b>KeyRepository</b>
			 * WMQ queue manager key repository used to (SSL) connect as client to the WMQ server
			 */
			WMQKEYREPOS,
			/**
			 * Config name : <b>SSLCypherSpec</b>
			 * WMQ Cipher specification for use with SSL 
			 */
			WMQSSLCYPHERSPEC,
			/**
			 * Config name : <b>SSLPeerName</b>
			 * WMQ Peer name for use with SSL 
			 */
			WMQPEERNAME,
			/**
			 * Config name : <b>MessageFormat</b>
			 * Message format used to by publisher to upload messages <Note>Accepted values : 'MQHRF2  ', <U>'MQSTR   '</U></Note>
			 */
			WMQFMT,
			/**
			 * Config name : <b>IsSigned</b>
			 * Set this true if the incomming message is in pkcs7 format
			 */
			ISSIGNED,
			/**
			 * Config name : <b>CertificateFileName</b>
			 * File that contains certificate to use for sign
			 */
			CERTIFICATEFILENAME,
			/**
			 * Config name : <b>CertificatePasswd</b>
			 * Password to use for open  the certificate
			 */
			CERTIFICATEPASSWD,
			/**
			 * Config name <b>Type</b>
			 * MQ server type to work with
			 */
			MQSERVERTYPE,

			// File settings
			/**
			 * Config name : <b>SourcePath</b>
			 * Folder watched for new messages to fetch ( FileFetcher )
			 */
			FILESOURCEPATH,	
			/**
			 * Config name : <b>DestinationPath</b>
			 * Folder used by FileFetcher to move processed messages to and by FilePublisher to upload messages
			 */
			FILEDESTPATH,
			/**
			 * Config name : <b>TempDestinationPath</b>
			 * Folder used by FilePublisher to intermediary write messages and param files, prior final upload to destination path
			 */
			FILETEMPDESTPATH,
			/**
			 * Config name : <b>MaxFilesPerFile</b>
			 * Maximum number of messages written to one file by FilePublisher
			 */
			MAXFILESPERFILE,
			/**
			 * Config name : <b>ErrorPath</b>
			 * Folder used by FileFetcher to move undeliverable messages to
			 */
			FILEERRPATH,
			/**
			 * Config name : <b>FilePattern</b>
			 * Filter used by FileFetcher to match data filenames and by FilePublisher to generate filenames
			 */
			FILEFILTER,
			/**
			 * Config name : <b>RepliesPath</b>
			 * Folder to write reply messages 
			 */
			RPLDESTTPATH,
			/**
			 * Config name : <b>RepliesPattern</b>
			 * Filter used by FilePublisher to generate filenames for replies
			 */
			RPLFILEFILTER,
			/**
			 * Config name : <b>ReplyFeedback</b>
			 * Feedback information for reply
			 */
			RPLFEEDBACK,
			/**
			 * Config name : <b>RepliesXslt</b>
			 * Xslt used by MqFetcher to generate reply message
			 */
			RPLXSLT,
			/**
			 * Config name : <b>AppInterfaceReplyXslt</b>
			 * Xslt used by FilePublisher to generate reply message
			 */
			APPINTRPLXSLT,
			/**
			 * Config name : <b>TransformFile</b>
			 * Transform applied by FilePublisher on the message before writing it to file
			 */
			FILEXSLT,
			/**
			 * Config name : <b>StrictSWIFTFormat</b>
			 * Used by FilePublisher to output strict swift format ( <code>x512, 0x1, 0x3</code> ... )
			 */
			STRICTSWIFTFMT,

			/**
			 * Config name : <b>ReconSFormat</b>
			 * Used by FileFetcher to parse file in xxx format
			 */
			 RECONSFORMAT,

			/**
			 * Config name : <b>ReplyServiceName</b>
			 * Used by FileFetcher to change Requestor for generated replies
			 */
			 RPLSERVICENAME,

			/**
			* Config name : <b>IsIDEnabled</b>
			* Set "true" for connectors processing IDs
			*/
			 ISIDENABLED,

			/**
			 * Config name : <b>IDCertificateFileName</b>
			 * File that contains certificate to use for sign ID
			 */

			 IDCERTIFICATEFILENAME,

			/**
			 * Config name : <b>IDCertificatePasswd</b>
			 * Password to use for open  the certificate
			 */
			 IDCERTIFICATEPASSWD,

			/**
			 * Config name : <b>SSLCertificateName</b>
			 * Certificate to use for open  the SSL certificate
			 */
			 SSLCERTIFICATEFILENAME,
			
			/**
			 * Config name : <b>SSLCertificatePasswd</b>
			 * Password to use for open  the SSL key
			 */
			 SSLCERTIFICATEPASSWD,

			 SERVICENAME, 
			 PARAMFPATTERN,
			 PARAMFXSLT,
			 RENAMEPATTERN,
			/**
			 * Config name : <b>InsertBlob</b>
			 * Signal Publisher to cleanup BLOB form Payload/Original
			 */
			 INSERTBLOB,
			 DBCFGNAME,
			 DBCFGUSER,
			 DBCFGPASS,
			 BLOBLOCATOR,
			 BLOBPATTERN,
			 TRACKMESSAGES,
			 HEADERREGEX,
			 /**
			 * Config name : <b>LAUCertificateFile</b>
			 * Used by publiser to sign message data and .par file for FTA-LAU
			  */
			  
			 LAUCERTIFICATE
		};

		static string getName( const ConfigDirection prefix, const ConfigSettings setting );
};

class ExportedTestObject Endpoint : public InstrumentedObject
{
	private :

		string internalPrepare();
		void internalCommit( const string& correlationId );
		void internalAbort( const string& correlationId );
		void internalRollback( const string& correlationId );
		void internalProcess( const string& correlationId );	

		void fireManagementEvent( TransactionStatus::TransactionStatusEnum, const void* additionalData );

#if defined( WIN32 ) && defined ( CHECK_MEMLEAKS )
		_CrtMemState m_NextMemoryState, m_OldMemoryState;
		bool m_OldStateAvailable;
#endif

		bool m_FatalError;
		static AppSettings *m_GlobalSettings;

		//performance
		TimeUtil::TimeMarker m_LastReportTime;
		//unsigned long m_PerfAborted, m_PerfCommited, m_PerfTotal;

	protected : //methods

		// methods for controlling the endpoint 
		virtual void internalStart() = 0;
		virtual void internalStop() = 0;

		//should be noexcept
		virtual void internalCleanup() {}
	
		Endpoint();

		string getServiceName() const { return m_ServiceName; }
		void setCorrelationId( const string& correlationId );

		AppSettings& getGlobalSettings() const;
		bool haveGlobalSetting( const EndpointConfig::ConfigDirection prefix, const EndpointConfig::ConfigSettings setting ) const;
		string getGlobalSetting( const EndpointConfig::ConfigDirection prefix, const EndpointConfig::ConfigSettings setting, const string& defaultValue = "__NODEFAULT" ) const;

	protected : // members

		static void ( *m_ManagementCallback )( TransactionStatus::TransactionStatusEnum, void* additionalData );

		BatchManagerBase* m_BatchManager;
		AbstractStatePersistence* m_PersistenceFacility;
		
		unsigned int m_BackoutCount;
		unsigned int m_CurrentStage;
		unsigned int m_CrtBatchItem;

		unsigned int m_MessageThrottling;

		string m_ServiceName;
		string m_ServiceThreadId;
		string m_CorrelationId, m_LastFailureCorrelationId, m_TransactionKey;

		bool m_IsFetcher;

		pthread_t m_SelfThreadId;
		bool m_Running, m_LastOpSucceeded;

		AbstractWatcher::NotificationPool m_NotificationPool;
		
		NameValueCollection m_TransportHeaders;
		
		string m_XmlData;
		AppException m_TrackingData;
				
		//indicate for Batch processing if last message or not
		bool m_IsLast; 
		bool m_TrackMessages;

		FilterChain* m_FilterChain;
	
	public:

		virtual ~Endpoint();
				
		// methods for controlling the endpoint 
		static void* StartInNewThread( void* );
		void Start();
		void Stop();

		// called before start. allows the endpoint to prepare environment
		virtual void Init() = 0;

		// methods for transactional execution
		/// <summary>Make an engagement whether it can/can't process the data</summary>
		/// <returns>A transaction key</returns>
		virtual string Prepare() = 0;

		/// <summary>Commits the work</summary>
		virtual void Commit() = 0;

		/// <summary>Aborts the work </summary>
		virtual void Abort() = 0;
		
		/// <summary>Rolls back the data</summary>
		virtual void Rollback() = 0;

		/// <summary>Processes the data</summary>
		/// <returns>True if everything worked fine</returns>
		virtual void Process( const string& correlationId ) = 0;	

		// workflow methods
		bool PerformMessageLoop();
		bool PerformMessageLoop( bool inBatch );
		
		virtual bool moreMessages() const { return false; }

		virtual void trackMessage( const string& payload, const NameValueCollection& transportHeaders );
				
		// management methods		
		static void setManagementCallback( void ( *callback )( TransactionStatus::TransactionStatusEnum, void* ) )
		{
			m_ManagementCallback = callback;
		}

		// state persistence
		void setPersistenceFacility( AbstractStatePersistence* facility );
		AbstractStatePersistence* getPersistenceFacility();
		
		void setServiceName( const string& value ){ m_ServiceName = value; }
		void setServiceThreadId( const string& serviceThreadId ) { m_ServiceThreadId = serviceThreadId; }
		void setServiceType( const bool& isFetcher ) { m_IsFetcher = isFetcher; }

		pthread_t getThreadId() { return m_SelfThreadId; }
		const pthread_t getThreadId() const { return m_SelfThreadId; }

		virtual pthread_t getWatcherThreadId() = 0;
		
		FilterChain* getFilterChain(){ return m_FilterChain; }

		BatchManagerBase* getBatchManager(){ return m_BatchManager; }

		static void setGlobalSettings( AppSettings* settings ) { m_GlobalSettings = settings; }
};

class EndpointFactory
{
	public :
		static Endpoint* CreateEndpoint( const string& serviceName, const string& type, bool fetcher );
};

#endif //FTPENDPOINT_H
