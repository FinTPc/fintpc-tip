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

#include <string>
#include <sstream>

#ifndef NO_DB
#include <tiffio.h>
#include <tiffio.hxx>
#include "DataParameter.h"
#include "ConnectionString.h"
#endif

#include <xalanc/XercesParserLiaison/XercesDocumentWrapper.hpp>

#include "BatchManager/Storages/BatchFlatfileStorage.h"
#include "BatchManager/Storages/BatchZipArchiveStorage.h"
#include "BatchManager/Storages/BatchXMLfileStorage.h"
#include "SSL/SSLFilter.h"
#include "AbstractWatcher.h"
#include "MqFetcher.h"
#include "../Connector.h"
#include "AppSettings.h"

#include "TransportHelper.h"
#include "MQ/MqFilter.h"

#include "Trace.h"
#include "LogManager.h"
#include "AppExceptions.h"

#include "WorkItemPool.h"
#include "PlatformDeps.h"
#include "Collaboration.h"
#include "StringUtil.h"
#include "Base64.h"

#ifdef WIN32
#define __MSXML_LIBRARY_DEFINED__
#include "windows.h"
#define sleep(x) Sleep( (x)*1000 )
#else
#include <unistd.h>
#endif

using namespace std;

//constructor
MqFetcher::MqFetcher() : Endpoint(), m_Watcher( &m_NotificationPool ),	m_WatchQueue( "" ), 
	m_WatchQueueManager( "" ), m_TransportURI( "" ), m_SSLKeyRepos( "" ), m_SSLCypherSpec( "" ), m_SSLPeerName( "" ), 
	m_IsSigned( false ), m_IsIDsEnabled( false), m_IsCurrentMessageID( false ), 
	m_firstBatchId( "" ), m_PreviousImageRef( "" ), m_CurrentMessageId( "" ), m_CurrentGroupId( "" ), m_CurrentMessageLength( 0 ), m_CurrentSequence( 0 ), 
	m_BatchXsltFile( "" ), m_CurrentHelper( NULL ), m_StrictSwiftFormat ( "" ), m_SAAGroupFilter( NULL ), m_SAASingleFilter( NULL )
	
{
	DEBUG2( "CONSTRUCTOR" );

#ifndef NO_DB
	m_CurrentDatabase = NULL;
	m_CurrentProvider = NULL;
#endif
}
	
//destructor
MqFetcher::~MqFetcher()
{
	DEBUG2( "DESTRUCTOR" );

#ifndef NO_DB
	try
	{
		if ( m_CurrentDatabase != NULL )
		{
			delete m_CurrentDatabase;
			m_CurrentDatabase = NULL;
		}
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while releasing current database" );
		} catch( ... ){}
	}

	try
	{
		if ( m_CurrentProvider != NULL )
		{
			delete m_CurrentProvider;
			m_CurrentProvider = NULL;
		}
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while releasing current database provider" );
		} catch( ... ){}
	}
#endif
	if ( m_SAASingleFilter != NULL )
	{
		delete m_SAASingleFilter;
		m_SAASingleFilter = NULL;
	}

	if ( m_SAAGroupFilter != NULL )
	{
		delete m_SAAGroupFilter;
		m_SAAGroupFilter = NULL;
	}
}

