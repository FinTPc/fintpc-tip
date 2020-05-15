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

#include <string>
#include <sstream>
//#include <iostream>

#include "WorkItemPool.h"
#include "AbstractWatcher.h"
#include "RESTPublisher.h"
#include "MQ/MqFilter.h"

#include "Trace.h"
#include "AppExceptions.h"
#include "Base64.h"
#include "StringUtil.h"

#include "Swift/SwiftFormatFilter.h"
//#include "XmlUtil.h"
//#include "XPathHelper.h"
//#include "SSL/SSLFilter.h"
//#include "CommonExceptions.h"

#ifdef WIN32
#define __MSXML_LIBRARY_DEFINED__
#include "windows.h"
#define sleep( x ) Sleep( ( x )*1000 )
#else
#include <unistd.h>
#endif

using namespace std;

//constructor
RESTPublisher::RESTPublisher() : Endpoint(), m_Watcher( &m_NotificationPool ), m_ClientCertificate( "" ), m_ClientCertificatePassword( "" ), 
	m_SSLCertificateFile( "" ), m_SSLCertificatePhrase( "" ), m_CurrentHelper(), m_Metadata(), m_BackupLoader( &m_BackupPool ), m_BackupURI( "" )
{	

	DEBUG2( "CONSTRUCTOR" );	
}

//destructor
RESTPublisher::~RESTPublisher()
{
	DEBUG2( "DESTRUCTOR" );

	//TODO: what else to destroy
}

void RESTPublisher::Init()
{
	DEBUG( "INIT" );
	INIT_COUNTERS( &m_Watcher, RESTPublisherWatcher );

	m_RESTChannel = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::RESTURI, "" );
	string clientAppId = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::RESTCLIENTID, "" );
	m_CurrentHelper.setClientId( clientAppId );
	
	string certificateFile = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::CERTIFICATEFILENAME, "" );
	if( !certificateFile.empty() )
		m_ClientCertificate = StringUtil::DeserializeFromFile( certificateFile );
	m_ClientCertificatePassword = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::CERTIFICATEPASSWD, "" );
	m_SSLCertificateFile = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::SSLCERTIFICATEFILENAME, "" );
	m_SSLCertificatePhrase = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::SSLCERTIFICATEPASSWD, "" );
	
	//TODO:JSON Notification sometimes
	string notificationType = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::NOTIFTYPE, "XML" );
	m_Metadata.setFormat( notificationType );

	m_TransformFile = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::FILEXSLT, "" );
	
	// expect the first filter to be of WMQ type
	AbstractFilter* firstFilter = ( *m_FilterChain )[ 0 ];
	if ( ( firstFilter == NULL ) || ( firstFilter->getFilterType() != FilterType::MQ ) )
		throw logic_error( "First filter in a publisher's chain should be of MQ type. Please check the config file." );

	MqFilter* getterFilter = dynamic_cast< MqFilter* >( firstFilter );
	if ( getterFilter == NULL )
		throw logic_error( "First filter in a publisher's chain should be of MQ type. Please check the config file." );
	
	string watchQueueManager = getterFilter->getQueueManagerName();
	string watchTransportURI = getterFilter->getTransportURI();
	m_Watcher.setHelperType( getterFilter->getHelperType() );
	m_WatchQueue = getterFilter->getQueueName();

	DEBUG( "Watch queue [" << m_WatchQueue << "] on queue manager [" << watchQueueManager << "] using channel definition [" << watchTransportURI << "]" );

	m_Watcher.setQueue( m_WatchQueue );
	m_Watcher.setQueueManager( watchQueueManager );
	m_Watcher.setTransportURI( watchTransportURI );

	m_RespliesHelper = TransportHelper::CreateHelper( TransportHelper::parseTransportType( "AMQ" ) );
	m_RepliesQueue = getGlobalSetting(EndpointConfig::WMQToApp, EndpointConfig::RPLYQUEUE, "");
	m_RepliesQueueManager = getGlobalSetting(EndpointConfig::WMQToApp, EndpointConfig::WMQQMGR, "");
	m_RepliesTransportURI = getGlobalSetting(EndpointConfig::WMQToApp, EndpointConfig::MQURI, "");

	m_BackupURI = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::RESTBACKUPURI, "" );
	if( !m_BackupURI.empty() )
	{
		m_BackupLoader.setSFTPChannel( m_BackupURI );
		m_BackupLoader.setNotificationPool( &m_BackupPool );
	}
}

