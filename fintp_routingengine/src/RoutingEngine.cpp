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
	#ifdef _DEBUG
		/*
		//#define _CRTDBG_MAP_ALLOC
		#include <stdlib.h>
		#include <crtdbg.h>*/

		#include <windows.h>
#ifdef USE_STACKWALKER
		#include "Stackwalker.h"
#endif
	#endif
	
	#define sleep(x) Sleep( (x)*1000 )
#else
	#include <unistd.h>
#endif

#include <omp.h> 

#include <iostream>

#include "DumpContext.h"
#include "Trace.h"
#include "AppExceptions.h"
#include "LogManager.h"

#include "XSLT/XSLTFilter.h"
#include "XSLT/ExtensionLookup.h"

#include "Base64.h"
#include "Collaboration.h"
#include "AppSettings.h"
#include "StringUtil.h"
#include "TimeUtil.h"
#include "PlatformDeps.h"

#include "RoutingEngine.h"
#include "RoutingExceptions.h"
#include "RoutingJob.h"
#include "RoutingAggregationManager.h"
#include "RoutingMessage.h"
#include "RoutingDbOp.h"

using namespace std;

RoutingKeywordMappings RoutingEngine::KeywordMappings;
RoutingIsoMessageTypes RoutingEngine::IsoMessageTypes;
RoutingKeywordCollection RoutingEngine::Keywords;

RoutingEngine* RoutingEngine::TheRoutingEngine = NULL;

pthread_once_t RoutingEngine::SchemaKeysCreate = PTHREAD_ONCE_INIT;
pthread_key_t RoutingEngine::SchemaKey;

RoutingSchema RoutingEngine::DefaultSchema;
AbstractStatePersistence* RoutingEngine::m_IdFactory = NULL;

string RoutingEngine::m_OwnCorrelationId = "";

bool RoutingEngine::m_ShouldStop = false;
unsigned int RoutingEngine::m_CotDelay = 300;
int RoutingEngine::m_ParallelJobs = -1;

unsigned int RoutingEngine::m_ProfileMessageCount = 0;

pthread_mutex_t RoutingEngine::m_CotMarkersMutex = PTHREAD_MUTEX_INITIALIZER;
//pthread_rwlock_t RoutingEngine::m_CotMarkersRWLock = PTHREAD_RWLOCK_INITIALIZER;

RoutingCOTMarker RoutingEngine::m_ActiveCotMarker;
RoutingCOTMarker RoutingEngine::m_PreviousCotMarker;

map< string, bool > RoutingEngine::m_DuplicateChecks;
/*WorkItemPool< RoutingMessage > RoutingEngine::m_MessagePool;
WorkItemPool< RoutingJob > RoutingEngine::m_JobPool;
WorkItemPool< RoutingJobExecutor > RoutingEngine::m_ThreadExecPool;*/

omp_lock_t RoutingEngine::parallelJobLock;

static pthread_cond_t ShutdownCOTMonitorCond;

#ifdef CHECK_MEMLEAKS
unsigned int RoutingEngine::m_Notifications = 0;
#endif

//static unsigned int m_Notifications;
// RoutingEngine implementation
#if defined(WIN32_SERVICE)
RoutingEngine::RoutingEngine( const string& configEncryptionKey ) : m_CotThreadId( 0 ), m_RouterThreadId( 0 ), m_BMThreadId( 0 ), m_Watcher( ( AbstractWatcher::NotificationPool* )NULL ),
	m_LiquiditiesHasIBAN( false ), m_LiquiditiesHasIBANPL( false ), m_LiquiditiesHasCorresps( false ), m_DuplicateDetectionActive( false ),
	m_DuplicateDetectionTimeout( 0 ), m_IdleArchivingActive( false ), m_Running( false ), Keywords( NULL ), KeywordMappings( NULL ), 
	m_BulkReactivateQueue( "" ), m_TrackMessages( false ), CNTService( TEXT( "RoutingEngine" ), TEXT( "Routing Engine" ) )
#else
RoutingEngine::RoutingEngine( const string& configEncryptionKey ) : m_CotThreadId( 0 ), m_RouterThreadId( 0 ), m_BMThreadId( 0 ), m_Watcher( ( AbstractWatcher::NotificationPool* )NULL ),
	m_LiquiditiesHasIBAN( false ), m_LiquiditiesHasIBANPL( false ), m_LiquiditiesHasCorresps( false ), m_DuplicateDetectionActive( false ),
	m_DuplicateDetectionTimeout( 0 ), m_IdleArchivingActive( false ), m_Running( false ),
	m_BulkReactivateQueue( "" ), m_TrackMessages( false )
#endif
{
#ifdef SCROLL_CURSOR
	m_Watcher.setCallback( RoutingEngine::OnNewJob );
#else
	m_Watcher.setCallback( RoutingEngine::NotificationCallback );
#endif

	m_IdFactory = new MemoryStatePersist();
	int onceResult = pthread_once( &RoutingEngine::SchemaKeysCreate, &RoutingEngine::CreateKeys );
	if ( 0 != onceResult )
	{
		TRACE( "One time key creation for Schema failed [" << onceResult << "]" );
	}
}

RoutingEngine::~RoutingEngine()
{
	try
	{
		if ( RoutingEngine::m_IdFactory != NULL )
		{
			delete m_IdFactory;
			m_IdFactory = NULL;
		}
	} catch( ... )
	{
		try
		{
			TRACE( "An error occured while deleting RoutingEngine::m_IdFactory" );
		}catch( ... ){}
	}
	try
	{
		RoutingDbOp::Terminate();
	} catch( ... )
	{
		try
		{
			TRACE( "An error occured in RoutingDbOp::Terminate" );
		}catch( ... ){}
	}
	try
	{
		//DumpContext::Terminate();
	} catch( ... )
	{
		try
		{
			TRACE( "An error occured in DumpContext::Terminate" );
		}catch( ... ){}
	}

	//XPathHelper::Terminate();
	//XSLTFilter::Terminate();

	try
	{
		DESTROY_COUNTER( TRN_COMMITED );
	}catch( ... ){};
	try
	{
		DESTROY_COUNTER( TRN_ABORTED );
	}catch( ... ){};
	try
	{
		DESTROY_COUNTER( TRN_TOTAL );
	}catch( ... ){};
	try
	{
		DESTROY_COUNTER( TRN_ROLLBACK );
	}catch( ... ){};

#if !defined( __GNUC__ )
	m_CotThreadId = NULL;
	m_RouterThreadId = NULL;
	m_BMThreadId = NULL;
#endif
}

