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

#include <xercesc/util/PlatformUtils.hpp>

#include "SyslogPublisher.h"
#include "XmlUtil.h"
#include "StringUtil.h"
#include "Trace.h"
#include "LogManager.h"
#include "LogPublisher.h"
#include "Collaboration.h"

#ifdef WIN32
	#define __MSXML_LIBRARY_DEFINED__
	#include <windows.h>
	#define sleep(x) Sleep( (x)*1000 )
#else
	#include <unistd.h>
#endif

using namespace std;
using namespace FinTP;

static bool m_Threaded = false;
LogManager LogManager::Instance;
LogManager::EventsPool LogManager::m_EventsPool;
pthread_mutex_t LogManager::InstanceMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_once_t LogManager::KeysCreate = PTHREAD_ONCE_INIT;
pthread_key_t LogManager::CorrelationKey;

LogManager::LogManager() : m_Threaded( false ), m_PublishThreadId( 0 )
{
//	DEBUG( "CONSTRUCTOR" );
	m_Initialized = false;
	
	// set defaultPublisher
	// I don't like this ( while creating Instance, the ctor is called on a not-yet-created-object )
	//Instance.setDefaultPublisher( new ScreenLogPublisher() );
	//m_CorrelationId = Collaboration::GenerateGuid();
	
	m_DefaultPublisher = new ScreenLogPublisher();

	int onceResult = pthread_once( &LogManager::KeysCreate, &LogManager::CreateKeys );
	if ( 0 != onceResult )
	{
		TRACE_NOLOG( "Unable to create LogManager keys [" << onceResult << "]" );
	}

    m_Initialized = true;
}

void LogManager::CreateKeys()
{
	cout << "Thread [" << pthread_self() << "] creating correlation keys..." << endl;
	
	int keyCreateResult = pthread_key_create( &LogManager::CorrelationKey, &LogManager::DeleteCorrelIds );
	if ( 0 != keyCreateResult )
	{
		TRACE_NOLOG( "Unable to create thread key LogManager::CorrelationKey [" << keyCreateResult << "]" );
	}
}

void LogManager::DeleteCorrelIds( void* data )
{
	string* threadCorrelId = ( string* )data;
	if ( threadCorrelId != NULL )
		delete threadCorrelId;
	
	int setSpecificResult = pthread_setspecific( LogManager::CorrelationKey, NULL );
	if ( 0 != setSpecificResult )
	{
		TRACE_NOLOG( "Unable to reset thread specific string LogManager::CorrelationKey [" << setSpecificResult << "]" );
	}
}

void LogManager::ClearPublishers()
{
	int lockResult = pthread_mutex_lock( &( LogManager::InstanceMutex ) );
	if ( 0 != lockResult )
	{
		TRACE_NOLOG( "Unable to lock mutex LogManager::InstanceMutex [" << lockResult << "]" );
	}
	
	try
	{
		vector< AbstractLogPublisher* >::iterator finder;
		
		for( finder = m_Publishers.begin(); finder != m_Publishers.end(); finder++ )
		{
			if ( *finder != NULL )
			{
				delete *finder;
				*finder = NULL;
			}
		}
		
		DEBUG2( "All publishers deleted" );
	
		while( m_Publishers.size() > 0 )
			m_Publishers.pop_back();
	}
	catch( ... )
	{
		int unlockResult = pthread_mutex_unlock( &( LogManager::InstanceMutex ) );
		if ( 0 != unlockResult )
		{
			TRACE_NOLOG( "Unable to unlock mutex LogManager::InstanceMutex [" << unlockResult << "]" );
		}
	}
	int unlockResult = pthread_mutex_unlock( &( LogManager::InstanceMutex ) );
	if ( 0 != unlockResult )
	{
		TRACE_NOLOG( "Unable to unlock mutex LogManager::InstanceMutex [" << unlockResult << "]" );
	}

	DEBUG2( "All publishers erased" );
}

