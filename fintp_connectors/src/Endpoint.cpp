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

#ifdef WIN32
	/*#ifdef _DEBUG
		//#define _CRTDBG_MAP_ALLOC
		#include <stdlib.h>
		#include <crtdbg.h>
	#endif*/
#endif

#ifdef LINUX
#include <errno.h>
#endif

#include "Endpoint.h"
#include "Trace.h"
#include "LogManager.h"

#include <sstream>

#include "FileConnector/FileFetcher.h"
#include "FileConnector/FilePublisher.h"
#include "MqConnector/MqFetcher.h"
#include "MqConnector/MqPublisher.h"
#ifndef NO_DB
#include "DbConnector/DbPublisher.h"
#include "DbConnector/DbFetcher.h"
#endif

#include "Collaboration.h"
#include "StringUtil.h"
#include "XmlUtil.h"
#include "XSLT/XSLTFilter.h"
#include "XSLT/ExtensionUrl.h"

#include "Connector.h"
extern Connector *ConnectorInstance;

#ifndef WIN32
#include <time.h>
void Sleep( unsigned long millisecondsToWait )
{
        timespec ts;

        time_t seconds = millisecondsToWait / 1000;
        long milliseconds = millisecondsToWait % 1000;

        ts.tv_sec = seconds;
        ts.tv_nsec = milliseconds * 1000000 ;
        nanosleep(&ts, NULL);
}
#endif

#define sleep(x) Sleep( (x) )

AppSettings *Endpoint::m_GlobalSettings = NULL;
void ( *Endpoint::m_ManagementCallback )( TransactionStatus::TransactionStatusEnum, void* additionalData ) = NULL;