void RoutingEngine::Start( bool startWatcher )
{
	const string serviceName = "RoutingEngine";

#if defined( TESTDLL_EXPORT ) || defined ( TESTDLL_IMPORT )
	m_ShouldStop = false;
	m_LiquiditiesHasIBAN = true;
	m_LiquiditiesHasIBANPL = true;
	m_LiquiditiesHasCorresps = true;

	m_BatchXSLT.insert( pair< string, string >( DEFAULT_QUEUE, "SwiftToACH.xslt" ) );

	RoutingAction::CreateXSLTFilter();

	RoutingDbOp::Initialize();
	RoutingEngine::DefaultSchema.Load();

	RoutingKeywordCollection Keywords;
	RoutingKeywordMappings KeywordMappings;

	return;
#endif // UNITTESTS

	const string configContent = StringUtil::DeserializeFromFile( "RoutingEngine.config" );

	if ( Base64::isBase64( configContent ) )
	{
		cout << "Config is encrypted. Please enter encryption password:";
		Platform::EnableStdEcho( false );
		string key;
		getline( cin, key );
		Platform::EnableStdEcho( true );
		GlobalSettings.loadConfig( "RoutingEngine.config", configContent, key );
		for ( size_t i = 0; i < key.size(); i++ )
			key[i] = 'x';
	}
	else
		GlobalSettings.loadConfig( "RoutingEngine.config", configContent );

	if( GlobalSettings.getSettings().ContainsKey( "LogPrefix" ) )
		FileOutputter::setLogPrefix( GlobalSettings[ "LogPrefix" ] );
	else
		FileOutputter::setLogPrefix( serviceName );	

	if( GlobalSettings.getSettings().ContainsKey( "LogMaxLines" ) )
		FileOutputter::setLogMaxLines( StringUtil::ParseLong( GlobalSettings[ "LogMaxLines" ] ) );
	else
		FileOutputter::setLogMaxLines( 0 );
	
	if( GlobalSettings.getSettings().ContainsKey( "TrackMessages" ) )
	{
		string trackMessages = GlobalSettings[ "TrackMessages" ];
		m_TrackMessages = ( trackMessages == "true" );
	}

	if( GlobalSettings.getSettings().ContainsKey( "LogMaxExtraFiles" ) )
		FileOutputter::setLogMaxExtraFiles( StringUtil::ParseULong( GlobalSettings[ "LogMaxExtraFiles" ] ) );
	else
		FileOutputter::setLogMaxExtraFiles( 0 );
	
	TRACE( "[Main] Starting " << serviceName );
	TRACE( "\t" << FinTPRoutingEngine::VersionInfo::Name() << "\t" << FinTPRoutingEngine::VersionInfo::Id() );
	TRACE( "\t" << FinTP::VersionInfo::Name() << "\t" << FinTP::VersionInfo::Id() );
	TRACE( "\t" << FinTPWS::VersionInfo::Name() << "\t\t" << FinTPWS::VersionInfo::Id() );
	
	GlobalSettings.Dump();
	
	bool eventsThreaded = ( GlobalSettings.getSettings().ContainsKey( "EventsThreaded" ) && ( GlobalSettings.getSettings()[ "EventsThreaded" ] == "true" ) );
	LogManager::Initialize( GlobalSettings.getSettings(), eventsThreaded );
	bool isDefault = false;
	AbstractLogPublisher* mqLogPublisher = TransportHelper::createMqLogPublisher( GlobalSettings.getSettings(), isDefault );
	LogManager::Register( mqLogPublisher, isDefault );
	LogManager::setSessionId( Collaboration::GenerateGuid() );

	INIT_COUNTERS( &m_Watcher, RoutingEngineWatcher );

	INIT_COUNTER( TRN_COMMITED );
	INIT_COUNTER( TRN_ABORTED );
	INIT_COUNTER( TRN_TOTAL );
	INIT_COUNTER( TRN_ROLLBACK );

	INIT_COUNTERS( RoutingEngine::TheRoutingEngine, TheRoutingEngine );

	// Register to EventsWatcher
	DEBUG( "Registering self" );
	AppException utilRegister( "Register", EventType::Management );
	utilRegister.addAdditionalInfo( "ServiceName", serviceName ); 
	utilRegister.addAdditionalInfo( "Version", FinTPRoutingEngine::VersionInfo::Id() );
	utilRegister.addAdditionalInfo( "Name", FinTPRoutingEngine::VersionInfo::Name() );

	LogManager::Publish( utilRegister );

	// create the only instance of the transformer
	RoutingAction::CreateXSLTFilter();

	RoutingDbOp::Initialize();

	// Get COTMarkers and find the active pattern.
	RoutingCOT cotMarkers;
	cotMarkers.Update();
	m_ActiveCotMarker = cotMarkers.ActiveMarker();
	m_PreviousCotMarker = m_ActiveCotMarker;	
	DEBUG( "Active COT marker [" << m_ActiveCotMarker.getName() << "]" );

	// Load active schemas
	try
	{
		RoutingEngine::DefaultSchema.Load();
		RoutingEngine::DefaultSchema.Explain();
	}
	catch( const RoutingException& rex )
	{
		throw RoutingException::EmbedException( "Unable to create default routing schema.",
			RoutingExceptionLocation, rex );
	}

	stringstream routingSchemaMessage;
	routingSchemaMessage << "Using routing schema(s) [" << RoutingEngine::DefaultSchema.getName() << "]";
	LogManager::Publish( routingSchemaMessage.str(), EventType::Info );
	
	// testonly
	//return;

	// Load keywords
	LoadKeywords(); 
	LoadMappings();
	
	// Generate an id used for correlating messages not related to a message being routed
	m_OwnCorrelationId = Collaboration::GenerateGuid();
	LogManager::setCorrelationId( m_OwnCorrelationId );
		
	// perform recovery tests
	RecoverLastSession();

	// assign static callbacks 
	FunctionLookup::setDatabaseCallback( RoutingDbOp::getData );
	FunctionLookup::setProviderCallback( RoutingDbOp::getProvider );

	// read application settings
	ReadApplicationSettings();

	RoutingMessageEvaluator::setGetOriginalRefFunction( &RoutingDbOp::GetOriginalRef );
	RoutingMessageEvaluator::setGetBatchTypeFunction( &RoutingDbOp::GetBatchType );
	RoutingMessageEvaluator::ReadEvaluators( GlobalSettings["PluginLocation"]  );

	// Load services to check for duplicates
	if ( RoutingEngine::isDuplicateDetectionActive() )
	{
		m_DuplicateChecks = RoutingDbOp::GetDuplicateServices();
	}

	// start watcher
	string dbName = GlobalSettings.getSectionAttribute( "DataConnectionString", "database" );
	string dbUser = GlobalSettings.getSectionAttribute( "DataConnectionString", "user" );
	string dbPass = GlobalSettings.getSectionAttribute( "DataConnectionString", "password" );
	
	ConnectionString asyncCS( dbName, dbUser, dbPass ); 
		
	m_Watcher.setConnectionString( asyncCS );
	m_Watcher.setSelectSPName( GlobalSettings[ "RoutingJobs.SelectSP" ] );
	m_Watcher.setProvider( GlobalSettings[ "RoutingJobs.Provider" ] );
	m_Watcher.setNotificationType( AbstractWatcher::NotificationObject::TYPE_CHAR );

	unsigned int timeoutArchive = 30;
	if( GlobalSettings.getSettings().ContainsKey( "IdleTimeoutArchive" ) )
	{
		m_IdleArchivingActive = true;
		timeoutArchive = StringUtil::ParseInt( GlobalSettings[ "IdleTimeoutArchive" ] );
		if ( timeoutArchive < 30 )
		{
			DEBUG( "Timeout for archiving when idle set too low ( min value : 30 seconds )" );
			timeoutArchive = 30;
		}
	}
	else
	{
		DEBUG( "Archiving when idle was not activated in config file. Set IdleTimeoutArchive to a value > 30 to enable it." );
	}
	
	if ( m_IdleArchivingActive || m_DuplicateDetectionActive )
		m_Watcher.setIdleCallback( RoutingEngine::IdleProcess, timeoutArchive );
		
	if ( startWatcher )
		m_Watcher.setEnableRaisingEvents( true );
	
	pthread_attr_t ThreadAttr;

	int attrInitResult = pthread_attr_init( &ThreadAttr );
	if ( 0 != attrInitResult )
	{
		TRACE( "Error initializing thread attribute [" << attrInitResult << "]" );
		throw runtime_error( "Error initializing thread attribute" );
	}
	
	int setDetachedResult = pthread_attr_setdetachstate( &ThreadAttr, PTHREAD_CREATE_JOINABLE );
	if ( 0 != setDetachedResult )
	{
		TRACE( "Error setting joinable option to thread attribute [" << setDetachedResult << "]" );
		throw runtime_error( "Error setting joinable option to thread attribute" );
	}
	
	int threadStatus = 0;
	
#ifndef CHECK_MEMLEAKS
	// if running in debugger, reattempt to create thread on failed request
	
	do
	{
		threadStatus = pthread_create( &m_CotThreadId, &ThreadAttr, RoutingEngine::COTMonitor, this );
		// allow cot monitor to start and acquire the locks
		sleep( 5 );
	} while( ( threadStatus != 0 ) && ( errno == EINTR ) );
	if ( threadStatus != 0 )
	{
		int errCode = errno;
		int attrDestroyResult = pthread_attr_destroy( &ThreadAttr );
		if ( 0 != attrDestroyResult )
		{
			TRACE( "Unable to destroy thread attribute [" << attrDestroyResult << "]" );
		}	
		
		stringstream errorMessage;
#ifdef CRT_SECURE
		char errBuffer[ 95 ];
		strerror_s( errBuffer, sizeof( errBuffer ), errCode );
		errorMessage << "Unable to create COT monitor thread. [" << errBuffer << "]";
#else
		errorMessage << "Unable to create COT monitor thread. [" << strerror( errCode ) << "]";
#endif
		throw runtime_error( errorMessage.str() );
	}
	
#else
	m_Watcher.waitForExit();
#endif

	/*int mutexInitResult = pthread_mutex_init( &m_BMMutex, NULL );
	if ( 0 != mutexInitResult )
	{
		TRACE_NOLOG( "Unable to init mutex BMMutex [" << mutexInitResult << "]" );
	}
	int condInitResult = pthread_cond_init( &m_BMCondition, NULL );
	if ( 0 != condInitResult )
	{
		TRACE_NOLOG( "Unable to init condition BMCondition [" << condInitResult << "]" );
	}

	do
	{
		threadStatus = pthread_create( &m_BMThreadId, &ThreadAttr, RoutingEngine::BusinessMessageProc, this );
	} while( ( threadStatus != 0 ) && ( errno == EINTR ) );
	if ( threadStatus != 0 )
	{
		pthread_attr_destroy( &ThreadAttr );
		
		stringstream errorMessage;
		errorMessage << "Unable to create BM thread. [" << sys_errlist[ errno ] << "]";
		throw runtime_error( errorMessage.str() );
	}
	*/

#ifdef SCROLL_CURSOR
	do
	{
		threadStatus = pthread_create( &m_RouterThreadId, &ThreadAttr, RoutingEngine::ThreadPoolWatcher, this );
	} while( ( threadStatus != 0 ) && ( errno == EINTR ) );
	if ( threadStatus != 0 )
	{
		pthread_attr_destroy( &ThreadAttr );
		
		stringstream errorMessage;
		errorMessage << "Unable to create thread pool watcher thread. [" << sys_errlist[ errno ] << "]";
		throw runtime_error( errorMessage.str() );
	}
#else
	do
	{
		threadStatus = pthread_create( &m_RouterThreadId, &ThreadAttr, RoutingEngine::RouterRoutine, this );
	} while( ( threadStatus != 0 ) && ( errno == EINTR ) );
	if ( threadStatus != 0 )
	{
		int errCode = errno;
		int attrDestroyResult = pthread_attr_destroy( &ThreadAttr );
		if ( 0 != attrDestroyResult )
		{
			TRACE( "Unable to destroy thread attribute [" << attrDestroyResult << "]" );
		}
		
		stringstream errorMessage;
#ifdef CRT_SECURE
		char errBuffer[ 95 ];
		strerror_s( errBuffer, sizeof( errBuffer ), errCode );
		errorMessage << "Unable to create router thread. [" << errBuffer << "]";
#else
		errorMessage << "Unable to create router thread. [" << strerror( errCode ) << "]";
#endif
		throw runtime_error( errorMessage.str() );
	}
#endif //SCROLL_CURSOR
	int attrDestroyResult = pthread_attr_destroy( &ThreadAttr );
	if ( 0 != attrDestroyResult )
	{
		TRACE( "Unable to destroy thread attribute [" << attrDestroyResult << "]" );
	}
#ifdef WIN32_SERVICE
	m_bAssumeRunBlocks = FALSE;
#endif
	m_Running = true;
}

