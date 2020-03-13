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

#include <iostream>
#include <pthread.h>
#include <string>
#include <iomanip>

#ifdef WIN32
	#ifdef CHECK_MEMLEAKS
		//#define _CRTDBG_MAP_ALLOC
		#include <stdlib.h>
		#include <crtdbg.h>
	#endif
	#include <signal.h>
#endif

#include "LogManager.h"
#include "Trace.h"
#include "Connector.h"
#include "XmlUtil.h"
#include "XPathHelper.h"
#include "TimeUtil.h"
#include "Base64.h"
#include "InstrumentedObject.h"
#include "SSL/SSLFilter.h"
#include "Service/ServiceUtil.h"

#ifdef USING_REGULATIONS
#include "RoutingRegulations.h"
#endif // USING_REGULATIONS

#ifdef LINUX
	#include <signal.h>
#endif

#ifdef AIX
	#include <core.h> 
#endif

using namespace std;

pthread_mutex_t ShutdownSyncMutex;
pthread_cond_t ShutdownCond;
bool ShutdownCondSignaled = false;

map< string, pthread_t > m_MonitoredThreads;

#define HB_PERF_REPORT 5
#define HB_PERF_REPORT_DELAY 15

Connector *ConnectorInstance = NULL;

void sigTrapHandler( int signo )
{
	TRACE( "Caught trap/segv... " );
	
	map< string, pthread_t >::const_iterator threadWalker = m_MonitoredThreads.begin();
	while( threadWalker != m_MonitoredThreads.end() )
	{
		if ( pthread_equal( pthread_self(), threadWalker->second ) )
		{ 
			stringstream errorMessage;
			
			errorMessage << threadWalker->first << " thread 0x" << setbase( 16 ) << threadWalker->second << 
				setbase( 10 ) << " handles signal " << signo;
			try
			{
				TRACE( errorMessage.str() );
				LogManager::Publish( errorMessage.str() );
			}
			catch( ... ){}

			break;
		}		
		threadWalker++;
	}
	if ( threadWalker == m_MonitoredThreads.end() )
	{
		TRACE( "Unmonitored thread 0x" << setbase( 16 ) << pthread_self() << 
			setbase( 10 ) << " handles signal " << signo );
	}

#ifdef AIX

	struct coredumpinfo coreinfo;
	memset( &coreinfo, 0, sizeof( coreinfo ) );
	
	char corefilename[ 128 ];
	
	string crttime = TimeUtil::Get( "%H%M%S", 6 );
	string progname = ConnectorInstance->getMainProgramName();
	
	TRACE( "Dumping core ..." );
	sprintf( corefilename, "%s.%s.core", progname.c_str(), crttime.c_str() );
		
	coreinfo.length = strlen( corefilename );
	coreinfo.name = corefilename;
	
#ifndef AIX51
	coreinfo.pid = getpid();
	coreinfo.flags = GENCORE_VERSION_1;

	int corerc = gencore( &coreinfo );
#else // #ifndef AIX51
	
	int corerc = coredump( &coreinfo );	
#endif // #ifndef AIX51

	if( corerc != 0 )
	{
		TRACE( "Core dump failed [" << corefilename << "] with code [" << errno << "] " << sys_errlist[ errno ] );
	}
	else
	{
		TRACE( "Core dumped to file [" << corefilename << "]" );
	}

//	pthread_exit( NULL );

#endif // ifdef AIX

	throw runtime_error( "Segmentation fault/trap" );
}

static void sigFinish( int signo ) 
{
	if ( signo == SIGINT ) 
	{
		TRACE_GLOBAL( "Emergency exit. Ending ... please wait." ); 

		int mutexLockResult = pthread_mutex_lock( &ShutdownSyncMutex );
		if ( 0 != mutexLockResult )
		{
			TRACE( "Unable to lock ShutdownSyncMutex mutex [" << mutexLockResult << "]" );
		}
		ShutdownCondSignaled = true;
		int condSignalResult = pthread_cond_signal( &ShutdownCond );
		if ( 0 != condSignalResult )
		{
			TRACE_NOLOG( "Signal ShutdownCond failed [" << condSignalResult << "]" );
		}
		int mutexUnlockResult = pthread_mutex_unlock( &ShutdownSyncMutex );
		if ( 0 != mutexUnlockResult )
		{
			TRACE_NOLOG( "Unable to unlock ShutdownSyncMutex mutex [" << mutexUnlockResult << "]" );
		}
	}
}

#if !defined( TESTDLL_EXPORT ) && !defined ( TESTDLL_IMPORT )