//EndpointConfig implementation
string EndpointConfig::getName( const EndpointConfig::ConfigDirection prefix, const EndpointConfig::ConfigSettings setting )
{
	string settingName = "";

	if ( prefix == EndpointConfig::AppToWMQ )
		settingName = "AppToMQSeries.";
	else if ( prefix == EndpointConfig::WMQToApp )
		settingName = "MQSeriesToApp.";
	else if ( prefix == EndpointConfig::AppToMQ )
		settingName = "AppToMQ.";
	else if ( prefix == EndpointConfig::MQToApp )
		settingName = "MQToApp.";
	
	switch( setting )
	{
		// Common settings
		case BATCHMGRTYPE :
			( void )settingName.append( "BatchManagerType" );
			break;

		case BATCHMGRXSLT :
			( void )settingName.append( "BatchXsltFile" );
			break;

		case BATCHMGRXPATH :
			( void )settingName.append( "BatchXPath" );
			break;

		case BATCHMGRTMPL : 
			( void )settingName.append( "BatchManagerTemplate" );
			break;

		case BATCHMGRFIXEDMQMSGSIZE:
			( void )settingName.append( "BatchManagerFixedMQMessageSize" );
			break;

		case RPLOPTIONS :
			( void )settingName.append( "ReplyOptions" );
			break;
			
		case RECONSOURCE :
			( void )settingName.append( "ReconSource" );
			break;			

		case REGULATIONS :
			( void )settingName.append( "Regulations" );
			break;

		// DB settings
		case DBPROVIDER :
			( void )settingName.append( "DatabaseProvider" );
			break;

		case DBNAME :
			( void )settingName.append( "DatabaseName" );
			break;

		case DBUSER :
			( void )settingName.append( "UserName" );
			break;

		case DBPASS :
			( void )settingName.append( "UserPassword" );
			break;

		case TABLENAME :
			( void )settingName.append( "TableName" );
			break;

		case REPLIESTABLE :
			( void )settingName.append( "RepliesTableName" );
			break;

		case DADOPTIONS:
			( void )settingName.append( "DadOptions" );
			break;

		case DADFILE :
			( void )settingName.append( "DadFileName" );
			break;

		case ACKDADFILE :
			(void )settingName.append( "AckDadFileName" );
			break;

		case SPINSERT :
			( void )settingName.append( "SPinsertXmlData" );
			break;

		case SPSELECT :
			( void )settingName.append( "SPselectforprocess" );
			break;

		case SPMARK :
			( void )settingName.append( "SPmarkforprocess" );
			break;

		case SPMARKCOMMIT :
			( void )settingName.append( "SPmarkcommit" );
			break;

		case SPMARKABORT :
			( void )settingName.append( "SPmarkabort" );
			break;

		case SPWATCHER :
			( void )settingName.append( "SPWatcher" );
			break;

		case UNCMTMAX :
			( void )settingName.append( "MaxUncommitedTrns" );
			break;

		case NOTIFTYPE :
			( void )settingName.append( "NotificationType" );
			break;

		case DATEFORMAT :
			( void )settingName.append( "DateFormat" );
			break;

		case TIMESTAMPFORMAT :
			( void )settingName.append( "TimestampFormat" );
			break;

		case DBTOXMLTRIMM :
			( void )settingName.append( "DatabaseToXmlTrimming" );
			break;

		// MQ settings
		case APPQUEUE :
			( void )settingName.append( "AppQueue" );
			break;

		case RPLYQUEUE :
			( void )settingName.append( "RepliesQueue" );
			break;

		case RPLXSLT:
			(void )settingName.append( "RepliesXslt" );
			break;

		case BAKQUEUE :
			( void )settingName.append( "BackupQueue" );
			break;

		case WMQQMGR :
			( void )settingName.append( "QueueManager" );
			break;

		case MQURI :
			( void )settingName.append( "MQURI" );
			break;

		case WMQFMT :
			( void )settingName.append( "MessageFormat" );
			break;

		case WMQKEYREPOS :
			( void )settingName.append( "KeyRepository" );
			break;
		
		case WMQSSLCYPHERSPEC :
			( void )settingName.append( "SSLCypherSpec" );
			break;
		
		case WMQPEERNAME :
			( void )settingName.append( "SSLPeerName" );
			break;
			
		case ISSIGNED :
			( void )settingName.append( "IsSigned" );
			break;
			
		case CERTIFICATEFILENAME :
			( void )settingName.append( "CertificateFileName" );
			break;
			
		case CERTIFICATEPASSWD :
			( void )settingName.append( "CertificatePasswd" );
			break;

		case MQSERVERTYPE :
			( void )settingName.append( "Type" );
			break;
		
		//ID settings
		case ISIDENABLED :
			( void )settingName.append( "IsIDEnabled" );
			break;
			
		case IDCERTIFICATEFILENAME :
			( void )settingName.append( "IDCertificateFileName" );
			break;
			
		case IDCERTIFICATEPASSWD :
			( void )settingName.append( "IDCertificatePasswd" );
			break;

		case SSLCERTIFICATEFILENAME :
			( void )settingName.append( "SSLCertificateFileName" );
			break;
			
		case SSLCERTIFICATEPASSWD :
			( void )settingName.append( "SSLCertificatePasswd" );
			break;

		// File settings
		case RPLDESTTPATH :
			( void )settingName.append( "RepliesPath" );
			break;

		case RPLFILEFILTER :
			( void )settingName.append( "RepliesPattern" );
			break;

		case FILESOURCEPATH :
			( void )settingName.append( "SourcePath" );
			break;

		case FILEDESTPATH :
			( void )settingName.append( "DestinationPath" );
			break;

		case MAXFILESPERFILE :
			( void )settingName.append( "MaxFilesPerFile" );
			break;

		case FILETEMPDESTPATH :
			( void )settingName.append( "TempDestinationPath" );
			break;

		case FILEERRPATH :
			( void )settingName.append( "ErrorPath" );
			break;

		case FILEFILTER :
			( void )settingName.append( "FilePattern" );
			break;

		case FILEXSLT :
			( void )settingName.append( "TransformFile" );
			break;

		case RPLFEEDBACK :
			( void )settingName.append( "ReplyFeedback" );
			break;

		case STRICTSWIFTFMT :
			( void )settingName.append( "StrictSWIFTFormat" );
			break;

		case RECONSFORMAT :
			( void )settingName.append( "ReconSFormat" );
			break;
			
		case HASHXSLT :
			( void )settingName.append( "HashXSLT" );
			break;

		case SERVICENAME :
			( void )settingName.append( "ServiceName" );
			break;

		case RPLSERVICENAME :
			( void )settingName.append( "ReplyServiceName" );
			break;

		case PARAMFXSLT :
			( void )settingName.append( "ParamFileXslt" );
			break;

		case PARAMFPATTERN :
			( void )settingName.append( "ParamFilePattern" );
			break;

		case RENAMEPATTERN :
			( void )settingName.append( "RenamePattern" );
			break;

		case INSERTBLOB :
			( void )settingName.append( "InsertBlob" );
			break;

		case DBCFGNAME :
			( void )settingName.append( "CfgDatabaseName" );
			break;

		case DBCFGUSER :
			( void )settingName.append( "CfgUserName" );
			break;

		case DBCFGPASS :
			( void )settingName.append( "CfgUserPassword" );
			break;

		case BLOBLOCATOR :
			( void )settingName.append( "BlobLocator" );
			break;

		case BLOBPATTERN :
			( void )settingName.append( "BlobFilePattern" );
			break;
			
		case LAUCERTIFICATE :
			( void )settingName.append( "LAUCertificateFile" );
			break;
		case TRACKMESSAGES :
			( void )settingName.append( "TrackMessages" );
			break;
		case HEADERREGEX :
			( void )settingName.append( "HeaderRegex" );
			break;

		default : 
			{
				stringstream errorMessage;
				errorMessage << "Invalid config file setting for prefix [" << prefix << "] : [" << setting << "]";
				throw runtime_error( errorMessage.str() );
			}
	}
	return settingName;
}