void RoutingEngine::Stop()
{
#ifdef WIN32_SERVICE
	TRACE_SERVICE( "Stop invoked for [RoutingEngine]" );
	if ( !ReportStatus( SERVICE_STOP_PENDING, 20000 ) )
	{
		TRACE( "Failed to send [SERVICE_STOP_PENDING,20000] to the service" );
	}
#endif
	try
	{
		m_Watcher.setEnableRaisingEvents( false );
	}
	catch( ... )
	{
		TRACE( "An error occured while stopping jobs monitor" );
	}

	m_ShouldStop = true;

	DEBUG( "Waiting for routing workers to stop ... " );
	
	DEBUG( "Shutting down job pool ..." );
	m_JobPool.ShutdownPool();
	
	DEBUG( "Joining the routing thread [" << m_RouterThreadId << "]... " );

#if !defined( __GNUC__ )
	if ( 0 != pthread_kill( m_RouterThreadId, 0 ) )
	{ 
		DEBUG( "Thread " << m_RouterThreadId << " is not running" );
	}
	else
	{
		DEBUG( "Thread " << m_RouterThreadId << " is running" );
	}
#endif

	int joinResult = pthread_join( m_RouterThreadId, NULL );
	if ( 0 != joinResult )
	{
		TRACE( "Joining Router thread ended in error [" << joinResult << "]" );
	}
	
	DEBUG( "Shutting down message pool ..." );
	m_MessagePool.ShutdownPool();

	DEBUG( "Shutting down thread exec pool ..." );
	m_ThreadExecPool.ShutdownPool();

	int mutexLockResult = pthread_mutex_lock( &m_CotMarkersMutex );
	if ( mutexLockResult != 0 )
		TRACE( "Unable to lock COTMarkers mutex [" << mutexLockResult << "]"  )

	int condSignalResult = pthread_cond_signal( &ShutdownCOTMonitorCond );
	if ( condSignalResult != 0 )
		TRACE( "Unable to send shutdown condition signal [" << condSignalResult << "]" )
	int mutexUnlockResult = pthread_mutex_unlock( &m_CotMarkersMutex );
	if ( mutexUnlockResult != 0 )
		TRACE( "Unable to unlock COTMarkers mutex [" << mutexUnlockResult << "]" )

	DEBUG( "Waiting for cot monitor to stop ... " );
	joinResult = pthread_join( m_CotThreadId, NULL );
	if ( 0 != joinResult )
	{
		TRACE( "Joining COT thread ended in error [" << joinResult << "]" );
	}

	DEBUG( "Stopped. " );

	m_Running = false;
#if defined( WIN32_SERVICE )
	if ( !ReportStatus( SERVICE_STOPPED ) )
	{
		TRACE( "Failed to send [SERVICE_STOPPED] to the service" );
	}
#endif
}

/*void* RoutingEngine::BusinessMessageProc( void* data )
{
	pthread_mutex_lock( )
}*/

// Watches for terminated threads ( worker thread )
void* RoutingEngine::ThreadPoolWatcher( void* data )
{
	while( !RoutingEngine::m_ShouldStop )
	{
		DEBUG_GLOBAL( "Thread pool watcher [" << pthread_self() << "] waiting for threads in pool" );
		
		try
		{
			// removing from pool will give this thread ownership of the thread
			WorkItem< RoutingJobExecutor > workExecutor = RoutingEngine::getThreadExecPool().removePoolItem();

			// don't allow cot monitor to change rules between processing
			int mutexLockResult = pthread_mutex_lock( &m_CotMarkersMutex );
			//int mutexLockResult = pthread_rwlock_rdlock( &m_CotMarkersRWLock );
			if ( 0 != mutexLockResult )
			{
				TRACE( "Unable to lock COT mutex [" << mutexLockResult << "]" );
			}

			DEBUG_GLOBAL( "Locked COT monitor" );

			TimeUtil::TimeMarker startTime;
													
			// signal the worker thread to begin
			workExecutor.get()->Signal( true );
			workExecutor.get()->Lock();

			TimeUtil::TimeMarker stopTime;
			DEBUG( "Time taken to execute job [" << workExecutor.get()->GetSignature() << "] was [" << stopTime - startTime << "]" );

			int mutexUnlockResult = pthread_mutex_unlock( &m_CotMarkersMutex );
			//int mutexUnlockResult = pthread_rwlock_unlock( &m_CotMarkersRWLock );
			if ( 0 != mutexUnlockResult )
			{
				TRACE( "Unable to unlock COT mutex [" << mutexUnlockResult << "]" );
			}
			// upon exiting this block, the workExecutor will be destroyed, 
			// RoutingJobExecutor waits for the worker thread to finish (join) and releases control
		}
		catch( ... )
		{
			int mutexUnlockResult = pthread_mutex_unlock( &m_CotMarkersMutex );
			//int mutexUnlockResult = pthread_rwlock_unlock( &m_CotMarkersRWLock );
			if ( 0 != mutexUnlockResult )
			{
				TRACE( "Unable to unlock COT mutex [" << mutexUnlockResult << "]" );
			}
			TRACE( "Fatal error while waiting for worker thread to finish" );
		}
	}
	return NULL;
}

void RoutingEngine::IdleProcess( void )
{
	if ( isIdleArchivingActive() )
		ArchiveIdle();
	if ( isDuplicateDetectionActive() )
		PurgeHashes();
}

void RoutingEngine::PurgeHashes( void )
{
	DEBUG_GLOBAL( "Purging hashes ( idle )" );
	LogManager::setCorrelationId( m_OwnCorrelationId );

	try
	{
		TimeUtil::TimeMarker startTime;
		RoutingDbOp::PurgeHashes( TheRoutingEngine->m_DuplicateDetectionTimeout );
		
		TimeUtil::TimeMarker stopTime;
		stringstream archiveTime;
		archiveTime << stopTime - startTime;
		DEBUG( "Time taken to purge hashes was [" << stopTime - startTime << "]" );
	}
	catch( const AppException& ex )
	{
		TRACE( ex.getMessage() );
		stringstream errorMessage;
		errorMessage << "An exception has occured while purging hashes [" << ex.getMessage() << "]";
		AppException aex( errorMessage.str(), ex );

		LogManager::Publish( aex );
	}
	catch( const std::exception& ex )
	{
		stringstream errorMessage;
		errorMessage << "An exception has occured while purging hashes [" << typeid( ex ).name() << " - " << ex.what() << "]";
		AppException aex( errorMessage.str(), ex );

		TRACE( aex.getMessage() );
		LogManager::Publish( aex );
	}
	catch( ... )
	{
		stringstream errorMessage;
		errorMessage << "A fatal error has occured  while purging hashes";
		TRACE( errorMessage.str() );
		LogManager::Publish( errorMessage.str() );
	}
}

// Perform automatic archiving when idle
void RoutingEngine::ArchiveIdle( void )
{
	DEBUG_GLOBAL( "Arhiving( idle )" );
	LogManager::setCorrelationId( m_OwnCorrelationId );

	try
	{
		TimeUtil::TimeMarker startTime;
		unsigned long archivedMessages = RoutingDbOp::Archive();
		TimeUtil::TimeMarker stopTime;
		if ( archivedMessages > 0 )
		{
			stringstream archiveTime;
			archiveTime << stopTime - startTime;
			DEBUG( "Time taken to archive " << archivedMessages << " messages was [" << stopTime - startTime << "]" );

			stringstream archiveInfo;
			archiveInfo << "Archived " << archivedMessages << " messages while idle";
			AppException aex( archiveInfo.str(), EventType::Info );
			aex.addAdditionalInfo( "Time taken", archiveTime.str() );
			LogManager::Publish( aex );
		}
		else
		{
			DEBUG( "No messages to archive. Time taken to archive 0 messages was [" << stopTime - startTime << "]" );
		}
	}
	catch( const AppException& ex )
	{
		TRACE( ex.getMessage() );
		stringstream errorMessage;
		errorMessage << "An exception has occured during archiving [" << ex.getMessage() << "]";
		AppException aex( errorMessage.str(), ex );

		LogManager::Publish( aex );
	}
	catch( const std::exception& ex )
	{
		stringstream errorMessage;
		errorMessage << "An exception has occured during archiving [" << typeid( ex ).name() << " - " << ex.what() << "]";
		AppException aex( errorMessage.str(), ex );

		TRACE( aex.getMessage() );
		LogManager::Publish( aex );
	}
	catch( ... )
	{
		stringstream errorMessage;
		errorMessage << "A fatal error has occured during archiving";
		TRACE( errorMessage.str() );
		LogManager::Publish( errorMessage.str() );
	}
}