void MqFetcher::Init()
{
	const int AUTO_ABANDON = 3;

	DEBUG( "INIT" );
	INIT_COUNTERS( &m_Watcher, MqFetcherWatcher );
	
	TransportHelper::TRANSPORT_HELPER_TYPE currentHelperType  = TransportHelper::parseTransportType ( getGlobalSetting( EndpointConfig::AppToMQ, EndpointConfig::MQSERVERTYPE ) );
	m_CurrentHelper = TransportHelper::CreateHelper( currentHelperType );

	m_Watcher.setHelperType( currentHelperType );

	m_WatchQueue = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::APPQUEUE );
	m_WatchQueueManager = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::WMQQMGR );

	string backupQueue = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::BAKQUEUE, "" );

	m_SSLKeyRepos = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::WMQKEYREPOS, "" );
	m_SSLCypherSpec = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::WMQSSLCYPHERSPEC, "" );
	m_SSLPeerName = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::WMQPEERNAME, "" );
	
	m_CertificateFileName = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::CERTIFICATEFILENAME, "" );
	m_CertificatePasswd = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::CERTIFICATEPASSWD, "" );

	m_RepliesQueue = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::RPLYQUEUE, "" );
	m_ReplyXsltFile = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::RPLXSLT, "" );

	string isSigned = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::ISSIGNED, "false" );
	m_IsSigned = ( isSigned == "true" );

	//Strict Swift Format config
	m_StrictSwiftFormat = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::STRICTSWIFTFMT , "" );
	m_LAUKey = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::LAUCERTIFICATE, "" );

	// if we have ssl, the channel definition is missing, so allow blank
	m_TransportURI = getGlobalSetting( EndpointConfig::AppToMQ, EndpointConfig::MQURI, "" );

	if( !m_StrictSwiftFormat.empty() )
	{
		if( m_SAASingleFilter != NULL  )
			delete m_SAASingleFilter;
		m_SAASingleFilter = new SwiftFormatFilter();
		if( m_StrictSwiftFormat == SwiftFormatFilter::SAA_FILEACT )
		{
			DEBUG( "SAA MQHA message should be part of wmq group" );
			boost::shared_ptr< BatchManager< BatchMQStorage > > batchManager( new BatchManager< BatchMQStorage > ( BatchManagerBase::MQ, BatchResolution::SYNCHRONOUS ) );
			batchManager->storage().initialize( currentHelperType );
			batchManager->storage().setQueue( m_WatchQueue );
			batchManager->storage().setQueueManager( m_WatchQueueManager );
			batchManager->storage().setTransportURI( m_TransportURI );
			batchManager->storage().setAutoAbandon( AUTO_ABANDON );
			if( !backupQueue.empty() )
				batchManager->storage().setBackupQueue( backupQueue );

			if( m_SAAGroupFilter != NULL  )
				delete m_SAAGroupFilter;
			m_SAAGroupFilter = new SAAFilter( batchManager );
		}
	}

	// ID config
	string isIDEnabled = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::ISIDENABLED, "false" );
	m_IsIDsEnabled = ( isIDEnabled == "true" );
	
	// ID certificate
	m_IDCertificateFileName = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::IDCERTIFICATEFILENAME, "" );
	m_IDCertificatePasswd = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::IDCERTIFICATEPASSWD, "" );
	
	// ID DB config
	m_DatabaseProvider = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::DBPROVIDER, "" );
	m_DatabaseName = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::DBNAME, "" );
	m_UserName = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::DBUSER, "" );
	m_UserPassword = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::DBPASS, "" );

	if ( haveGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::BATCHMGRTYPE ) )
	{
		string batchManagerType = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::BATCHMGRTYPE );		
		try
		{
			if( batchManagerType == "Flatfile" )
			{
				m_BatchManager = BatchManagerBase::CreateBatchManager( BatchManagerBase::Flatfile );
				BatchManager< BatchFlatfileStorage >* batchManager = static_cast< BatchManager< BatchFlatfileStorage >* >( m_BatchManager );

				string templateName = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::BATCHMGRTMPL, "" );				
				if ( templateName.empty() )
				{
					string messageSize = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::BATCHMGRFIXEDMQMSGSIZE );	
					batchManager->storage().setBufferSize( StringUtil::ParseLong( messageSize ) );
				}
				else
					batchManager->storage().setTemplate( templateName );
			}
			else if ( batchManagerType == "XMLfile" )
			{
				m_BatchManager = BatchManagerBase::CreateBatchManager( BatchManagerBase::XMLfile );
				if ( haveGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::BATCHMGRXSLT ) )
				{
					m_BatchXsltFile = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::BATCHMGRXSLT );
										
					BatchManager< BatchXMLfileStorage >* batchManager = static_cast< BatchManager< BatchXMLfileStorage >* >( m_BatchManager );

					//Set BatchXsltFile
					batchManager->storage().setXslt( m_BatchXsltFile );
				}
			}
			else
			{
				TRACE( "Batch manager type [" << batchManagerType << "] not implemented. No batch processing will be performed." );
			}
		}
		catch( const std::exception& ex )
		{
			TRACE( "Batch manager could not be created. [" << ex.what() << "]" );
			if ( m_BatchManager != NULL )
			{
				delete m_BatchManager;
				m_BatchManager = NULL;
			}
			throw;
		}
		catch( ... )
		{
			TRACE( "Batch manager could not be created. [unknown exception]" );
			if ( m_BatchManager != NULL )
			{
				delete m_BatchManager;
				m_BatchManager = NULL;
			}
			throw;
		}
	}
	else
	{
		DEBUG( "No batch manager specified in config file." );
	}
	
	m_Watcher.setQueue( m_WatchQueue );
	m_Watcher.setQueueManager( m_WatchQueueManager );
	m_Watcher.setTransportURI( m_TransportURI );
	m_Watcher.setSSLCypherSpec( m_SSLCypherSpec );
	m_Watcher.setSSLKeyRepository( m_SSLKeyRepos );
	m_Watcher.setSSLPeerName( m_SSLPeerName );

	//TODO check delete rights on m_WatchQueue - can't delete source otherwise
	m_CurrentHelper->setAutoAbandon( 3 );

	if ( backupQueue.length() )
		m_CurrentHelper->setBackupQueue( backupQueue );

	//instatiate DBProvider and Database only for ID-Enabled connectors
	//toate throw-urile de aici ar trebui sa se incheie cu terminate()
	if ( m_IsIDsEnabled )
	{
		if ( ( m_IDCertificateFileName.length() == 0 ) || ( m_IDCertificatePasswd.length() == 0 ) )
		{
			stringstream errorMessage;
			errorMessage << "Certificate filename [" << m_IDCertificateFileName << "] and/or password [" << m_IDCertificatePasswd.length() << "] settings missing. Please check the config file.";
			TRACE( errorMessage.str() );
			throw logic_error( errorMessage.str() );
		}
		if ( m_DatabaseProvider.length() == 0 )
		{
			TRACE( "Unable to find database provider definition. Please check the config file." );
			throw logic_error( "Unable to find database provider definition. Please check the config file." );
		}
		if ( m_DatabaseName.length() == 0 )
		{
			TRACE( "Unable to find database SID definition. Please check the config file." );
			throw logic_error( "Unable to find database SID definition. Please check the config file." );
		}
		if ( ( m_UserPassword.length() == 0 ) || ( m_UserName.length() == 0 ) ) 
		{
			TRACE( "Unable to find database credentials definition. Please check the config file." );
			throw logic_error( "Unable to find database credentials definition. Please check the config file." );
		}
			
#ifndef NO_DB
		// everything is defined for ID processing
		try
		{
			//Create Database Provider
			m_CurrentProvider = DatabaseProvider::GetFactory( m_DatabaseProvider );

			//Create Database
			if ( m_CurrentProvider != NULL )
			{
				m_CurrentDatabase = m_CurrentProvider->createDatabase();
				if ( m_CurrentDatabase == NULL )
					throw runtime_error( "Unable to create database " );
			}
			else
			{
				throw runtime_error( "Unable to create database provider" );
			}
		}
		catch( const std::exception& error )
		{
			if ( m_CurrentProvider != NULL )
				delete m_CurrentProvider;
			if ( m_CurrentDatabase != NULL )
				delete m_CurrentDatabase;

			DEBUG( "Create database error : [" << error.what() << "]" );
			throw;
		}
		catch( ... )
		{
			if ( m_CurrentProvider != NULL )
				delete m_CurrentProvider;
			if ( m_CurrentDatabase != NULL )
				delete m_CurrentDatabase;

			DEBUG( "Unknown error while creating database" );
			throw;
		}
#endif
	}
}