// Endpoint implementation
Endpoint::Endpoint() : InstrumentedObject(), m_FatalError( false ), m_BatchManager( NULL ), m_PersistenceFacility( NULL ), 
	m_BackoutCount( 0 ), m_CurrentStage( 0 ), m_CrtBatchItem( 0 ), m_Running( false ), m_LastOpSucceeded( false ), 
	m_IsLast( false ), m_TrackMessages( false )
{
	m_FilterChain = new FilterChain();

	DEBUG( "CONSTRUCTOR" );
	m_LastFailureCorrelationId = Collaboration::EmptyGuid();
	
	/**
	 * Configuration options to support future functionality: Sending report MQ message on Abort/Commit
	 */
	string fetcherReplyOptions = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::RPLOPTIONS, "" );
	if( fetcherReplyOptions.length() > 0 )
		DEBUG( "Send reply message report not implemented yet on Fetcher" );
	string publisherReplyOptions = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::RPLOPTIONS, "" );
	if( publisherReplyOptions.length() > 0 )
		DEBUG( "Send reply message report not implemented yet on Publisher" );


	string trackMessages = getGlobalSetting( EndpointConfig::Common, EndpointConfig::TRACKMESSAGES, "false" );
	m_TrackMessages = ( trackMessages == "true" );

	if ( getGlobalSettings().getSettings().ContainsKey( "MessageThrottling" ) )
		m_MessageThrottling = StringUtil::ParseUInt ( getGlobalSettings().getSettings()["MessageThrottling"] );
	else
		m_MessageThrottling = 0;

	if ( getGlobalSettings().getSettings().ContainsKey( "BasicAccessAuthentication" ) )
	{
		string header = getGlobalSettings().getSettings()["BasicAccessAuthentication"];
		FunctionUrl::m_BAAuthentication = header;
	}

	if ( getGlobalSettings().getSettings().ContainsKey( "SourceEncoding" ) )
		XStr::m_SourceEncoding = getGlobalSettings().getSettings()["SourceEncoding"];

	m_SelfThreadId = pthread_self();

	INIT_COUNTER( TRN_COMMITED );
	INIT_COUNTER( TRN_ABORTED );
	INIT_COUNTER( TRN_TOTAL );
	INIT_COUNTER( TRN_ATTEMPTS );
		
#if defined( WIN32 ) && defined ( CHECK_MEMLEAKS )
	m_OldStateAvailable = false;
#endif
}

Endpoint::~Endpoint()
{
	try
	{
		DEBUG( "DESTRUCTOR" );
	} catch( ... ){}

	try
	{
		if ( m_FilterChain != NULL )
		{
			delete m_FilterChain;
			m_FilterChain = NULL;
		}
	}
	catch( ... ){}
	
	try
	{
		if ( m_BatchManager != NULL )
		{
			delete m_BatchManager;
			m_BatchManager = NULL;
		}
	}
	catch( ... ){}	

	try
	{
		if ( m_PersistenceFacility != NULL )
		{
			delete m_PersistenceFacility;
			m_PersistenceFacility = NULL;
		}
	}
	catch( ... ){}

	try
	{
		DESTROY_COUNTER( TRN_COMMITED );
	} catch( ... ){}
	try
	{
		DESTROY_COUNTER( TRN_ABORTED );
	} catch( ... ){}
	try
	{
		DESTROY_COUNTER( TRN_TOTAL );
	} catch( ... ){}
	try
	{
		DESTROY_COUNTER( TRN_ATTEMPTS );
	} catch( ... ){}
}

void Endpoint::fireManagementEvent( TransactionStatus::TransactionStatusEnum event, const void* additionalData )
{
	try
	{
		switch ( event )
		{
			case TransactionStatus::Begin :
				INCREMENT_COUNTER( TRN_ATTEMPTS );
				break;

			case TransactionStatus::Commit :
				INCREMENT_COUNTER( TRN_COMMITED );
				INCREMENT_COUNTER( TRN_TOTAL );
				break;

			case TransactionStatus::Abort :
				INCREMENT_COUNTER( TRN_ABORTED );
				INCREMENT_COUNTER( TRN_TOTAL );
				break;
		}

		if ( ( TransactionStatus::Begin != event ) && ( COUNTER( TRN_TOTAL ) % 100 ) == 0 )
		{
			TimeUtil::TimeMarker stopTime;
			
			TRACE( m_ServiceName << " performance report : Last 100 messages were processed in [" <<
				stopTime - m_LastReportTime << "] milliseconds." );

			m_LastReportTime = stopTime;
		}
		if ( m_ManagementCallback != NULL ) 
			m_ManagementCallback( event, NULL );
	}
	catch( ... )
	{
		//TODO report this
	}
}