void LogManager::Terminate()
{
	if ( !Instance.m_Initialized )
		return;

	// clean threads
	Instance.setThreaded( false );
	Instance.ClearPublishers();

	if ( Instance.m_DefaultPublisher != NULL )
	{
		delete Instance.m_DefaultPublisher;
		Instance.m_DefaultPublisher = NULL;
	}
	
	Instance.m_Initialized = false;

	// kill output to files .. 2 reasons :
	// 1) don't open and rewrite files with termination lines
	// 2) don't use objects destroyed at thread termination ( like streams )
	FileOutputter::setTerminated( true );
}

LogManager::~LogManager()
{
	try
	{
		if ( m_Initialized ) 
			Terminate();
	}
	catch( ... )
	{
		TRACE_NOLOG( "Error terminating LogManager" );
	}
}

void LogManager::setDefaultPublisher( AbstractLogPublisher* defaultPublisher )
{
	// replace old publisher
	if ( m_DefaultPublisher != NULL )
	{
		delete m_DefaultPublisher;
		m_DefaultPublisher = NULL;
	}
		
	m_DefaultPublisher = defaultPublisher;
	m_DefaultPublisher->setDefault( true );
}

void LogManager::setThreaded( const bool threaded )
{
	int lockResult = pthread_mutex_lock( &( LogManager::InstanceMutex ) );
	if ( 0 != lockResult )
	{
		TRACE_NOLOG( "Unable to lock mutex LogManager::InstanceMutex [" << lockResult << "]" );
	}

	try
	{
		if ( m_Threaded != threaded )
		{
			m_Threaded = threaded;

			// if it wasn't threaded, but now is 
			if ( m_Threaded )
			{
				DEBUG( "Starting threaded LogPublisher... " );
				// start a publisher thread
				pthread_attr_t publishThreadAttr;

				int attrInitResult = pthread_attr_init( &publishThreadAttr );
				if ( 0 != attrInitResult )
				{
					TRACE( "Error initializing publish thread attribute [" << attrInitResult << "]" );
					throw runtime_error( "Error initializing publish thread attribute" );
				}

				int setDetachResult = pthread_attr_setdetachstate( &publishThreadAttr, PTHREAD_CREATE_JOINABLE );
				if ( 0 != setDetachResult )
				{
					TRACE( "Error setting joinable option to publish thread attribute [" << setDetachResult << "]" );
					throw runtime_error( "Error setting joinable option to publish thread attribute" );
				}				
				
				// if running in debugger, reattempt to create thread on failed request
				int threadStatus = 0;
				do
				{
					threadStatus = pthread_create( &m_PublishThreadId, &publishThreadAttr, LogManager::PublisherThread, this );    
				} while( ( threadStatus != 0 ) && ( errno == EINTR ) );
				if ( threadStatus != 0 )
				{
					int errCode = errno;
					int attrDestroyResult = pthread_attr_destroy( &publishThreadAttr );
					if ( 0 != attrDestroyResult )
					{
						TRACE( "Unable to destroy publish thread attribute [" << attrDestroyResult << "]" );
					}	
					
					stringstream errorMessage;
#ifdef CRT_SECURE
					char errBuffer[ 95 ];
					strerror_s( errBuffer, sizeof( errBuffer ), errCode );
					errorMessage << "Unable to create publisher thread. [" << errBuffer << "]";
#else
					errorMessage << "Unable to create publisher thread. [" << strerror( errCode ) << "]";
#endif	
					throw runtime_error( errorMessage.str() );
				}
				
				int attrDestroyResult2 = pthread_attr_destroy( &publishThreadAttr );
				if ( 0 != attrDestroyResult2 )
				{
					TRACE( "Unable to destroy publish thread attribute [" << attrDestroyResult2 << "]" );
				}
			}
			// if it was threaded, but now isn't
			else
			{
				DEBUG( "Stopping threaded LogPublisher... " );
				
				// timeout because the pool uses debug, and debug uses pool...
				m_EventsPool.waitForPoolEmpty( 30 );
				Instance.m_Initialized = false;
			
				DEBUG( "Pool empty or timed out... " );
				
				m_EventsPool.ShutdownPool();
				
				int joinResult = pthread_join( m_PublishThreadId, NULL );
				if ( 0 != joinResult )
				{
					TRACE( "An error occured when joining the publish thread [" << joinResult << "]" );
				}
				DEBUG( "Threaded LogPublisher was stopped." );
			}
		}
	}
	catch( ... )
	{
		int unlockResult = pthread_mutex_unlock( &( LogManager::InstanceMutex ) );
		if ( 0 != unlockResult )
		{
			TRACE_NOLOG( "Unable to unlock mutex LogManager::InstanceMutex [" << unlockResult << "]" );
		}
	}
	int unlockResult = pthread_mutex_unlock( &( LogManager::InstanceMutex ) );
	if ( 0 != unlockResult )
	{
		TRACE_NOLOG( "Unable to unlock mutex LogManager::InstanceMutex [" << unlockResult << "]" );
	}
}