void MqFetcher::internalStart()
{
	DEBUG( "Starting watcher... " );
	m_Watcher.setEnableRaisingEvents( true );

	TRACE_SERVICE( "[" << m_ServiceThreadId << "] starting to process... " );

	try
	{
		bool succeeded = false;
		while( m_Running )
		{
			DEBUG( "Fetcher [" << m_SelfThreadId << "] waiting for notifications in pool" );
			WorkItem< AbstractWatcher::NotificationObject > notification = m_NotificationPool.removePoolItem();
			
			AbstractWatcher::NotificationObject *notificationObject = notification.get();

			// if we're getting groups, and the previous group is the same as this batch, skip notification
			if ( succeeded && ( notificationObject->getObjectGroupId() == m_CurrentGroupId ) && ( notificationObject->getObjectId() == m_CurrentMessageId ) )
			{
				TRACE( "This message: Message Id = [" << m_CurrentMessageId << "], Group Id = [" << m_CurrentGroupId << "] was already processed. Skipping ..." );
				continue;
			}

			m_CurrentMessageId = notificationObject->getObjectId();
			m_CurrentMessageLength = notificationObject->getObjectSize();
			m_CurrentGroupId = notificationObject->getObjectGroupId();
	
			DEBUG( "Notified : [" << m_CurrentMessageId << "] in group [" << m_CurrentGroupId
				<< "] size [" << m_CurrentMessageLength << "]" );

			m_CurrentSequence = 0;
			m_IsLast = false;
			m_firstBatchId= "";
			//TODO throw only on fatal error. The connector should respawn this thread
			succeeded = true;
			try
			{
				DEBUG( "Performing message loop ... " );
				succeeded = PerformMessageLoop( m_BatchManager != NULL );
				DEBUG( "Message loop finished ok. " );
			}
			catch( const AppException& ex )
			{
				string exceptionType = typeid( ex ).name();
				string errorMessage = ex.getMessage();
				
				TRACE_GLOBAL( exceptionType << " encountered while processing message : " << errorMessage );
				succeeded = false;
			}
			catch( const std::exception& ex )
			{
				string exceptionType = typeid( ex ).name();
				string errorMessage = ex.what();
				
				TRACE_GLOBAL( exceptionType << " encountered while processing message : " << errorMessage );
				succeeded = false;
			}
			catch( ... )
			{
				TRACE_GLOBAL( "[unknown exception] encountered while processing message. " );
				succeeded = false;
			}

			if ( !succeeded && m_Running )
			{
				TRACE( "Sleeping 10 seconds before next attempt( previous message failed )" );
				sleep( 10 );
			}
		} //while true
	}
	catch( const WorkPoolShutdown& shutdownError )
	{
		TRACE_GLOBAL( shutdownError.what() );
	}
	catch( ... )
	{
		TRACE_GLOBAL( "Unhandled exception encountered while processing. " );
	}
}

void MqFetcher::internalStop()
{
	DEBUG( "STOP" );
	
	// ensure watcher is dead ( the watcher will lock on pool and wait until it is empty )
	m_Watcher.setEnableRaisingEvents( false );

	// wait for fetcher to remove all items if the thread is running
	//if ( 0 == pthread_kill( m_SelfThreadId, 0 ) )
	//{
		//m_NotificationPool.waitForPoolEmpty();
	//}
	// the endpoint must now be locked on pool

	// the watcher must be dead by now...
	m_Running = false;

	m_NotificationPool.ShutdownPool();
	
	TRACE( m_ServiceThreadId << " joining endpoint ..." );
	int joinResult = pthread_join( m_SelfThreadId, NULL );
	if ( 0 != joinResult )
	{
		TRACE( "Joining self thread ended in error [" << joinResult << "]" );
	}

	// close queue, disconnect
	m_CurrentHelper->closeQueue();
	m_CurrentHelper->disconnect();
}

string MqFetcher::Prepare()
{
	DEBUG( "PREPARE" );
		
	// try to connect and open the queue
	// this ops. will throw if an error occurs ... no plan B for now
	/*	NameValueCollection sslOptions;
	
	if ( m_SSLKeyRepos.length() > 0 )
		sslOptions.Add( "KEYREPOS", m_SSLKeyRepos);
	if ( m_SSLCypherSpec.length() > 0 )
		sslOptions.Add( "SSLCYPHERSPEC", m_SSLCypherSpec );
	if ( m_SSLPeerName.length() > 0 )
		sslOptions.Add( "SSLPEERNAME", m_SSLPeerName );
	*/
	if ( m_SSLKeyRepos.length() > 0 )
//	if( sslOptions.getCount() >  0 ) 
		m_CurrentHelper->connect( m_WatchQueueManager, m_TransportURI,/* sslOptions*/ m_SSLKeyRepos, m_SSLCypherSpec, m_SSLPeerName );
	else
		m_CurrentHelper->connect( m_WatchQueueManager, m_TransportURI );

	m_CurrentHelper->openQueue( m_WatchQueue );

	return m_CurrentMessageId;
}

string MqFetcher::GetIDImageReference( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument *doc )
{
	string imgRef = "";

	if ( ( doc == NULL ) || ( doc->getDocumentElement() == NULL ) )
		throw logic_error( "Document is empty" );

	const DOMElement* root = doc->getDocumentElement();
	DOMNodeList* refNodes = root->getElementsByTagName( unicodeForm( "InstrRef" ) );
	if ( ( refNodes == NULL ) || ( refNodes->getLength() == 0 ) )
		throw logic_error( "Missing required [InstrRef] element [root//InstrRef] element" );

	const DOMElement* refNode = dynamic_cast< DOMElement* >( refNodes->item( 0 ) );
	if ( refNode == NULL )
		throw logic_error( "Bad type : [root//InstrRef[0]] should be and element" );

	DOMText* reftext = dynamic_cast< DOMText* >( refNode->getFirstChild() );
	if ( reftext == NULL )
		throw runtime_error( "Missing required [TEXT] element child of [root//InstrRef[0]]" );

	imgRef = localForm( reftext->getData() );
	return imgRef;
}

