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

#include <signal.h>
#include <iostream>
#include <stdexcept>

#ifdef WIN32
#include <windows.h>
#define sleep(x) Sleep( (x)*1000 )

	#ifdef WIN32_SERVICE
		#include <tchar.h>
		#include <io.h>	
	#endif
#else
	#include <unistd.h>
#endif

#include "Base64.h"
#include "Connector.h"
#include "AppSettings.h"
//#include "Transactions/FileMetadataStatePersist.h"
#include "Transactions/MemoryStatePersist.h"
#include "Trace.h"
#include "LogManager.h"
#include "XSLT/XSLTFilter.h"
#include "XmlUtil.h"
#include "StringUtil.h"
#include "TransportHelper.h"
#include "PlatformDeps.h"

using namespace std;

bool Connector::m_ShouldStop = false;
Endpoint *Connector::m_Fetcher = NULL;
Endpoint *Connector::m_Publisher = NULL;	
		
//constructor
#if defined( WIN32 ) && defined( WIN32_SERVICE )
Connector::Connector( const string& mainProgram, const string& startupFolder ) : CNTService( TEXT( "Connector" ), TEXT( "Connector" ) ), 
	m_hStop( 0 ), m_FullProgramName( mainProgram ), m_Running( false )
#else
Connector::Connector( const string& mainProgram, const string& startupFolder ) : m_FullProgramName( mainProgram ), m_Running( false )
#endif
{
	TRACE_SERVICE( "Creating connector [" << m_FullProgramName << "]. Startup path is [" << startupFolder << "]" );

	try
	{
		stringstream configFilename;

#if _MSC_VER >= 1400
		configFilename << startupFolder << mainProgram << ".config";
#else
		configFilename << startupFolder << mainProgram << ".config";
#endif //_MSC_VER >= 1400

		const string configName = configFilename.str();
		const string configContent = StringUtil::DeserializeFromFile( configName );

		if ( Base64::isBase64( configContent ) )
		{
			cout << "Config is encrypted. Please enter encryption password:";
			Platform::EnableStdEcho( false );
			string key;
			getline( cin, key );
			Platform::EnableStdEcho( true );
			GlobalSettings.loadConfig( configName, configContent, key );
			for ( size_t i = 0; i < key.size(); i++ )
				key[i] = 'x';
		}
		else
			GlobalSettings.loadConfig( configName, configContent );

		if( GlobalSettings.getSettings().ContainsKey( "LogPrefix" ) )
			FileOutputter::setLogPrefix( GlobalSettings[ "LogPrefix" ] );

		if( GlobalSettings.getSettings().ContainsKey( "LogMaxLines" ) )
			FileOutputter::setLogMaxLines( StringUtil::ParseULong( GlobalSettings[ "LogMaxLines" ] ) );
		else
			FileOutputter::setLogMaxLines( 0 );

		if( GlobalSettings.getSettings().ContainsKey( "LogMaxExtraFiles" ) )
			FileOutputter::setLogMaxExtraFiles( StringUtil::ParseULong( GlobalSettings[ "LogMaxExtraFiles" ] ) );
		else
			FileOutputter::setLogMaxExtraFiles( 0 );
		
		if ( GlobalSettings.getSettings().ContainsKey( "ServiceName" ) )
			m_FullProgramName = GlobalSettings[ "ServiceName" ];

		DEBUG_GLOBAL( "Application settings for [" << m_FullProgramName << "] read." );

		bool eventsThreaded = ( GlobalSettings.getSettings().ContainsKey( "EventsThreaded" ) && ( GlobalSettings.getSettings()[ "EventsThreaded" ] == "true" ) );
		if ( eventsThreaded )
		{
			DEBUG( "Events publisher will start in a new thread." );
		}
		else
		{
			DEBUG( "Events publisher will share endpoint thread." );
		}

		LogManager::Initialize( GlobalSettings.getSettings(), eventsThreaded );
		bool isDefault = false;
		AbstractLogPublisher* mqLogPublisher = TransportHelper::createMqLogPublisher( GlobalSettings.getSettings(), isDefault );
		LogManager::Register( mqLogPublisher, isDefault );
		LogManager::setSessionId( Collaboration::GenerateGuid() );
	}
	catch( const std::exception& ex )
	{
		TRACE_GLOBAL( "Failed to load config settings. " << ex.what() );
		LogManager::Publish( ex );
		throw;
	}
	catch( ... )
	{
		TRACE_GLOBAL( "Failed to load config settings." );
		LogManager::Publish( "Failed to load config settings." );
		throw;
	}

#ifdef WIN32_SERVICE

#ifdef CRT_SECURE
	_tcscpy_s( m_lpServiceName, sizeof( m_lpServiceName ), TEXT( "Connector-" ) );
	_tcscat_s( m_lpServiceName, sizeof( m_lpServiceName ), TEXT( m_FullProgramName.c_str() ) );
	_tcscpy_s( m_lpDisplayName, sizeof( m_lpDisplayName ), m_lpServiceName );
#else
	_tcscpy( m_lpServiceName, TEXT( "Connector-" ) );
	_tcscat( m_lpServiceName, TEXT( m_FullProgramName.c_str() ) );
	_tcscpy( m_lpDisplayName, m_lpServiceName );
#endif // CRT_SECURE

	//should be set only when running on a mqserver machine
	//m_pszDependencies = TEXT( "MQSeriesServices" );
	m_bAssumeRunBlocks = FALSE;
#endif // WIN32_SERVICE
}