void LogManager::setSessionId( const string& value )
{
	Instance.m_SessionId = value; 
}

void LogManager::setCorrelationId( const string& value ) 
{
	string *correlationId = ( string * )pthread_getspecific( LogManager::CorrelationKey );
	if( correlationId == NULL ) 
	{
		correlationId = new string( 26, 0 );
		int setSpecificResult = pthread_setspecific( LogManager::CorrelationKey, correlationId );
		if ( 0 != setSpecificResult )
		{
			TRACE_NOLOG( "Unable to set thread specific string LogManager::CorrelationKey [" << setSpecificResult << "]" );
		}
	}
	correlationId->assign( value );
}

void LogManager::Initialize( const NameValueCollection& propSettings, bool threaded )
{
	DEBUG( "LogManager init [" << threaded << "]" );
	Instance.setThreaded( threaded );
	Instance.ClearPublishers();

	if ( propSettings.getCount() > 0 )
	{
		for( unsigned int i=0; i<propSettings.getCount(); i++ )
		{
			AbstractLogPublisher* logPublisher = NULL;
			
			try
			{
				if ( propSettings[ i ].first == "Log.PublisherToFile" )
				{
					DEBUG_GLOBAL( "File publisher" );
					// check required info
					if ( !propSettings.ContainsKey( "Log.PublisherToFile.Path" ) )
					{
						DEBUG_GLOBAL( "No file path" );
						AppException ex( "Missing Log.PublisherToFile.Path for file publisher in config file." );
						Instance.InternalPublish( ex );
						continue;
					}
					logPublisher = new FileLogPublisher( propSettings[ "Log.PublisherToFile.Path" ] );
					if ( propSettings.ContainsKey( "Log.PublisherToFile.EventFilter" ) )
					{
						int eventFilter = StringUtil::ParseInt( propSettings[ "Log.PublisherToFile.EventFilter" ] );
						DEBUG_GLOBAL( "Setting file publisher event filter to [" << eventFilter << "]" );
						logPublisher->setEventFilter( eventFilter );
					}
					else
					{
						DEBUG_GLOBAL( "Setting file publisher event filter to default [" << logPublisher->eventFilter() << "]" );
					}
					
					if ( propSettings[ i ].second == "true" )
					{
						DEBUG_GLOBAL( "Added file publisher [" << propSettings[ "Log.PublisherToFile.Path" ] << "]" );
						Instance.m_Publishers.push_back( logPublisher );
					}
					else if ( propSettings[ i ].second == "default" )
					{
						Instance.setDefaultPublisher( logPublisher );
					}
					else
					{
						// not activated and not default... why is it even there ?
						if ( logPublisher != NULL )
						{
							delete logPublisher;
							logPublisher = NULL;
						}
					}

					continue;
				} // endif Log.PublisherToFile

				if ( propSettings[i].first == SyslogPublisher::CONFIG_NAME )
				{
					if ( propSettings.ContainsKey( SyslogPublisher::CONFIG_HOST ) )
					{
						if ( propSettings.ContainsKey( SyslogPublisher::CONFIG_PORT ) )
							logPublisher = new SyslogPublisher( propSettings[SyslogPublisher::CONFIG_HOST], propSettings[SyslogPublisher::CONFIG_PORT] );
						else
						{
							TRACE( "Missing " << SyslogPublisher::CONFIG_PORT << " in config file. Assuming UDP port 514." )
							logPublisher = new SyslogPublisher( propSettings[SyslogPublisher::CONFIG_HOST] );
						}
					}
					else
					{
						AppException ex( "Missing " +  SyslogPublisher::CONFIG_HOST + " for Syslog publisher in config file." );
						Instance.InternalPublish( ex );
						continue;
					}
					if ( propSettings[i].second == "true" )
					{
						DEBUG_GLOBAL( "Added Syslog publisher." );
						Instance.m_Publishers.push_back( logPublisher );
					}
					else
						if ( propSettings[i].second == "default" )
						{
							DEBUG_GLOBAL( "Added Syslog publisher as default log publisher." )
							Instance.setDefaultPublisher( logPublisher );
						}
						else
						{
							// not activated and not default... why is it even there ?
							delete logPublisher;
							logPublisher = NULL;
						}
					continue;
				}
			}
			catch( const AppException& e )
			{
				if ( logPublisher != NULL )
				{
					delete logPublisher;
					logPublisher = NULL;
				}
					
				TRACE_GLOBAL( "Unable to create publisher : " << typeid( logPublisher ).name() << ". Error is  :" << e.getMessage() );
				Instance.InternalPublish( e );
			}
			catch( ... )
			{
				if ( logPublisher != NULL )
				{
					delete logPublisher;
					logPublisher = NULL;
				}
					
				AppException ex( "An unhandled exception has occured while loading config settings." );

				TRACE_GLOBAL( ex.getMessage() );
				Instance.InternalPublish( ex );
			}
		}	
	}	
	DEBUG_GLOBAL( "LogManager Initialize End" );
}

