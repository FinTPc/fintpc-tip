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
#include <xalanc/XercesParserLiaison/XercesDocumentWrapper.hpp>

#include "AbstractWatcher.h"
#include "RESTFetcher.h"
#include "../Connector.h"
#include "AppSettings.h"
#include "FileUtils.h"

#include "Trace.h"
#include "LogManager.h"
#include "AppExceptions.h"

#include "WorkItemPool.h"
#include "PlatformDeps.h"
#include "Collaboration.h"
#include "StringUtil.h"
#include "MQ/MqFilter.h"

#include "Swift/SwiftFormatFilter.h"
#include "Base64.h"

#ifdef WIN32
#define __MSXML_LIBRARY_DEFINED__
#include "windows.h"
#define sleep( x ) Sleep( ( x )*1000 )
#else
#include <unistd.h>
#endif

using namespace std;

#define IP_DISCONNECT_TIME 5

const string RESTFetcher::m_ProcessStepName[3] = { "REQUESTED MESSAGE", "GENERATED REPLY OF REQUESTED MESSAGE", "RESPONSE OF GENERATED REPLY " };
const unsigned int RESTFetcher::MAX_IDLETIME = 20;



//constructor
RESTFetcher::RESTFetcher() : Endpoint(), m_CurrentMessageId( "" ), m_BackupURI( "" ), m_BackupLoader( &m_BackupPool ),
	m_ReplyXsltFile( "" ), m_IsLast( false ), m_ProcessStep( ProcessSteps::MESSAGE ), m_IPEnabled( true ), m_IdleTime( 0 )
{
	m_CurrentMessage = new ManagedBuffer();
	DEBUG2( "CONSTRUCTOR" );
}
	
//destructor
RESTFetcher::~RESTFetcher()
{
	DEBUG2( "DESTRUCTOR" );
	if( m_CurrentMessage )
	{
		delete m_CurrentMessage;
		m_CurrentMessage = NULL;
	}
}

void RESTFetcher::Init()
{
	const int AUTO_ABANDON = 3;

	DEBUG( "INIT" );
	INIT_COUNTERS( &m_BackupLoader, FTPFetcherBackup );
	//INIT_COUNTERS( &m_Watcher, RESTFetcherWatcher );
	
	m_RESTChannel = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::RESTURI, "" );
	string clientAppId = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::RESTCLIENTID, "" );
	string watchResource = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::RESTSOURCEPATH );
	//same rest resource path but POST instead of GET
	m_ReplyResourcePath = watchResource;
	m_ConfirmationPath = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::RESTCONFIRMATIONPATH, "" );
			 
	m_BackupURI = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::RESTBACKUPURI, "" );
	if (!m_BackupURI.empty())
	{
		m_BackupLoader.setSFTPChannel( m_BackupURI );
		m_BackupLoader.setNotificationPool( &m_BackupPool );
	}


	m_CertificateSSL = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::SSLCERTIFICATEFILENAME, "" );
	m_CertificateSSLPhrase = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::SSLCERTIFICATEPASSWD, "" );

	string clientCertificate = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::CERTIFICATEFILENAME, "" );
	if( !clientCertificate.empty() )
		m_ClientCertificate = StringUtil::DeserializeFromFile( clientCertificate );

	m_ReplyXsltFile = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::RPLXSLT, "" );

	m_ReplyHelper.setClientId( clientAppId );
	m_RequestHelper.setClientId( clientAppId );

	m_MessageThrottling = ( m_MessageThrottling > ( 24 * 3600 ) ) ? ( 24 * 3600 ) : m_MessageThrottling;	
	if( m_MessageThrottling == 0 )
	{
		string idleTime = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::IDLETIME, "0" );
		m_IdleTime = StringUtil::ParseInt( idleTime.c_str() );
		unsigned int maxIdleTime = m_IPEnabled ? ( IP_DISCONNECT_TIME - 2 ) : MAX_IDLETIME;
		if( m_IdleTime < 0 )
			m_IdleTime = 0;
		else if( m_IdleTime > maxIdleTime )
			m_IdleTime = maxIdleTime;
		DEBUG( "Idle time is set for [" << m_IdleTime << "] seconds ( allowed values: 0 < idle time < " << maxIdleTime << ")" );
	}
	else
		DEBUG( "Throttling time is set for [" << m_MessageThrottling << "] seconds ( allowed values: 0 < throttling time < " << ( 24 * 3600 ) << ") . Ignore IdleTime settings." );

	//Publisher certificate
	string replyCertificateFile = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::CERTIFICATEFILENAME, "" );
	if( !replyCertificateFile.empty() )
		m_ReplyCertificate = StringUtil::DeserializeFromFile( replyCertificateFile );

}

