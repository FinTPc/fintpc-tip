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
	#define __MSXML_LIBRARY_DEFINED__ 

	/*#ifdef _DEBUG
		//#define _CRTDBG_MAP_ALLOC
		#include <stdlib.h>
		#include <crtdbg.h>
	#endif*/
	#include <windows.h>
	#if defined( _DEBUG ) && defined( USE_STACKWALKER )
		#include "Stackwalker.h"
	#endif
	#include "signal.h"
//	#define sleep(x) Sleep( (x)*1000 ) 
#elif defined( __GNUC__ )
	#include <signal.h>
	#include <unistd.h>
#else
	#ifdef LEAK_DEBUG
		#define MALLOC_LOG_STACKDEPTH 10
		#include <malloc.h>
	#endif
	#include <unistd.h>
#endif

#include <iomanip>
#include <iostream>
#include <vector>
#include <deque>
#include <pthread.h>

using namespace std;

#include <xalanc/Include/PlatformDefinitions.hpp>
#include <xalanc/XalanTransformer/XalanTransformer.hpp>

// this must be the first
#include "XmlUtil.h"
#include "Collaboration.h"
#include "TimeUtil.h"
#include "LogManager.h"
#include "Trace.h"
#include "Service/ServiceUtil.h"

#include "RoutingEngine.h"

#ifdef USING_REGULATIONS
#include "RoutingRegulations.h"
#endif // USING_REGULATIONS

#ifdef AIX
	#include <core.h>
#endif

#define HB_PERF_REPORT 5
#define HB_PERF_REPORT_DELAY 15

map< string, pthread_t > m_MonitoredThreads;

pthread_mutex_t ShutdownSyncMutex;
pthread_cond_t ShutdownCond;
bool ShutdownCondSignaled = false;
bool ShouldStop = false;
bool profile = false;

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
	string progname = "RoutingEngine";
	
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

	//pthread_exit( NULL );

#endif // ifdef AIX

	throw runtime_error( "Segmentation fault/trap" );
}

static void sigFinish( int signo )
{
	if ( signo == SIGINT ) 
	{
		TRACE_GLOBAL( "[" << pthread_self() << "] Emergency exit. Ending ... please wait. Locking on ShutdownSyncMutex..." );
		int mutexLockResult = pthread_mutex_lock( &ShutdownSyncMutex );
		if ( 0 != mutexLockResult )
		{
			TRACE( "Unable to lock COT mutex [" << mutexLockResult << "]" );
		}
		ShutdownCondSignaled = true;
		TRACE_NOLOG( "Signaling Shutdown condition..." )
		int condSignalResult = pthread_cond_signal( &ShutdownCond );
		if ( 0 != condSignalResult )
		{
			TRACE_NOLOG( "Signal ShutdownCond failed [" << condSignalResult << "]" );
		}
		TRACE_NOLOG( "Unlocking ShutdownSyncMutex..." )
		int mutexUnlockResult = pthread_mutex_unlock( &ShutdownSyncMutex );
		if ( 0 != mutexUnlockResult )
		{
			TRACE_NOLOG( "Unable to unlock ShutdownSyncMutex mutex [" << mutexUnlockResult << "]" );
		}
		TRACE_NOLOG( "Done handling SIGINT..." )
	}
}