void MqFetcher::Process( const string& correlationId )
{
	DEBUG( "PROCESS" );
	DEBUG( "Current message length : " << m_CurrentMessageLength );

	m_TransportHeaders.Clear();
	string wmqFormat = TransportHelper::TMT_STRING;
	long mqType = TransportHelper::TMT_DATAGRAM;
	long mqFeedback = 0;
	string mqPutTime = "";

	WorkItem< ManagedBuffer > managedBuffer( new ManagedBuffer() );
	ManagedBuffer* buffer = managedBuffer.get();

	if ( m_CurrentSequence == 0 )
	{		
		try
		{
			long result = -1;
			unsigned char emptyGroupId[24];
			for( int i = 0; i < 24; i++ ) emptyGroupId[i] = '\0';
			bool isSingle = ( Base64::encode( emptyGroupId, 24 ) == m_CurrentGroupId || m_CurrentGroupId.empty()  ) ? true : false;

			if( isSingle )
			{
				//TODO check return code
				m_CurrentHelper->setMessageId( m_CurrentMessageId );
				result = m_CurrentHelper->getOne( buffer, true );

				DEBUG2( "OriginalMessage is : [" << buffer->str() << "]" );
				if( m_CurrentMessageLength != buffer->size() )
				{
					TRACE( "The current message size is not the same as notification size, override the current message length to " << buffer->size() );
					m_CurrentMessageLength = buffer->size();
				}

				wmqFormat = m_CurrentHelper->getLastMessageFormat();
				mqType = m_CurrentHelper->getLastMessageType();
				if (  mqType == TransportHelper::TMT_REPLY )
					mqFeedback = m_CurrentHelper->getLastFeedback();
				time_t putTime = m_CurrentHelper->getMessagePutTime();
				mqPutTime = TimeUtil::Get( "%d/%m/%Y %H:%M:%S", 19, &putTime );
			}

			// with StrictSwiftFormat::SAA_FILEACT messages will be get as MQ groups
			// with StrictSwiftFormat::SAA_FIN messages will only be LAU checked
			if( !m_StrictSwiftFormat.empty() )
			{
				WorkItem<ManagedBuffer> managedInputBuffer( new ManagedBuffer() );
				ManagedBuffer* inputBuffer = managedInputBuffer.get();

				WorkItem< ManagedBuffer > managedOutputBuffer ( new ManagedBuffer( ) );
				ManagedBuffer* outputBuffer = managedOutputBuffer.get();

				NameValueCollection transportHeaders;
				transportHeaders.Add( MqFilter::MQGROUPID, m_CurrentGroupId );
				transportHeaders.Add( SwiftFormatFilter::LAU_KEY, m_LAUKey );
				transportHeaders.Add( SwiftFormatFilter::SERVICE_KEY, m_StrictSwiftFormat );

				if( isSingle )
				{
					inputBuffer->copyFrom( buffer->buffer(), m_CurrentMessageLength );
					m_SAASingleFilter->ProcessMessage( managedInputBuffer, managedOutputBuffer, transportHeaders, true );
				}
				else if( m_SAAGroupFilter != NULL )
					m_SAAGroupFilter->ProcessMessage( managedInputBuffer, managedOutputBuffer, transportHeaders, true );
				else
				{
					AppException aex( "Group messages process only if AppToMQSeries.StrictSWIFTFormat key is SAA_FILEACT", EventType::Error );
					aex.setSeverity( EventSeverity::Fatal );

					throw aex;
				}

				buffer->copyFrom( managedOutputBuffer.get() );
				if( mqPutTime.empty() )
					 mqPutTime = transportHeaders[SAAFilter::MESSAGE_DATE];
				result = 0;
			}

			// no message
			if ( result == -1 )
			{
				AppException aex( "No message matching the specified ids was found.", EventType::Warning );
				aex.setSeverity( EventSeverity::Fatal );

				throw aex;
			}
			// the message was moved to dead letter! If message is not member of a group, transaction is already commited by m_CurrentHelper->getOne();
			if ( result == -2 )
			{
				AppException aex( "Undeliverable message sent to dead letter queue because the backout count was exceeded", EventType::Warning );
				aex.setSeverity( EventSeverity::Fatal );

				throw aex;
			}
		}
		catch( const AppException& ex )
		{
			// format error
			stringstream errorMessage;
			errorMessage << "Can't read the message [" << m_CurrentMessageId << 
				"] from queue [" << m_WatchQueue << "]. Check inner exception for details.";
				
			TRACE( errorMessage.str() );
			TRACE( ex.getMessage() );
			
			throw;
		}
		catch( const std::exception& ex )
		{
			// format error
			stringstream errorMessage;
			errorMessage << "Can't read the message [" << m_CurrentMessageId << 
				"] from queue [" << m_WatchQueue << "]. Check inner exception for details.";
				
			TRACE( errorMessage.str() );
			TRACE( ex.what() );
			
			//TODO check this for returning a pointer to a const ref
			throw AppException( errorMessage.str(), ex );
		}
		catch( ... )
		{
			// format error
			stringstream errorMessage;
			errorMessage << "Can't read the message [" << m_CurrentMessageId << 
				"] from queue [" << m_WatchQueue << "]. Reason unknown";
			
			TRACE( errorMessage.str() );
			
			throw AppException( errorMessage.str() );
		}

		if( m_IsSigned )
		{
			SSLFilter* sslFilter = NULL;

			try
			{
				sslFilter = new SSLFilter();

				DEBUG( "Applying SSL filter..." );

				WorkItem< ManagedBuffer > managedInputBuffer( new ManagedBuffer() );
				ManagedBuffer* inputBuffer = managedInputBuffer.get();

				unsigned long dataLength = m_CurrentMessageLength;
				//skip headers ( jms )
				inputBuffer->copyFrom( buffer->buffer(), dataLength );

				WorkItem< ManagedBuffer > managedOutputBuffer( new ManagedBuffer() );
				ManagedBuffer* outputBuffer = managedOutputBuffer.get();

				sslFilter->ProcessMessage( managedInputBuffer, managedOutputBuffer, m_TransportHeaders, true );

				buffer->copyFrom( outputBuffer );

				DEBUG( "Current message is : [" << buffer->str() << "]" );

				if( sslFilter != NULL )
				{
					delete sslFilter;
					sslFilter = NULL;
				}

			}
			catch( const std::exception& ex )
			{
				if ( sslFilter != NULL )
				{
					delete sslFilter;
					sslFilter = NULL;
				}
				throw;
			}
			catch( ... )
			{
				if( sslFilter != NULL )
				{
					delete sslFilter;
					sslFilter = NULL;
				}
				TRACE( "Error while applying ssl filter" );
				throw;
			}

		}

#ifndef NO_DB
		m_IsCurrentMessageID = false;
		if( m_IsIDsEnabled && ( m_CurrentHelper->getLastMessageFormat() != TransportHelper::TMT_STRING ) )
		{	
			DEBUG( "Starting to process DI message ..." );
			SSLFilter* sslFilterID = NULL;
			try
			{
				m_firstBatchId = Collaboration::GenerateGuid();
				m_IsCurrentMessageID = true;

				if ( m_CurrentDatabase == NULL )
					throw logic_error( "Database not created" );

				while ( !m_CurrentDatabase->IsConnected() )
				{
					DEBUG( "Connecting to database ..." );
					ConnectionString connString = ConnectionString( m_DatabaseName, m_UserName, m_UserPassword );
					m_CurrentDatabase->Connect( connString );

					if ( !m_CurrentDatabase->IsConnected() )
					{
						TRACE( "Still not connected to database. Sleeping 30 seconds before retrying..." );
						sleep( 30 );
					}
				}

				m_CurrentDatabase->BeginTransaction();

				DEBUG( "Getting zip entries from DI batch ..." );
				BatchManager< BatchZipArchiveStorage > zipBatchManager( BatchManagerBase::ZIP, BatchResolution::SYNCHRONOUS );
				zipBatchManager.storage().setBuffer( buffer->buffer(), buffer->size() );
				//zipBatchManager.storage().setDequeFirst(".xml");
				zipBatchManager.open(  m_CurrentGroupId, ios_base::in | ios_base::binary );

				WorkItem< ManagedBuffer > managedBufferCrt( new ManagedBuffer() );
				ManagedBuffer* crtBuffer = managedBufferCrt.get();
				
				sslFilterID = new SSLFilter ();
				sslFilterID->addProperty( SSLFilter::SSLCERTFILENAME, m_IDCertificateFileName );
				sslFilterID->addProperty( SSLFilter::SSLCERTPASSWD, m_IDCertificatePasswd );

				while ( zipBatchManager.moreMessages() )
				{
					BatchItem zipItem;

					zipBatchManager >> zipItem;
					crtBuffer->copyFrom( zipItem.getBinPayload() );
					string eyecatcher = zipItem.getEyecatcher();
					string eyecatcherExt = ( eyecatcher.length() >= 4) ? eyecatcher.substr( eyecatcher.length() - 4 ) : eyecatcher;
					if ( eyecatcherExt == ".p7m" )
					{
						DEBUG ( "Unsigning DI zip entry..." );
						string checkedInputString = crtBuffer->str();
						checkedInputString = StringUtil::Replace( checkedInputString, "\r\n", "" );
						checkedInputString = StringUtil::Replace( checkedInputString, "\n", "" );
						if ( ( checkedInputString.length() > 64 ) )
						{
							SSLFilter::Hack64bLines( checkedInputString );
						}
						if ( SSLFilter::IsSigned( checkedInputString ) )
						{
							ManagedBuffer* inputSignedBuffer = new ManagedBuffer();
							inputSignedBuffer->copyFrom( checkedInputString );
							AbstractFilter::buffer_type inputSignedDataBuffer( inputSignedBuffer );

							ManagedBuffer* outputBufferUnsigned = new ManagedBuffer();
							AbstractFilter::buffer_type outputDataBufferUnsigned( outputBufferUnsigned );
							NameValueCollection transportHeaders;
							sslFilterID->ProcessMessage( inputSignedDataBuffer, outputDataBufferUnsigned, transportHeaders, true );

							crtBuffer->copyFrom( outputBufferUnsigned );
						}

						checkedInputString = "";
						eyecatcher = eyecatcher.substr( 0, eyecatcher.length() - 4 );
					}
					if ( eyecatcher.find( ".xml" ) != string::npos )
					{
						buffer->copyFrom( crtBuffer );
					}
					if ( eyecatcher.find( ".tif" ) != string::npos )
					{	
						string imgRef = "";
						TIFF* tif = NULL;
						const string tiffFileContents = crtBuffer->str();
						try
						{
							istringstream tempTiffFile( tiffFileContents );
							tif = TIFFStreamOpen( "tempTiffFile", &tempTiffFile );
							if ( tif == NULL )
								throw logic_error( "Unable to open DI image." );

							char* imgTagDescr = 0;
							if ( !TIFFGetField( tif, TIFFTAG_IMAGEDESCRIPTION, &imgTagDescr ) )
								throw runtime_error( "Error encountered while fetching DI IMAGEDESCRIPTION TAG from file" );

							imgRef = imgTagDescr;
							if ( imgRef.empty() )
								throw logic_error( "Extracted reference from DI image is blank." );

							TIFFClose( tif );
						}
						catch( ... )
						{
							if ( tif != NULL )
								TIFFClose( tif );

							throw;
						}
						
						string tempLobStr = Base64::encode( tiffFileContents );
						string guid = Collaboration::GenerateGuid();
						
						ParametersVector myParams;
						DataParameterBase *paramGuid = m_CurrentProvider->createParameter( DataType::CHAR_TYPE );
						paramGuid->setDimension( guid.length() );	  	 
						paramGuid->setString( guid );
						paramGuid->setName( "Guid" );
						myParams.push_back( paramGuid );

						DataParameterBase *paramCorrelId = m_CurrentProvider->createParameter( DataType::CHAR_TYPE );
						paramCorrelId->setDimension( correlationId.length() );	  	 
						paramCorrelId->setString( correlationId );
						paramCorrelId->setName( "CorrelId" );
						myParams.push_back( paramCorrelId );

						DataParameterBase *paramBatchId = m_CurrentProvider->createParameter( DataType::CHAR_TYPE );
						paramBatchId->setDimension( m_firstBatchId.length() );	  	 
						paramBatchId->setString( m_firstBatchId );
						paramBatchId->setName( "BatchId" );
						myParams.push_back( paramBatchId );

						DataParameterBase *paramImgRef = m_CurrentProvider->createParameter( DataType::CHAR_TYPE );
						paramImgRef->setDimension( imgRef.length() );	  	 
						paramImgRef->setString( imgRef );
						paramImgRef->setName( "ImageRef" );
						myParams.push_back( paramImgRef );

						DataParameterBase *paramImage = m_CurrentProvider->createParameter( DataType::BINARY );
						paramImage->setDimension( tempLobStr.length() );	  	 
						paramImage->setString( tempLobStr );
						paramImage->setName( "Image" );
						myParams.push_back( paramImage );
						try
						{
							DEBUG( "Inserting one DI image to database. Image reference is [" << imgRef << "]" );
							m_CurrentDatabase->ExecuteNonQuery( DataCommand::SP, "InsertImgBlobsqueue", myParams );
						}
						catch( const std::exception& ex )
						{
							string exceptionType = typeid( ex ).name();
							TRACE( exceptionType << " encountered while inserting DI image to database. Image reference is [" << imgRef << "]"  );
							throw;
						}
						catch( ... )
						{
							TRACE( " Error encountered while inserting DI image to database. Image reference is [" << imgRef << "]" );
							throw;
						}
					} // if tif
				} //while
				if ( sslFilterID != NULL )
				{
					delete sslFilterID;
					sslFilterID = NULL;
				}
			}
			catch( const std::exception& ex )
			{	
				m_firstBatchId = "";

				if ( sslFilterID != NULL )
				{
					delete sslFilterID;
					sslFilterID = NULL;
				}
				throw;
			}
			catch( ... )
			{
				m_firstBatchId = "";

				if ( sslFilterID != NULL )
				{
					delete sslFilterID;
					sslFilterID = NULL;
				}
				throw;
			}
		}// end DI pre-process
#endif

		DEBUG( "Message is in buffer; size is : " << buffer->size() );

		// If BatchManager then the message read from mq is a batch Xml
		// ordinary messages will be read from Batch and processed by filters
		
		// open BatchXMLFile
		if ( m_BatchManager != NULL )
		{
			DEBUG_GLOBAL( "First batch item ... opening storage" );
			// open batch ( next iteration it will already be open )
			BatchManager< BatchXMLfileStorage >* batchXmlManager = dynamic_cast< BatchManager< BatchXMLfileStorage >* >( m_BatchManager );
			if ( batchXmlManager == NULL )
			{
				BatchManager< BatchFlatfileStorage >* batchFileManager = static_cast< BatchManager< BatchFlatfileStorage >* >( m_BatchManager );
				batchFileManager->storage().open( buffer->str() );
			}
			else
			{
				// this may fail for non-xml messages ... 
				try
				{
					if( m_IsSigned || m_IsCurrentMessageID || ( !m_StrictSwiftFormat.empty() ) )
						batchXmlManager->storage().setSerializedXml( buffer->buffer(), buffer->size() );
					else
						//Set batch Xml message read from MQ, skip headers ( jms )
						batchXmlManager->storage().setSerializedXml(  buffer->buffer(), buffer->size() );
				}
				catch( ... )
				{
					// HACK for ach text messages that can't be parsed
					throw;
				}
				m_BatchManager->open( m_CurrentGroupId, ios_base::in );
			}
		}
	}
	
	WorkItem< ManagedBuffer > managedInputBuffer( new ManagedBuffer() );
	ManagedBuffer* inputBuffer = managedInputBuffer.get();

	string batchId = "";

	// if it's a batch, dequeue every message
	if ( m_BatchManager != NULL )
	{
		if ( !m_IsLast )
		{
			BatchItem item;
			*m_BatchManager >> item;

			//m_CurrentGroupId = item.getBatchId();
			m_IsLast = item.isLast();
			
			inputBuffer->copyFrom( item.getPayload() );
			m_CurrentMessageLength = inputBuffer->size();
			batchId = item.getBatchId();
		}
		else
			throw logic_error( "Attempt to read past the end of batch" );
	}
	else
	{
		batchId = m_CurrentGroupId.substr( 0, 30 );
		m_IsLast = true;

		if( m_IsSigned || ( !m_StrictSwiftFormat.empty() ) )
		{
			m_CurrentMessageLength = buffer->size();
			inputBuffer->copyFrom( buffer->buffer(), buffer->size(), m_CurrentMessageLength );
		}
		else
			inputBuffer->copyFrom( buffer->buffer(), m_CurrentMessageLength );
	}

#ifndef NO_DB
	// update batchId and correlationId for DI images
	if( m_IsCurrentMessageID )
	{
		if ( batchId == "" ) 
			throw logic_error( "Error encounter while update batch id. for current DI image [ batch id. is blank ]" );
		else if ( m_firstBatchId == "" )
			throw logic_error( "Error encounter while update batch id. for current DI image " );
		else
		{
			XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument *currentDoc = NULL;
			string imageRef = "";
			try
			{
				currentDoc = XmlUtil::DeserializeFromString( inputBuffer->str() );
				imageRef = GetIDImageReference ( currentDoc );
				if( imageRef == "" )
					throw logic_error( "Error encounter while geting DI image reference [ image reference is blank ]" );
				
				if ( currentDoc != NULL )
				{
					currentDoc->release();
					currentDoc = NULL;
				}
	
				ParametersVector updParams;
				DataParameterBase *paramBatchId = m_CurrentProvider->createParameter( DataType::CHAR_TYPE );
				paramBatchId->setDimension( batchId.length() );	  	 
				paramBatchId->setString( batchId );
				paramBatchId->setName( "BatchId" );
				updParams.push_back( paramBatchId );

				DataParameterBase *paramCorrelId = m_CurrentProvider->createParameter( DataType::CHAR_TYPE );
				paramCorrelId->setDimension( correlationId.length() );	  	 
				paramCorrelId->setString( correlationId );
				paramCorrelId->setName( "CorrelationId" );
				updParams.push_back( paramCorrelId );

				DataParameterBase *paramFirstBatchId = m_CurrentProvider->createParameter( DataType::CHAR_TYPE );
				paramFirstBatchId->setDimension( m_firstBatchId.length() );	  	 
				paramFirstBatchId->setString( m_firstBatchId );
				paramFirstBatchId->setName( "FirstBatchId" );
				updParams.push_back( paramFirstBatchId );

				DataParameterBase *paramImageRef = m_CurrentProvider->createParameter( DataType::CHAR_TYPE );
				paramImageRef->setDimension( imageRef.length() );	  	 
				paramImageRef->setString( imageRef );
				paramImageRef->setName( "ImageRef" );
				updParams.push_back( paramImageRef );
				
				m_CurrentDatabase->ExecuteNonQuery( DataCommand::SP, "UpdateCorrIDBlobsqueue", updParams );
				DEBUG( "Update correlation id for DI image. Image correlation id is [" << correlationId << "]" );
			}
			catch ( const std::exception& ex )
			{	
				if ( currentDoc != NULL )
				{
					currentDoc->release();
					currentDoc = NULL;
				}
				m_firstBatchId = "";
				string exceptionType = typeid( ex ).name();
				TRACE( exceptionType << " encountered while updating batch id for DI images "  );
				throw;
			}
			catch (...)
			{
				if ( currentDoc != NULL )
				{
					currentDoc->release();
					currentDoc = NULL;
				}
				m_firstBatchId = "";
				TRACE( " Error encountered while updating batch id for DI images " );
				throw;
			}

		}
	}
#endif
	// delegate work to the chain of filters

	AbstractFilter::buffer_type filterChainOutput( m_RepliesQueue.empty()?NULL:new ManagedBuffer() );
	try
	{
		m_TransportHeaders.Add( "XSLTPARAMBATCHID", StringUtil::Pad( batchId, "\'","\'" ) );
		m_TransportHeaders.Add( "XSLTPARAMGUID", StringUtil::Pad( Collaboration::GenerateGuid(), "\'","\'" ) );
	  	m_TransportHeaders.Add( "XSLTPARAMCORRELID", StringUtil::Pad( correlationId, "\'","\'" ) );
	  	m_TransportHeaders.Add( "XSLTPARAMSESSIONID", "'0001'" );
	  	m_TransportHeaders.Add( "XSLTPARAMREQUESTOR", StringUtil::Pad( getServiceName(), "\'","\'" ) );
	  	m_TransportHeaders.Add( "XSLTPARAMREQUEST", "'SingleMessage'" );
		m_TransportHeaders.Add( MqFilter::MQAPPNAME, getServiceName() );
	  	
	  	if( m_CurrentHelper->getLastMessageType() == TransportHelper::TMT_REQUEST )
	  	{
	  		DEBUG( "Message is REQUEST so we add the reply settings to transport headers" );
	  		m_TransportHeaders.Add( MqFilter::MQMESSAGETYPE, "REQUEST" );
	  		m_TransportHeaders.Add( MqFilter::MQREPLYQUEUE, m_CurrentHelper->getLastReplyQueue() );
	  		m_TransportHeaders.Add( MqFilter::MQREPLYQUEUEMANAGER, m_CurrentHelper->getLastReplyQueueManager() );
	  		m_TransportHeaders.Add( MqFilter::MQMSGID, m_CurrentHelper->getLastMessageId() );
	  		m_TransportHeaders.Add( MqFilter::MQMSGCORELID, m_CurrentHelper->getLastCorrelId() );
	  		m_TransportHeaders.Add( MqFilter::MQREPLYOPTIONS, ( m_CurrentHelper->getLastReplyOptions() ).ToString() );

	  		//ssl options
	  		m_TransportHeaders.Add( MqFilter::MQSSLKEYREPOSITORY, m_SSLKeyRepos );
	  		m_TransportHeaders.Add( MqFilter::MQSSLCYPHERSPEC, m_SSLCypherSpec );
	  		m_TransportHeaders.Add( MqFilter::MQSSLPEERNAME, m_SSLPeerName );
	  	}
		
		stringstream formatFeedback;
		formatFeedback << "W|" << m_CurrentHelper->getLastCorrelId() << "|";
		
		if ( m_CurrentHelper->getLastMessageType() == TransportHelper::TMT_REPLY )
			formatFeedback << m_CurrentHelper->getLastFeedback();
		else 
			formatFeedback << "0";
		
		string formattedFeedbackStr = formatFeedback.str();
		m_TransportHeaders.Add( "XSLTPARAMFEEDBACK", StringUtil::Pad( formattedFeedbackStr, "\'","\'" ) );
		
		time_t putTime = m_CurrentHelper->getMessagePutTime();
		m_TransportHeaders.Add( "MQPUTMESSAGETIME", TimeUtil::Get( "%d/%m/%Y %H:%M:%S", 19, &putTime ) );
		
		// as the message will end up in MQ, no output is needed
		MEM_CHECKPOINT_INIT();
		try
		{
			trackMessage( inputBuffer->str(), m_TransportHeaders );
			m_FilterChain->ProcessMessage( managedInputBuffer, filterChainOutput, m_TransportHeaders, true );
		}
		catch( const FilterInvalidMethod& ex )
		{
			DEBUG( "Falling back to deserializing input message to an XML... " );
			//try using a dom as input
			XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument *doc = NULL;
			try
			{
				doc = XmlUtil::DeserializeFromString( inputBuffer->str() );
				m_FilterChain->ProcessMessage( doc, filterChainOutput, m_TransportHeaders, true );
			}
			catch( ... )
			{
				if ( doc != NULL )
				{
					doc->release();
					doc = NULL;
				}
				throw;
			}
			if ( doc != NULL )
			{
				doc->release();
				doc = NULL;
			}
		}
		MEM_CHECKPOINT_END( "message processing", "MqFetcher" );
	}
	// base class( Endpoint ) will get error specifics and report it further
	catch( ... )
	{
		try
		{
			m_TransportHeaders.Dump();
		} catch( ... ){};

		throw;
	}

	if ( !m_RepliesQueue.empty() )
	{
		XALAN_USING_XERCES( XMLPlatformUtils )
		XALAN_USING_XALAN( XercesDocumentWrapper );
		XALAN_USING_XALAN( XalanDocument );

		XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* xml = NULL;
		try
		{
			xml = XmlUtil::DeserializeFromString( filterChainOutput.get()->str() );

			XercesDocumentWrapper docWrapper( *XMLPlatformUtils::fgMemoryManager, xml, true, true, true );
			string base64Payload = XPathHelper::SerializeToString( XPathHelper::Evaluate( "/qPCMessageSchema/Message/Payload/Transformed/child::text()", &docWrapper ) );
			string service = XPathHelper::SerializeToString( XPathHelper::Evaluate( "/qPCMessageSchema/Message/RequestorService/child::text()", &docWrapper ) );
			string payload = Base64::decode( base64Payload );

			xml->release();
			xml = NULL;

			xml = XmlUtil::DeserializeFromString( payload );

			XSLTFilter finalTransform;
			NameValueCollection transportHeaders;

			transportHeaders.Add( XSLTFilter::XSLTFILE, m_ReplyXsltFile );
			transportHeaders.Add( XSLTFilter::XSLTUSEEXT, "true" );
			transportHeaders.Add( "XSLTPARAMREQUESTOR", '\''+service+'\'' );

			AbstractFilter::buffer_type outputBuffer( new ManagedBuffer() );
			finalTransform.ProcessMessage( xml, outputBuffer, transportHeaders, true );

			xml->release();
			xml = NULL;

			m_CurrentHelper->openQueue( m_RepliesQueue );
			m_CurrentHelper->putOne( outputBuffer.get()->buffer(), outputBuffer.get()->size() );
		}
		catch (...)
		{
			if ( xml != NULL )
			{
				xml->release();
				xml = NULL;
			}

			throw;
		}
	}

}