void RESTFetcher::internalStart()
{
	TRACE_SERVICE( "[" << m_ServiceThreadId << "] starting to process... " );
	unsigned int ERROR_SLEEP = m_IPEnabled ? ( IP_DISCONNECT_TIME - 2 ) : MAX_IDLETIME;
	bool requireErrorEvent = true;

	DEBUG( "Starting backup loader..." );
	m_BackupLoader.setEnableRaisingEvents( true );

	try
	{
		while( m_Running )
		{
			//reset current message payload before new request
			m_CurrentMessage->copyFrom( string().c_str(), 0 );
			bool isMessageAvailable = false;

			DEBUG_GLOBAL( TimeUtil::Get( "%d/%m/%Y %H:%M:%S", 19 ) << " - Fetch new messages from [" << m_RESTChannel << m_ResourcePath << "] ... " );

			try
			{
				m_RequestHelper.connect( m_RESTChannel, m_CertificateSSL, m_CertificateSSLPhrase );
				DEBUG_GLOBAL( "Connected to resource host" );

				int result = 0;
				vector<unsigned char> httpResponse;
				result = m_RequestHelper.getOne( m_ResourcePath, httpResponse );
				m_CurrentMessage->copyFrom( &httpResponse[0], httpResponse.size() );

				/*
				string msg = XmlUtil::SerializeToString( XmlUtil::DeserializeFromFile( "test-recon.xml" ) );
				m_CurrentMessage->copyFrom( msg.c_str(), msg.size() );
				*/
				m_CurrentMessageId = m_RequestHelper.getLastRequestSequence();
				string messageType = m_RequestHelper.getLastMessageType();
				float retrievingTime = m_RequestHelper.getLastRetrievingTime();

				isMessageAvailable = ( m_CurrentMessage->size() > 0 ) ?  true : false ;;
				m_LastOpSucceeded = true;

				if( m_IPEnabled )
				{
					string requestFeedback = m_RequestHelper.getHeaderField( "X-MONTRAN-RTP-ReqSts" );
					isMessageAvailable = ( isMessageAvailable && ( requestFeedback != "EMPTY" ) ) ?  true : false ;
					DEBUG_GLOBAL( "IP request status is: " << requestFeedback );
				}

				DEBUG_GLOBAL( "New message retreived [" << m_CurrentMessageId << "], in [" << retrievingTime << "] seconds ! " );

				//TODO: Content-Type helper property to notify plain sau xml
				if( isMessageAvailable )
				{
					if( !m_ConfirmationPath.empty() && isConfirmationRequired( messageType ) )
					{
						//reset for new posible response
						httpResponse.clear();
						m_RequestHelper.postOneConfirmation( m_ConfirmationPath, m_CurrentMessageId );
						retrievingTime = m_RequestHelper.getLastRetrievingTime();

						DEBUG_GLOBAL( "Confirmation admitted for [" << m_CurrentMessageId << "], in [" << retrievingTime << "] seconds !" );
					}
				
					m_IsLast = false;
					m_ProcessStep = ProcessSteps::MESSAGE;
					
					DEBUG( "Performing message loop ... " );
					m_LastOpSucceeded = PerformMessageLoop( true );
					DEBUG( "Message loop finished ok." );
				}
				else 
				{
					DEBUG_GLOBAL( "No message available! Next attempt in " << m_IdleTime << " seconds" );
					sleep( m_IdleTime );
				}
			}
			catch( const AppException& ex )
			{
				m_CurrentMessage->copyFrom( string().c_str(), 0 );
				string exceptionType = typeid( ex ).name();
				string errorMessage = ex.getMessage();

				TRACE_GLOBAL( exceptionType << " encountered while processing message : " << errorMessage );
				m_LastOpSucceeded = false;

			}
			catch( const std::exception& ex )
			{
				m_CurrentMessage->copyFrom( string().c_str(), 0 );
				string exceptionType = typeid( ex ).name();
				string errorMessage = ex.what();

				TRACE_GLOBAL( exceptionType << " encountered while processing message : " << errorMessage );
				m_LastOpSucceeded = false;
			}
			catch( ... )
			{
				m_CurrentMessage->copyFrom( string().c_str(), 0 );
				TRACE_GLOBAL( "[unknown exception] encountered while processing message. " );
				m_LastOpSucceeded = false;
			}	

			unsigned int sleepTime = m_MessageThrottling;
			if( m_MessageThrottling > 3600 )
			{
				//int START_OFFSET = now.time_of_day().hours();

				sleepTime= m_MessageThrottling / 3600;
				boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
				int currentOffset = 24 - now.time_of_day().hours();
				if( currentOffset < m_IdleTime )
					sleepTime += currentOffset;
				stringstream message;
				if( m_LastOpSucceeded )
				{
					message << "End message fetch at: [" <<  TimeUtil::Get( "%d/%m/%Y %H:%M:%S", 19 ) << "]. Next try in: [" << sleepTime << "] hours";
					DEBUG( message.str() );
					LogManager::Publish( message.str(), EventType::Info );
				}
				sleepTime = sleepTime * 3600;
			}

			if( m_LastOpSucceeded )
			{
				if( !requireErrorEvent && !isMessageAvailable )
				{
					stringstream message;
					message << "Fetch process recovered but no message available: [" <<  TimeUtil::Get( "%d/%m/%Y %H:%M:%S", 19 ) << "]." ;
					DEBUG( message.str() );
					LogManager::Publish( message.str(), EventType::Info );
				}
				requireErrorEvent = true;
			}
			else
			{
				if( requireErrorEvent )
				{
					stringstream message;
					message << "Message fetch failed at: [" <<  TimeUtil::Get( "%d/%m/%Y %H:%M:%S", 19 ) << "]. Next try in: [" << ERROR_SLEEP << "] seconds";
					DEBUG( message.str() );
					LogManager::Publish( message.str(), EventType::Error );
					requireErrorEvent = false;
					sleepTime = ERROR_SLEEP;
				}
			}
			sleep( sleepTime );
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

void RESTFetcher::internalStop()
{
	DEBUG( "STOP" );
	
	// ensure watcher is dead ( the watcher will lock on pool and wait until it is empty )
	//m_Watcher.setEnableRaisingEvents( false );

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

	m_ReplyHelper.disconnect();
}

string RESTFetcher::Prepare()
{
	DEBUG( "PREPARE" );
	//TODO: Move to Init()? Or keep here to handle unexpected disconnection
	//TODO: check certificates rights
	try
	{
		m_ReplyHelper.connect( m_RESTChannel, m_CertificateSSL, m_CertificateSSLPhrase);
	}
	catch( runtime_error& er )
	{
		AppException aex( "Can't connect to remote server, to fetch messages.", EventType::Error );
		aex.setSeverity( EventSeverity::Fatal );
		throw aex;
	}

	return m_CurrentMessageId;
}

void RESTFetcher::Process( const string& correlationId )
{
	vector<unsigned char> responseMessage;

	if( m_ProcessStep == ProcessSteps::MESSAGE )
		DEBUG( " PROCESS NOTIFICATION IN 3 STEPS ..." );
	
	DEBUG( "STEP " << ( m_ProcessStep ) << ". " << m_ProcessStepName[ m_ProcessStep - 1 ] << ". Current message length: " << m_CurrentMessage->size() << "." );

	m_TransportHeaders.Clear();
	stringstream currentAction;

	WorkItem<ManagedBuffer> managedInBuffer( new ManagedBuffer() );
	ManagedBuffer* inputBuffer = managedInBuffer.get();
	inputBuffer->copyFrom( m_CurrentMessage );
	try
	{
		if( m_ProcessStep == ProcessSteps::REPLY )
		{
			currentAction << "POST reply";
			if( !m_ReplyCertificate.empty() )
			{
				WorkItem<ManagedBuffer> managedBuffer( new ManagedBuffer() );
				ManagedBuffer* signedMessage = managedBuffer.get();
				IPMediator ipMediator = IPMediator();
				ipMediator.PublishPreparation( inputBuffer, signedMessage, m_ReplyCertificate, "" );

				m_ReplyHelper.postOne( m_ReplyResourcePath, signedMessage, responseMessage );
			}
			else 
				m_ReplyHelper.postOne( m_ReplyResourcePath, inputBuffer, responseMessage );
			
		}
		else 
		{
			if( !m_ClientCertificate.empty() && requiresSecurityCheck() )
			{
				currentAction.str("");
				currentAction << "CHECK signature"; 
 				IPMediator ipMediator = IPMediator();
				ipMediator.FetchPreparation( m_CurrentMessage, NULL, m_ClientCertificate, "" );

				DEBUG( "Message ["<< m_CurrentMessageId << "] authenticated " );
			}
		}

	}
	catch( const runtime_error& ex )
	{
		// format error
		stringstream errorMessage;
		errorMessage << "Can't " << currentAction.str() << ". Current message ID [" << m_CurrentMessageId<< "]. " << ex.what();
		TRACE( errorMessage.str() );

		//TODO check this for returning a pointer to a const ref
		AppException aex = AppException( errorMessage.str() );
		aex.setSeverity( EventSeverity::Failure );
		throw aex;
	}
	catch( ... )
	{
		stringstream errorMessage;
		errorMessage << "Can't " << currentAction.str() << ". Current message ID [" << m_CurrentMessageId << "]. Unknown error.";
		TRACE( errorMessage.str() );
		AppException aex = AppException( errorMessage.str() );
		aex.setSeverity( EventSeverity::Failure );
		throw aex;
	}

	// Output message by delegating the work to the chain of filters
	// As the message will end up in MQ, no output is needed
	WorkItem<ManagedBuffer> managedOutBuffer( NULL );
	try
	{
		m_TransportHeaders.Add( "XSLTPARAMBATCHID", StringUtil::Pad( Collaboration::EmptyGuid(), "\'","\'" ) );
		m_TransportHeaders.Add( "XSLTPARAMGUID", StringUtil::Pad( Collaboration::GenerateGuid(), "\'","\'" ) );
	  	m_TransportHeaders.Add( "XSLTPARAMCORRELID", StringUtil::Pad( correlationId, "\'","\'" ) );
	  	m_TransportHeaders.Add( "XSLTPARAMSESSIONID", "'I'" );
		if( m_ProcessStep == ProcessSteps::RESPONSE )
			m_TransportHeaders.Add( "XSLTPARAMREQUESTOR", StringUtil::Pad( getServiceName().append( "Response" ), "\'","\'" ) );
		else
			m_TransportHeaders.Add( "XSLTPARAMREQUESTOR", StringUtil::Pad( getServiceName(), "\'","\'" ) );
	  	
	  	m_TransportHeaders.Add( "XSLTPARAMREQUEST", "'SingleMessage'" );
		m_TransportHeaders.Add( MqFilter::MQAPPNAME, getServiceName() );
	  	
		stringstream formatFeedback;
				
		//Generated reply run backward
		if( m_ProcessStep == ProcessSteps::REPLY )
		{
			m_TransportHeaders.Add( "XSLTPARAMIOID", "'O'" );
			formatFeedback << "W|" << correlationId << "|0";
		}
		else
		{
			m_TransportHeaders.Add( "XSLTPARAMIOID", "'I'" );
			formatFeedback << "W|" << correlationId << "|FTP00";
		}
		m_TransportHeaders.Add( "XSLTPARAMFEEDBACK", StringUtil::Pad( formatFeedback.str(), "\'","\'" ) );
		
		//Send and backup current message
		m_FilterChain->ProcessMessage( managedInBuffer, managedOutBuffer, m_TransportHeaders, true );
		if( !m_BackupURI.empty() )
		{
			DEBUG( "Put backup notification for " <<  m_ProcessStepName[ m_ProcessStep - 1 ] << ". Rquest id: [" + m_CurrentMessageId + "]" );

			WorkItem< AbstractWatcher::DeepNotificationObject > notification( new AbstractWatcher::DeepNotificationObject( m_CurrentMessageId, m_CurrentMessage, "", m_CurrentMessage->size() ) );
			m_BackupPool.addPoolItem( m_CurrentMessageId, notification );
		}

		// !!! Switch m_CurrentMessage with new generated reply and repeat entire Process() !!!
		managedOutBuffer = WorkItem<ManagedBuffer>( new ManagedBuffer() );
		if( requiresReply() )
		{
			XSLTFilter generateReplyFilter = XSLTFilter();
			generateReplyFilter.addProperty( XSLTFilter::XSLTFILE, m_ReplyXsltFile );

			//this filter use WS to ask BO how to reply
			generateReplyFilter.ProcessMessage( managedInBuffer, managedOutBuffer, m_TransportHeaders, true );
			m_CurrentMessage->copyFrom( managedOutBuffer.get() );

			m_ProcessStep = ProcessSteps::REPLY;
		}
		else
		{
			if( responseMessage.size() > 0 )
			{
				m_CurrentMessage->copyFrom( &responseMessage[0], responseMessage.size() );
				m_ProcessStep = ProcessSteps::RESPONSE;
				m_IsLast = false;
			}
			else
			{
				m_IsLast = true;
				m_ProcessStep = ProcessSteps::MESSAGE;
			}
		}
	}
	catch( const FilterInvalidMethod& ex )
	{
		try
		{
			//string inputStr = XmlUtil::SerializeToString( m_CurrentMessage );
			DEBUG( "Falling back to serialize XML input message to string ... [" << m_CurrentMessage->buffer() << "]." );

			WorkItem<ManagedBuffer> inputBuffer( m_CurrentMessage ); //new ManagedBuffer( ( unsigned char* )inputStr.c_str(), ManagedBuffer::Ref, inputStr.length(), inputStr.length() ) );
			m_FilterChain->ProcessMessage( managedInBuffer, managedOutBuffer, m_TransportHeaders, true );
		}
		catch( ... ) 
		{
			TRACE( "Can't process current message. Unhandled error while fall back filter chain processing" );
			throw;
		}
	}
	// base class( Endpoint ) will get error specifics and report it further
	catch( ... )
	{
		try
		{
			TRACE( "Can't process current message. Unhandled error filter chain processing" );  
			m_TransportHeaders.Dump();
		} catch( ... ){};

		throw;
	}
}

bool RESTFetcher::moreMessages() const
{
	return !m_IsLast;
}

void RESTFetcher::Commit()
{
	m_FilterChain->Commit();
	// one or more steps possible for a single GET
	unsigned int prevStep = ( m_ProcessStep > 1 ) ? m_ProcessStep - 1 : m_ProcessStep ;
	DEBUG( "COMMIT STEP " << ( prevStep ) << ". " << m_ProcessStepName[ prevStep - 1 ] << "[" << m_CurrentMessageId << "]" );
}

void RESTFetcher::Abort()
{
	DEBUG( "ABORT" );
	m_ProcessStep = ProcessSteps::MESSAGE;
	m_FilterChain->Abort();
}

void RESTFetcher::Rollback()
{
	DEBUG( "ROLLBACK" );
	//There is no  "retry requiered" since the next message from interface (IPS) will not be available if this 3 step process not succeeded
	Abort();
}

bool RESTFetcher::requiresReply()
{
	bool returnValue = true;
	XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc = NULL;
	try
	{
		//TODO: Rework to use RR like evaluators
		doc = XmlUtil::DeserializeFromString( m_CurrentMessage->buffer(), m_CurrentMessage->size() );
		const DOMElement* root = doc->getDocumentElement();
		//Not all messages (evaluators) requires reply
		DOMNodeList* list = root->getElementsByTagNameNS( unicodeForm( "urn:montran:message.01" ), unicodeForm( "FIToFICstmrCdtTrf" ) );
		if( list != NULL && list->getLength() >  0 )
			returnValue = true;
		else
			returnValue = false;

		doc->release();
		doc = NULL;
	}
	catch( ... )
	{
		if( doc != NULL )
		{
			doc->release();
			doc = NULL;
		}
		TRACE( "Error while evaluating if message requires reply. Reply anyway !" );
	}
	return returnValue;
}

bool RESTFetcher::requiresSecurityCheck()
{
	bool returnValue = true;
	XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc = NULL;

	try
	{
		doc = XmlUtil::DeserializeFromString( m_CurrentMessage->buffer(), m_CurrentMessage->size() );
		string nms = XmlUtil::getNamespace( doc );
		returnValue = ( nms != "urn:montran:rcon.001" ) && ( nms != "urn:montran:positions.001" );
	}
	catch( ... )
	{
		TRACE( "Error while evaluating if message requires security check. Check security anyway!" );
	}

	if( doc != NULL )
	{
		doc->release();
		doc= NULL;
	}
	return returnValue;
}

/*
void RESTFetcher::GetNewNotification()
{
	//!result && ( succeeded ) && m_Enabled )
	unsigned long requestCount = 0;
	int fetchThrottling = 5;
	string requestFeedback = m_RequestHelper.getHeaderField( "X-MONTRAN-RTP-ReqSts" );

		try
		{
			DEBUG_GLOBAL( TimeUtil::Get( "%d/%m/%Y %H:%M:%S", 19 ) << " - Fetch new messages from [" << m_RESTChannel << m_ResourcePath << "] ... " );

			m_RequestHelper.connect( m_RESTChannel, m_CertificateSSL, m_CertificateSSLPhrase );

			DEBUG_GLOBAL( "Connected to resource host" );
			int result = 0;

			vector<unsigned char> message;
			result = m_RequestHelper.getOne( m_ResourcePath, message );
				
			m_CurrentMessageId = m_RequestHelper.getLastRequestSequence();
			string messageType = m_RequestHelper.getLastMessageType();
			float retrievingTime = m_RequestHelper.getLastRetrievingTime();

			//TODO: Rework for generic request status
			bool IPOKStatus = m_IPEnabled ? ( requestFeedback != "EMPTY" )  : true ;

			DEBUG_GLOBAL( "New message retreived [" << m_CurrentMessageId << "], in [" << retrievingTime << "] seconds ! " << ( m_IPEnabled ? ( "[" + requestFeedback + "]" ) : "" ) );

			//TODO: Content-Type helper property to notify plain sau xml
			if( ( message.size() > 0 ) && IPOKStatus )
			{
				if( !m_ConfirmationPath.empty() && isConfirmationRequired( messageType ) )
				{
					m_RequestHelper.postOneConfirmation( m_ConfirmationPath, m_CurrentMessageId );
					retrievingTime = m_RequestHelper.getLastRetrievingTime();

					DEBUG_GLOBAL( "Confirmation admitted for [" << m_CurrentMessageId << "], in [" << retrievingTime << "] seconds !" );
				}

				m_CurrentMessage->copyFrom( &message[0], message.size() );
				m_CurrentMessageLength = message.size();
				//string msg = XmlUtil::SerializeToString( XmlUtil::DeserializeFromFile( "test-pacs008.xml" ) );
				//docMessage->copyFrom( msg.c_str(), msg.size() )
			}

			ASSIGN_COUNTER( REQUEST_ATTEMPTS, requestCount++ );

		}
		catch( const AppException& ex )
		{
			string exceptionType = typeid( ex ) .name();
			string errorMessage = ex.getMessage();

			TRACE_GLOBAL( exceptionType << " encountered when request message : " << errorMessage );
		}
		catch( const std::exception& ex )
		{
			string exceptionType = typeid( ex ) .name();
			string errorMessage = ex.what();

			TRACE_GLOBAL( exceptionType << " encountered when request message: " << errorMessage );
		}
		catch( ... )
		{
			TRACE_GLOBAL( "Unhandled exception encountered when request resource. " );
		}
		
		ASSIGN_COUNTER( REQUEST_ATTEMPTS, 0 );
}
*/
bool RESTFetcher::isConfirmationRequired( string& messageType  ) const
{
	//TODO Extend this using some kind of evaluators
	return ( ( messageType != "pacs.008" ) && ( messageType != "position.001" ) ) ;
}

//void RESTFetcher::NotificationCallback( string groupId, string messageId, int messageLength )
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