void* LogManager::PublisherThread( void *param )
{
	while( Instance.m_Initialized )
	{
		DEBUG_GLOBAL( "EventsPublisher pool watcher [" << pthread_self() << "] waiting for events in pool" );
		
		try
		{
			// removing from pool will give this thread ownership of the thread
			WorkItem< AppException > eventItem = m_EventsPool.removePoolItem();
			AppException *newEx = eventItem.get(); 

			if ( newEx == NULL )
				throw logic_error( "NULL event removed from pool. IGNORED" );
				
			Instance.InternalPublishProc( *newEx );
		}
		catch( const WorkPoolShutdown& shutdownError )
		{
			TRACE_GLOBAL( "Shutting down... closing [" << shutdownError.what() << "]" );
			DEBUG( "Exiting with [" << m_EventsPool.getSize() << "] items in pool" );

			break;
		}
		// don't allow exceptions to surface
		catch( const std::exception& ex )
		{
			stringstream errorMessage;
			errorMessage << typeid( ex ).name() << " exception encountered by events publisher thread [" << ex.what() << "]";
			TRACE( errorMessage.str() );
			
			sleep( 10 );
		}
		catch( ... )
		{
			stringstream errorMessage;
			errorMessage << "Exception encountered by events publisher thread [unhandled exception]";
			TRACE( errorMessage.str() );
			
			sleep( 10 );
		}
	}
	DEBUG( "Exiting with [" << m_EventsPool.getSize() << "] items in pool" );
	
	pthread_exit( NULL );
	return NULL;
}

void LogManager::InternalPublish( const AppException& except ) throw()
{
	string *correlationId = ( string * )pthread_getspecific( LogManager::CorrelationKey );
			
	if ( m_Threaded )
	{
		try
		{
			AppException *newEx = new AppException( except );
			
			if ( ( newEx->getCorrelationId() == Collaboration::EmptyGuid() ) && ( correlationId != NULL ) && ( correlationId->length() > 0 ) ) 
				newEx->setCorrelationId( *correlationId );
			
			WorkItem< AppException > eventItem( newEx );
			m_EventsPool.addPoolItem( except.getCorrelationId(), eventItem );
		} catch( ... ){}
	}
	else // not threaded
	{
		try
		{
			if ( ( except.getCorrelationId() == Collaboration::EmptyGuid() ) && ( correlationId != NULL ) && ( correlationId->length() > 0 ) ) 
				const_cast< AppException& >( except ).setCorrelationId( *correlationId );
		} catch( ... ){}
		try
		{
			InternalPublishProc( except );
		} catch( ... ){}
	}
}