void MqFetcher::Commit()
{
	if ( m_IsLast )
	{
		DEBUG( "COMMIT [last message]" );

#ifndef NO_DB
		//TODO check return code
		if ( m_IsCurrentMessageID )
		{
			try
			{
				m_IsCurrentMessageID = false;
				m_CurrentDatabase->EndTransaction( TransactionType::COMMIT );
			}
			catch( ... )
			{
				TRACE( "An error occured in commit [unknown reason]" );
				throw;
			}
		}		
#endif
		m_FilterChain->Commit();
		if( m_SAAGroupFilter != NULL )
			m_SAAGroupFilter->Commit();
		m_CurrentHelper->commit();
		
		if( m_BatchManager != NULL )
			m_BatchManager->close( m_CurrentGroupId );	
	}
	else
	{
		DEBUG( "COMMIT [message #" << m_CurrentSequence++ << "]" );
	}
}

///<Summary>
///Aborts the current message/batch
///</Summary>
///<Note>
///Rollback messages that were put and the commit messages that we got.
///Messages that we got should be moved to dead letter queue instead of being committed.
///</Note>
void MqFetcher::Abort()
{
	DEBUG( "ABORT" );

	// Rollback filter chain( messages put to mq )
	m_FilterChain->Rollback();
	if( m_SAAGroupFilter != NULL )
		m_SAAGroupFilter->Commit();

	//TODO move the message to dead letter queue or backout queue... for now, commit message got
	m_CurrentHelper->commit();
	
	if( m_BatchManager != NULL )
		m_BatchManager->close( m_CurrentGroupId );

#ifndef NO_DB
	if( m_IsCurrentMessageID && m_CurrentDatabase->IsConnected() )
	{
		m_IsCurrentMessageID = false;
		m_CurrentDatabase->EndTransaction( TransactionType::ROLLBACK );
	}
#endif
}