void RESTPublisher::internalStart()
{
	DEBUG( "Starting watcher... " );
	m_Watcher.setEnableRaisingEvents( true );

	DEBUG( "Starting backup loader..." );
	m_BackupLoader.setEnableRaisingEvents( true );

	TRACE_SERVICE( "[" << m_ServiceThreadId << "] starting to process... " );

	try
	{
		while( m_Running )
		{
			DEBUG( "Publisher [" << m_SelfThreadId << "] waiting for notifications in pool" );
			WorkItem< AbstractWatcher::NotificationObject > notification = m_NotificationPool.removePoolItem();
			
			AbstractWatcher::NotificationObject *notificationObject = notification.get();
			
			m_Metadata.setId( notificationObject->getObjectId() );
			m_Metadata.setLength( notificationObject->getObjectSize() );
			
			DEBUG( "Notified : [" << m_Metadata.id() << "] size [" << m_Metadata.length() << "]" );
			
			m_LastOpSucceeded = true;
			//TODO throw only on fatal error. The connector should respawn this thread
			try
			{
				DEBUG( "Performing message loop ... " );
				m_LastOpSucceeded = PerformMessageLoop( false );
				DEBUG( "Message loop finished ok. " );
			}
			catch( const AppException& ex )
			{
				string exceptionType = typeid( ex ).name();
				string errorMessage = ex.getMessage();
				
				TRACE_GLOBAL( exceptionType << " encountered while processing message : " << errorMessage );
				m_LastOpSucceeded = false;
			}
			catch( const std::exception& ex )
			{
				string exceptionType = typeid( ex ).name();
				string errorMessage = ex.what();
				
				TRACE_GLOBAL( exceptionType << " encountered while processing message : " << errorMessage );
				m_LastOpSucceeded = false;
			}
			catch( ... )
			{
				TRACE_GLOBAL( "[unknown exception] encountered while processing message. " );
				m_LastOpSucceeded = false;
			}

			if ( !m_LastOpSucceeded && m_Running )
			{
				TRACE( "Sleeping 10 seconds before next attempt( previous message failed )" );
				sleep( 2 );
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

void RESTPublisher::internalStop()
{
	DEBUG( "STOP" );
	
	// ensure watcher an backup-loader are dead ( the watcher will lock on pool and wait until it is empty )
	m_Watcher.setEnableRaisingEvents( false );
	m_BackupLoader.setEnableRaisingEvents( false );

	// wait for fetcher to remove all items if the thread is running
	//if ( 0 == pthread_kill( m_SelfThreadId, 0 ) )
	//{
		//m_NotificationPool.waitForPoolEmpty();
	//}
	// the endpoint must now be locked on pool

	// the watcher must be dead by now...
	m_Running = false;

	m_NotificationPool.ShutdownPool();
	m_BackupPool.ShutdownPool();
	
	TRACE( m_ServiceThreadId << " joining endpoint ..." );
	int joinResult = pthread_join( m_SelfThreadId, NULL );
	if ( 0 != joinResult )
	{
		TRACE( "Joining self thread ended in error [" << joinResult << "]" );
	}

	m_RespliesHelper->disconnect();
	m_CurrentHelper.disconnect();
}

string RESTPublisher::Prepare()
{
	DEBUG( "PREPARE" );
	
	//TODO: Connect to DESTINATION
	try
	{
		m_CurrentHelper.connect( m_RESTChannel, m_SSLCertificateFile, m_SSLCertificatePhrase );
	}
	catch( runtime_error& er )
	{
		AppException aex( "Can't connect to remote server, to publish messages.", EventType::Error );
		aex.setSeverity( EventSeverity::Fatal );
		throw aex;
	}

	try
	{
		m_RespliesHelper->connect( m_RepliesQueueManager, m_RepliesTransportURI, true );
	}
	catch ( runtime_error& er )
	{
		AppException aex( "Can't connect to local Mq manager, to deliver replies messages ", EventType::Error );
		aex.setSeverity( EventSeverity::Fatal );
		throw aex;
	}
	m_RespliesHelper->openQueue( m_RepliesQueue );

	return m_Metadata.id();
}

void RESTPublisher::Process( const string& correlationId )
{
	DEBUG( "PROCESS" );	

	DEBUG( "Current message length : " << m_Metadata.length() );

	string payload = "", messageOutput = "";	
	FinTPMessage::Message message( m_Metadata ); 

	try
	{
		m_TransportHeaders.Clear();
		m_TransportHeaders.Add( MqFilter::MQMSGID, m_Metadata.id() );
		m_TransportHeaders.Add( MqFilter::MQMSGSIZE, StringUtil::ToString( m_Metadata.length() ) );
		m_TransportHeaders.Add( MqFilter::MQAPPNAME, getServiceName() );
		m_TransportHeaders.Add( MqFilter::MQGROUPID, Collaboration::EmptyGuid() );

		// delegate work to the chain of filters.
		m_FilterChain->ProcessMessage( message.dom(), m_TransportHeaders, false );

		DEBUG( "Message is in buffer; size is : " << m_Metadata.length() );

		message.getInformation( FinTPMessage::Message::RESTCONNECTOR );

		if ( message.correlationId().length() != 0 )
			setCorrelationId( message.correlationId() );

		payload = message.payload();
		//DEBUG( "Payload is ( first 100 bytes ): [" << payload.substr( 0, 100 ) << "]" );
		DEBUG( "Payload is: [" << payload << "]" );
		
		messageOutput = Base64::decode( payload );
		message.releaseDom();

	}
	catch( const std::exception& ex )
	{
		TRACE( "An exception has occured while processing message in chain ["  << typeid( ex ).name() << "] : " << ex.what() );
		message.releaseDom();
		throw;
	}
	catch( ... )
	{
		TRACE( "An exception has occured while processing message in chain [ reason unknown ]" );
		message.releaseDom();
		throw;
	}

	// POST message to destination
	DEBUG( "Try to send message..." );		
	try
	{
		ManagedBuffer requestMessage( ( unsigned char* )messageOutput.c_str(), ManagedBuffer::Ref ,messageOutput.length() );
		vector<unsigned char> responseMessage;

		if( !m_ClientCertificate.empty() )
		{
			WorkItem<ManagedBuffer> managedBuffer( new ManagedBuffer() );
			ManagedBuffer* signedMessage = managedBuffer.get();
			IPMediator ipMediator = IPMediator();
			ipMediator.PublishPreparation( &requestMessage, signedMessage, m_ClientCertificate, "" );
			m_CurrentHelper.postOne( "", signedMessage, responseMessage );
		}
		else 
			m_CurrentHelper.postOne( "", &requestMessage, responseMessage );

		//TODO: Use same strategy as in RESTFetcher to process response
		DEBUG( "POST respose sent back to backoffice..." );
		m_RespliesHelper->putOne( &responseMessage[0], responseMessage.size(), true );

		if( !m_BackupURI.empty() )
		{
			DEBUG( "Put backup notification for requested message. Rquest id: [" + m_Metadata.id() + "]" );
			WorkItem< AbstractWatcher::DeepNotificationObject > notification( new AbstractWatcher::DeepNotificationObject( m_Metadata.id(), &requestMessage, "", requestMessage.size() ) );
			m_BackupPool.addPoolItem( m_Metadata.id(), notification );

			DEBUG( "Put backup notification for the response of requested message." );
			ManagedBuffer responseBuffer( &responseMessage[0], ManagedBuffer::Ref , responseMessage.size() );
			WorkItem< AbstractWatcher::DeepNotificationObject > responseNotification( new AbstractWatcher::DeepNotificationObject( ( "Response-" + m_Metadata.id() ), &responseBuffer, "", responseBuffer.size() ) );
			m_BackupPool.addPoolItem( m_Metadata.id(), responseNotification );
		}

	}
	catch( const std::exception& ex )
	{		
		stringstream errorMessage;
		errorMessage << "Can't write the message [" << m_Metadata.id() << "] to destination [" << m_RESTChannel << "]. Check inner exception for details." << endl;

		TRACE( errorMessage.str() );
		TRACE( ex.what() );

		throw AppException( errorMessage.str(), ex );
	}
	catch( ... )
	{
		stringstream errorMessage;
		errorMessage << "Can't write the message [" << m_Metadata.id() << "] to queue [" << m_RESTChannel << "]. Reason unknown" << endl;

		TRACE( errorMessage.str() );

		throw AppException( errorMessage.str() );
	}		
}

void RESTPublisher::Commit()
{
	DEBUG( "COMMIT" );

	m_FilterChain->Commit();
	m_RespliesHelper->commit();
}

void RESTPublisher::Abort()
{
	DEBUG( "ABORT" );

	m_FilterChain->Abort();
	m_RespliesHelper->rollback();
	m_RespliesHelper->closeQueue();
	m_RespliesHelper->disconnect();
}

void RESTPublisher::Rollback()
{
	DEBUG( "ROLLBACK" );
	m_FilterChain->Rollback();
	m_RespliesHelper->rollback();
	m_RespliesHelper->closeQueue();
	m_RespliesHelper->disconnect();
}