// Inserts a new thread in the thread pool 
void RoutingEngine::OnNewJob( const AbstractWatcher::NotificationObject* notification )
{
	DEBUG_GLOBAL( "Notified" );

	LogManager::setCorrelationId( m_OwnCorrelationId );

	try
	{
		// enqueue thread
		DEBUG_GLOBAL( "Waiting on thread pool to allow inserts - thread [" << pthread_self() << "]." );

		WorkItem< RoutingJobExecutor > workExecutor( new RoutingJobExecutor() );
		RoutingJobExecutor* executor = workExecutor.get();

		if( !executor->Startable() )
			return;

		RoutingEngine::getThreadExecPool().addPoolItem( executor->getJobId(), workExecutor );

		// TODO check uniqueness of jobId
		DEBUG_GLOBAL( "Inserted routing thread in pool [" << executor->getJobId() << "]" );
	}
	catch( const WorkPoolShutdown& shutdownError )
	{
		TRACE( shutdownError.what() );
	}
	catch( ... )
	{
		TRACE( "A fatal error has occured during thread pool insert" );
	}
}

void RoutingEngine::NotificationCallback( const AbstractWatcher::NotificationObject* notification )
{
#ifdef CHECK_MEMLEAKS

	m_Notifications++;
	if ( m_Notifications > 5 )
		pthread_exit( NULL ); 

#endif

	DEBUG_GLOBAL( "Notified" );

	LogManager::setCorrelationId( m_OwnCorrelationId );

	WorkItem< RoutingJob > workJob( new RoutingJob() );
	RoutingJob* routingJob = workJob.get();

	// the routing message will keep a "live" reference if the message is added to the pool
	// TODO check what happens if an error occures before adding the item to the pool
	RoutingMessage* routingMessage = NULL;

	bool inTransaction = true;
	try
	{
		RoutingDbOp::getData()->BeginTransaction();

		// read job
		routingJob->ReadNextJob( notification->getObjectId() );
		DEBUG_GLOBAL( "Routing job read" );

		// read message
		string tableName = routingJob->getJobTable();

		// sanity check ... if no table name provided, the job must have complete/reactivate
		if ( ( tableName.length() == 0 ) && ( !routingJob->isComplete() ) )
		{
			RoutingDbOp::getData()->EndTransaction( TransactionType::COMMIT );
			inTransaction = false;

			routingJob->Abort();

			throw AppException( "Unable to perform routing job [routing point not defined]" );
		}
		
		// the job may not be related to a message ( virtual ):
		RoutingMessage* relatedMessage = NULL;
		if ( routingJob->hasFunctionParam( "V" ) && ( routingJob->getFunctionParam( "V" ) == "true" ) )
		{
			// set table name empty, so that the message will not be read
			relatedMessage = new RoutingMessage( "", Collaboration::GenerateGuid() );
			relatedMessage->setVirtual( true );
			relatedMessage->setTableName( tableName );

			// rapid batch works in parallel
			routingJob->setParallel( true );
		}
		else
		{
			// try to read the message ( in some cases the message isn't there )
			try
			{
				relatedMessage = new RoutingMessage( tableName, routingJob->getMessageId(), true );
				if ( tableName.length() > 0 )
					relatedMessage->Read();
			}
			catch( const RoutingExceptionMessageNotFound& ex )
			{
				TRACE( "Orphan job found [" << ex.getMessage() );
				if ( relatedMessage != NULL )
				{
					delete relatedMessage;
					relatedMessage = NULL;
				}
				AppException aex( ex.getMessage() );
				aex.setSeverity( EventSeverity::Fatal );
				throw aex;
			}
		}

		WorkItem< RoutingMessage > workMessage( relatedMessage );
		routingMessage = workMessage.get();
		//DEBUG_GLOBAL( "Routing message read [" << routingMessage->getPayload()->getTextConst() << "]" );

		RoutingDbOp::getData()->EndTransaction( TransactionType::COMMIT );
		inTransaction = false;

		pthread_t selfId = pthread_self();

		// enqueue message
		DEBUG_GLOBAL( "Waiting on message pool to allow inserts - thread [" << selfId << "]." );
		RoutingEngine::getMessagePool().addPoolItem( routingJob->getJobId(), workMessage );
		DEBUG_GLOBAL( "Inserted routing message in pool [" << routingMessage->getMessageId() << "]" );

		// enqueue job
		DEBUG_GLOBAL( "Waiting on job pool to allow inserts - thread [" << selfId << "]." );
		RoutingEngine::getJobPool().addPoolItem( routingJob->getJobId(), workJob );
		DEBUG_GLOBAL( "Inserted routing job in pool [" << routingJob->getJobId() << "]" );
	}
	catch( const RoutingExceptionJobAttemptsExceded &rex )
	{
		if ( inTransaction )
			RoutingDbOp::getData()->EndTransaction( TransactionType::ROLLBACK );

		routingJob->Abort();
		
		stringstream errorMessage;
		errorMessage << "Routing job aborted because it exceeded backout count (" << ROUTINGJOB_MAX_BACKOUT << ")";
		
		if ( routingMessage != NULL )
		{
			// try to remove the message from the pool
			( void )RoutingEngine::getMessagePool().removePoolItem( routingJob->getJobId(), false );
		}

		TRACE( rex.what() );
		LogManager::Publish( errorMessage.str() );

		return;
	}
	catch( const WorkPoolShutdown& shutdownError )
	{
		if ( inTransaction )
			RoutingDbOp::getData()->EndTransaction( TransactionType::ROLLBACK );
		TRACE_GLOBAL( shutdownError.what() );

		if ( routingMessage != NULL )
		{
			// try to remove the message from the pool
			( void )RoutingEngine::getMessagePool().removePoolItem( routingJob->getJobId(), false );
		}
	}
	catch( const AppException& ex )
	{
		if ( inTransaction )
			RoutingDbOp::getData()->EndTransaction( TransactionType::ROLLBACK );
		
		EventSeverity severity = ex.getSeverity();
		TRACE_GLOBAL( "A " << severity.ToString() << " error has occured during routing : " << ex.getMessage() );
		
		if ( routingJob != NULL )
		{
			TRACE( "Job details : Job table [" << routingJob->getJobTable() << "]; Job function [" <<
				routingJob->getFunction() << "]; Job user [" << routingJob->getUserId() << "]" );
			if ( severity.getSeverity() == EventSeverity::Fatal ) 
				routingJob->Abort();
			else
				routingJob->Rollback();
		}

		if ( ( routingMessage != NULL ) && ( routingJob != NULL ) )
		{
			// try to remove the message from the pool
			( void )RoutingEngine::getMessagePool().removePoolItem( routingJob->getJobId(), false );
		}
	}
	catch( ... )
	{
		if ( inTransaction )
			RoutingDbOp::getData()->EndTransaction( TransactionType::ROLLBACK );

		if ( routingJob != NULL )
		{
			TRACE( "Job details : Job table [" << routingJob->getJobTable() << "]; Job function [" <<
				routingJob->getFunction() << "]; Job user [" << routingJob->getUserId() << "]" );
			routingJob->Rollback();
		}

		if ( ( routingMessage != NULL ) && ( routingJob != NULL ) )
		{
			// try to remove the message from the pool
			( void )RoutingEngine::getMessagePool().removePoolItem( routingJob->getJobId(), false );
		}
	}
}