RoutingMessage testMessageProvider()
{
	RoutingMessage message( "<?xml version=\"1.0\" encoding=\"us-ascii\"?><smt:MT103 xmlns:smt=\"urn:xmlns:SWIFTMTs\"><sg:AckNack xmlns:sg=\"urn:xmlns:SWIFTgenerics\"><sg:AckNackBasicHeader AckNackBasicHeaderText=\"SPXAROBUAXXX1646131235\"></sg:AckNackBasicHeader><sg:AckNackMessageText><sg:tag177 tagValue=\"0210020105\"></sg:tag177><sg:tag451 tagValue=\"0\"></sg:tag451></sg:AckNackMessageText></sg:AckNack><sg:BasicHeader BlockIdentifier=\"1\" ApplicationIdentifier=\"F\" ServiceIdentifier=\"01\" SenderLT=\"SPXAROBUAXXX\" SessionNumber=\"1646\" SequenceNumber=\"131235\" xmlns:sg=\"urn:xmlns:SWIFTgenerics\"></sg:BasicHeader><sg:ApplicationHeader MessageType=\"103\" xmlns:sg=\"urn:xmlns:SWIFTgenerics\"><sg:ApplicationHeaderOutput BlockIdentifier=\"2\" IOIdentifier=\"O\" SenderTime=\"1759\" MessageInputReference=\"021001PNBPUS3NANYC6929661437\" Date=\"021002\" ReceiverTime=\"0059\" MessagePriority=\"N\"></sg:ApplicationHeaderOutput></sg:ApplicationHeader><smt:MessageText><smt:tag20 tagValue=\"F61001195399000 \"></smt:tag20><smt:tag23B tagValue=\"CRED\"></smt:tag23B><smt:tag32A tagValue=\"021001ROL20000,00\"></smt:tag32A><smt:tag50K tagValue=\"/000112229917375                   &#xA;ANGHEL N ERICKSON                  &#xA;10750 ROCKFORD RD APT 103          &#xA;PLYMOUTH MN 55442                  \"></smt:tag50K><smt:tag52A tagValue=\"USBKUS44IMT\"></smt:tag52A><smt:tag59 tagValue=\"/1549925                           &#xA;SIMA OCTAVIAN                      &#xA;SLOBOZIA ROMANIA                   \"></smt:tag59><smt:tag71A tagValue=\"SCA\"></smt:tag71A><smt:tag72 tagValue=\"/REC/SLOBOZIA ROMANIA\"></smt:tag72></smt:MessageText><smt:Trailer TrailerText=\"{MAC:33720AD7}{CHK:0E269BD55A05}}{S:{SPD:}{SAC:}{COP:S}\"></smt:Trailer></smt:MT103>" );
	
	DEBUG_GLOBAL( "Message ready" );
	return message;
}

#if !defined( TESTDLL_EXPORT ) && !defined ( TESTDLL_IMPORT )