bool Endpoint::haveGlobalSetting( const EndpointConfig::ConfigDirection prefix, const EndpointConfig::ConfigSettings setting ) const
{
	string settingName = EndpointConfig::getName( prefix, setting );
	DEBUG( "Looking for application setting [" << settingName << "] ..." );
	return getGlobalSettings().getSettings().ContainsKey( settingName );
}

string Endpoint::getGlobalSetting( const EndpointConfig::ConfigDirection prefix, const EndpointConfig::ConfigSettings setting, const string& defaultValue ) const
{
	string settingName = EndpointConfig::getName( prefix, setting );

	// assign default value
	string settingValue = defaultValue;

	// if the setting is found in config, set it
	if ( haveGlobalSetting( prefix, setting ) )
	{
		settingValue = getGlobalSettings()[ settingName ];
		DEBUG( "Found config key [" << settingName << "] = [" << settingValue << "]" );
	}
	else
	{
		if ( defaultValue == "__NODEFAULT" )
		{
			stringstream errorMessage;
			errorMessage << "Required config key [" << settingName << "] not found.";
			throw runtime_error( errorMessage.str() );
		}
		DEBUG( "Config key [" << settingName << "] not found. Defaulting to [" << settingValue << "]" );
	}

	return settingValue;
}

AppSettings& Endpoint::getGlobalSettings() const
{
	if ( ConnectorInstance != NULL )
		return ConnectorInstance->GlobalSettings;
	if ( m_GlobalSettings != NULL )
		return *m_GlobalSettings;
	throw logic_error( "You must set global setting if running outside a Connector container." );
}

void* Endpoint::StartInNewThread( void* pThis )
{
	Endpoint* me = static_cast< Endpoint* >( pThis );
	TRACE( "Service [" << me->getServiceName() << "] thread " << me->m_ServiceThreadId );

	try
	{
		me->internalStart();
	}
	catch( const std::exception& ex )
	{
		stringstream errorMessage;
		errorMessage << typeid( ex ).name() << " exception encountered by endpoint [" << ex.what() << "]";
		TRACE( errorMessage.str() );

		try
		{
			LogManager::Publish( AppException( "Error encountered by endpoint", ex ) );
		}
		catch( ... ){}
	}
	catch( ... )
	{
		stringstream errorMessage;
		errorMessage << "Exception encountered by endpoint [unhandled exception]";
		TRACE( errorMessage.str() );

		try
		{
			LogManager::Publish( errorMessage.str() );
		}
		catch( ... ){}
	}

	pthread_exit( NULL );
	return NULL;
}

void Endpoint::Start()
{
	pthread_attr_t scanThreadAttr;

	int attrInitResult = pthread_attr_init( &scanThreadAttr );
	if ( 0 != attrInitResult )
	{
		TRACE( "Error initializing scan thread attribute [" << attrInitResult << "]" );
		throw runtime_error( "Error initializing scan thread attribute" );
	}

	int setDetachResult = pthread_attr_setdetachstate( &scanThreadAttr, PTHREAD_CREATE_JOINABLE );
	if ( 0 != setDetachResult )
	{
		TRACE( "Error setting joinable option to scan thread attribute [" << setDetachResult << "]" );
		throw runtime_error( "Error setting joinable option to scan thread attribute" );
	}

	m_Running = true;
	
	int threadStatus = 0;
	
	// if running in debugger, reattempt to create thread on failed request	
	do
	{
		threadStatus = pthread_create( &m_SelfThreadId, &scanThreadAttr, Endpoint::StartInNewThread, ( void* )this );
	} while( ( threadStatus != 0 ) && ( errno == EINTR ) );
	if ( threadStatus != 0 )
	{
		int errCode = errno;
		int attrDestroyResult = pthread_attr_destroy( &scanThreadAttr );
		if ( 0 != attrDestroyResult )
		{
			TRACE( "Unable to destroy thread attribute [" << attrDestroyResult << "]" );
		}
		
		stringstream errorMessage;
#ifdef CRT_SECURE
		char errBuffer[ 95 ];
		strerror_s( errBuffer, sizeof( errBuffer ), errCode );
		errorMessage << "Unable to create endpoint thread. [" << errBuffer << "]";
#else
		errorMessage << "Unable to create endpoint thread. [" << strerror( errCode ) << "]";
#endif
		throw runtime_error( errorMessage.str() );
	}
	
	int attrDestroyResult = pthread_attr_destroy( &scanThreadAttr );
	if ( 0 != attrDestroyResult )
	{
		TRACE( "Unable to destroy thread attribute [" << attrDestroyResult << "]" );
	}
}

