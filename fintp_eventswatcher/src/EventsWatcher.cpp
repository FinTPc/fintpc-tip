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

#include <sstream>
#include <string>

#include "EventsWatcher.h"
#include "EventsWatcherDbOp.h"

#include "AppSettings.h"
#include "AbstractLogPublisher.h"

#include "LogManager.h"
#include "Trace.h"
#include "CommonExceptions.h"

#include "Collaboration.h"
#include "StringUtil.h"
#include "XmlUtil.h"
#include "TimeUtil.h"
#include "Base64.h"
#include "PlatformDeps.h"

#include "TransportHelper.h"
#include "DataSet.h"

#include "xercesc/util/PlatformUtils.hpp"
#include "xercesc/dom/DOM.hpp"
#include "xercesc/sax/ErrorHandler.hpp"

#ifdef WIN32
	#define __MSXML_LIBRARY_DEFINED__
	
	#include <windows.h>
	#include <io.h>

#ifdef CRT_SECURE
	#define access( exp1, exp2 ) _access_s( exp1, exp2 )
#else
	#define access(x,y) _access( x, y )
#endif
	
	#define sleep(x) Sleep( (x)*1000 )

#else
	#include <unistd.h> 
#endif

using namespace std;

#define HBM_DELAY 10

EventsWatcher* EventsWatcher::m_Instance = NULL;
bool EventsWatcher::ShouldStop = true;
pthread_mutex_t EventsWatcher::StatusSyncMutex = PTHREAD_MUTEX_INITIALIZER;

#define FALLBACK_UPDATE_STATE( passServiceId, passServiceName, passNewState ) \
	try \
	{ \
		UpdateServiceState( passServiceId, passServiceName, passNewState ); \
	} \
	catch( const std::exception& inner_ex_ ) \
	{ \
		AppException regex( "Failed to update service state", inner_ex_ ); \
		regex.setPid( m_SessionId ); \
		regex.setEventType( EventType::Error ); \
		UploadMessage( regex ); \
		return; \
	} \
	catch( ... ) \
	{ \
		AppException regex( "Failed to update service state [unknown reason]" ); \
		regex.setPid( m_SessionId ); \
		regex.setEventType( EventType::Error ); \
		UploadMessage( regex ); \
		return; \
	}

// Service implementation
string Service::ToString( SERVICE_STATE value )
{
	switch( value )
	{
		case RUNNING :
			return "Running";
		case STOPPED :
			return "Stopped";
		default :
			throw invalid_argument( "service state" );
	}
}		

string Service::Report() const
{
	stringstream reportMessage;
		
	reportMessage << "Service [" << m_ServiceName << "] with id [" << m_ServiceId << "] is " <<
		ToString( m_State ) << ". Last heartbeat was on " << TimeUtil::Get( "%d/%m/%Y %H:%M:%S", 19, &m_LastHeartbeat );
	
	if ( m_State == Service::RUNNING )
		reportMessage << ". Last session id is [" << m_SessionId << "]";

	return reportMessage.str();
}

double Service::getTimeSinceLastHeartbeat() const
{
	time_t now;
	time( &now );
	
	return difftime( now, m_LastHeartbeat );
}

void Service::setState( const Service::SERVICE_STATE newState )
{
	m_State = newState;	
	EventsWatcherDbOp::UpdateServiceState( m_ServiceId, ( long )newState, m_SessionId );
}
		
// EventsWatcher implementation
EventsWatcher* EventsWatcher::getInstance( const string& configFilename )
{
	if( m_Instance == NULL )
		m_Instance = new EventsWatcher( configFilename );
		
	return m_Instance;
}