//destructor
Connector::~Connector()
{
	try
	{
		DEBUG( "DESTRUCTOR" );
	} catch( ... ){}

	try
	{
		if ( Connector::m_Fetcher != NULL )
		{
			DEBUG( "Deleting fetcher ... " );
			// delete endpoint
			delete Connector::m_Fetcher;
		}
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while releasing the fetcher" );
		} catch( ... ){}
	}

	try
	{
		if ( Connector::m_Publisher != NULL )
		{
			DEBUG( "Deleting publisher ..." );
			// delete endpoint
			delete Connector::m_Publisher;
		}
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while releasing the publisher" );
		} catch( ... ){}
	}

	try
	{
		DEBUG( "DESTRUCTOR done" );
	} catch( ... ){}
}

void Connector::Start( const string& recoveryMessageId )
{	
	TRACE( "[Main] Starting " << m_FullProgramName << ( ( recoveryMessageId.length() > 0 ) ? " in recovery mode." : "" ) );
	TRACE( "\t" << FinTPConnector::VersionInfo::Name() << "\t" << FinTPConnector::VersionInfo::Id() );
	TRACE( "\t" << FinTP::VersionInfo::Name() << "\t" << FinTP::VersionInfo::Id() );

	DEBUG( "Registering self" );
	AppException utilRegister( "Register", EventType::Management );
	utilRegister.addAdditionalInfo( "ServiceName", m_FullProgramName ); 
	utilRegister.addAdditionalInfo( "Version", FinTPConnector::VersionInfo::Id() );
	utilRegister.addAdditionalInfo( "Name", FinTPConnector::VersionInfo::Name() );

	LogManager::Publish( utilRegister );

	Connector::m_ShouldStop = false;
	
	DEBUG( "Reading endpoints from config file..." );
	try
	{
		// get fetcher settings
		if ( GlobalSettings.ContainsSection( "Fetcher" ) )
		{
			// get the value of "on" attribute
			string fetcherOn = GlobalSettings.getSectionAttribute( "Fetcher", "on" );
			
			// if on is not present, or it has the value "true" , create the fetcher
			if( ( fetcherOn.length() == 0 ) || ( fetcherOn == "true" ) )
			{
				DEBUG( "Fetcher enabled in config file. Type is [" << GlobalSettings.getSectionAttribute( "Fetcher", "type" ) << "]" );
				
				m_Fetcher = EndpointFactory::CreateEndpoint( m_FullProgramName, GlobalSettings.getSectionAttribute( "Fetcher", "type" ), true );
				m_Fetcher->setPersistenceFacility( new MemoryStatePersist() );
			}
			else
			{
				DEBUG( "Fetcher not enabled in config file. Messages will not flow from the application to  server." );
				LogManager::Publish( "Fetcher not enabled in config file. Messages will not flow from the application to  server.", EventType::Warning );
			}
		}
		// get publisher settings
		if( GlobalSettings.ContainsSection( "Publisher" ) )
		{
			// get the value of "on" attribute
			string publisherOn = GlobalSettings.getSectionAttribute( "Publisher", "on" );
			
			// if on is not present, or it has the value "true" , create the publisher
			if( ( publisherOn.length() == 0 ) || ( publisherOn == "true" ) )
			{
				DEBUG( "Publisher enabled in config file. Type is ["  << GlobalSettings.getSectionAttribute( "Publisher", "type" ) << "]" );
				
				m_Publisher = EndpointFactory::CreateEndpoint( m_FullProgramName, GlobalSettings.getSectionAttribute( "Publisher", "type" ), false );
				m_Publisher->setPersistenceFacility( new MemoryStatePersist() );
				
				//m_Fetcher->getFilterChain()->AddFilter( FilterType::WMQ, &( GlobalSettings.getSettings() ) );
				//m_Fetcher->getFilterChain()->Report( true, true );
			}
			else
			{
				DEBUG( "Publisher not enabled in config file. Messages will not flow from the FinTP server to the application." );
				LogManager::Publish( "Publisher not enabled in config file. Messages will not flow from the FinTP server to the application.", EventType::Warning );
			}
		}
		
		// get all filters
		map< std::string, NameValueCollection > sections = GlobalSettings.getSections();
		map< std::string, NameValueCollection >::iterator filters = sections.begin();

		for( ;filters != sections.end(); filters++ )
		{
			// only look for sections starting with "Filter"
			const size_t filterTextLength = sizeof( "Filter" ) - 1;
			if( ( filters->first ).length() < filterTextLength || ( filters->first ).substr( 0, filterTextLength ) != "Filter" ) 
			{
				DEBUG( "Skipping section : "  << filters->first );
				continue;
			}
			
			DEBUG( "Filter to add : [" << filters->first << "] is of type : " << FilterType::Parse( ( filters->second )[ "type" ] ) );
			
			if ( ( ( filters->second )[ "chain" ] == "Fetcher" ) && ( m_Fetcher != NULL ) )
			{
				DEBUG( "Adding filter to fetcher" );
				// add the filter
				m_Fetcher->getFilterChain()->AddFilter(	FilterType::Parse( ( filters->second )[ "type" ] ),&( filters->second ) );

				//m_Fetcher->getFilterChain()->Report( true, true );
			}
			if ( ( ( filters->second )[ "chain" ] == "Publisher" ) && ( m_Publisher != NULL ) )
			{			
				DEBUG( "Adding filter to publisher" );
				// add the filter
				m_Publisher->getFilterChain()->AddFilter( FilterType::Parse( ( filters->second )[ "type" ] ),
				 	&( filters->second ) );
				//m_Publisher->getFilterChain()->Report( true, true );
				DEBUG( "Done adding filter to publisher" );
			}
		}

		DEBUG( "Publisher ready" );		
		if( ( m_Publisher != NULL ) && ( m_Publisher->getFilterChain() != NULL ) )
			m_Publisher->getFilterChain()->Report( true, true );
			
		DEBUG( "Fetcher ready" );
		if( ( m_Fetcher != NULL ) && ( m_Fetcher->getFilterChain() != NULL ) )
			m_Fetcher->getFilterChain()->Report( true, true );
	}
	catch( ... )
	{
		TRACE( "Unexpected error while creating endpoints." );
		throw;
	}
	
	if ( m_Fetcher != NULL )
	{
		INIT_COUNTERS( m_Fetcher, ConnectorFetcher );
		m_Fetcher->Init();
		m_Fetcher->Start();
	}
	if ( m_Publisher != NULL )
	{
		INIT_COUNTERS( m_Publisher, ConnectorPublisher );
		m_Publisher->Init();
		m_Publisher->Start();
	}

	m_Running = true;
	TRACE( "[Main] Finished starting endpoints [" << m_FullProgramName << "]" );

	//TODO Monitor threads and restart if necessary

	/*pthread_attr_t scanThreadAttr;
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

	int threadStatus = 0;
	
	// if running in debugger, reattempt to create thread on failed request	
	do
	{
		threadStatus = pthread_create( &m_MonitorThreadId, &scanThreadAttr, Connector::MonitorThread, &( this->m_Running ) );
	} while( ( threadStatus != 0 ) && ( errno == EINTR ) );
	if ( threadStatus != 0 )
	{
		pthread_attr_destroy( &scanThreadAttr );
		
		stringstream errorMessage;
		errorMessage << "Unable to create monitor thread. [" << sys_errlist[ errno ] << "]";
		throw runtime_error( errorMessage.str() );
	}
	pthread_attr_destroy( &scanThreadAttr );
	return m_MonitorThreadId;*/
}