void Endpoint::Stop()
{
	TRACE( m_ServiceThreadId << " stopping... shutting down message pool ..." );
	
	// allow watchers to die, keep worker active
	m_NotificationPool.ShutdownPoolWriters();

	TRACE( m_ServiceThreadId << " stopped thread pool ..." );
	internalStop();

	TRACE( m_ServiceThreadId << " terminated." );
}

bool Endpoint::PerformMessageLoop( bool assemble )
{
	if( m_MessageThrottling && ( m_BatchManager != NULL ) )
		DEBUG( "Performing transaction in Aggregate message mode" )
	else if( assemble )
		DEBUG( "Performing transaction in Batch message mode" )
	else
		DEBUG( "Performing transaction in Single meessage mode." )

	m_CrtBatchItem = 0;
	m_FatalError = false;
	bool result = false;

	// do this in loop ( if there are more messages or backout for the last message is not exhausted )
	if ( assemble )
	{
		while( moreMessages() && !m_FatalError )
		{
			result = Endpoint::PerformMessageLoop();
			if ( result )
				continue;
			
			while( moreMessages() && ( m_BackoutCount < 3 ) && !m_FatalError )
			{
				result = Endpoint::PerformMessageLoop();
				if ( result )
					break;
			}
		}
		DEBUG( "No more messages!" );
	}
	else
	{
		result = Endpoint::PerformMessageLoop();
	}
	
	return result;
}

bool Endpoint::PerformMessageLoop()
{
	// MEM_CHECKPOINT_INIT();

	unsigned int attempts = 0;
	m_TransactionKey = "";
	m_CorrelationId = Collaboration::EmptyGuid();
	
	bool prepareSucceded = false;
	
	// first ask the connector to vote whether it can/can't perform this task
	try
	{
		// if prepare fails, we don't get a unique transaction key
		m_TransactionKey = internalPrepare();
		prepareSucceded = true;
		
		if( m_PersistenceFacility != NULL )
		{
			m_CorrelationId = m_PersistenceFacility->GetString( m_TransactionKey, "CorrelationId" );
			
			// no correlation ( first attempt )
			if( m_CorrelationId.size() == 0 )
			{
				m_CorrelationId = Collaboration::GenerateGuid();
				m_PersistenceFacility->Set( m_TransactionKey, "CorrelationId", m_CorrelationId );
			}
		}
		setCorrelationId( m_CorrelationId );
		m_LastFailureCorrelationId = Collaboration::EmptyGuid();
	}
	catch( ... )
	{
		// if this is the first prepare failure in this run
		if ( m_LastFailureCorrelationId == Collaboration::EmptyGuid() )
		{
			// generate a failure code uniqueue for this run
			m_LastFailureCorrelationId = Collaboration::GenerateGuid();
			m_TransactionKey = m_LastFailureCorrelationId;
			m_CorrelationId = m_LastFailureCorrelationId;

			if( m_PersistenceFacility != NULL )
			{
				m_PersistenceFacility->Set( m_TransactionKey, "CorrelationId", m_CorrelationId );
			}
			setCorrelationId( m_CorrelationId );
		}

		//failure was already reported by internalPrepare
		prepareSucceded = false;
	}
	
	// if prepare failed and no persist. facility defined, abort
	// also abort if a unique key wasn't provided
	if( ( ( m_PersistenceFacility == NULL ) && !prepareSucceded ) )
	{
		internalAbort( m_CorrelationId );
		return false;
	}
	
	// read attempts for this message
	if( m_PersistenceFacility != NULL )
	{
		attempts = m_PersistenceFacility->GetInt( m_TransactionKey, "Attempt" );
		m_BackoutCount = attempts;
	}
		
	//#region Performance
	// if no attempt made so far, communicate begin of transaction
	if ( ( attempts == 0 ) && ( prepareSucceded ) )
		fireManagementEvent( TransactionStatus::Begin, NULL );
		
	//#endregion

	// increment attempts sequence
	if( m_PersistenceFacility != NULL )
		m_PersistenceFacility->Set( m_TransactionKey, "Attempt", ++attempts );
	
	DEBUG( "----------------- At " << TimeUtil::Get( "%d/%m/%Y %H:%M:%S", 19 ) << " - attempt #" << attempts << " for [" << m_TransactionKey << "]" );

	// attempt to recover as we haven't depleted out trials count
	if( !prepareSucceded )
	{
		if ( attempts > 3 ) 
		{
			internalAbort( m_CorrelationId );
			
			if( m_PersistenceFacility != NULL )
				m_PersistenceFacility->ReleaseStorage( m_TransactionKey );

			//#region Performance
			// only send abort if max trials no is depleted
			fireManagementEvent( TransactionStatus::Abort, NULL );
	
			//#endregion
		}
		else
		{	
			// attempt to recover
			internalRollback( m_CorrelationId );
		}
		return false;
	}
		
	// Do work on the data and commit or rollback based on it's outcome
	try
	{
		internalProcess( m_CorrelationId );
		internalCommit( m_CorrelationId );
					
		if( m_PersistenceFacility != NULL )
			m_PersistenceFacility->ReleaseStorage( m_TransactionKey );

		//#region Performance
		fireManagementEvent( TransactionStatus::Commit, NULL );
		//#endregion
	}
	catch( ... )
	{
		if ( ( attempts > 3 ) || m_FatalError ) 
		{
			internalAbort( m_CorrelationId );
						
			if( m_PersistenceFacility != NULL )
				m_PersistenceFacility->ReleaseStorage( m_TransactionKey );

			//#region Performance
			// only send abort if max trials no is depleted
			fireManagementEvent( TransactionStatus::Abort, NULL );
	
			//#endregion
		}
		else
		{	
			// attempt to recover
			internalRollback( m_CorrelationId );
		}
		return false;
	}
	return true;
}