// Router routine ( worker thread )
void* RoutingEngine::RouterRoutine( void* data )
{
	RoutingEngine* instance = ( RoutingEngine * )data;
	
#if defined( _OPENMP )
	omp_init_lock( &RoutingEngine::parallelJobLock );

	if ( m_ParallelJobs > 0 )
		omp_set_num_threads( m_ParallelJobs );

#pragma omp parallel
	#pragma omp master
	{
		if ( m_ParallelJobs > 0 )
		{
			DEBUG( "Using " << omp_get_num_threads() << " out of specified " << m_ParallelJobs << " threads to execute jobs in parallel." );
		}
		else
		{
			DEBUG( "Using " << omp_get_num_threads() << " out of max " << omp_get_max_threads() << " threads to execute jobs in parallel." );
		}
	}
#ifdef AIX
		// block SIGINT every thread
		sigset_t signalSet;
		sigemptyset( &signalSet );
		sigaddset( &signalSet, SIGINT );

		if ( pthread_sigmask( SIG_BLOCK, &signalSet, NULL ) != 0 )
		{
			TRACE( "Can't catch SIGINT" );
		}
#endif //AIX
#pragma omp parallel 
	{
		// eeach thread should acquire the routing schema to avoid a deadlock :
		// for 1 job, one thread blocks waiting at the omp barrier and one blocks waiting for jobs, thus never reaching the barrier
		( void )getRoutingSchema();
#endif //_OPENMP
#if !defined(__ICL)
	try
	{
		for( ;; )
		{
#else
		int mCount = 0;
		for( ;; )		
		{
			if ( m_ProfileMessageCount > 0 )
			{
				mCount++;
				if ( mCount > m_ProfileMessageCount ) 
				{
#pragma omp master
					{
						DEBUG( "Master sleeping 5 seconds... " );
						try
						{
							TheRoutingEngine->m_Watcher.setEnableRaisingEvents( false );
						}
						catch( ... )
						{
							TRACE( "An error occured while stopping jobs monitor" );
						}

						TheRoutingEngine->m_ShouldStop = true;

						DEBUG( "Waiting for cot monitor to stop ... " );
						int joinResult = pthread_join( TheRoutingEngine->m_CotThreadId, NULL );
						if ( 0 != joinResult )
						{
							TRACE( "Joining COT thread ended in error [" << joinResult << "]" );
						}

						DEBUG( "Master exiting... " );
						TheRoutingEngine->m_Running = false;
					}
				}
			}
#endif
			if ( RoutingEngine::m_ShouldStop )
				break;

			pthread_t selfId = pthread_self();

			DEBUG_GLOBAL( "Worker [" << selfId << "] waiting for messages in pool" );
			TimeUtil::TimeMarker waitTime;
			WorkItem< RoutingJob > workJob = RoutingEngine::getJobPool().removePoolItem();
			
			TimeUtil::TimeMarker startTime;

			DEBUG_GLOBAL( "Waited [" << startTime - waitTime << "] milliseconds to retrieve a job" );
			INCREMENT_COUNTER_ON_T( instance, TRN_TOTAL );

#if defined ( CHECK_MEMLEAKS )
			InitAllocCheck( ACOutput_Advanced ); 
#endif

			MEM_CHECKPOINT_INIT();
			
			RoutingJob* job = workJob.get();

			int userId = job->getUserId();
			string jobId = job->getJobId();
			string tableName = job->getJobTable();

			// process a job
			try
			{
				//DumpContext::Clear();
				DEBUG_GLOBAL( TimeUtil::Get( "%d/%m/%Y %H:%M:%S", 19 ) << " - processing job [" << jobId << "]" );

#if defined( _OPENMP )
				if ( !job->isParallel() )
				{
					omp_set_lock( &RoutingEngine::parallelJobLock );
					
					try
					{
						RoutingDbOp::getData()->BeginTransaction();
						getRoutingSchema()->ApplyRouting( job );
						RoutingDbOp::getData()->EndTransaction( TransactionType::COMMIT );
					}
					catch( ... )
					{
						omp_unset_lock( &RoutingEngine::parallelJobLock );
						//throw;
					}
					omp_unset_lock( &RoutingEngine::parallelJobLock );
				}
				else
#endif // _OPENMP
				{
					RoutingDbOp::getData()->BeginTransaction();
					getRoutingSchema()->ApplyRouting( job );
					RoutingDbOp::getData()->EndTransaction( TransactionType::COMMIT );
				}

				// the job was defered
				if ( job->isDefered() )
				{
					DEBUG_GLOBAL( "Job was defered" );
				}
				else
				{
					job->Commit();
				}

				MEM_CHECKPOINT_COLLECT();

				( void )RoutingEngine::getMessagePool().removePoolItem( jobId, false );

				stringstream successMessage;
				if ( job->getDestination() == "?" )
					successMessage << "Message routed in " << tableName;
				else
				{
					if ( tableName.length() > 0 )
						successMessage << "Message routed from " << tableName << " to " << job->getDestination();
					else
						successMessage << "Message routed to " << job->getDestination();
				}

				AppException aex( successMessage.str(), EventType::Info );
				if ( userId > 0 )
					aex.addAdditionalInfo( "UserId", StringUtil::ToString( userId ) );

				job->populateAddInfo( aex );
				LogManager::Publish( aex );

				INCREMENT_COUNTER_ON_T( instance, TRN_COMMITED );
			}
			catch( const RoutingException& rex )
			{
				RoutingDbOp::getData()->EndTransaction( TransactionType::ROLLBACK );

				TRACE_GLOBAL( "An exception has occured during routing [" << rex.what() << "] at [" << rex.getLocation() << "]" );
				RoutingEngine::getMessagePool().erasePoolItem( jobId, false );
				job->Rollback();

				INCREMENT_COUNTER_ON_T( instance, TRN_ROLLBACK );

				//LogManager::Publish( rex );
			}
			catch( const RoutingExceptionMoveInvestig& ex )
			{
				if( job->isBatch() )
				{
					TRACE( "Batch [" << job->getBatchId() << "] terminated." );
					RoutingDbOp::TerminateBatch( job->getBatchId(), job->getBatchType(), BatchManagerBase::BATCH_FAILED, ex.getMessage() );
				}
				if( job->hasFunctionParam( "V" ) && ( job->getFunctionParam( "V" ) == "true" ) )
				{
					TRACE( "Rapid batch [" << job->getBatchId() << "] terminated." );
					RoutingDbOp::TerminateBatch( job->getBatchId(), job->getBatchType(), BatchManagerBase::BATCH_FAILED, ex.getMessage() );
				}

				RoutingDbOp::getData()->EndTransaction( TransactionType::COMMIT );
				
				EventSeverity severity = ex.getSeverity();
				TRACE_GLOBAL( "A " << severity.ToString() << " error has occured during routing : " << ex.getMessage() );
				RoutingEngine::getMessagePool().erasePoolItem( jobId, false );
				TRACE_GLOBAL( "Message removed from pool" );

				AppException aex( ex.getMessage(), ex );
				if ( userId > 0 )
					aex.addAdditionalInfo( "UserId", StringUtil::ToString( userId ) );

				job->populateAddInfo( aex );
				LogManager::Publish( aex );
				
				job->Commit();

				INCREMENT_COUNTER_ON_T( instance, TRN_ABORTED );
			}
			catch( const AppException& ex )
			{
				EventSeverity severity = ex.getSeverity();
				if ( severity.getSeverity() == EventSeverity::Fatal )
				{
					if( job->isBatch() )
					{
						TRACE( "Batch [" << job->getBatchId() << "] terminated." );
						RoutingDbOp::TerminateBatch( job->getBatchId(), job->getBatchType(), BatchManagerBase::BATCH_FAILED, ex.getMessage() );
					}
					if( job->hasFunctionParam( "V" ) && ( job->getFunctionParam( "V" ) == "true" ) )
					{
						TRACE( "Rapid batch [" << job->getBatchId() << "] terminated." );
						RoutingDbOp::TerminateBatch( job->getBatchId(), job->getBatchType(), BatchManagerBase::BATCH_FAILED, ex.getMessage() );
					}
					RoutingDbOp::getData()->EndTransaction( TransactionType::COMMIT );
				}
				else
					RoutingDbOp::getData()->EndTransaction( TransactionType::ROLLBACK );
				
				TRACE_GLOBAL( "A " << severity.ToString() << " error has occured during routing : " << ex.getMessage() );
				RoutingEngine::getMessagePool().erasePoolItem( jobId, false );

				stringstream errorMessage;
				errorMessage << "An exception has occured during routing [" << typeid( ex ).name() << " - " << ex.getMessage() << "]";
				AppException aex( errorMessage.str(), ex );
				if ( userId > 0 )
					aex.addAdditionalInfo( "UserId", StringUtil::ToString( userId ) );

				job->populateAddInfo( aex );
				LogManager::Publish( aex );
				
				if( severity.getSeverity() == EventSeverity::Fatal )
				{
					job->Abort();
					INCREMENT_COUNTER_ON_T( instance, TRN_ABORTED );
				}
				else
				{
					job->Rollback();
					INCREMENT_COUNTER_ON_T( instance, TRN_ROLLBACK );
				}
			}
			catch( const std::exception& ex )
			{
				// we need to terminate the batch 
				if ( ( ( job->isBatch() ) || ( ( job->hasFunctionParam( "V" ) && ( job->getFunctionParam( "V" ) == "true" ) ) ) ) && ( job->getBackoutCount() == ROUTINGJOB_MAX_BACKOUT ) )
				{
					TRACE( "Batch [" << job->getBatchId() << "] terminated." );
					RoutingDbOp::TerminateBatch( job->getBatchId(), job->getBatchType(), BatchManagerBase::BATCH_FAILED, ex.what() );

					RoutingDbOp::getData()->EndTransaction( TransactionType::COMMIT );
				}
				else 
					RoutingDbOp::getData()->EndTransaction( TransactionType::ROLLBACK );
				
				TRACE_GLOBAL( "A [" << typeid( ex ).name() << "] exception has occured during routing : " << ex.what() );
				RoutingEngine::getMessagePool().erasePoolItem( jobId, false );

				stringstream errorMessage;
				errorMessage << "An exception has occured during routing [" << typeid( ex ).name() << " - " << ex.what() << "]";
				AppException aex( errorMessage.str(), ex );
				if ( userId > 0 )
					aex.addAdditionalInfo( "UserId", StringUtil::ToString( userId ) );
					
				job->populateAddInfo( aex );
				LogManager::Publish( aex );

				job->Rollback();
				INCREMENT_COUNTER_ON_T( instance, TRN_ROLLBACK );
			}
			catch( ... )
			{
				RoutingDbOp::getData()->EndTransaction( TransactionType::ROLLBACK );
				
				TRACE_GLOBAL( "An exception has occured during routing : unknown error"  );
				RoutingEngine::getMessagePool().erasePoolItem( jobId, false );

				AppException aex( "An exception has occured during routing [unknown error]" );
				if ( userId > 0 )
					aex.addAdditionalInfo( "UserId", StringUtil::ToString( userId ) );

				job->populateAddInfo( aex );
				LogManager::Publish( aex );
					
				//TODO report exception
				job->Rollback();
				INCREMENT_COUNTER_ON_T( instance, TRN_ROLLBACK );
			}

			TimeUtil::TimeMarker stopTime;
			DEBUG_GLOBAL( TimeUtil::Get( "%d/%m/%Y %H:%M:%S", 19 ) << " - job [" << jobId << " - " << job->getFunction() << "] finished in [" << stopTime - startTime << " ms]" );

			//DumpContext::Dump();
			MEM_CHECKPOINT_END_REPORT( "message processing", "RoutingEngine");

#if defined ( CHECK_MEMLEAKS ) 
			DeInitAllocCheck();
#endif
		} //infinite loop
#if !defined(__ICL)
	} //try
	catch( const WorkPoolShutdown& shutdownError )
	{
		TRACE_GLOBAL( "Routing engine is shutting down... closing [" << shutdownError.what() << "]" );
	}
	catch( const std::exception& ex )
	{
		stringstream errorMessage;
		errorMessage << "An exception has occured during routing [" << ex.what() << "]";
		
		TRACE_GLOBAL( errorMessage.str() );
		
		AppException aex( errorMessage.str(), EventType::Error );		
		LogManager::Publish( aex );
	}
	catch( ... )
	{
		AppException xex( "An exception has occured during routing [unknown error]" );
		TRACE_GLOBAL( xex.getMessage() );
				
		LogManager::Publish( xex );
	}
#endif
#if defined( _OPENMP )
	} //omp section

	omp_destroy_lock( &RoutingEngine::parallelJobLock );
#endif // _OPENMP

	DEBUG( "Thread exiting... " );

	pthread_exit( NULL );
	return NULL;
}

//COT monitor
void* RoutingEngine::COTMonitor( void* data )
{
	RoutingCOT cotMarkers;

	int condInitResult = pthread_cond_init( &ShutdownCOTMonitorCond, NULL );
	if ( 0 != condInitResult )
	{
		TRACE( "Unable to init condition ShutdownCOTMonitorCond [" << condInitResult << "]" )
	}
	
	while( !RoutingEngine::m_ShouldStop )
	{
		int mutexLockResult = pthread_mutex_lock( &m_CotMarkersMutex );	
		//int mutexLockResult = pthread_rwlock_wrlock( &m_CotMarkersRWLock );	
		if ( 0 != mutexLockResult )
		{
			TRACE( "Unable to lock COT mutex [" << mutexLockResult << "]" );
		}
		
		try
		{
			bool cotMarkersChanged = cotMarkers.Update();
			m_ActiveCotMarker = cotMarkers.ActiveMarker();
			
			if ( ( cotMarkersChanged ) || ( m_PreviousCotMarker != m_ActiveCotMarker ) )
			//if ( m_PreviousCotMarker != m_ActiveCotMarker )
			{
				stringstream reloadMessage;
				if ( cotMarkersChanged )
					reloadMessage << "Reloading routing schema ( COT markers changed )";
				else
					reloadMessage << "Reloading routing schema ( COT marker passed [" << m_ActiveCotMarker.getName() << "] )";
					
				DEBUG_GLOBAL( reloadMessage.str() );
				LogManager::Publish( reloadMessage.str(), EventType::Info );
				
				// this "dirties" the schema
				DefaultSchema.Load();
			
				stringstream routingSchemaMessage;
				routingSchemaMessage << "Using routing schema(s) [" << RoutingEngine::DefaultSchema.getName() << "]";
				LogManager::Publish( routingSchemaMessage.str(), EventType::Info );
				
				DefaultSchema.Explain();
				m_PreviousCotMarker = m_ActiveCotMarker;
			}
			else
			{
				DEBUG_GLOBAL( "COT marker [" << m_ActiveCotMarker.getName() << "] still active" );
			}
		}
		catch( ... ){}

		struct timespec abstime;
		abstime.tv_sec = time( NULL ) + RoutingEngine::m_CotDelay;
		abstime.tv_nsec = 0;
		int result = pthread_cond_timedwait( &ShutdownCOTMonitorCond, &m_CotMarkersMutex, &abstime );
		switch( result )
		{
			case 0: 
				DEBUG( "COT Monitor dead." )
				break;
			case ETIMEDOUT:
				DEBUG2( "COT Monitor timeout." )
				break;
			default:
				TRACE( "pthread_cond_timedwait failed. [" << result << "]")
		}
		int mutexUnlockResult = pthread_mutex_unlock( &m_CotMarkersMutex );
		//int mutexUnlockResult = pthread_rwlock_unlock( &m_CotMarkersRWLock );
		if ( 0 != mutexUnlockResult )
		{
			TRACE( "Unable to unlock COT mutex [" << mutexUnlockResult << "]" );
		}
	}

	int condDestroyResult = pthread_cond_destroy( &ShutdownCOTMonitorCond );
	if ( 0 != condDestroyResult )
	{
		TRACE( "Unable to destroy ShutdownCOTMonitorCond [" << condDestroyResult << "]" )
	}
	
	pthread_exit( NULL );
	return NULL;
}

// ReadApplicationSettings
void RoutingEngine::ReadApplicationSettings()
{
	// options for duplicate detection
	m_DuplicateDetectionActive = ( ( GlobalSettings.getSettings().ContainsKey( "DuplicateDetectionPurge" ) ) &&
		( GlobalSettings[ "DuplicateDetectionPurge" ] != "0" ) );
	if( m_DuplicateDetectionActive )
	{
		DEBUG( "Parsing DuplicateDetectionPurge interval..." );
		m_DuplicateDetectionTimeout = StringUtil::ParseInt( GlobalSettings[ "DuplicateDetectionPurge" ] );
		if ( ( m_DuplicateDetectionTimeout < 0 ) || ( m_DuplicateDetectionTimeout > 96 ) )
		{
			TRACE( "Config option : DuplicateDetectionPurge setting not valid [" << m_DuplicateDetectionTimeout << "]" );
			m_DuplicateDetectionTimeout = 24;
		}
		DEBUG( "[Business Rules] Duplicate detection is on. Expiration set to [" << m_DuplicateDetectionTimeout << "] hours." );
	}
	else
	{
		DEBUG( "[Business Rules] Duplicate detection is off." );
	}

	// get liquidities sp to call
	m_LiquiditiesHasIBAN = ( ( GlobalSettings.getSettings().ContainsKey( "LiquiditiesHasIBAN" ) ) &&
		( GlobalSettings[ "LiquiditiesHasIBAN" ] == "true" ) );
	if( m_LiquiditiesHasIBAN )
	{
		DEBUG( "[Business Rules] Options set for saving IBAN" );
	}
	else
	{
		DEBUG( "[Business Rules] Options set for ignoring IBAN" );
	}

	m_LiquiditiesHasIBANPL = ( ( GlobalSettings.getSettings().ContainsKey( "LiquiditiesHasIBANPL" ) ) &&
		( GlobalSettings[ "LiquiditiesHasIBANPL" ] == "true" ) );
	if( m_LiquiditiesHasIBANPL )
	{
		DEBUG( "[Business Rules] Options set for saving IBANPL" );
	}
	else
	{
		DEBUG( "[Business Rules] Options set for ignoring IBANPL" );
	}

	m_LiquiditiesHasCorresps = ( ( GlobalSettings.getSettings().ContainsKey( "LiquiditiesHasCorresps" ) ) &&
		( GlobalSettings[ "LiquiditiesHasCorresps" ] == "true" ) );
	if( m_LiquiditiesHasCorresps )
	{
		DEBUG( "[Business Rules] Options set for saving SenderCorresp and ReceiverCorresp" );
	}
	else
	{
		DEBUG( "[Business Rules] Options set for ignoring SenderCorresp and ReceiverCorresp" );
	}
		
	// get liquidities sp to call
	if( GlobalSettings.getSettings().ContainsKey( "LiquiditiesSP" ) )
		m_LiquiditiesSP = GlobalSettings[ "LiquiditiesSP" ];
	else
		m_LiquiditiesSP = "UpdateLiquidities";
	DEBUG( "[Business Rules] Liquidities SP set to [" << m_LiquiditiesSP << "]" );

	// get xslt for changing value date 
	if( GlobalSettings.getSettings().ContainsKey( "UpdateDateXSLT" ) )
		m_UpdateDateXSLT = GlobalSettings[ "UpdateDateXSLT" ];
	else
		m_UpdateDateXSLT = "addCurrentDate.xslt";
	DEBUG( "[Business Rules] Update value date transform set to [" << m_UpdateDateXSLT << "]" );
		
	// get xslt for building batch envelope
	if( GlobalSettings.getSettings().ContainsKey( "BatchXSLT" ) )
		m_BatchXSLT.insert( pair< string, string >( DEFAULT_QUEUE, GlobalSettings[ "BatchXSLT" ] ) );
	else
		m_BatchXSLT.insert( pair< string, string >( DEFAULT_QUEUE, "SwiftToACH.xslt" ) );
	DEBUG( "[Business Rules] Default batch transform set to [" << m_BatchXSLT[ DEFAULT_QUEUE ] << "]" );

	const NameValueCollection& crtSettings = GlobalSettings.getSettings();
	for( unsigned int xsltIterator = 0; xsltIterator < crtSettings.getCount(); xsltIterator++ )
	{
		if ( StringUtil::StartsWith( crtSettings[ xsltIterator ].first, "BatchXSLT_" ) )
		{
			string queue = crtSettings[ xsltIterator ].first.substr( 10 );
			string batchXsltFileName = crtSettings[ xsltIterator ].second;
			DEBUG( "Found custom XSLT for batching messages in [" << queue << "] : filename : [" << batchXsltFileName << "]" );
			TheRoutingEngine->m_BatchXSLT.insert( pair< string, string >( queue, batchXsltFileName ) );
		}
	}
	
	// get xsd for validating batch items
	if( GlobalSettings.getSettings().ContainsKey( "BatchSchema" ) )
		m_BatchSchema = GlobalSettings[ "BatchSchema" ];
	else
		m_BatchSchema = "cblrct.xsd";
	DEBUG( "[Business Rules] Batch validation schema set to [" << m_BatchSchema << "]" );
		
	// get xsd namespace for validating batch items
	if( GlobalSettings.getSettings().ContainsKey( "BatchSchemaNamespace" ) )
		m_BatchSchemaNamespace = GlobalSettings[ "BatchSchemaNamespace" ];
	else
		m_BatchSchemaNamespace = "urn:swift:xsd:CoreBlkLrgRmtCdtTrf";
	DEBUG( "[Business Rules] Batch element namespace schema set to [" << m_BatchSchemaNamespace << "]" );	

	// get ACK method
	if( GlobalSettings.getSettings().ContainsKey( "SAAACKMethod" ) )
		RoutingMessageEvaluator::setSAAACKMethod( GlobalSettings[ "SAAACKMethod" ] );
	DEBUG( "[Business Rules] SAA ACK method set to [" << RoutingMessageEvaluator::getSAAACKMethod() << "]" );		
			
	// get ACK method
	if( GlobalSettings.getSettings().ContainsKey( "SwiftACKMethod" ) )
		RoutingMessageEvaluator::setSwiftACKMethod( GlobalSettings[ "SwiftACKMethod" ] );
	DEBUG( "[Business Rules] SWIFT ACK method set to [" << RoutingMessageEvaluator::getSwiftACKMethod() << "]" );		
	
	// get TFD ACK method
	if( GlobalSettings.getSettings().ContainsKey( "TFDACKMethod" ) )
	{
		DEBUG( "Using TFDACKMethod setting found in config" );
		RoutingMessageEvaluator::setTFDACKMethod( GlobalSettings[ "TFDACKMethod" ] );
	}
	else
	{
		DEBUG( "TFDACKMethod setting not found in config. Using default value." );
	}
	DEBUG( "[Business Rules] TFD ACK method set to [" << RoutingMessageEvaluator::getTFDACKMethod() << "]" );		


	//mark approved replies messages
	if( GlobalSettings.getSettings().ContainsKey( "TFDACHApproved" ) )
	{
		DEBUG( "[Business Rules] TFD ACH reply process is set to mark approved messages" );
		RoutingMessageEvaluator::setMarkApproved( GlobalSettings[ "TFDACHApproved" ] == "true" );
	}
	else
	{
		DEBUG( "TFDACKMethod setting not found in config. Using default value [ignore approved messages]." );
	}

	// get parallel job limit
	if( GlobalSettings.getSettings().ContainsKey( "ParallelJobThreads" ) )
	{
		m_ParallelJobs = StringUtil::ParseInt( GlobalSettings[ "ParallelJobThreads" ] );
		DEBUG( "Using " << m_ParallelJobs << " threads to execute jobs in parallel" );
	}
	else
	{
		DEBUG( "Using unlimited ( processor# bound ) threads to execute jobs in parallel" );
		m_ParallelJobs = -1;
	}	

	//create the cot scheduler thread
	string rmInterval = GlobalSettings[ "RulesMonitorInterval" ];
	if ( rmInterval.length() > 0 )
		m_CotDelay = StringUtil::ParseInt( rmInterval );
	
	if ( m_CotDelay < 60 )
	{
		DEBUG( "RulesMonitorInterval must be between 60 and 600 (seconds)." );
		m_CotDelay = 60;
	}
	DEBUG( "Rules monitoring interval set to " << m_CotDelay << " seconds." );

	if( GlobalSettings.getSettings().ContainsKey( "BulkReactivateQueue" ) )
	{
		DEBUG( "[Business Rules] Bulk reject with no transactions specified trigger reactivation in [" << GlobalSettings[ "BulkReactivateQueue" ] << "]" );	
		m_BulkReactivateQueue = GlobalSettings[ "BulkReactivateQueue" ];
	}
	else
	{
		DEBUG( "BulkReactivateQueue setting not found in config. Using default value [don't reactivate]." );
	}	
}

string RoutingEngine::getBatchXslt( const string& queue )
{
	map< string, string >::const_iterator xsltFinder = TheRoutingEngine->m_BatchXSLT.find( queue );
	if ( xsltFinder != TheRoutingEngine->m_BatchXSLT.end() )
		return xsltFinder->second;

	return TheRoutingEngine->m_BatchXSLT[ DEFAULT_QUEUE ];
}

// RecoverLastSession
void RoutingEngine::RecoverLastSession() const
{
	// check for orphan routingjobs
	DEBUG( "[Recovery] Looking for orphan jobs ..." );

	// HACK( reactivate jobs sets job status 0 for jobs with status -1 )
	RoutingDbOp::ResumeJobs( -1 );
	DEBUG( "[Recovery] Orphan jobs reactivated ..." );
}

RoutingSchema* RoutingEngine::getRoutingSchema()
{
	RoutingSchema* schema = ( RoutingSchema* )pthread_getspecific( RoutingEngine::SchemaKey );
	if ( ( schema == NULL ) || DefaultSchema.isDirty() )
	{
		pthread_t selfId = pthread_self();
		// the master thread blocks cotmonitor
#pragma omp master
		{
			DEBUG( "Thread [" << selfId << "] waiting to acquire COT mutex ..." );
			int mutexLockResult = pthread_mutex_lock( &m_CotMarkersMutex );	
			//int mutexLockResult = pthread_rwlock_rdlock( &m_CotMarkersRWLock );
			if ( 0 != mutexLockResult )
			{
				stringstream errorMessage;
				errorMessage << "Unable to lock [read] COT mutex [" << mutexLockResult << "]";

				TRACE( errorMessage.str() );
				//throw runtime_error( errorMessage.str() );
			}
			DEBUG( "Thread [" << selfId << "] acquired COT mutex" );
		} //#pragma omp master

//all threads should be here
#pragma omp barrier
		
		// all reload and reset specific
		try
		{
#pragma omp single
			{
				// "clean" schema
				DefaultSchema.setDirty( false );
			}

			// delete the old schema
			if ( schema != NULL )
			{
				delete schema;
				schema = NULL;
			}

			bool duplicatePlans = false;
			if ( TheRoutingEngine->GlobalSettings.getSettings().ContainsKey( "CreatePlans" ) || TheRoutingEngine->GlobalSettings.getSettings().ContainsKey( "UsePlan" ) )
				duplicatePlans = true;

			schema = DefaultSchema.Duplicate( duplicatePlans );
			
			// one plan only
			if( TheRoutingEngine->GlobalSettings.getSettings().ContainsKey( "UsePlan" ) )
				schema->UsePlan( TheRoutingEngine->GlobalSettings[ "UsePlan" ] );
			else // cleanup created plans
				schema->DeletePlans();
			
			int setSpecificResult = pthread_setspecific( RoutingEngine::SchemaKey, schema );
			if ( 0 != setSpecificResult )
			{
				TRACE( "Set thread specific SchemaKey failed [" << setSpecificResult << "]" );
			}
		}
		catch( const std::exception& ex )
		{
			TRACE( "An error occured while reloading the routing schema [" << ex.what() << "]" );
		}
		catch( ... )
		{
			TRACE( "An unknown error occured while reloading the routing schema" );
		}

// now all threads should have the same schema
#pragma omp barrier

		// the master thread unblocks cotmonitor
#pragma omp master
		{
			int mutexUnlockResult = pthread_mutex_unlock( &m_CotMarkersMutex );
			//int mutexUnlockResult = pthread_rwlock_unlock( &m_CotMarkersRWLock );
			if ( 0 != mutexUnlockResult )
			{
				TRACE( "Unable to unlock [read] COT mutex [" << mutexUnlockResult << "]" );
			}		
		}
	}
	return schema;
}

void RoutingEngine::CreateKeys()
{
	cout << "Thread [" << pthread_self() << "] creating Schema keys..." << endl;

	int keyCreateResult = pthread_key_create( &RoutingEngine::SchemaKey, &RoutingEngine::DeleteSchema );
	if ( 0 != keyCreateResult )
	{
		TRACE( "An error occured while creating Schema key [" << keyCreateResult << "]" );
	}
}

void RoutingEngine::DeleteSchema( void* data )
{
	pthread_t selfId = pthread_self();	
	DEBUG2( "Deleting Schema for thread [" << selfId << "]" );
	RoutingSchema* schema = ( RoutingSchema* )data;
	if ( schema != NULL )
	{
		delete schema;
		schema = NULL;
	}
	int setSpecificResult = pthread_setspecific( RoutingEngine::SchemaKey, NULL );
	if ( 0 != setSpecificResult )
	{
		TRACE_NOLOG( "Set thread specific SchemaKey failed [" << setSpecificResult << "]" );
	}
}

void RoutingEngine::LoadMappings()
{
	// read keyword mappings
	DEBUG( "Reading keyword mappings" );
	DataSet* keywords = NULL;

	try
	{
		keywords = RoutingDbOp::GetKeywordMappings();
		if ( keywords == NULL )
			throw runtime_error( "No keywords found in database" );

		DEBUG( "Read " << keywords->size() << " keyword mappings from the database." );

		for( unsigned int i=0; i< keywords->size(); i++ )
		{
			//get keyword name
			string keyword = StringUtil::Trim( keywords->getCellValue( i, "NAME" )->getString() );
			
			//get mt
			string messageType = StringUtil::Trim( keywords->getCellValue( i, "MESSAGETYPE" )->getString() );

			map< string, map< string, string > >::iterator mtFinder = KeywordMappings.find( messageType );
			if( mtFinder == KeywordMappings.end() )
				KeywordMappings.insert( pair< string, map< string, string > >( messageType, map< string, string >() ) );
			
			//get tag
			string tag = StringUtil::Trim( keywords->getCellValue( i, "TAG" )->getString() );
			string selector = StringUtil::Trim( keywords->getCellValue( i, "SELECTOR" )->getString() );

			bool iso = ( StringUtil::ToUpper( selector ) == "SELECTORISO" );
									
			// find a place in sequence
			KeywordMappings[ messageType ].insert( pair< string, string >( keyword, tag ) );
			IsoMessageTypes.insert( pair< string, bool >( messageType, iso ) );
		}
	}
	catch( ... )
	{
		// cleanup		
		if( keywords != NULL )
		{
			delete keywords;
			keywords = NULL;
		}
		throw;
	}
	
	if( keywords != NULL )
	{
		delete keywords;
		keywords = NULL;
	}

	//init thread failed keyword mappings
	int onceResult = pthread_once( &NotFoundKeywordMappings::NotFoundKeysCreate, &NotFoundKeywordMappings::CreateKeys );
	if ( 0 != onceResult )
	{
		TRACE( "One time key creation for keyword mappings failed [" << onceResult << "]" );
	}
}

void RoutingEngine::LoadKeywords()
{
	// read keywords
	DEBUG( "Reading keywords" );
	DataSet* keywords = RoutingDbOp::GetKeywords();
	
	DEBUG( "Read " << keywords->size() << " keywords from the database." );
	
	try
	{
		for( unsigned int i = 0; i < keywords->size(); i++ )
		{
			//get keyword name, com, sel
			string name = StringUtil::Trim( keywords->getCellValue( i, "NAME" )->getString() );
			string comparer = StringUtil::Trim( keywords->getCellValue( i, "COMPARER" )->getString() );

			// the regex
			string valueSelector = StringUtil::Trim( keywords->getCellValue( i, "SELECTOR" )->getString() );
			string valueSelectorIso = StringUtil::Trim( keywords->getCellValue( i, "SELECTORISO" )->getString() );
			
			// insert the keyword
			RoutingKeyword keyword( name, comparer, valueSelector, valueSelectorIso );
			Keywords.insert( pair< string, RoutingKeyword >( name, keyword ) );
		}
		RoutingMessageEvaluator::setKeywords( Keywords );
	}
	catch( ... )
	{
		//cleanup
		if( keywords != NULL )
			delete keywords;
		throw;
	}
	
	if( keywords != NULL )
		delete keywords;
}
/*
// usage : MapId( "RNCB20051212", "RNCB200512120001" )
string RoutingEngine::MapId( const string id, const string mappedId, const int autoUnmap ) 
{
	if ( m_IdFactory == NULL )
		m_IdFactory = new MemoryStatePersist();

	// init a storage ( has no effect on MemoryStateOersistence )
	m_IdFactory->InitStorage( id );

	// now internal m_Storages has an item ( id->( "MappedId", mappedId ) )
	m_IdFactory->Set( id, "MappedId", mappedId );

	// if mappedId == id, m_Storages still has 1 item, otherwise it has two
	// case 1 : ( id->( "MappedId", mappedId ),( "RefCount", autoUnmap ) )
	// case 2 : ( id->( "MappedId", mappedId )
	//			( mappedId->( "RefCount", autoUnmap ) )
	m_IdFactory->Set( mappedId, "RefCount", autoUnmap );

	if ( mappedId != id )
	{
		// we now have	( id->( "MappedId", mappedId )
		//				( mappedId->( "RefCount", autoUnmap ) )
		//				( mappedId->( "ReverseMapId", id ) )
		m_IdFactory->Set( mappedId, "ReverseMapId", id );
	}
	return mappedId;
}

string RoutingEngine::RemapId( const string id, const string mappedId ) 
{
	if ( m_IdFactory == NULL )
		m_IdFactory = new MemoryStatePersist();

	m_IdFactory->Set( id, "MappedId", mappedId );
	return mappedId;
}

string RoutingEngine::GetReverseMapId( const string id )
{
	if ( m_IdFactory == NULL )
		m_IdFactory = new MemoryStatePersist();
	
	// if no storage is found, or the storage has no such key ( "ReverseMapId" ), "" is returned
	// else, the mapped id is returned
	return m_IdFactory->GetString( id, "ReverseMapId" );
}

string RoutingEngine::GetMappedId( const string id ) 
{
	if ( m_IdFactory == NULL )
		m_IdFactory = new MemoryStatePersist();
	
	// if no storage is found, or the storage has no such key ( "MappedId" ), "" is returned
	// else, the mapped id is returned
	return m_IdFactory->GetString( id, "MappedId" ); 
}

bool RoutingEngine::UnmapId( const string id )
{
	if ( m_IdFactory == NULL )
		m_IdFactory = new MemoryStatePersist();
	
	int refCount = m_IdFactory->GetInt( id, "RefCount" );
	if ( --refCount == 0 )
	{
		DEBUG( "Unmap id reached 0 refcount for id [" << id << "] and will release map" );
		
		string revMapId = m_IdFactory->GetString( id, "ReverseMapId" );

		// remove id storage
		m_IdFactory->ReleaseStorage( id );

		// remove reverse mapped id
		if ( id != revMapId )
			m_IdFactory->ReleaseStorage( revMapId );

		// return "released"
		return true;
	}
	else
	{
		DEBUG( "Unmap id reached " << refCount << " refcount for id [" << id << "]" );
		m_IdFactory->Set( id, "RefCount", refCount );

		// return "not released"
		return false;
	}
}*/

extern bool profile;

#if defined( WIN32_SERVICE )
void RoutingEngine::Run( DWORD argc, LPTSTR* argv )
{
	TRACE_SERVICE( "Run invoked for [ RoutingEngine ]" );
	
	try
	{
		if ( !ReportStatus( SERVICE_START_PENDING ) )
		{
			TRACE( "Failed to send [SERVICE_START_PENDING] to the service" );
			throw runtime_error( "Failed to send [SERVICE_START_PENDING] to the service" );
		}

		if ( profile )
			RoutingEngine::SetProfileMessageCount( 10 );
		RoutingEngine::TheRoutingEngine->Start( true );
	}
	catch( ... )
	{
		TRACE_SERVICE( "Service [ RoutingEngine ] failed to start" );
		if ( !ReportStatus( SERVICE_STOP_PENDING, 5000 ) )
		{
			TRACE( "Failed to send [SERVICE_STOP_PENDING,5000] to the service" );
		}
		throw;
	}
	if ( !ReportStatus( SERVICE_RUNNING ) )
	{
		TRACE( "Failed to send [SERVICE_RUNNING] to the service" );
		throw runtime_error( "Failed to send [SERVICE_RUNNING] to the service" );
	}
	TRACE_SERVICE( "Run dispatched [ RoutingEngine ]" );
}
#endif