void Connector::Stop( void )
{
#if defined( WIN32_SERVICE )
	TRACE_SERVICE( "Stop invoked for [" << m_FullProgramName << "]" );
	if ( !ReportStatus( SERVICE_STOP_PENDING, 20000 ) )
	{
		TRACE( "Failed to send [SERVICE_STOP_PENDING,20000] to the service" );
	}
#endif

	try
	{
		TRACE_GLOBAL( "Waiting for the fetcher to stop ... " );

		// the call to stop will block current thread an join the endpoint thread
		if ( m_Fetcher != NULL )
			m_Fetcher->Stop();
			
		TRACE_GLOBAL( "Waiting for the publisher to stop ... " );
		if ( m_Publisher != NULL )
			m_Publisher->Stop();

		DEBUG_GLOBAL( "Unregistering self" );
		AppException utilUnregister( "Unregister", EventType::Management );
		LogManager::Publish( utilUnregister );
		sleep( 3 );
	}
	catch( ... )
	{
		// don't fail here
	}
	m_Running = false;
	
#if defined( WIN32_SERVICE )
	if ( !ReportStatus( SERVICE_STOPPED ) )
	{
		TRACE( "Failed to send [SERVICE_STOPPED] to the service" );
	}
#endif
}

#if defined( WIN32_SERVICE )
void Connector::Run( DWORD, LPTSTR * )
{
	TRACE_SERVICE( "Run invoked for [" << m_FullProgramName << "]" );
	
	try
	{
		if ( !ReportStatus( SERVICE_START_PENDING ) )
		{
			TRACE( "Failed to send [SERVICE_START_PENDING] to the service" );
			throw runtime_error( "Failed to send [SERVICE_START_PENDING] to the service" );
		}
		Start();
	}
	catch( ... )
	{
		TRACE_SERVICE( "Service [" << m_FullProgramName << "] failed to start" );
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
	TRACE_SERVICE( "Run dispatched [" << m_FullProgramName << "]" );
}
#endif