void Endpoint::setPersistenceFacility( AbstractStatePersistence* facility )
{
	m_PersistenceFacility = facility;
}

AbstractStatePersistence* Endpoint::getPersistenceFacility()
{
	return m_PersistenceFacility;
}

string Endpoint::internalPrepare()
{
	try
	{
		DEBUG( "1. PREPARE" );
		return Prepare();
	}
	catch( const AppException& ex )
	{
		stringstream errorMessage;
		errorMessage << "Message prepare failed. Reason : " << ex.getMessage();

		TRACE( errorMessage.str() );		
		AppException aex( errorMessage.str(), ex );
		LogManager::Publish( aex );

		throw;
	}
	catch( const std::exception& ex )
	{
		stringstream errorMessage;
		errorMessage << "Message prepare failed. Reason : " << ex.what();

		TRACE( errorMessage.str() );		
		AppException aex( errorMessage.str(), ex );
		LogManager::Publish( aex );

		throw;
	}
	catch( ... )
	{
		string errorMessage = "Message prepare failed. Reason : unknown";

		TRACE( errorMessage );		
		AppException aex( errorMessage );
		LogManager::Publish( aex );

		throw;
	}
}

void Endpoint::internalCommit( const string& correlationId )
{
	try
	{
		DEBUG( "3. COMMIT" );
		m_CrtBatchItem++;
		Commit();
		
		stringstream message;
		message << "Message [" << correlationId << "] successfully processed by [" << m_ServiceName << "]";
		
		AppException ex( message.str(), EventType::Info );
		ex.setCorrelationId( correlationId );
		ex.addAdditionalInfo( "Trasaction_key", m_TransactionKey );

		
		if ( m_IsFetcher )
			ex.setClassType( ClassType::TransactionFetch );
		else
			ex.setClassType( ClassType::TransactionPublish );

		LogManager::Publish( ex );

		//m_TrackingData.setAdditionalInfo( "State", "commit" );
		//LogManager::Publish( m_TrackingData );
	}
	catch( const AppException& ex )
	{
		stringstream errorMessage;
		errorMessage << "Message commit failed. Reason : " << ex.getMessage();	

		TRACE( errorMessage.str() );		
		AppException aex( errorMessage.str(), ex );
		aex.setCorrelationId( correlationId );
		aex.addAdditionalInfo( "Trasaction_key", m_TransactionKey );

		LogManager::Publish( aex );
		//throw;
	}
	catch( const std::exception& ex )
	{
		stringstream errorMessage;
		errorMessage << "Message commit failed. Reason : " << ex.what();

		TRACE( errorMessage.str() );		
		AppException aex( errorMessage.str(), ex );
		aex.setCorrelationId( correlationId );
		aex.addAdditionalInfo( "Trasaction_key", m_TransactionKey );
		
		LogManager::Publish( aex );
		//throw;
	}
	catch( ... )
	{
		string errorMessage = "Message commit failed. Reason : unknown";
		
		TRACE( errorMessage );		
		AppException aex( errorMessage, EventType::Error );
		aex.setCorrelationId( correlationId );
		aex.addAdditionalInfo( "Trasaction_key", m_TransactionKey );
		
		LogManager::Publish( aex );
		//throw;
	}
}