EventsWatcher::EventsWatcher( const string& configFile ) : m_Watcher( &m_NotificationPool )
{	
	const string configContent = StringUtil::DeserializeFromFile( configFile );

	AppSettings GlobalSettings;

	if ( Base64::isBase64( configContent ) )
	{
		cout << "Config is encrypted. Please enter encryption password:";
		Platform::EnableStdEcho( false );
		string key;
		getline( cin, key );
		Platform::EnableStdEcho( true );
		GlobalSettings.loadConfig( configFile, configContent, key );
		for ( size_t i = 0; i < key.size(); i++ )
			key[i] = 'x';
	}
	else
		GlobalSettings.loadConfig( configFile, configContent );
	
	if( GlobalSettings.getSettings().ContainsKey( "LogPrefix" ) )
		FileOutputter::setLogPrefix( GlobalSettings[ "LogPrefix" ] );
	else
		FileOutputter::setLogPrefix( "EventsWatcher" );

	if( GlobalSettings.getSettings().ContainsKey( "LogMaxLines" ) )
		FileOutputter::setLogMaxLines( StringUtil::ParseULong( GlobalSettings[ "LogMaxLines" ] ) );
	else
		FileOutputter::setLogMaxLines( 0 );

	if( GlobalSettings.getSettings().ContainsKey( "LogMaxExtraFiles" ) )
		FileOutputter::setLogMaxExtraFiles( StringUtil::ParseULong( GlobalSettings[ "LogMaxExtraFiles" ] ) );
	else
		FileOutputter::setLogMaxExtraFiles( 0 );
		
	bool eventsThreaded = ( GlobalSettings.getSettings().ContainsKey( "EventsThreaded" ) && ( GlobalSettings.getSettings()[ "EventsThreaded" ] == "true" ) );
	LogManager::Initialize( GlobalSettings.getSettings(), eventsThreaded );
	bool isDefault = false;
	AbstractLogPublisher* mqLogPublisher = TransportHelper::createMqLogPublisher( GlobalSettings.getSettings(), isDefault );
	LogManager::Register( mqLogPublisher, isDefault );
	m_SessionId = Collaboration::GenerateGuid();
	LogManager::setSessionId( m_SessionId );

	TRACE( "[Main] Starting EventsWatcher... " );
			
	TRACE( "\t" << FinTPEventsWatcher::VersionInfo::Name() << "\t" << FinTPEventsWatcher::VersionInfo::Id() );
	TRACE( "\t" << FinTP::VersionInfo::Name() <<	"\t" << FinTP::VersionInfo::Id() );
	
	EventsWatcherDbOp::SetConfigCfgSection( GlobalSettings.getSection( "ConfigConnectionString" ) );
	EventsWatcherDbOp::SetConfigDataSection( GlobalSettings.getSection( "DataConnectionString" ) );
	
	DEBUG( "Setting up WMQ events watcher ... " );
	
	m_WatchQueue = GlobalSettings[ "MQToDB.WatchQueue" ];
	m_WatchQueueManager = GlobalSettings[ "MQToDB.QueueManager" ];
	m_TransportURI = GlobalSettings[ "MQToDB.URI" ];
	m_HelperType = TransportHelper::parseTransportType( GlobalSettings[ "MQToDB.Type" ] );

	m_Watcher.setQueue( m_WatchQueue );
	m_Watcher.setQueueManager( m_WatchQueueManager );
	m_Watcher.setTransportURI( m_TransportURI );
		
	DEBUG( "Creating MQ helper ... " );
	m_CurrentHelper = TransportHelper::CreateHelper( m_HelperType );
	m_Watcher.setHelperType( m_HelperType );

	//m_CurrentHelper->setOpenQueueOptions( MQOO_INPUT_AS_Q_DEF );
	m_CurrentHelper->setAutoAbandon( 3 );
	
#if defined( WIN32 ) && defined ( _DEBUG )
	m_OldStateAvailable = false;
#endif

	DEBUG( "Ready!" );
}

//destructor
EventsWatcher::~EventsWatcher()
{
	try
	{
		if ( m_CurrentHelper != NULL )
		{
			delete m_CurrentHelper;
			m_CurrentHelper = NULL;
		}
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while deleting WMQ helper" );
		} catch( ... ){}
	}

	try
	{
		EventsWatcherDbOp::Terminate();
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while terminating EventsWatcherDbOp" );
		} catch( ... ){}
	}

	try
	{
		DESTROY_COUNTER( TRN_COMMITED );
	} catch( ... ){};
	try
	{
		DESTROY_COUNTER( TRN_ABORTED );
	} catch( ... ){};
	try
	{
		DESTROY_COUNTER( TRN_TOTAL );
	} catch( ... ){};
}