void cleanup_exit( int retCode )
{
	RoutingMessageEvaluator::EvaluatorsCleanup();

	XALAN_USING_XALAN( XalanTransformer );
	XALAN_USING_XALAN( XPathEvaluator )
	
	XalanTransformer::terminate();
	XPathEvaluator::terminate();
	XmlUtil::Terminate();
	LogManager::Terminate();

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

#if defined( WIN32 ) && defined( _DEBUG )
//	DeInitAllocCheck();
#endif

#if defined( AIX ) && defined( LEAK_DEBUG )
	
	struct malloc_log *log_ptr = get_malloc_log_live( NULL );
	if ( log_ptr != NULL )
		for( int i=0; i<DEFAULT_RECS_PER_HEAP; i++ )
		{
			if ( log_ptr->cnt > 0 )
			{
				cerr << "Alloc [" << i << "] size : " << log_ptr->size << endl;
				cerr << "Stack trace : " << endl;
				for( int j=0; j<MALLOC_LOG_STACKDEPTH; j++ )
				{
					if ( !log_ptr->callers[ j ] )
						break;
					cerr << "\t0x" << setbase( 16 ) << log_ptr->callers[ j ] << setbase( 10 ) << endl;
				}
			}
			log_ptr++;
		}
		
#endif
	
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


#if defined( WIN32 ) && defined( _DEBUG )
	//_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );

	//_CrtSetBreakAlloc( 14987 );
	//_CrtSetBreakAlloc( 9699 );
	//_CrtSetBreakAlloc( 845 );
	//_CrtSetBreakAlloc( 29364 );
#endif

#if defined( WIN32 ) && defined( _DEBUG )
//	InitAllocCheck( ACOutput_XML );
#endif

	XALAN_USING_XALAN( XalanTransformer );
	XALAN_USING_XALAN( XPathEvaluator );

	XMLPlatformUtils::Initialize();
	XalanTransformer::initialize();
	XPathEvaluator::initialize();

	string RoutingEngineConfigEncryptionKey;
	
	if ( argc > 1 )
	{
		if( !strcmp( argv[ 1 ], "--profile" ) )
		{
			profile = true;
		}
		else if( !strcmp( argv[ 1 ], "--version" ) )
		{
			cout << "Version info : " << 
				endl << "\t" << argv[ 0 ] << " : " << 
				endl << "\t" << FinTPRoutingEngine::VersionInfo::Name() << "\t" << FinTPRoutingEngine::VersionInfo::Id() << 
				endl << FinTPRoutingEngine::VersionInfo::Url() <<
				endl << "\t" << FinTP::VersionInfo::Name() << "\t" << FinTP::VersionInfo::Id() <<
				endl << FinTP::VersionInfo::Url() <<
				endl << "\t" << FinTPWS::VersionInfo::Name() << "\t" << FinTPWS::VersionInfo::Id() <<
				endl << FinTPWS::VersionInfo::Url() <<
#ifdef USING_REGULATIONS
				endl << "\t" << FinTPRR::VersionInfo::Name() << "\t" << FinTPRR::VersionInfo::Id() <<
				endl << FinTPRR::VersionInfo::Url() <<
#endif //USING_REGULATIONS
				endl << endl;
				
			cleanup_exit( 0 );				
		}

		for ( size_t i = 1; i < argc; ++i )
		{
#ifdef WIN32_SERVICE
			if ( strstr( argv[ i ], "--service_key=" ) )
				RoutingEngineConfigEncryptionKey = ServiceUtil::decryptServiceKey( argv[ i ] + 14 );
#endif
			if ( strstr( argv[ i ], "--key=" ) )
			{
				RoutingEngineConfigEncryptionKey = argv[ i ] + 6;
#ifndef WIN32_SERVICE
				memset( argv[ i ] + 6, 'x', strlen( argv[ i ] ) - 6 ); //Process Hacker can still see the key
#endif
				break;
			}
		}
	}

#ifdef AIX

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

#else // #ifdef AIX

	signal( SIGINT, sigFinish );
	//signal( SIGSEGV, sigTrapHandler );

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

	try
	{
		RoutingEngine::TheRoutingEngine = new RoutingEngine( RoutingEngineConfigEncryptionKey );
		fill( RoutingEngineConfigEncryptionKey.begin(), RoutingEngineConfigEncryptionKey.end(), 'x' );
		RoutingEngineConfigEncryptionKey = "";
#ifdef WIN32_SERVICE
		RoutingEngine::TheRoutingEngine->RegisterService( argc, argv );
#else
		// manual override
		if ( argc == 4 )
		{
			WorkItem< RoutingJob > workJob( new RoutingJob( argv[ 1 ], argv[ 2 ], argv[ 3 ], StringUtil::ParseInt( argv[ 4 ] ) ) );
			RoutingJob* explicitJob = workJob.get();

			DEBUG_GLOBAL( "Explicit route from queue [" << argv[ 1 ] << "] for message [" << argv[ 2 ] << "]. Applying function [" << argv[ 3 ] << "]" );
			
			RoutingEngine::TheRoutingEngine->Start( false ); // don't start the watcher for jobs
			RoutingEngine::getRoutingSchema()->ApplyRouting( explicitJob );
			ShouldStop = true;
		}
		else
		{
			//INIT_COUNTERS_STATIC( TheRoutingEngine );
			if ( profile )
				RoutingEngine::SetProfileMessageCount( 10 );
			RoutingEngine::TheRoutingEngine->Start( true );
		}
#endif
	}
	catch( const RoutingException& rex )
	{
		TRACE_GLOBAL( rex.what() );
		LogManager::Publish( rex );
		
		if ( RoutingEngine::TheRoutingEngine != NULL )
			delete RoutingEngine::TheRoutingEngine;

		cleanup_exit( -1 );
	}
	catch( const AppException& rex )
	{
		TRACE_GLOBAL( rex.getMessage() );
		LogManager::Publish( rex );
		
		if ( RoutingEngine::TheRoutingEngine != NULL )
			delete RoutingEngine::TheRoutingEngine;

		cleanup_exit( -1 );
	}
	catch( const std::exception& ex )
	{
		TRACE_GLOBAL( ex.what() );
		LogManager::Publish( ex );
		
		if ( RoutingEngine::TheRoutingEngine != NULL )
			delete RoutingEngine::TheRoutingEngine;

		cleanup_exit( -1 );
	}
	catch( ... )
	{
		TRACE_GLOBAL( "[Main] Unexpected error while creating routing engine." );
		
		if ( RoutingEngine::TheRoutingEngine != NULL )
			delete RoutingEngine::TheRoutingEngine;

		cleanup_exit( -1 );
	}
	
	if ( !ShouldStop )
	{
#ifdef AIX
		//reset the sigint in main thread
		if ( pthread_sigmask( SIG_UNBLOCK, &signalSet, NULL ) != 0 )
		{
			TRACE( "Can't reset SIGINT" );
		}
		
		actions.sa_handler = sigFinish;
		if ( sigaction( SIGINT, &actions, NULL ) != 0 )
		{
			TRACE( "Can't handle SIGINT" );
			cleanup_exit( -1 );
		}
#endif

		if ( !RoutingEngine::TheRoutingEngine->isRunning() )
			cleanup_exit( 0 );

		struct timespec wakePerf;

		wakePerf.tv_sec = time( NULL ) + HB_PERF_REPORT_DELAY;
		wakePerf.tv_nsec = 0;

		int condWaitExit = 0;
		unsigned int perfReportCounter = 0;
		
		// get endpoint threads
		m_MonitoredThreads.insert( pair< string, pthread_t >( "Main", pthread_self() ) );
		pthread_t publisherThread = LogManager::getPublisherThreadId();
		if( !pthread_equal( publisherThread, 0 ) )
			m_MonitoredThreads.insert( pair< string, pthread_t >( "Events publisher", publisherThread ) );

		if ( RoutingEngine::TheRoutingEngine != NULL )
		{
			m_MonitoredThreads.insert( pair< string, pthread_t >( "Router", RoutingEngine::TheRoutingEngine->getRouterThreadId() ) );
			m_MonitoredThreads.insert( pair< string, pthread_t >( "COT monitor", RoutingEngine::TheRoutingEngine->getCotMonitorThreadId() ) );
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
						DEBUG( threadWalker->first << " thread 0x" << setbase( 16 ) << threadWalker->second << 
							setbase( 10 ) << " is not running" );
					}
					else
					{
						DEBUG( threadWalker->first << " thread 0x" << setbase( 16 ) << threadWalker->second << 
							setbase( 10 ) << " is running" );
					}

					threadWalker++;
				}
			
				string partReport = InstrumentedObject::Collect();
				TRACE( partReport );
				if ( perfReportCounter == HB_PERF_REPORT - 1 )
				{
					// clear perfcounters
					InstrumentedObject::Report();
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
		while ( !ShutdownCondSignaled && ( RoutingEngine::TheRoutingEngine != NULL ) &&  RoutingEngine::TheRoutingEngine->isRunning() );
	}

	TRACE( "Shutting down ... " );
	try
	{
		DEBUG_GLOBAL( "Unregistering self" );
		AppException utilUnregister( "Unregister", EventType::Management );
		LogManager::Publish( utilUnregister );
	
		if ( RoutingEngine::TheRoutingEngine != NULL )
			RoutingEngine::TheRoutingEngine->Stop();
	}
	catch( ... )
	{
		TRACE_GLOBAL( "[Main] Unexpected error while unregistering routing engine." );
		if ( RoutingEngine::TheRoutingEngine != NULL )
			delete RoutingEngine::TheRoutingEngine;
		cleanup_exit( -1 );
	}
		
	if ( RoutingEngine::TheRoutingEngine != NULL )
		delete RoutingEngine::TheRoutingEngine;

	cleanup_exit( 0 );
	return 0;
}

#endif // !defined( TESTDLL_EXPORT ) && !defined ( TESTDLL_IMPORT )