void Endpoint::internalAbort( const string& correlationId )
{
	try
	{
		DEBUG( "3. ABORT" );

		// allow further messages to be processed ( if the next message is to fail, at least it will get 3 chances to succeed );
		m_LastFailureCorrelationId = Collaboration::EmptyGuid();
		m_CrtBatchItem++;
				
		Abort();

		stringstream message;
		message << "Message processing failed and the message [" << correlationId << "] was rejected.";
		AppException ex( message.str(), EventType::Error );
		ex.setCorrelationId( correlationId );
		ex.addAdditionalInfo( "Trasaction_key", m_TransactionKey );

		LogManager::Publish( ex );

		//m_TrackingData.setAdditionalInfo( "State", "abort" );
		//LogManager::Publish( m_TrackingData );
	}
	catch( const AppException& ex )
	{
		stringstream errorMessage;
		errorMessage << "Message abort failed. Reason : " << ex.getMessage();
		
		TRACE( errorMessage.str() );
		
		AppException aex( errorMessage.str(), ex );
		aex.setCorrelationId( m_CorrelationId );
		aex.addAdditionalInfo( "Trasaction_key", m_TransactionKey );

		LogManager::Publish( aex );
		//throw;
	}
	catch( const std::exception& ex )
	{
		stringstream errorMessage;
		errorMessage << "Message abort failed. Reason : " << ex.what();
		
		TRACE( errorMessage.str() );
		
		AppException aex( errorMessage.str(), EventType::Error );
		aex.setCorrelationId( correlationId );
		aex.addAdditionalInfo( "Trasaction_key", m_TransactionKey );
		
		LogManager::Publish( aex );
		//throw;
	}
	catch( ... )
	{
		string errorMessage = "Message abort failed. Reason : unknown";
		
		TRACE( errorMessage );
		
		AppException aex( errorMessage, EventType::Error );
		aex.setCorrelationId( correlationId );
		aex.addAdditionalInfo( "Trasaction_key", m_TransactionKey );
		
		LogManager::Publish( aex );
		//throw;
	}
}

void Endpoint::internalRollback( const string& correlationId )
{
	try
	{
		DEBUG( "3. ROLLBACK" );
		Rollback();
		
		stringstream message;
		message << "Message processing failed. The operation will be retried " << StringUtil::ToString( 3 - m_BackoutCount ) << " more time(s)";
		
		AppException ex( message.str(), EventType::Warning );
		ex.setCorrelationId( correlationId );
		ex.addAdditionalInfo( "Trasaction_key", m_TransactionKey );

		LogManager::Publish( ex );

		//m_TrackingData.setAdditionalInfo( "State", "rollback" );
		//LogManager::Publish( m_TrackingData );
	}
	catch( const AppException& ex )
	{
		stringstream errorMessage;
		errorMessage << "Message rollback failed. Reason : " << ex.getMessage();
		
		TRACE( errorMessage.str() );
		
		AppException aex( errorMessage.str(), ex );
		aex.setCorrelationId( correlationId );
		aex.addAdditionalInfo( "Trasaction_key", m_TransactionKey );

		LogManager::Publish( aex );
		//throw;
	}
	catch( const std::exception& ex )
	{
		stringstream errorMessage;
		errorMessage << "Message rollback failed. Reason : " << ex.what();
		
		TRACE( errorMessage.str() );
		
		AppException aex( errorMessage.str(), EventType::Error );
		aex.setCorrelationId( correlationId );
		aex.addAdditionalInfo( "Trasaction_key", m_TransactionKey );
		
		LogManager::Publish( aex );
		//throw;
	}
	catch( ... )
	{
		string errorMessage = "Message rollback failed. Reason : unknown";
		
		TRACE( errorMessage );		
		AppException aex( errorMessage, EventType::Error );
		aex.setCorrelationId( correlationId );
		aex.addAdditionalInfo( "Trasaction_key", m_TransactionKey );
		
		LogManager::Publish( aex );
		//throw;
	}
}