void EventsWatcher::Start()
{	
	INIT_COUNTERS( &m_Watcher, EventsWatcherWatcher );

	INIT_COUNTER( TRN_COMMITED );
	INIT_COUNTER( TRN_ABORTED );
	INIT_COUNTER( TRN_TOTAL );
	
	INIT_COUNTERS( this, EventsWatcher );

	DEBUG( "Reading last state ... " );
	DataSet *states = EventsWatcherDbOp::ReadServiceStatus();

	if ( states == NULL )
		throw runtime_error( "Unable to read service states from config DB" );

	try
	{
		DEBUG( "Read [" << states->size() << "] services from service map" );
		// set initial heartbeat as now
		time_t now;
		time( &now );
		
		//get all connectors and their state
		for( unsigned int i=0; i<states->size(); i++ )
		{	
			long serviceId = states->getCellValue( i, "ID" )->getLong();
			long state = states->getCellValue( i, "STATUS" )->getLong();
			string serviceName = StringUtil::Trim( states->getCellValue( i, "NAME" )->getString() );

			string lastSession = "";
			try
			{
				if ( states->getCellValue( i, "SESSIONID" ) != NULL )
					lastSession = StringUtil::Trim( states->getCellValue( i, "SESSIONID" )->getString() );
			}
			catch( ... ){}

			long hbInterval = states->getCellValue( i, "HEARTBEATINTERVAL" )->getLong();
			
			// compatibility : generate a session id if none is found in DB
			if ( lastSession.length() == 0 )
				lastSession = Collaboration::GenerateGuid();

			m_ConnectorState.insert( pair< string, Service >( lastSession, Service( serviceId, state, now, serviceName, lastSession, hbInterval ) ) );
				
			DEBUG( m_ConnectorState[ lastSession ].Report() );
		}
	}
	catch( ... )
	{
		if ( states != NULL )
		{
			delete states;	
			states = NULL;
		}
		throw;
	}

	if ( states != NULL )
	{
		delete states;	
		states = NULL;
	}
			
	//register self	

	DEBUG( "Registering self" );
	AppException utilRegister( "Register", EventType::Management );
	utilRegister.addAdditionalInfo( "ServiceName", "EventsWatcher" );
	utilRegister.addAdditionalInfo( "Version", FinTPEventsWatcher::VersionInfo::Id() );
	utilRegister.addAdditionalInfo( "Name", FinTPEventsWatcher::VersionInfo::Name() );
	utilRegister.setPid( m_SessionId );
	UploadMessage( utilRegister );
	
	// start monitoring thread
	ShouldStop = false;
	
	//create the management thread
	pthread_attr_t ThreadAttr;
	int attrInitResult = pthread_attr_init( &ThreadAttr );
	if ( 0 != attrInitResult )
	{
		TRACE( "Error initializing scan thread attribute [" << attrInitResult << "]" );
		throw runtime_error( "Error initializing scan thread attribute" );
	}
	int setDetachResult = pthread_attr_setdetachstate( &ThreadAttr, PTHREAD_CREATE_JOINABLE );
	if ( 0 != setDetachResult )
	{
		TRACE( "Error setting joinable option to scan thread attribute [" << setDetachResult << "]" );
		throw runtime_error( "Error setting joinable option to scan thread attribute" );
	}
		
	// if running in debugger, reattempt to create thread on failed request
	int threadStatus = 0;
	do
	{
		threadStatus = pthread_create( &m_MonitorThreadId, &ThreadAttr, EventsWatcher::HeartbeatMonitor, ( void* )this );
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
		errorMessage << "Unable to create heartbeat monitor thread. [" << errBuffer << "]";
#else
		errorMessage << "Unable to create heartbeat monitor thread. [" << strerror( errCode ) << "]";
#endif
		throw runtime_error( errorMessage.str() );
	}

	// create processing thread
	do
	{
		threadStatus = pthread_create( &m_SelfThreadId, &ThreadAttr, EventsWatcher::StartInNewThread, ( void* )this );
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
		errorMessage << "Unable to create processing thread. [" << errBuffer << "]";
#else
		errorMessage << "Unable to create processing thread. [" << strerror( errCode ) << "]";
#endif
		throw runtime_error( errorMessage.str() );
	}
	
	int attrDestroyResult = pthread_attr_destroy( &ThreadAttr );
	if ( 0 != attrDestroyResult )
	{
		TRACE( "Unable to destroy thread attribute [" << attrDestroyResult << "]" );
	}
}