void MqFetcher::Rollback()
{
	DEBUG( "ROLLBACK" );
	
	// TODO check return codes...	
	// Rollback filter chain( messages put to mq )
	m_FilterChain->Rollback();
	if( m_SAAGroupFilter != NULL )
		m_SAAGroupFilter->Rollback();
	// Rollback current helper ( the message got from mq )
	m_CurrentHelper->rollback();

#ifndef NO_DB
	// Rollback db transaction for ID
	if( m_IsCurrentMessageID && m_CurrentDatabase->IsConnected() )
	{
		m_IsCurrentMessageID = false;
		m_CurrentDatabase->EndTransaction( TransactionType::ROLLBACK );
	}
#endif

	// close the batch, the whole batch will be retried
	if( m_BatchManager != NULL )
	{
		m_BatchManager->close( m_CurrentGroupId );
		m_IsLast = true;
	}
}

bool MqFetcher::moreMessages() const
{
	DEBUG( "Checking for more messages ..." );

	// Always return false when no batch manager is specified in cfg.
	if ( m_BatchManager == NULL )
	{	
		DEBUG2( "Batch manager is NULL ..." );
		return false;
	}
	else
	{	
		DEBUG2( "Batch manager is not NULL ..." );
		bool batchManagerAnswer = m_BatchManager->moreMessages();
		
		if ( !batchManagerAnswer && !m_IsLast )
		{
			DEBUG( "There are more messages in batch ..." );
			return true;
		}
		return batchManagerAnswer;
	}
}

//void MqFetcher::NotificationCallback( string groupId, string messageId, int messageLength )
//{
//	DEBUG_GLOBAL( "NotificationCallback invoked." );
//
//	m_CurrentMessageId = messageId;
//	m_CurrentMessageLength = messageLength;
//	m_CurrentGroupId = groupId;
//	
//	DEBUG_GLOBAL( "Notified : [" << m_CurrentMessageId << "] size [" << m_CurrentMessageLength << "]" );
//
//	m_CurrentSequence = 0;
//	m_IsLast = false;
//	
//	try
//	{
//		DEBUG_GLOBAL( "Perform message loop" );
//		ConnectorInstance->getFetcher()->PerformMessageLoop( ConnectorInstance->getFetcher()->getBatchManager() != NULL );
//	}
//	catch( const exception &e )
//	{
//		TRACE_GLOBAL( "Perform message loop exception [" << e.what() << "]" );
//		throw;
//	}
//	catch( ... )
//	{
//		TRACE_GLOBAL( "Perform message loop exception"  );
//		throw;
//	}
//}
