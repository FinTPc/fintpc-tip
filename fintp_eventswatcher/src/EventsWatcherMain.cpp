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
	#include "windows.h"
	#include "signal.h"
	#define sleep(x) Sleep( (x)*1000 )
#elif defined( __GNUC__ )
	#include <signal.h>
	#include <unistd.h>
#else
	#include <unistd.h>
#endif

#include <iostream>
#include <iomanip>
#include <pthread.h>

#include <vector>
#include <deque>

#include <xalanc/Include/PlatformDefinitions.hpp>
#include <xalanc/XalanTransformer/XalanTransformer.hpp>

// this must be the first
#include "XmlUtil.h"
#include "LogManager.h"
#include "Trace.h"
#include "AppSettings.h"
#include "EventsWatcher.h"
#include "XPathHelper.h"
#include "StringUtil.h"
#include "TimeUtil.h"
 
#ifdef AIX
	#include <core.h>
#endif

using namespace std;

#define HB_PERF_REPORT_DELAY 30
#define HB_PERF_REPORT 5

std::map< std::string, pthread_t > m_MonitoredThreads;

pthread_mutex_t ShutdownSyncMutex;
pthread_cond_t ShutdownCond;
bool ShutdownCondSignaled = false;

EventsWatcher* TheEventsWatcher = NULL;

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
	string progname = "EventsWatcher";
	
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
	//XALAN_USING_XALAN( XalanTransformer );
	//XALAN_USING_XALAN( XPathEvaluator )
	
	//XalanTransformer::terminate();
	//XPathEvaluator::terminate();
	XmlUtil::Terminate();
	XMLPlatformUtils::Terminate();
	LogManager::Terminate();
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
	//XALAN_USING_XALAN( XalanTransformer );
	//XALAN_USING_XALAN( XPathEvaluator );

	XMLPlatformUtils::Initialize();
	//XalanTransformer::initialize();
	//XPathEvaluator::initialize();
	
#if defined( WIN32 ) && defined( CHECK_MEMLEAKS )
	//_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_CRT_DF );
	//_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	
	//_CrtSetBreakAlloc( 1361 );
	/*stringstream dumpBuffer;
	dumpBuffer << "Breakpoint set on alloc {" << debugAlloc << "}" << endl;
	
	_CrtDbgReport( _CRT_WARN, __FILE__, __LINE__, "EventsWatcher", dumpBuffer.str().c_str() );
	*/
#endif
	
	if ( argc > 1 )
	{
		if( !strcmp( argv[ 1 ], "--version" ) )
		{
			cout << "Version info : " << 
				endl << "\t" << argv[ 0 ] << " : " <<
				endl << "\t" << FinTPEventsWatcher::VersionInfo::Name() << "\t" << FinTPEventsWatcher::VersionInfo::Id() <<
				endl << FinTPEventsWatcher::VersionInfo::Url() <<
				endl << "\t" << FinTP::VersionInfo::Name() << "\t" << FinTP::VersionInfo::Id() << 
				endl << FinTP::VersionInfo::Url() <<
				endl << endl;
				
			cleanup_exit( 0 );
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

	try
	{
		string serviceName = string( argv[ 0 ] );
		DEBUG_GLOBAL( "Service name [" << serviceName << "]" );
		
		TheEventsWatcher = EventsWatcher::getInstance( serviceName.append( ".config" ) );
		//INIT_COUNTERS_STATIC( TheEventsWatcher );
		TheEventsWatcher->Start();
	}
	catch( const std::exception& ex )
	{
		TRACE_GLOBAL( ex.what() );
		LogManager::Publish( ex );
		
		if ( TheEventsWatcher != NULL )
			delete TheEventsWatcher;
		
		cleanup_exit( -1 );
	}
	catch( ... )
	{
		TRACE_GLOBAL( "[Main] Unexpected error while creating events watcher." );
		if ( TheEventsWatcher != NULL )
			delete TheEventsWatcher;

		cleanup_exit( -1 );
	}
	
#ifdef AIX
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
	m_MonitoredThreads.insert( pair< string, pthread_t >( "Main", pthread_self() ) );

	pthread_t publisherThread = LogManager::getPublisherThreadId();
	if( !pthread_equal( publisherThread, 0 ) )
		m_MonitoredThreads.insert( pair< string, pthread_t >( "Events publisher", publisherThread ) );

	if ( TheEventsWatcher != NULL )
	{
		m_MonitoredThreads.insert( pair< string, pthread_t >( "Publisher", TheEventsWatcher->getPublisherThreadId() ) );
		m_MonitoredThreads.insert( pair< string, pthread_t >( "Heartbeat monitor", TheEventsWatcher->getHBThreadId() ) );
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
	while ( !ShutdownCondSignaled && TheEventsWatcher->isRunning() );
	
	try
	{
		if ( TheEventsWatcher != NULL )
		{
			TheEventsWatcher->Stop();
			delete TheEventsWatcher;
		}
	}	
	catch( ... ){}

	cleanup_exit( 0 );
	return 0;
}
#endif // !defined( TESTDLL_EXPORT ) && !defined ( TESTDLL_IMPORT )