void cleanup_exit( int retCode )
{
	XALAN_USING_XALAN( XalanTransformer );
	XALAN_USING_XALAN( XPathEvaluator )
	
	XalanTransformer::terminate();
	XPathEvaluator::terminate();
	XmlUtil::Terminate();
	LogManager::Terminate();
	XMLPlatformUtils::Terminate();
	SSLFilter::Terminate();
	//FileOutputter::Terminate();
	
	int condDestroyResult = pthread_cond_destroy( &ShutdownCond );
	if ( 0 != condDestroyResult )
	{
		TRACE_NOLOG( "Unable to destroy condition ShutdownCond [" << condDestroyResult << "]" );
	}
	int mutexDestroyResult = pthread_mutex_destroy( &ShutdownSyncMutex );
	if ( 0 != mutexDestroyResult )
	{
		TRACE_NOLOG( "Unable to destroy mutex ShutdownSyncMutex [" << mutexDestroyResult << "]" );
	}

	exit( retCode );
}

int main( int argc, char** argv )
{
#ifdef WIN32_SERVICE
	TCHAR buffer[MAX_PATH];
	DWORD result = GetModuleFileName( NULL, buffer, MAX_PATH );
	if ( result != 0 && result < MAX_PATH )
		ServiceUtil::changeCurrentDirectory( buffer );

	TRACE_SERVICE( "Entering run mode... " );
#endif

	XALAN_USING_XALAN( XalanTransformer );
	XALAN_USING_XALAN( XPathEvaluator )

	XMLPlatformUtils::Initialize(); 	
	XalanTransformer::initialize();		
	XPathEvaluator::initialize();

#if defined( WIN32 ) && defined ( CHECK_MEMLEAKS )
	//_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_CRT_DF );
	//_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
		
	//_CrtSetBreakAlloc( 182376 );
	//_CrtSetBreakAlloc( 198 );
#endif

	//Sleep( 10000 );
	string startupFolder = "";
	string recoveryMessageId = "";

	//if ( ( FileOutputter::Prefix.length() == 0 ) || ( FileOutputter::Prefix == "Unnamed" ) )
	try
	{
		FileOutputter::setLogPrefix( argv[ 0 ] );
	}
	catch( const std::exception& ex )
	{
		TRACE( ex.what() );
		cleanup_exit( -1 );
	}
	catch( ... )
	{
		TRACE( "An error occured while setting logs prefix" );
		cleanup_exit( -1 );
	}

	if ( argc > 1 )
	{
		if( !strcmp( argv[ 1 ], "--version" ) )
		{
			cout << "Version info : " << 
				endl << "\t" << argv[ 0 ] << " : " <<
				endl << "\t" << FinTPConnector::VersionInfo::Name() << "\t" << FinTPConnector::VersionInfo::Id() <<
				endl << FinTPConnector::VersionInfo::Url() << FinTPConnector::VersionInfo::BuildDate() <<
				endl << "\t" << FinTP::VersionInfo::Name() << "\t" << FinTP::VersionInfo::Id() <<
				endl << FinTP::VersionInfo::Url() << FinTP::VersionInfo::BuildDate() <<
#ifdef USING_REGULATIONS
				endl << "\t" << FinTPRR::VersionInfo::Name() << "\t" << FinTPRR::VersionInfo::Id() <<
				endl << FinTPRR::VersionInfo::Url() <<
#endif //USING_REGULATIONS
				endl << endl;
			
			cleanup_exit( 0 );
		}
		else if ( !strcmp( argv[ 1 ], "--startup_folder" ) )
		{
			startupFolder = string( argv[ 2 ] );
		}
		else if ( !strcmp( argv[ 1 ], "--msgid" ) )
		{
			recoveryMessageId = string( argv[ 2 ] );
		}
	}
	
#if defined( AIX ) || defined( LINUX )
	// block SIGINT in all threads ( including children of this thread )
	sigset_t signalSet;
	sigemptyset( &signalSet );
	sigaddset( &signalSet, SIGINT );

	if ( pthread_sigmask( SIG_BLOCK, &signalSet, NULL ) != 0 )
	{
		TRACE( "Can't catch SIGINT" );
		cleanup_exit( -1 );
	}

	struct sigaction actions;
	memset( &actions, 0, sizeof( actions ) );
	sigemptyset( &actions.sa_mask );
	actions.sa_flags = 0;

	actions.sa_handler = sigTrapHandler;
	if ( sigaction( SIGSEGV, &actions, NULL ) != 0 )
	{
		TRACE( "Can't handle SIGSEGV" );
		cleanup_exit( -1 );
	}
	
	actions.sa_handler = sigTrapHandler;
	if ( sigaction( SIGTRAP, &actions, NULL ) != 0 )
	{
		TRACE( "Can't handle SIGTRAP" );
		cleanup_exit( -1 );
	}

	actions.sa_handler = sigFinish;
	if ( sigaction( SIGINT, &actions, NULL ) != 0 )
	{
		TRACE( "Can't handle SIGINT" );
		cleanup_exit( -1 );
	}

#else // #ifdef AIX

	signal( SIGINT, sigFinish );
	signal( SIGSEGV, sigTrapHandler );

#endif

	// prepare shutdown condition
	int mutexInitResult = pthread_mutex_init( &ShutdownSyncMutex, NULL );
	if ( 0 != mutexInitResult )
	{
		TRACE_NOLOG( "Unable to init mutex ShutdownSyncMutex [" << mutexInitResult << "]" );
	}
	int condInitResult = pthread_cond_init( &ShutdownCond, NULL );
	if ( 0 != condInitResult )
	{
		TRACE_NOLOG( "Unable to init condition ShutdownCond [" << condInitResult << "]" );
	}

	bool initFailed = true;

	try
	{
		string serviceName = string( argv[ 0 ] );
		DEBUG_GLOBAL( "Service name [" << serviceName << "]" );

		// create the connector
		ConnectorInstance = new Connector( serviceName, startupFolder );

		if( ConnectorInstance == NULL )
		{
			TRACE( "Unable to create connector instance" );
			throw runtime_error( "unknown reason" );
		}

		DEBUG_GLOBAL( "Connector created." );

#ifdef WIN32_SERVICE
		// registerservice will call start if no params were given
		ConnectorInstance->RegisterService( argc, argv );
		DEBUG_GLOBAL( "Connector started at " << TimeUtil::Get( "%d/%m/%Y %H:%M:%S", 19 ) );
#else
		ConnectorInstance->Start( recoveryMessageId );
		DEBUG_GLOBAL( "Connector started at " << TimeUtil::Get( "%d/%m/%Y %H:%M:%S", 19 ) );
#endif

		initFailed = false;
	}
	catch( const AppException& rex )
	{
		TRACE_GLOBAL( "Failed to create/start connector. [" << rex.getMessage() << "]" );
		LogManager::Publish( rex );
	}
	catch( const std::exception& ex )
	{
		TRACE_GLOBAL( "Failed to create/start connector. [" << ex.what() << "]" );
		LogManager::Publish( ex );
	}
	catch( ... )
	{
		TRACE_GLOBAL( "Failed to create/start connector. [unknown error]" );
		LogManager::Publish( "Failed to create/start connector. [unknown error]" );
	}

	if ( initFailed )
	{
		if ( ConnectorInstance != NULL )
		{
			delete ConnectorInstance;
			ConnectorInstance = NULL;
		}
		cleanup_exit( -1 );		
		return -1;
	}
	else if ( !ConnectorInstance->isRunning() )
	{
		TRACE( "The service is not running." );
		delete ConnectorInstance;
		ConnectorInstance = NULL;

		cleanup_exit( 0 );
		return 0;
	}

#if defined( AIX ) || defined( LINUX )
	//reset the sigint in main thread
	if ( pthread_sigmask( SIG_UNBLOCK, &signalSet, NULL ) != 0 )
	{
		TRACE( "Can't reset SIGINT" );
	}
#endif

	struct timespec wakePerf;

	wakePerf.tv_sec = time( NULL ) + HB_PERF_REPORT_DELAY;
	wakePerf.tv_nsec = 0;

	int condWaitExit = 0;
	unsigned int perfReportCounter = 0;
	
	// get endpoint threads
	( void )m_MonitoredThreads.insert( pair< string, pthread_t >( "Main", pthread_self() ) );

	pthread_t publisherThread = LogManager::getPublisherThreadId();
	if( !pthread_equal( publisherThread, 0 ) )
		m_MonitoredThreads.insert( pair< string, pthread_t >( "Events publisher", publisherThread ) );

	if ( ConnectorInstance->getFetcher() != NULL )
	{
		( void )m_MonitoredThreads.insert( pair< string, pthread_t >( "Fetcher", ConnectorInstance->getFetcher()->getThreadId() ) );
		pthread_t watcherThread = const_cast< Endpoint* >( ConnectorInstance->getFetcher() )->getWatcherThreadId();
		if( !pthread_equal( watcherThread, 0 ) )
			( void )m_MonitoredThreads.insert( pair< string, pthread_t >( "Fetcher watcher", watcherThread ) );
	}

	if ( ConnectorInstance->getPublisher() != NULL )
	{
		( void )m_MonitoredThreads.insert( pair< string, pthread_t >( "Publisher", ConnectorInstance->getPublisher()->getThreadId() ) );
		pthread_t watcherThread = const_cast< Endpoint* >( ConnectorInstance->getPublisher() )->getWatcherThreadId();
		if( !pthread_equal( watcherThread, 0 ) )
			( void )m_MonitoredThreads.insert( pair< string, pthread_t >( "Publisher watcher", watcherThread ) );
	}
	
	do
	{
		// collect performance data at HB_PERF_REPORT_DELAY intervals.
		int mutexLockResult = pthread_mutex_lock( &ShutdownSyncMutex );
		if ( 0 != mutexLockResult )
		{
			TRACE( "Unable to lock ShutdownSyncMutex mutex [" << mutexLockResult << "]" );
		}
		try
		{
			condWaitExit = pthread_cond_timedwait( &ShutdownCond, &ShutdownSyncMutex, &wakePerf );
			wakePerf.tv_sec += HB_PERF_REPORT_DELAY;

			// Sanity check.. send kill with signal 0 ( check if the thread is alive )
			map< string, pthread_t >::const_iterator threadWalker = m_MonitoredThreads.begin();
			while( threadWalker != m_MonitoredThreads.end() )
			{
				// skip ourselves
				if ( pthread_equal( pthread_self(), threadWalker->second ) )
				{
					threadWalker++;
					continue;
				}
				
				if ( 0 != pthread_kill( threadWalker->second, 0 ) )
				{ 
					TRACE( threadWalker->first << " thread 0x" << setbase( 16 ) << threadWalker->second << 
						setbase( 10 ) << " is not running" );
				}
				else
				{
					TRACE( threadWalker->first << " thread 0x" << setbase( 16 ) << threadWalker->second << 
						setbase( 10 ) << " is running" );
				}

				threadWalker++;
			}
		
			string partReport = InstrumentedObject::Collect();
			DEBUG( partReport );
			if ( perfReportCounter == HB_PERF_REPORT - 1 )
			{
				AppException perfReport( "Performance", EventType::Management ); 
				perfReport.addAdditionalInfo( "Report", Base64::encode( InstrumentedObject::Report() ) );
				LogManager::Publish( perfReport );
				perfReportCounter = 0;
			}
			else
				perfReportCounter++;
		}
		catch( const AppException& rex )
		{
			TRACE_GLOBAL( "Failed to send perfomance report. [" << rex.getMessage() << "]" );
		}
		catch( const std::exception& ex )
		{
			TRACE_GLOBAL( "Failed to send perfomance report. [" << ex.what() << "]" );
		}
		catch( ... )
		{
			TRACE_GLOBAL( "Failed to send perfomance report. [unknown error]" );
		}
		int mutexUnlockResult = pthread_mutex_unlock( &ShutdownSyncMutex );
		if ( 0 != mutexUnlockResult )
		{
			TRACE_NOLOG( "Unable to unlock ShutdownSyncMutex mutex [" << mutexUnlockResult << "]" );
		}

	} 
	while ( !ShutdownCondSignaled && ConnectorInstance->isRunning() );

	TRACE( "Shutting down ... " );	
	try
	{
		if( ( ConnectorInstance != NULL ) && ConnectorInstance->isRunning() )
		{
			ConnectorInstance->Stop();
		}
		TRACE( "Connector stopped at " << TimeUtil::Get( "%d/%m/%Y %H:%M:%S", 19 ) );
	}
	catch( const AppException& rex )
	{
		TRACE( "Attempt to stop connector returned an error [" << rex.getMessage() << "]" );
		LogManager::Publish( rex );
	}
	catch( const std::exception& ex )
	{
		TRACE( "Attempt to stop connector returned an error [" << ex.what() << "]" );
		LogManager::Publish( ex );
	}
	catch( ... )
	{
		TRACE( "Attempt to stop connector returned an error [unknown error]" );
		LogManager::Publish( "Attempt to stop connector returned an error [unknown error]" );
	}

	if ( ConnectorInstance != NULL )
	{
		delete ConnectorInstance;
		ConnectorInstance = NULL;
	}

	cleanup_exit( 0 );
	return 0;
}
#endif