void LogManager::InternalPublishProc( const AppException& except )
{
	const_cast< AppException& >( except ).setPid( m_SessionId );
	vector< AbstractLogPublisher* >::const_iterator iter = m_Publishers.begin();

	bool atLeastOne = false;

	for( iter = m_Publishers.begin(); iter != m_Publishers.end(); iter++ )
	{
		try
		{
			// publish if the event type is matched by the filter
			if( (( *iter )->eventFilter() & except.getEventType().getType()) != 0 )
			{
				( *iter )->Publish( except );
				atLeastOne = true;
			}
		}
		catch( AppException& ex )
		{
			stringstream errorMessage;
			errorMessage << "LogManager was unable to publish to custom publisher [" <<
				typeid( *iter ).name() << "]. The error was : " << ex.getMessage();
				
			TRACE( errorMessage.str() );
			TRACE( except );
			ex.setMessage( errorMessage.str() );			
			m_DefaultPublisher->Publish( ex );
		}
		catch( const std::exception& ex )
		{
			stringstream errorMessage;
			errorMessage << "LogManager was unable to publish to custom publisher [" <<
				typeid( *iter ).name() << "]. The error was : " << ex.what();
				
			TRACE( errorMessage.str() );
			TRACE( except );
			m_DefaultPublisher->Publish( AppException( errorMessage.str() ) );
		}
		catch( ... )
		{
			stringstream errorMessage;
			errorMessage << "LogManager was unable to publish to custom publisher [" <<
				typeid( *iter ).name() << "]. The error is unknown";
				
			TRACE( errorMessage.str() );
			TRACE( except );
			m_DefaultPublisher->Publish( AppException( errorMessage.str() ) );
		}
	}
	if( !atLeastOne )
	{
		try
		{
			m_DefaultPublisher->Publish( except );
		}
		catch( ... )
		{
			TRACE( "Failed to publish to default publisher" );
		}
	}
}

void LogManager::Publish( const AppException& except )
{
	//DEBUG_GLOBAL( "Publish ( AppException& )" );
	Instance.InternalPublish( except );
}

void LogManager::Publish( const string& message, const EventType::EventTypeEnum eventType )
{
	AppException ex( message, eventType );
	Instance.InternalPublish( ex );
}

void LogManager::Publish( const char* message, const EventType::EventTypeEnum eventType )
{
//	DEBUG_GLOBAL( "Publish ( char* ... )" );
	AppException ex( message, eventType );	
	Instance.InternalPublish( ex );
}

void LogManager::Publish( const std::exception& innerException, const EventType::EventTypeEnum eventType, const string& message )
{	
	//DEBUG_GLOBAL( "Publish ( exception& ... )" )
	
	if ( string( "AppException" ).compare( typeid( innerException ).name() ) != 0 )
	{
		if( message.size() == 0 )
		{
			AppException ex( innerException.what(), innerException, eventType );
			Instance.InternalPublish( ex );
		}
		else
		{
			AppException ex( message, innerException, eventType );
			Instance.InternalPublish( ex );
		}
	}
}
bool LogManager::isInitialized() 
{ 
	return Instance.m_Initialized; 
}
pthread_t LogManager::getPublisherThreadId() 
{ 
	return Instance.m_PublishThreadId; 
}


void LogManager::Register( AbstractLogPublisher* logPublisher, bool isDefault )
{
	if ( logPublisher )
	{
		if ( !isDefault )
			Instance.m_Publishers.push_back( logPublisher );
		else
			Instance.setDefaultPublisher( logPublisher );
	}
}