void Endpoint::internalProcess( const string& correlationId )
{
	try
	{
		DEBUG( "2. PROCESS" );
/*
#ifdef WIN32
	#ifdef MEM_LEAK
		_CrtMemState s1, s2, s3;
	#endif
#endif
#ifdef WIN32
	#ifdef MEM_LEAK
		_CrtMemCheckpoint( &s1 );
		_CrtMemDumpStatistics( &s1 );
	#endif
#endif
*/
		Process( correlationId );
/*
#ifdef WIN32
	#ifdef MEM_LEAK
		_CrtMemCheckpoint( &s1 );
		_CrtMemDumpStatistics( &s1 );
	#endif
#endif
*/
	}
	catch( const AppException& ex )
	{
		stringstream errorMessage;
		errorMessage << "Message processing failed. Reason : " << ex.what();
		
		TRACE( errorMessage.str() );
		
		AppException aex( errorMessage.str(), ex );
		aex.setCorrelationId( m_CorrelationId );
		aex.addAdditionalInfo( "Trasaction_key", m_TransactionKey );

		LogManager::Publish( aex );

		if ( ex.getSeverity().getSeverity() == EventSeverity::Fatal )
			m_FatalError = true;

		internalCleanup();

		throw;
	}
	catch( const bad_alloc& ex )
	{
		stringstream errorMessage;
		errorMessage << "Message processing failed (memory). Reason : " << ex.what() << ". Unable to continue processing ...";
		
		AppException aex( errorMessage.str(), ex );
		aex.setCorrelationId( m_CorrelationId );
		aex.addAdditionalInfo( "Trasaction_key", m_TransactionKey );
		
		LogManager::Publish( aex );
		
		exit( -1 );	
	}
	catch( const std::exception& ex )
	{
		stringstream errorMessage;
		errorMessage << "Message processing failed. Reason : " << ex.what();
		
		TRACE( errorMessage.str() );
		
		AppException aex( errorMessage.str(), ex );
		aex.setCorrelationId( m_CorrelationId );
		aex.addAdditionalInfo( "Trasaction_key", m_TransactionKey );
		
		LogManager::Publish( aex );

		internalCleanup();

		throw;
	}
	catch( ... )
	{
		string errorMessage = "Message processing failed. Reason : unknown";
		
		TRACE( errorMessage );		
		AppException aex( errorMessage, EventType::Error );
		aex.setCorrelationId( m_CorrelationId );
		aex.addAdditionalInfo( "Trasaction_key", m_TransactionKey );

		internalCleanup();
		
		LogManager::Publish( aex );
		throw;
	}
}

void Endpoint::setCorrelationId( const string& correlationId )
{
	m_CorrelationId = correlationId;
	LogManager::setCorrelationId( m_CorrelationId );
}

// EndpointFactory implementation
Endpoint* EndpointFactory::CreateEndpoint( const string& serviceName, const string& endpointType, bool fetcher )
{
	Endpoint* returnedEndpoint = NULL;

	try
	{
		if( endpointType == "MQ" )
		{
			if ( fetcher ) 
				returnedEndpoint = new MqFetcher();
			else
				returnedEndpoint = new MqPublisher();
		}
		else if ( endpointType == "File" )
		{
			if ( fetcher ) 
				returnedEndpoint = new FileFetcher();
			else 
				returnedEndpoint = new FilePublisher();
		}
#ifndef NO_DB
		else if( endpointType == "DB" )
		{
			if ( fetcher ) 
				returnedEndpoint = new DbFetcher();
			else
				returnedEndpoint = new DbPublisher();
		}
#endif
		stringstream serviceThreadId;
		serviceThreadId << ( ( fetcher ) ? "Fetcher" : "Publisher" ) << " of type [" << endpointType << "]";

		if( returnedEndpoint == NULL )
		{
			stringstream errorMessage;
			errorMessage << "Unable to create " << serviceThreadId.str();
			TRACE_GLOBAL( errorMessage.str() );

			throw runtime_error( errorMessage.str() );
		}
		else 
		{
			if ( returnedEndpoint->getFilterChain() == NULL )
			{
				stringstream errorMessage;
				errorMessage << serviceThreadId.str() << " doesn't create a filter chain. See MqFetcher.cpp for reference";
				TRACE_GLOBAL( errorMessage.str() );

				throw logic_error( errorMessage.str() );
			}
		}
		returnedEndpoint->setServiceName( serviceName );
		returnedEndpoint->setServiceThreadId( serviceThreadId.str() );
		returnedEndpoint->setServiceType( fetcher );
	}
	catch( ... )
	{
		if ( returnedEndpoint != NULL )
			delete returnedEndpoint;
		throw;
	}

	return returnedEndpoint;
}

void Endpoint::trackMessage( const string& payload, const NameValueCollection& transportHeaders )
{
	if( !m_TrackMessages )
		return;

	m_TrackingData.InitDefaultValues();
	m_TrackingData.setMessage( "Tracking" );
	m_TrackingData.setEventType( EventType::Management );

	m_TrackingData.setCorrelationId( m_CorrelationId );

	m_TrackingData.setAdditionalInfo( "Payload", payload );	
	m_TrackingData.setAdditionalInfo( "Headers", transportHeaders.ConvertToXml("Headers") );
}