void* EventsWatcher::StartInNewThread( void* pThis )
{
	EventsWatcher* me = static_cast< EventsWatcher* >( pThis );
	try
	{
		me->internalStart();
	}
	catch( const std::exception& ex )
	{
		stringstream errorMessage;
		errorMessage << typeid( ex ).name() << " exception encountered by watcher [" << ex.what() << "]";
		TRACE( errorMessage.str() );

		try
		{
			LogManager::Publish( AppException( "Error encountered by watcher", ex ) );
		}
		catch( ... ){}
	}
	catch( ... )
	{
		stringstream errorMessage;
		errorMessage << "Exception encountered by watcher [unhandled exception]";
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

void EventsWatcher::internalStart( void )
{
	DEBUG( "Starting watcher... " );
	m_Watcher.setEnableRaisingEvents( true );

	//try to connect to MQSeries queue (get messages from queue)
	try
	{
		m_CurrentHelper->connect( m_WatchQueueManager, m_TransportURI );
		m_CurrentHelper->openQueue( m_WatchQueue );
	}
	catch( const std::exception& ex )
	{
		string exceptionType = typeid( ex ).name();
		string errorMessage = ex.what();
		
		TRACE_GLOBAL( "Unable to open watch queue [" << exceptionType << "] : " << errorMessage );
		throw;
	}
	catch( ... )
	{
		//TODO add reconect checks here
		TRACE( "Unable to open watch queue [unknown exception]" );
		throw;
	}

	bool succeeded = true;
	for(;;)
	{
		if ( !succeeded )
		{
			sleep( 30 );
			try
			{
				// disconnect, reconnect
				m_CurrentHelper->closeQueue();
				m_CurrentHelper->disconnect();
				m_CurrentHelper->connect( m_WatchQueueManager, m_TransportURI );
				m_CurrentHelper->openQueue( m_WatchQueue );
			}
			catch( ... )
			{
				continue;
			}
		}
		succeeded = true;

		string currentMessageId = "";
		unsigned long currentMessageLength = 0;

		try
		{
			AbstractWatcher::NotificationObject *notificationObject = NULL;
					
			// remove the first notification
			DEBUG( "EventsWatcher [" << m_SelfThreadId << "] waiting for notifications in pool" );
			WorkItem< AbstractWatcher::NotificationObject > notification = m_NotificationPool.removePoolItem();
			
			// get associated object
			notificationObject = notification.get();
			if( notificationObject == NULL )
				throw logic_error( "NULL notification received... ignoring. Please report this error !" );

			// get id
			currentMessageId = notificationObject->getObjectId();
			if ( currentMessageId.length() == 0 )
				throw runtime_error( "Notification object id is empty" );

			// get length
			currentMessageLength = notificationObject->getObjectSize();
			if ( currentMessageLength == 0 )
				throw runtime_error( "Notification object size is < 0" );
		}
		catch( const WorkPoolShutdown& shutdownError )
		{
			// while waiting, the pool may be shut down ( exit )
			TRACE_GLOBAL( shutdownError.what() );
			return;
		}
		catch( const std::exception& ex )
		{
			string exceptionType = typeid( ex ).name();
			string errorMessage = ex.what();
			
			TRACE_GLOBAL( exceptionType << " encountered while processing notification : " << errorMessage );
			succeeded = false;
			continue;
		}
		catch( ... )
		{
			TRACE_GLOBAL( "[unknown exception] encountered while processing notification. " );
			succeeded = false;
			continue;
		}
		
		DEBUG( "Notified : [" << currentMessageId << "] size [" << currentMessageLength << "]" );

		//TODO throw only on fatal error. The connector should respawn this thread
		try
		{
			DEBUG( "Performing message loop ... " );
			Process( currentMessageId, currentMessageLength );
			DEBUG( "Message loop finished ok. " );
		}
		catch( const AppException& ex )
		{
			string exceptionType = typeid( ex ).name();
			string errorMessage = ex.getMessage();
			
			TRACE_GLOBAL( exceptionType << " encountered while processing event : " << errorMessage );
			succeeded = false;
		}
		catch( const std::exception& ex )
		{
			string exceptionType = typeid( ex ).name();
			string errorMessage = ex.what();
			
			TRACE_GLOBAL( exceptionType << " encountered while processing event : " << errorMessage );
			succeeded = false;
		}
		catch( ... )
		{
			TRACE_GLOBAL( "[unknown exception] encountered while processing event. " );
			succeeded = false;
		}
	} //while true
}

void EventsWatcher::Stop()
{
	DEBUG( "Stop." );
	
	//unregister self	
	DEBUG( "Unregistering self" );
	AppException utilRegister( "Unregister", EventType::Management );
	utilRegister.addAdditionalInfo( "ServiceName", "EventsWatcher" ); 
	utilRegister.setPid( m_SessionId );
	UploadMessage( utilRegister );

	// allow hbmonitor to update the state
	//sleep( ( HBM_DELAY * 2 ) );
	
	ShouldStop = true;
	DEBUG( "Waiting for all monitors to stop ... " );
	//pthread_join( m_MonitorThreadId, NULL ); 
	
	DEBUG( "Waiting for all watchers to stop ... " );
	m_Watcher.setEnableRaisingEvents( false );

	m_CurrentHelper->closeQueue();
	m_CurrentHelper->disconnect();
	
	DEBUG( "Dead" );
}

void EventsWatcher::Process( const string& messageId, const unsigned long messageLength )
{
	MEM_CHECKPOINT_INIT();

	DEBUG( "Getting the message from WMQ [" << messageId << "] ..." );

	WorkItem< ManagedBuffer > managedBuffer( new ManagedBuffer() );
	ManagedBuffer* buffer = managedBuffer.get();
		
	try
	{
		//TODO check return code
		m_CurrentHelper->setMessageId( messageId );
	
		// get one message using syncpoint
		long result = m_CurrentHelper->getOne( buffer, true );
		
		switch( result )
		{
			// if no message found, return
			case -1 :
				return;

			// ok
			case 0 :
				break;

			default :
				TRACE( "Getting one message failed [" << result << "]" );
				break;
		}
			
		//ProcessMessage - parse message and insert into Status table
		//put the buffer in Db

		XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc = NULL;
		AppException recEx;
		bool uploaded = false;

		INCREMENT_COUNTER( TRN_TOTAL );

		try
		{	
			doc = XmlUtil::DeserializeFromString( buffer->str() );		
			if( doc == NULL )
				throw runtime_error( "Invalid [non-deserializable] event received" );
				
			// upload event
			recEx = AbstractLogPublisher::DeserializeFromXml( doc );
			
			if ( doc != NULL )
			{
				doc->release();
				doc = NULL;
			}
				
			uploaded = true;
			INCREMENT_COUNTER( TRN_COMMITED );
		}
		catch( const std::exception& ex )
		{
			//TODO Commit ( count backout )
			if ( doc != NULL )
			{
				doc->release();
				doc = NULL;
			}
				
			stringstream errorMessage;
			errorMessage << "Could not parse/deserialize original event. Check additional info or the log on the remote machine. [" << ex.what() << "]";
			
			TRACE( errorMessage.str() );
			
			recEx.setMessage( errorMessage.str() );
			recEx.setPid( m_SessionId );
			recEx.InitDefaultValues();
			recEx.addAdditionalInfo( "Original event", Base64::encode( buffer->str() ) );
			INCREMENT_COUNTER( TRN_ABORTED );
		}
		catch( ... )
		{					
			//TODO Commit ( count backout )
			if ( doc != NULL )
			{
				doc->release();
				doc = NULL;
			}
				
			stringstream errorMessage;
			errorMessage << "Could not parse/deserialize original event. Check additional info or the log on the remote machine.";
			
			TRACE( errorMessage.str() );
			
			recEx.setMessage( errorMessage.str() );
			recEx.InitDefaultValues();
			recEx.setPid( m_SessionId );
			recEx.addAdditionalInfo( "Original event", Base64::encode( buffer->str() ) );
			INCREMENT_COUNTER( TRN_ABORTED );
		}			
		
		DEBUG2( "Uploading ... " );
		//LogManager::Publish( recEx );
			
		UploadMessage( recEx );
		
		// upload event
		if ( uploaded )
		{
			DEBUG2( "Commiting event got." )
			m_CurrentHelper->commit();
			DEBUG2( "Commited" );
		}
	}
	catch( const AppException& ex )
	{
		TRACE( "Unable to publish event to DB [" << ex.getMessage() << "]" );
		throw;	
	}	
	catch( const std::exception& ex )
	{
		TRACE( "Unable to publish event to DB [" << ex.what() << "]" );
		throw;		
	}	
	catch( ... )
	{
		TRACE( "Unable to publish event to DB [unknown exception]" );
		throw;
	}

	MEM_CHECKPOINT_END( "event processing", "EventsWatcher" );
}

void EventsWatcher::UploadMessage( AppException& ex )
{
	// if Type == EventType::Management
	// then insert into ServicePerformance
	// else insert into Status
	
	DEBUG( "Uploading message" );
	
	NameValueCollection addInfo;
	if ( ex.getAdditionalInfo() != NULL ) 
		addInfo = *( ex.getAdditionalInfo() );

	// sessionId identifies a connector session ( start->stop cycle )
	string sessionId = ex.getPid();
	
	//validate passed innerException
	string innerException = ex.getException();

	long serviceId = getServiceId( sessionId );
	
	DEBUG2( "Checking event type ..." );	
	if( ex.getEventType().getType() == EventType::Management )
	{
		bool processed = false;
		string eventMessage = ex.getMessage();
		string serviceName = "qPUnk00";

		// process management info
		// process register info
		if ( eventMessage == "Register" )
		{
			// register requests must pass the servicename
			serviceName = addInfo[ "ServiceName" ];
			if ( serviceName.size() == 0 )
			{
				TRACE( "Missing required info for Register : [ServiceName]" );
				AppException regex( "Missing required info for Register : [ServiceName]" );
				regex.setPid( m_SessionId );
				
				TRACE( regex.getMessage() );
				UploadMessage( regex );
				return;
			}

			// update service state as running
			FALLBACK_UPDATE_STATE( sessionId, Service::RUNNING, serviceName );
			
			stringstream messageMgmt;
			messageMgmt << "Service [" << serviceName << "] registered.";
			ex.setMessage( messageMgmt.str() );

			DEBUG( messageMgmt.str() );

			// insert the event in the DB
			processed = true;

			try
			{
				if ( addInfo.ContainsKey( "Version" ) && ( addInfo.ContainsKey( "Name" ) ) )
				{
					EventsWatcherDbOp::UpdateServiceVersion( serviceName, addInfo[ "Name" ], addInfo[ "Version" ], ex.getMachineName(), addInfo[ "Hash" ] );
					DEBUG( "Service [" << serviceName << "] [" << addInfo[ "Name" ] << "] version [" << addInfo[ "Version" ] << "]" );
				}
				else
				{
					DEBUG( "Service [" << serviceName << "] doesn't send version info" );
				}
			}
			catch( const std::exception& innerex )
			{
				TRACE( "Failed to update service version [" << innerex.what() << "]" );
			}
			catch( ... )
			{
				TRACE( "Failed to update service version" );
			}

		} // endif Register

		ServiceMap::const_iterator serviceWalker = m_ConnectorState.find( sessionId );

		// if the session is not registered... consider discarding this message
		if ( serviceWalker == m_ConnectorState.end() )
		{
			TRACE( "Service not registered but sending events. Session [" << sessionId << "]" );
			
			AppException regex( ex );

			regex.setPid( m_SessionId );
			regex.setMessage( "Service not registered but sending events." );
			regex.setEventType( EventType::Warning );

			regex.addAdditionalInfo( "Original_machine", ex.getMachineName() );
			regex.addAdditionalInfo( "Original_event", ex.getMessage() );

			UploadMessage( regex );
			return;
		}

		serviceName = serviceWalker->second.getName();
		serviceId = serviceWalker->second.getServiceId();
		
		if ( eventMessage == "Performance" )
		{
			DEBUG( serviceName << " performance report : " << Base64::decode( addInfo[ "Report" ] ) );
			FALLBACK_UPDATE_STATE( sessionId, Service::RUNNING, "" );
			
			return;
		}
		if ( eventMessage == "Tracking" )
		{
			DEBUG( serviceName << " tracking report : " << Base64::decode( addInfo[ "Report" ] ) );
			return;
		}
		if ( eventMessage == "Unregister" )
		{
			stringstream messageMgmt;
			messageMgmt << "Service [" << serviceName << "] unregistered.";
			
			FALLBACK_UPDATE_STATE( sessionId, Service::STOPPED, "" );
			
			ex.setMessage( messageMgmt.str() );
			DEBUG( messageMgmt.str() );
			processed = true;
		}
			
		if ( !processed )
		{
			stringstream messageMgmt;
			messageMgmt << "Unknown management message : [" << ex.getMessage() << "]";
			ex.setMessage( messageMgmt.str() );

			DEBUG( messageMgmt.str() );
		}
	}
	
	DEBUG( "About to insert message" );
	// insert into status 
	EventsWatcherDbOp::InsertEvent( serviceId, ex.getCorrelationId(), sessionId,
		ex.getEventType().ToString(), ex.getMachineName(), ex.getCreatedDateTime(), 
		ex.getMessage(), ex.getClassType().ToString(), ex.getAdditionalInfo()->ToString(), innerException );
		
	DEBUG( "Upload message complete." );
}

void* EventsWatcher::HeartbeatMonitor( void* data )
{
	while( !EventsWatcher::ShouldStop )
	{
		DEBUG_GLOBAL( "Requesting synchnonized access to statemap (HB)" );
		int mutexLockResult = pthread_mutex_lock( &( EventsWatcher::StatusSyncMutex ) );
		if ( 0 != mutexLockResult )
		{
			TRACE( "Unable to lock StatusSyncMutex mutex [" << mutexLockResult << "]" );
		}
		
		DEBUG_GLOBAL( "Synchnonized access aquired (HB)" );
		
		try
		{
			// loop through services to check elapsed time
			ServiceMap::iterator serviceMap = EventsWatcher::getConnectorStateMap()->begin();
			
			while( serviceMap != EventsWatcher::getConnectorStateMap()->end() )
			{
				// if a hearbeat is required and current state is not stopped and 
				// hb interval elapsed, consider it dead
				if( ( serviceMap->second.getHeartbeatInterval() > 0 ) &&
					( serviceMap->second.getState() != Service::STOPPED ) && 
					( serviceMap->second.getTimeSinceLastHeartbeat() > serviceMap->second.getHeartbeatInterval() ) )
				{
					string serviceName = serviceMap->second.getName();
					serviceMap->second.setState( Service::STOPPED );

					stringstream errorMessage;
					errorMessage << "Service [" << serviceName << "] failed to send performance report ( assumed dead ).";

					TRACE( errorMessage.str() );
					//AppException stopError( errorMessage.str() );
					//UploadMessage( stopError );
				}
				/*else
				{
					string serviceName = serviceMap->second.getName();
					DEBUG( "Service [" << serviceName << "] last seen " << serviceMap->second.getTimeSinceLastHeartbeat() << 
						" seconds ago. Heartbeat interval is [" << serviceMap->second.getHeartbeatInterval() << "]" );
				}*/
				serviceMap++;
			}
		}
		catch( ... )
		{
			TRACE_GLOBAL( "!!!!!!!!!!!!!!!!!!!!!! Error in HBM" );
		}

		int mutexUnlockResult = pthread_mutex_unlock( &( EventsWatcher::StatusSyncMutex ) );		
		if ( 0 != mutexUnlockResult )
		{
			TRACE_NOLOG( "Unable to unlock StatusSyncMutex mutex [" << mutexUnlockResult << "]" );
		}
		sleep( HBM_DELAY );
	}
	return NULL;
}

void EventsWatcher::UpdateServiceState( const string& sessionId, const Service::SERVICE_STATE newState, const string& serviceName )
{
	// sync update 
	DEBUG( "Requesting synchnonized access to statemap" );
	int mutexLockResult = pthread_mutex_lock( &( EventsWatcher::StatusSyncMutex ) );
	if ( 0 != mutexLockResult )
	{
		TRACE( "Unable to lock StatusSyncMutex mutex [" << mutexLockResult << "]" );
	}
	DEBUG( "Synchnonized access aquired" );
	
	Service crtService;
	bool sessionIdChanged = false;

	// if we have a service name, change servicemap ( reset session )
	if ( serviceName.length() > 0 )
	{
		ServiceMap::iterator serviceWalker = EventsWatcher::getConnectorStateMap()->begin();
		bool serviceFound = false;
		
		while( serviceWalker != EventsWatcher::getConnectorStateMap()->end() )
		{
			// find out service ( by name )
			if ( serviceName == serviceWalker->second.getName() )
			{
				crtService = serviceWalker->second;

				// same service, same session
				if ( sessionId == serviceWalker->first )
				{
					DEBUG( "Service [" << serviceName << "] has the same sessionId [" << sessionId << "]" );
					serviceFound = true;
					break;
				}
				else
				{
					// sessionId changed... reinsert with the new sessionid
					EventsWatcher::getConnectorStateMap()->erase( serviceWalker );

					crtService.setSessionId( sessionId );
					EventsWatcher::getConnectorStateMap()->insert( pair< string, Service >( sessionId, crtService ) );

					DEBUG( "Service [" << serviceName << "] changed sessionId to [" << sessionId << "]" );
					serviceFound = true;
					sessionIdChanged = true;
					break;
				}
			}
			serviceWalker++;
		}
		if ( !serviceFound )
		{
			stringstream errorMessage;
			errorMessage << "Service with name [" << serviceName << "] was not found in the database";
			TRACE( errorMessage.str() );

			int mutexUnlockResult = pthread_mutex_unlock( &( EventsWatcher::StatusSyncMutex ) );
			if ( 0 != mutexUnlockResult )
			{
				TRACE_NOLOG( "Unable to unlock StatusSyncMutex mutex [" << mutexUnlockResult << "]" );
			}
			throw runtime_error( errorMessage.str() );
		}
	}
	else
	{
		ServiceMap::iterator serviceWalker = EventsWatcher::getConnectorStateMap()->find( sessionId );
		if( serviceWalker == EventsWatcher::getConnectorStateMap()->end() )
		{
			stringstream errorMessage;
			errorMessage << "SessionId [" << sessionId << "] doesn't match a registered service";
			TRACE( errorMessage.str() );

			int mutexUnlockResult = pthread_mutex_unlock( &( EventsWatcher::StatusSyncMutex ) );
			if ( 0 != mutexUnlockResult )
			{
				TRACE_NOLOG( "Unable to unlock StatusSyncMutex mutex [" << mutexUnlockResult << "]" );
			}
			throw runtime_error( errorMessage.str() );
		}
		crtService = serviceWalker->second;
	}

	try
	{		
		Service::SERVICE_STATE crtState = crtService.getState();
		
		if ( newState == Service::RUNNING )
		{
			time_t now;
			time( &now );
			crtService.setLastHeartbeat( now );
		}

		// if state is unchanged update heartbeat time
		if ( ( crtState != newState ) || sessionIdChanged )
		{
			DEBUG2( "Updating service state in DB" );

			// persist changes to db
			crtService.setState( newState );
			string crtServiceName = crtService.getName();

			DEBUG2( "Service [" << crtServiceName << "] changed status/session." );
			crtService.Report();
		}
	}
	catch( const std::exception& ex )
	{
		stringstream errorMessage;
		errorMessage << "Unable to update service state for service [" << serviceName << "] [" << ex.what() << "]";
		TRACE( errorMessage.str() );

		int mutexUnlockResult = pthread_mutex_unlock( &( EventsWatcher::StatusSyncMutex ) );
		if ( 0 != mutexUnlockResult )
		{
			TRACE_NOLOG( "Unable to unlock StatusSyncMutex mutex [" << mutexUnlockResult << "]" );
		}
		throw runtime_error( errorMessage.str() );
	}
	catch( ... )
	{
		stringstream errorMessage;
		errorMessage << "Unable to update service state for service [" << serviceName << "] [unknown error]";
		TRACE( errorMessage.str() );

		int mutexUnlockResult = pthread_mutex_unlock( &( EventsWatcher::StatusSyncMutex ) );
		if ( 0 != mutexUnlockResult )
		{
			TRACE_NOLOG( "Unable to unlock StatusSyncMutex mutex [" << mutexUnlockResult << "]" );
		}
		throw runtime_error( errorMessage.str() );
	}
	int mutexUnlockResult = pthread_mutex_unlock( &( EventsWatcher::StatusSyncMutex ) );
	if ( 0 != mutexUnlockResult )
	{
		TRACE_NOLOG( "Unable to unlock StatusSyncMutex mutex [" << mutexUnlockResult << "]" );
	}
}

long EventsWatcher::getServiceId( const string& sessionId ) const
{
	map< string, Service >::const_iterator idFinder = m_ConnectorState.find( sessionId );
	if( idFinder != m_ConnectorState.end() )
		return idFinder->second.getServiceId();

	return -1;
}
