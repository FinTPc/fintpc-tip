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
#include <sstream>
#include <string>

#ifndef WIN32
	#include <unistd.h>
#else
	#include <windows.h>

	#define sleep(x) Sleep( (x)*1000 )
#endif

#include "../MqException.h"
#include "Base64.h"
#include "PlatformDeps.h"
#include "WMqHelper.h"

using namespace std;
using namespace FinTP;

// WMqHelper implementation
WMqHelper::WMqHelper() : TransportHelper( MQ_MSG_ID_LENGTH ), m_QueueManagerCCSID( 0 ), m_QueueManagerEncoding( 0 ), m_ConversionNeeded( false ), m_MessageFormat( MQFMT_STRING ), m_ConnectOptions( MQCNO_HANDLE_SHARE_NONE ),
#if defined( AIX )
m_QueueManagerPlatform( MQPL_AIX ),
#elif defined ( WIN32 )
m_QueueManagerPlatform( MQPL_WINDOWS ),
#else 
m_QueueManagerPlatform( MQPL_UNIX ),
#endif // AIX || WIN32
	m_OpenOption( MQOO_INQUIRE ), m_ConnectionString( "" ),
	m_MessageType( MQMT_DATAGRAM ), m_DataOffset( 0 ),
	m_ReplyOptions( MQRO_NONE ), m_ReplyQueueManager( "" ),
	m_UseSyncpoint( true ), m_QueueManagerOpenRefCount( 0 )
{
}

WMqHelper::~WMqHelper()
{
	try
	{
		DEBUG( "Destructor" );
	}catch( ... ){};
	try
	{
		disconnect();
	}catch( ... ){};
	try
	{
		DEBUG( "Destructor done." );
	}catch( ... ){};
}

void WMqHelper::setApplicationName( const string& passedAppName )
{
	unsigned int appNameLength = passedAppName.length();
	if ( appNameLength >= MQ_PUT_APPL_NAME_LENGTH )
		m_ApplicationName = passedAppName.substr( 0, MQ_PUT_APPL_NAME_LENGTH );
	else
		m_ApplicationName = passedAppName + string( MQ_PUT_APPL_NAME_LENGTH - appNameLength, 0 );

	m_UsePassedAppName = true;
}

void WMqHelper::connect( const string& queueManagerName, const string& channelDefinition, bool force )
{
	connect( queueManagerName, channelDefinition, "", "", "", force );
}

void WMqHelper::connect( const string& queueManagerName, const string& channelDefinition, const string& keyRepository, const string& sslCypherSpec, const string& sslPeerName, bool force )
{
	DEBUG( "Connecting to queue manager" );

	if ( ( keyRepository.length() > 0 ) && ( m_SSLKeyRepository.length() == 0 ) )
		m_SSLKeyRepository = keyRepository;

	if ( ( sslCypherSpec.length() > 0 ) && ( m_SSLCypherSpec.length() == 0 ) )
		m_SSLCypherSpec = sslCypherSpec;

	if ( ( sslPeerName.length() > 0 ) && ( m_SSLPeerName.length() == 0 ) )
		m_SSLPeerName = sslPeerName;

	bool reconnect = false;
	
	// update channel definition if necessary
	if ( channelDefinition.length() > 0 )
	{
		if ( m_ConnectionString != channelDefinition )
		{
			m_ConnectionString = channelDefinition;
			DEBUG( "Connection string changed" );
			reconnect = true;
		}
		else
			DEBUG( "Connection string is the same" );
	}
	DEBUG( "Channel definition set" );
	
	// qm name may be overriden by this call
	if ( queueManagerName.length() > 0 )
	{
		DEBUG( "Checking if queue manager name has changed..." );
		
		if ( queueManagerName != string( m_QueueManager.name() ) )
		{
			DEBUG( "Changing queue manager name" );
			disconnect();
			m_QueueManager.setName( queueManagerName.data() );
			
			DEBUG( "Queue manager name changed" );
			reconnect = true;
		}
		else
			DEBUG( "Queue manager name is the same" );
		DEBUG( "Queue manager name set" );
	}	
	// but if we didn't set a name in the .ctor, can't do anything ( no qm name )
	else if ( m_QueueManager.name().length() <= 0 )
	{
		TRACE( "Queue manager name not specified." );
		throw runtime_error( "Queue manager name not specified." );
	}
	DEBUG( "Queue manager name resolved" );
	
	if ( force )
	{
		DEBUG( "Forced connect" );
		reconnect = true;
	}
	else
		DEBUG( "Reconnect not forced" );
		 
	if ( reconnect || !m_QueueManager.connectionStatus() )
	{
		string lclQueueManagerName = ( char* )m_QueueManager.name();
		
		if ( reconnect )
		{
			DEBUG( "Reconnecting to " << lclQueueManagerName << " ( previous settings changed or forced reconnect )" );
		}
		else
		{
			DEBUG( "Reconnecting to " << lclQueueManagerName << " ( previous connection closed )" );
		}
		
		disconnect();
		setChannel();
		
		// use the option to share the connection among threads, using blocks
		
		m_QueueManager.setConnectOptions( m_ConnectOptions );

		if ( m_SSLKeyRepository.length() > 0 )
		{
#if !defined( AIX ) && !defined( LINUX )
			ImqBoolean result = m_QueueManager.setKeyRepository( m_SSLKeyRepository.c_str() );
			if ( result )
			{
				DEBUG( "Key repository set to [" << m_SSLKeyRepository << "] -> " << result  ); 
			}
			else
			{
				DEBUG( "Unable to set key repository to [" << m_SSLKeyRepository << "] -> " << result );
			}
#else
			throw runtime_error( "Unable to set key repository [setKeyRepository not exported]" );
#endif // !AIX || !LINUX
		}
		
		if ( !m_QueueManager.connect() )
		{
			stringstream errorMessage;
			int reasonCode = ( int )m_QueueManager.reasonCode();
			
			errorMessage << "Connect to queue manager [" << lclQueueManagerName << "] failed using channel definition [" << m_ConnectionString << "]. Reason : " << reasonCode;
			TRACE( errorMessage.str() );
			throw runtime_error( errorMessage.str() );
		}

		m_QueueManagerPlatform = m_QueueManager.platform();
#if defined( WIN32 )
		m_ConversionNeeded = ( ( m_QueueManagerPlatform != MQPL_WINDOWS ) && ( ( m_QueueManagerPlatform != MQPL_WINDOWS_NT ) ) );
#elif defined( AIX )
		m_ConversionNeeded = ( m_QueueManagerPlatform != MQPL_AIX );
#elif defined( LINUX )
		m_ConversionNeeded = ( m_QueueManagerPlatform != MQPL_UNIX );
#endif

		m_QueueManagerOpenRefCount++;
		DEBUG( "Connection refcount = " << m_QueueManagerOpenRefCount );
		
		DEBUG( "Connected to queue manager : [" << lclQueueManagerName << "]" );
	}
	else
		DEBUG( "Already connected to queue manager." );
}

void WMqHelper::disconnect()
{
	DEBUG( "Disconnecting from queue manager" );
	
	if( m_QueueManager.name().length() == 0 )
	{
		DEBUG( "Queue manager not set... disconnect shortcut" );
		return;
	}
	
	closeQueue();
	closeBackupQueue();

	try
	{
		bool prevStatus = m_QueueManager.connectionStatus()?true:false;
				
		// Disconnect from MQM if not already disconnected (the 
		// ImqQueueManager object handles this situation automatically) 
		if ( ! m_QueueManager.disconnect() ) 
		{ 
			// report reason, if any  
			string lclQueueManagerName = ( char* )m_QueueManager.name();
			int reasonCode = ( int )m_QueueManager.reasonCode();
			
			TRACE( "Disconnect from queue manager [" << lclQueueManagerName << "] failed. Reason : " << reasonCode );
		}
		if( prevStatus )
			m_QueueManagerOpenRefCount--;
		DEBUG( "Connection refcount = " << m_QueueManagerOpenRefCount );
	}
	catch( ... )
	{
		TRACE( "Error disconnecting from queue manager" );
	}
	DEBUG( "Disconnected from queue manager" );
}

void WMqHelper::openBackupQueue()
{
	DEBUG( "Opening backup queue [" << m_BackupQueueName << "]" );

	if( !m_BackupQueue.openStatus() )
	{
		m_BackupQueue.setName( m_BackupQueueName.c_str() );
		m_BackupQueue.setConnectionReference( m_QueueManager );
		m_BackupQueue.setOpenOptions( MQOO_OUTPUT );

		/* report reason, if any; stop if failed      */
		if ( !m_BackupQueue.open() )
		{
			int reasonCode = ( int )m_BackupQueue.reasonCode();		
			stringstream errorMessage;
			errorMessage << "Error opening backup queue [" << m_BackupQueueName << "]. Reason code " << reasonCode;

			throw runtime_error( errorMessage.str() );
		}
	}
	else
	{
		DEBUG( "Backup queue already open" );
	}
}

void WMqHelper::openBackoutQueue( string name )
{
	DEBUG( "Opening backout queue [" << name << "]" );

	if ( string( m_BackoutQueue.name() ) != name )
		closeBackoutQueue();

	if( !m_BackoutQueue.openStatus() )
	{
		m_BackoutQueue.setName( name.c_str() );
		m_BackoutQueue.setConnectionReference( m_QueueManager );
		m_BackoutQueue.setOpenOptions( MQOO_OUTPUT );

		/* report reason, if any; stop if failed      */
		if ( !m_BackoutQueue.open() )
		{
			int reasonCode = ( int )m_BackoutQueue.reasonCode();		
			stringstream errorMessage;
			errorMessage << "Error opening backout queue [" << name << "]. Reason code " << reasonCode;

			throw runtime_error( errorMessage.str() );
		}
	}
	else
	{
		DEBUG( "Backout queue already open" );
	}
}

void WMqHelper::openQueue( const string& queueName )
{
	DEBUG( "Opening queue [" << queueName << "]" );
	
	// qm name may be overriden by this call
	if ( queueName.length() > 0 )
	{
		if ( string( m_Queue.name() ) != queueName )
		{
			DEBUG( "Queue name changed" );
			closeQueue();
						
			m_Queue.setName( queueName.c_str() );
			m_Queue.setConnectionReference( m_QueueManager );
		}
	}
	// but if we didn't set a name in the .ctor, can't do anything ( no qm name )
	else if ( m_Queue.name().length() <= 0 )
		throw runtime_error( "Queue name not specified." );
	
	// may be needed -  not performed if already connected
	connect();
	
	string lclQueueName = ( char* )m_Queue.name();
	
	if( m_Queue.openStatus() )
	{
		if ( m_OpenOption != m_Queue.openOptions() )
		{
			DEBUG( "Queue already open, but with different options. Reopening queue... " );
			m_Queue.close();
		}
		else
		{
			DEBUG( "Queue [" << lclQueueName << "] already open" ); 
			return;
		}
	}
				
	// Open the target message queue for output
	m_Queue.setOpenOptions( m_OpenOption ); 

	/* report reason, if any; stop if failed      */
	if ( !m_Queue.open() )
	{
		int reasonCode = ( int )m_Queue.reasonCode();		
		stringstream errorMessage;
		errorMessage << "Error opening queue [" << lclQueueName << "]. Reason code " << reasonCode;

		TRACE( errorMessage.str() );
		throw runtime_error( errorMessage.str() );
	}
	m_QueueOpenRefCount++;
	DEBUG( "Queue [" << lclQueueName << "] open refcount = " << m_QueueOpenRefCount );
	
	if ( m_Queue.completionCode() == MQCC_FAILED ) 
	{
		stringstream errorMessage;
		int reasonCode = ( int )m_Queue.reasonCode();
		errorMessage << "Open queue [" << lclQueueName << "] failed with reason code : " << reasonCode;
		throw runtime_error( errorMessage.str() );
	}
	
	DEBUG( "Queue [" << lclQueueName << "] opened." );
}

void WMqHelper::closeBackupQueue()
{
	if ( m_SaveBackup )
	{
		DEBUG( "Closing backup queue [" << m_BackupQueueName << "]" );
		if ( !m_Queue.close() )
		{
			int reasonCode = ( int )m_BackupQueue.reasonCode();
			TRACE( "Close backup queue [" << m_BackupQueueName << "] ended with reason code : " << reasonCode );
		}
	}
}

void WMqHelper::closeBackoutQueue()
{
	DEBUG( "Closing backout queue [" << m_BackoutQueue.name() << "]" );
	if ( !m_BackoutQueue.close() )
	{
		int reasonCode = ( int )m_BackoutQueue.reasonCode();
		TRACE( "Close backout queue [" << m_BackoutQueue.name() << "] ended with reason code : " << reasonCode );
	}
}

void WMqHelper::closeQueue()
{
	if( m_Queue.name().length() == 0 )
	{
		DEBUG( "Queue not set... close shortcut" );
		return;
	}
	
	string lclQueueName = ( char* )m_Queue.name();
	DEBUG( "Closing queue [" << lclQueueName << "]" );

	try
	{
		if ( !m_Queue.close() )
		{
			int reasonCode = ( int )m_Queue.reasonCode();
			TRACE( "Close queue [" << lclQueueName << "] ended with reason code : " << reasonCode );
		}
		m_QueueOpenRefCount--;
		m_OpenOption = MQOO_INQUIRE;
		DEBUG( "Queue ["  << lclQueueName << "] open refcount = " << m_QueueOpenRefCount );
	}
	catch( ... )
	{
		TRACE( "Error closing queue [" << lclQueueName << "]" );
	}
	DEBUG( "Queue [" << lclQueueName << "] closed." );
}

void WMqHelper::putOneReply( ManagedBuffer* buffer, long feedback )
{
	DEBUG( "Putting one reply - wrapper" );
	ImqPutMessageOptions pmo;
	pmo.setOptions( MQPMO_FAIL_IF_QUIESCING );
	
	pmo.setSyncPointParticipation( true );
		
	ImqMessage msg;
	msg.setFeedback( feedback );
	msg.setMessageType( MQMT_REPLY );
	
	putOne( buffer, pmo, msg );

	DEBUG( "One reply put - wrapper" );
}

void WMqHelper::putOneReply( unsigned char* buffer, size_t bufferSize, long feedback, TRANSPORT_MESSAGE_TYPE replyType )
{
	DEBUG( "Putting one reply - wrapper" );
	ImqPutMessageOptions pmo;
	pmo.setOptions( MQPMO_FAIL_IF_QUIESCING );
	
	pmo.setSyncPointParticipation( true );
		
	ImqMessage msg;
	msg.setFeedback( feedback );
	if( replyType == TransportHelper::TMT_REPLY )
		msg.setMessageType( MQMT_REPLY );
	else
		msg.setMessageType( MQMT_APPL_FIRST + replyType );
	
	WorkItem< ManagedBuffer > managedBuffer( new ManagedBuffer( buffer, ManagedBuffer::Ref, bufferSize ) );
	putOne( managedBuffer.get(), pmo, msg );

	DEBUG( "One reply put - wrapper" );
}

// syncpoint = true, messagetype = MQMT_REQUEST, 
// setReplyToQueueManagerName, setReplyToQueueName, setReport
void WMqHelper::putOneRequest( unsigned char* buffer, size_t bufferSize, const string& rtqName, const string& rtqmName, TransportReplyOptions& replyOptions )
{
	DEBUG( "Putting one request - wrapper" );
	ImqPutMessageOptions pmo;
	pmo.setOptions( MQPMO_FAIL_IF_QUIESCING );
	
	pmo.setSyncPointParticipation( true );
		
	ImqMessage msg;
	msg.setReplyToQueueManagerName( rtqmName.data() );
	msg.setReplyToQueueName( rtqName.data() );
	msg.setReport( ParseReplyOptions( replyOptions ) );
	msg.setMessageType( MQMT_REQUEST );
	
	WorkItem< ManagedBuffer > managedBuffer( new ManagedBuffer( buffer, ManagedBuffer::Ref, bufferSize ) );
	putOne( managedBuffer.get(), pmo, msg );

	DEBUG( "One request put - wrapper" );
}
		
void WMqHelper::putOne( unsigned char* buffer, size_t bufferSize, bool syncpoint )
{
	DEBUG( "Putting one message - wrapper" );
	ImqPutMessageOptions pmo;
	ImqMessage msg;

	pmo.setOptions( MQPMO_FAIL_IF_QUIESCING );
	
	if ( syncpoint )
		pmo.setSyncPointParticipation( true );
		
	WorkItem< ManagedBuffer > managedBuffer( new ManagedBuffer( buffer, ManagedBuffer::Ref, bufferSize ) );
	putOne( managedBuffer.get(), pmo, msg );

	DEBUG( "One message put - wrapper" );
}

void WMqHelper::putOne( unsigned char* buffer, size_t bufferSize, ImqPutMessageOptions& pmo )
{
	ImqMessage msg;
	WorkItem< ManagedBuffer > managedBuffer( new ManagedBuffer( buffer, ManagedBuffer::Ref, bufferSize ) );
	putOne( managedBuffer.get(), pmo, msg );
}

void WMqHelper::putOne( unsigned char* buffer, size_t bufferSize, ImqPutMessageOptions& pmo, ImqMessage& msg )
{
	WorkItem< ManagedBuffer > managedBuffer( new ManagedBuffer( buffer, ManagedBuffer::Ref, bufferSize ) );
	putOne( managedBuffer.get(), pmo, msg );
}

void WMqHelper::putToDeadLetterQueue( ImqMsg& msg )
{
	bool queueOpen = false;	
	string dlQueueName = string( m_Queue.backoutRequeueName() );

	DEBUG( "Putting one message to deadletter queue" );

	if ( dlQueueName.length() > 0 )
	{
		try
		{
			openBackoutQueue( dlQueueName );
			queueOpen = true;
		}
		catch( ... ) 
		{
			stringstream errorMessage;
			errorMessage << "Unable to open backout requeue [" << dlQueueName << "] for queue [" << m_Queue.name() << "]. Reason " << m_Queue.reasonCode();
			TRACE( errorMessage.str() );
		}
	}

	if ( !queueOpen )
	{
		dlQueueName = m_QueueManager.deadLetterQueueName();

		// dead letter queue not set
		if ( dlQueueName.length() <= 0 )
			dlQueueName = "SYSTEM.DEAD.LETTER.QUEUE";

		try
		{
			openBackoutQueue( dlQueueName );
			queueOpen = true;
		}
		catch( ... ) 
		{
			stringstream errorMessage;
			errorMessage << "Unable to open dead letter queue [" << dlQueueName << "]. Reason " << m_Queue.reasonCode();
			TRACE( errorMessage.str() );

			throw MqException( errorMessage.str(), m_Queue.reasonCode() );
		}
	}

	DEBUG( "Resolved deadletter queue name : [" << dlQueueName << "]" );

	bool putSucceeded = false;
	string reason = "unknown reason";
	try
	{
		ImqPutMessageOptions pmo;
		pmo.setOptions( MQPMO_FAIL_IF_QUIESCING );
		pmo.setSyncPointParticipation( 1 );

		if ( !m_BackoutQueue.put( msg, pmo ) )
		{
			stringstream errorMessage;
			int reasonCode = ( int )m_BackoutQueue.reasonCode();

			errorMessage << "Put [" << m_BackoutQueue.name() << "] ended with reason code : " << reasonCode;
			throw runtime_error( errorMessage.str() );
		}
		putSucceeded = true;
	}
	catch( const std::exception& ex )
	{
		reason = ex.what();
	}
	catch( ... ){}

	if ( !putSucceeded )
	{
		stringstream errorMessage;
		errorMessage << "Unable to put the message to backout/dead letter queue [" << reason << "]";
		TRACE( errorMessage.str() );

		throw MqException( errorMessage.str(), MQRC_NONE );
	}
	else 
	{
		DEBUG( "One message put to deadletter queue [" << dlQueueName << "]" );
	}
}

void WMqHelper::putOne( ManagedBuffer* buffer, ImqPutMessageOptions& pmo, ImqMessage& msg )
{
	setOpenQueueOptions( MQOO_OUTPUT );
	DEBUG( "Putting one message" );
	try
	{
		openQueue();
	}
	catch( const std::exception& ex )
	{
		stringstream errorMessage;
		errorMessage << "Unable to put a message to WMQ. [" << ex.what() << "]";
		TRACE( errorMessage.str() );
		throw runtime_error( errorMessage.str() );
	}
	
	msg.setFormat( m_MessageFormat.c_str() );

	unsigned long m_HeaderSize = 0;
	if( m_MessageFormat == MQFMT_RF_HEADER_2 )
	{
		m_HeaderSize = InjectJMSHeader( msg );
	}
	
	msg.write( buffer->size(), ( const char* )buffer->buffer() );

	// Put the buffer to the message queue
	msg.setMessageLength( ( size_t )( buffer->size() + m_HeaderSize ) );
	msg.setPersistence( MQPER_PERSISTENT );

	// reset format
	m_MessageFormat = MQFMT_STRING;      // character string format    

#ifdef WMQTOAPP_BACKUP
	if ( m_BackupQueueName.length() > 0 )
	{
	//put to backup and set syncpoint
		try
		{
			openBackupQueue();

			//ImqMessage backupMsg;
			//backupMsg.setFormat( msg.format() );

			ImqPutMessageOptions pmo;
			pmo.setOptions( MQPMO_FAIL_IF_QUIESCING );
			pmo.setSyncPointParticipation( true ); 

			//backupMsg.write( buffer->size(), ( const char* )buffer->buffer() );
			//msg.setExpiry( 100 ); 

			if ( !m_BackupQueue.put( msg, pmo ) )
			{
				stringstream errorMessage;
				int reasonCode = ( int )m_BackupQueue.reasonCode();

				errorMessage << "Put [" << m_BackupQueueName << "] ended with reason code : " << reasonCode;
				throw runtime_error( errorMessage.str() );
			}
		}
		catch( const std::exception& ex )
		{
			TRACE( "A " << typeid( ex ).name() << " exception has occured while putting the message to backup queue [" << ex.what() << "]" );
		}
		catch( ... )
		{
			TRACE( "An unknown exception has occured while putting the message to backup queue" );
		}
	}
#endif

	int attempts = 0;
	do
	{
		if ( attempts++ > 0 )
		{
			DEBUG( "Retry #" << attempts << " in 10 seconds ... " );

			sleep( 10 );

			// reopen the queue
			disconnect();			
			openQueue();
		}

		if ( m_UsePassedMessageId )
			msg.setMessageId( ImqBinary( m_MessageId.data(), MQ_MSG_ID_LENGTH ) );
				
		if ( m_UsePassedCorrelId )
			msg.setCorrelationId( ImqBinary( m_CorrelationId.data(), MQ_CORREL_ID_LENGTH ) );
		
		if ( m_UsePassedGroupId )
			msg.setGroupId( ImqBinary( m_GroupId.data(), MQ_GROUP_ID_LENGTH ) );

		if ( m_UsePassedAppName )
			msg.setPutApplicationName( m_ApplicationName.data() ); 

		if ( !m_Queue.put( msg, pmo ) ) 
		{
			stringstream errorMessage;
			string lclQueueName = ( char* )m_Queue.name();
			int reasonCode = ( int )m_Queue.reasonCode();
			
			errorMessage << "Put message to [" << lclQueueName << "] ended with reason code : " << reasonCode;
			if ( attempts >= 2 )
			{
				m_UsePassedMessageId = false;
				m_UsePassedCorrelId = false;
				m_UsePassedGroupId = false;
				m_UsePassedAppName = false;

				throw runtime_error( errorMessage.str() );
			}
			else
				TRACE( errorMessage.str() );
		}
		else
		{
			m_UsePassedMessageId = false;
			m_UsePassedCorrelId = false;
			m_UsePassedGroupId = false;
			m_UsePassedAppName = false;

			//save ids
			m_MessageId = Base64::encode( ( unsigned char * )( msg.messageId().dataPointer() ), 24 );
			DEBUG( "Message id : " << m_MessageId );
					
			m_CorrelationId = Base64::encode( ( unsigned char * )( msg.correlationId().dataPointer() ), 24 );
			DEBUG( "Correlation id : " << m_CorrelationId );
			
			m_GroupId = Base64::encode( ( unsigned char * )( msg.groupId().dataPointer() ), 24 );
			DEBUG( "Group id : " << m_GroupId );
		}
		
	} while ( ( m_Queue.completionCode() == MQCC_FAILED ) && ( attempts < 3 ) ); 

	DEBUG( "One message put" );
}

void WMqHelper::putGroupMessage( ManagedBuffer* buffer, const string& batchId, long messageSequence, bool isLast )
{
	DEBUG( "Putting one message in group" );
	ImqPutMessageOptions pmo;
	pmo.setOptions( MQPMO_NEW_MSG_ID | MQPMO_FAIL_IF_QUIESCING );
	
	WMqHelper::setSyncPointParticipation( pmo, 1 );
		
	ImqMessage msg;
	MQBYTE24 MY_GROUP_ID;
	
	memset( MY_GROUP_ID, 0, sizeof( MY_GROUP_ID ) );
	string::size_type grpIdLen = batchId.length();
	if ( grpIdLen > sizeof( MY_GROUP_ID ) )
		grpIdLen = sizeof( MY_GROUP_ID );

	memcpy( MY_GROUP_ID, batchId.c_str(), grpIdLen );
	
	ImqBinary grpId = WMqHelper::createBinary( ( void* )MY_GROUP_ID );
	msg.setMessageType( MQMT_DATAGRAM );
	
	//TODO Check if setSequenceNumber is neccessary
	msg.setSequenceNumber( messageSequence );
	if ( isLast )
	{
		DEBUG( "Last message in group" );
		msg.setMessageFlags( MQMF_LAST_MSG_IN_GROUP );
	}
	else
	{
		msg.setMessageFlags( MQMF_MSG_IN_GROUP );
	}

	if ( msg.setGroupId( grpId ) )
	{
		stringstream grpIdStr;
		grpIdStr << string( ( char* )( msg.groupId().dataPointer() ) );
		
		DEBUG( "Group ID set successfully to [" << grpIdStr.str() << "]" );
	}
	else
	{
		DEBUG( "Group ID could not be set" );
	}

	if ( buffer->size() == 0 )
		throw runtime_error( "Enqueing empty payload to WMQ." );

	putOne( ( unsigned char * )buffer->buffer(), buffer->size(), pmo, msg );
}

long WMqHelper::getGroupMessage( ManagedBuffer* groupMessageBuffer, const string& groupId, bool& isCleaningUp )
{
	long result = -1;
	ImqGetMessageOptions gmo;	
	// Wait for the next message ( or the consumer - this may run out of messages before the sender has a chance to put them )
	gmo.setOptions( MQGMO_LOGICAL_ORDER | MQGMO_FAIL_IF_QUIESCING | MQGMO_WAIT );
	gmo.setWaitInterval( MQWI_UNLIMITED );
	gmo.setMatchOptions( MQMO_MATCH_GROUP_ID );
	WMqHelper::setSyncPointParticipation( gmo, 1 );
	
	ImqMessage msg;

	MQBYTE24 MY_GROUP_ID;
		
	memset( MY_GROUP_ID, 0, sizeof( MY_GROUP_ID ) );
	string::size_type grpIdLen = groupId.length();
	if ( grpIdLen > sizeof( MY_GROUP_ID ) )
		grpIdLen = sizeof( MY_GROUP_ID );
	
	memcpy( MY_GROUP_ID, groupId.c_str(), grpIdLen );
	
	ImqBinary grpId = WMqHelper::createBinary( ( void* )MY_GROUP_ID );

	if ( msg.setGroupId( grpId ) )
	{
		stringstream grpIdStr;
		grpIdStr << string( ( char* )( msg.groupId().dataPointer() ) ) << "\0" ;
		
		DEBUG( "Group ID set successfully to [" << grpIdStr.str() << "]" );
	}
	else
	{
		stringstream errorMessage;
		errorMessage << "Unable to set group ID to [" << groupId << "] for the for deque operation ";

		TRACE( errorMessage.str() );
		throw runtime_error( errorMessage.str() );
	}
	msg.setMessageId();
    msg.setCorrelationId();
	
	result = getOne( groupMessageBuffer, gmo, msg, isCleaningUp );
	
	//set group characteristics (other get characteristic are set in getOne(), including m_GroupId)
	if ( ( msg.messageFlags() & MQMF_LAST_MSG_IN_GROUP ) == MQMF_LAST_MSG_IN_GROUP )
		m_LastInGroup = true;
	m_GroupSequence =  msg.sequenceNumber();

	return result;
}

long WMqHelper::getOne( ManagedBuffer* buffer, ImqGetMessageOptions& gmo )
{
	m_UseSyncpoint = ( gmo.syncPointParticipation() == TRUE );
				
	ImqMessage msg;
	return getOne( buffer, gmo, msg );
}

long WMqHelper::getOne( ManagedBuffer* buffer, bool syncpoint, bool keepJMSHeader )
{
	ImqGetMessageOptions gmo;	// Get message options

	gmo.setOptions( MQGMO_WAIT | MQGMO_FAIL_IF_QUIESCING );
	gmo.setWaitInterval( 15000 );  /* 15 second limit for waiting     */

	if ( syncpoint )
		gmo.setSyncPointParticipation( true );
	
	m_UseSyncpoint = syncpoint;
		
	ImqMessage msg;
	return getOne( buffer, gmo, msg, false, keepJMSHeader );
}

long WMqHelper::getOne( unsigned char* buffer, size_t maxSize, bool syncpoint )
{
	DEBUG( "Getting one message - wrapper" );
	ImqGetMessageOptions gmo;	// Get message options

	gmo.setOptions( MQGMO_WAIT | MQGMO_FAIL_IF_QUIESCING );
	gmo.setWaitInterval( 15000 );  /* 15 second limit for waiting     */
	
	if ( syncpoint )
		gmo.setSyncPointParticipation( true );
	
	m_UseSyncpoint = syncpoint;
		
	ImqMessage msg;
	WorkItem< ManagedBuffer > managedBuffer( new ManagedBuffer( buffer, ManagedBuffer::Ref, maxSize ) );
	long result = getOne( managedBuffer.get(), gmo, msg );
		
	DEBUG( "Got one message - wrapper" );	
	return result;
}

long WMqHelper::getOne( unsigned char* buffer, size_t maxSize, ImqGetMessageOptions& gmo )
{
	ImqMessage msg;
	WorkItem< ManagedBuffer > managedBuffer( new ManagedBuffer( buffer, ManagedBuffer::Ref, maxSize ) );
	long result = getOne( managedBuffer.get(), gmo, msg );
	return result;
}

long WMqHelper::getOne( unsigned char* buffer, size_t maxSize, ImqGetMessageOptions& gmo, ImqMessage& msg )
{
	WorkItem< ManagedBuffer > managedBuffer( new ManagedBuffer( buffer, ManagedBuffer::Ref, maxSize ) );
	long result = getOne( managedBuffer.get(), gmo, msg );
	return result;
}

long WMqHelper::peek( const string& queue, bool first )
{
	DEBUG( "Peeking queue [" << queue << "]" );
	setOpenQueueOptions( MQOO_BROWSE );

	// Get message options
	ImqGetMessageOptions gmo;		

	// wait for new messages          
	if ( first )
		gmo.setOptions( MQGMO_ALL_MSGS_AVAILABLE | MQGMO_BROWSE_FIRST | MQGMO_WAIT |
			MQGMO_FAIL_IF_QUIESCING | MQGMO_ACCEPT_TRUNCATED_MSG );
	else
		gmo.setOptions( MQGMO_ALL_MSGS_AVAILABLE | MQGMO_BROWSE_NEXT | MQGMO_WAIT | 
			MQGMO_FAIL_IF_QUIESCING | MQGMO_ACCEPT_TRUNCATED_MSG );
                  
	// 15 second limit for waiting    
	gmo.setWaitInterval( 15000 ); 

	try
	{
		openQueue( queue );
	}
	catch( const std::exception& ex )
	{
		stringstream errorMessage;
		errorMessage << "Unable to peek for messages in WMQ. [" << ex.what() << "]";
		TRACE( errorMessage.str() );
		throw runtime_error( errorMessage.str() );
	}

	// get first 10 bytes of the message. 
	// this call returns 0 if a message is available, sets the last message id, 
	// last correlid and the size of the message 
	unsigned char buffer[ 10 ];
	long result = -1;
	
	try
	{
		ImqMessage msg;
		WorkItem< ManagedBuffer > managedBuffer( new ManagedBuffer( buffer, ManagedBuffer::Ref, 10 ) );
		result = getOne( managedBuffer.get(), gmo, msg );
	}
	catch( const MqException& ex )
	{
		// ignore trucated message error
		if( ex.getReasonCode() == MQRC_TRUNCATED_MSG_FAILED )
		{
			DEBUG( "Truncated message" );
			return 0;
		}
		throw runtime_error( ex.getMessage() );
	}
	
	DEBUG( "Peeked" );
	return result;
}

long WMqHelper::getOne( ManagedBuffer* buffer, ImqGetMessageOptions& gmo, ImqMessage& msg, bool getForClean, bool keepJMSHeader )
{
	//TODO check backout count
	int attempts = 0;
	bool isBrowsing = ( m_OpenOption & MQOO_BROWSE ||  gmo.options() & MQGMO_BROWSE_FIRST  || gmo.options() & MQGMO_BROWSE_NEXT );

	if ( !isBrowsing )
		setOpenQueueOptions( MQOO_INPUT_AS_Q_DEF );
	DEBUG( "Getting one message" );

	long result = 0;
	
	// on ref buffers don't resize
	if ( buffer->type() != ManagedBuffer::Adopt )
		msg.useEmptyBuffer( ( char* )( buffer->buffer() ), ( size_t )buffer->size() );

#ifndef FMAMC
	msg.setFormat( MQFMT_STRING );      // character string format    
#endif

	try
	{
		openQueue();
	}
	catch( const std::exception& ex )
	{
		stringstream errorMessage;
		errorMessage << "Unable to get a message from WMQ. Open queue reason [" << ex.what() << "]";
		TRACE( errorMessage.str() );
		throw runtime_error( errorMessage.str() );
	}

	do
	{
		result = 0;
		
		DEBUG( "Setting message ids used for retrieval ... " );
		MQLONG getMatchOptions = gmo.matchOptions();

		if ( m_UsePassedMessageId )
		{
			msg.setMessageId( ImqBinary( m_MessageId.data(), MQ_MSG_ID_LENGTH ) );
			getMatchOptions |= MQMO_MATCH_MSG_ID;
		}
				
		if ( m_UsePassedCorrelId )
		{
			msg.setCorrelationId( ImqBinary( m_CorrelationId.data(), MQ_CORREL_ID_LENGTH ) );
			getMatchOptions |= MQMO_MATCH_CORREL_ID;
		}
		
		if ( m_UsePassedGroupId )
		{
			msg.setGroupId( ImqBinary( m_GroupId.data(), MQ_GROUP_ID_LENGTH ) );
			getMatchOptions |= MQMO_MATCH_GROUP_ID;
		}

		gmo.setMatchOptions( getMatchOptions );

		DEBUG( "Calling get message... Buffer size : " << buffer->size() );
		if ( m_Queue.get( msg, gmo ) ) 
		{
			DEBUG( "Message got from queue." );

			m_MessageLength = msg.totalMessageLength();
			DEBUG( "Message length : " << m_MessageLength << " / Buffer size : " << buffer->size() );

			m_UsePassedMessageId = false;
			m_UsePassedCorrelId = false;
			m_UsePassedGroupId = false;
			
			m_LastInGroup = true;
			long messageFlags = msg.messageFlags();

			if ( ( ( messageFlags & MQMF_MSG_IN_GROUP ) == MQMF_MSG_IN_GROUP ) && 
				( ( msg.messageFlags() & MQMF_LAST_MSG_IN_GROUP ) != MQMF_LAST_MSG_IN_GROUP ) )
				m_LastInGroup = false;

			DEBUG( "Message flags : " << msg.messageFlags() );   
			
			// save msgid, correlid
			m_MessageId = Base64::encode( ( unsigned char * )( msg.messageId().dataPointer() ), 24 );
			DEBUG( "Message id : [" << m_MessageId << "]" );
			
			m_CorrelationId = Base64::encode( ( unsigned char * )( msg.correlationId().dataPointer() ), 24 );
			DEBUG( "Correlation id : [" << m_CorrelationId << "]" );
			
			m_GroupId = Base64::encode( ( unsigned char * )( msg.groupId().dataPointer() ), 24 );
			DEBUG( "Group id : [" << m_GroupId << "]" );

			m_DataOffset = 0;
			m_MessagePutDate = msg.putDate();
			m_MessagePutTime = msg.putTime();
			m_ApplicationName = msg.putApplicationName();

			if ( !isBrowsing )
			{
				// if setbackupqueue is used, save a copy of the message we got to the backup queue
				if ( m_SaveBackup && ( m_BackupQueueName.length() > 0 ) )
				{
					try
					{
						openBackupQueue();

						ImqPutMessageOptions pmo;
						pmo.setOptions( MQPMO_FAIL_IF_QUIESCING );
						pmo.setSyncPointParticipation( true ); 

						if ( !m_BackupQueue.put( msg, pmo ) )
						{
							stringstream errorMessage;
							int reasonCode = ( int )m_BackupQueue.reasonCode();
							
							errorMessage << "Put [" << m_BackupQueueName << "] ended with reason code : " << reasonCode;
							throw runtime_error( errorMessage.str() );
						}
					}
					catch( const std::exception& ex )
					{
						TRACE( "A " << typeid( ex ).name() << " exception has occured while putting the message to backup queue [" << ex.what() << "]" );
					}
					catch( ... )
					{
						TRACE( "An unknown exception has occured while putting the message to backup queue" );
					}
				}

				m_DataOffset = StripJMSHeader( msg, m_MessageFormat );
			}
			
			if ( buffer->type() == ManagedBuffer::Adopt )
			{
				if ( keepJMSHeader )
				{
					buffer->allocate( m_MessageLength + 1 );
					buffer->copyFrom( reinterpret_cast <unsigned char*> ( msg.bufferPointer() ), m_MessageLength );
				}
				else
				{
					m_MessageLength -= m_DataOffset;
					buffer->allocate( m_MessageLength + 1 );
					buffer->copyFrom( reinterpret_cast <unsigned char*> ( msg.bufferPointer() + m_DataOffset ), m_MessageLength );
				}
				DEBUG( "Buffer reallocated to fit the message [" << m_MessageLength << "] bytes" );
			}
			
			m_MessageType = msg.messageType();
			m_ReplyOptions = msg.report();
			m_ReplyQueue = msg.replyToQueueName();
			m_ReplyQueueManager = msg.replyToQueueManagerName();	
			m_Feedback = msg.feedback();
			if ( m_DataOffset == 0 )
				m_MessageFormat = msg.format();
		}
		else 
		{
			long reasonCodeForGet = m_Queue.reasonCode();
			// report reason, if any     
			if ( reasonCodeForGet == MQRC_NO_MSG_AVAILABLE ) 
			{
				result = -1;
				
				if ( m_UsePassedMessageId )
					DEBUG( "No message matching passed messageid [" << Base64::encode( ( unsigned char * )( msg.messageId().dataPointer() ), 24 ) << "]" );
				if ( m_UsePassedCorrelId )
					DEBUG( "No message matching passed correlid [" << Base64::encode( ( unsigned char * )( msg.correlationId().dataPointer() ), 24 ) << "]" );
				if ( m_UsePassedGroupId )
					DEBUG( "No message matching passed groupid [" << Base64::encode( ( unsigned char * )( msg.groupId().dataPointer() ), 24 ) << "]" );

				DEBUG( "... no message" );
				break;
			}
			else 
			{
				DEBUG( "Get failed. Reason [" << reasonCodeForGet << "]" );

				long messageLength = msg.totalMessageLength();
				DEBUG( "Message length : " << messageLength << " / Buffer size : " << buffer->size() );
			
				string messageId = Base64::encode( ( unsigned char * )( msg.messageId().dataPointer() ), 24 );
				DEBUG( "Message id : [" << messageId << "]" );
			
				string correlationId = Base64::encode( ( unsigned char * )( msg.correlationId().dataPointer() ), 24 );
				DEBUG( "Correlation id : [" << correlationId << "]" );
			
				string groupId = Base64::encode( ( unsigned char * )( msg.groupId().dataPointer() ), 24 );
				DEBUG( "Group id : [" << groupId << "]" );
			
				if( attempts <= 3 )
				{
					result = -3;
					DEBUG( "Attempting reopen of queue" );
					disconnect();
					openQueue();
				}
				else
				{
					m_UsePassedMessageId = false;
					m_UsePassedCorrelId = false;
					m_UsePassedGroupId = false;

					stringstream errorMessage;
					string lclQueueName = ( char* )m_Queue.name();
					
					errorMessage << "Get message from [" << lclQueueName << "] ended with reason code : " << reasonCodeForGet;
					throw MqException( errorMessage.str(), reasonCodeForGet );
				}
			}
		}

		// abort message ; if in browse mode, we cannot commit get
		if ( ( m_AutoAbandon > 0 ) && ( msg.backoutCount() >= m_AutoAbandon ) && !isBrowsing )
		{
			//TODO Move to deadletter
			stringstream errorMessage;
			errorMessage << "Message backout of " << msg.backoutCount() << " exceeded backout threshold. AutoAbandon set to " << m_AutoAbandon;
			
			TRACE( errorMessage.str() );
			
			try
			{
				putToDeadLetterQueue( msg );

				if ( ( !getForClean ) && m_LastInGroup )
				{
					closeBackoutQueue();
					commit();
				}
			}
			catch( const std::exception& ex )
			{
				string messageFile = "";

				stringstream errorMessageDL;
				errorMessageDL << "Unable to put message to the dead letter queue [" << ex.what() << "]. The message is saved to [" << messageFile << "]";
				commit();

				throw MqException( errorMessageDL.str(), m_Queue.reasonCode() );
			}

			result = -2;
			break;
		}

		if ( result == 0 )
			break;
	
		attempts++;
	}while ( true );
	
	DEBUG( "Get one message - done" );
	return result;
}

unsigned int WMqHelper::StripJMSHeader( ImqMessage& msg, string& msgFormat ) const
{
	if ( !msg.formatIs( MQFMT_RF_HEADER_2 ) )
	{
		DEBUG( "The message doesn't have an RF_HEADER_2 - nonJMS application :)" );
		return 0;
	}

	DEBUG( "The message has RF_HEADER_2 - JMS application :(" );
	
	// read the header and msd length
	unsigned int headerLength = sizeof( MQRFH2 );
	unsigned int returnValue = 0;
	bool conversionNeeded = ( msg.encoding() != MQENC_NATIVE );
	MQLONG nextHeaderLength = 0;

	char* headerBuffer = new char[ headerLength + 1 ];
	try
	{
		if ( msg.read( headerLength, headerBuffer ) )
		{
			headerBuffer[ headerLength ] = 0;
			MQRFH2 dummy = {MQRFH2_DEFAULT};
			memcpy( &dummy, headerBuffer, headerLength );

			DEBUG( "RFH2 header : " )

			//TODO : check requirements for conversion
			if ( ( dummy.StrucLength < 0 ) || ( dummy.Version < 0 ) || ( dummy.Version > MQRFH_VERSION_2 ) )
				conversionNeeded = true;

			// if the encoding is not the native encoding, convert longs
			if ( conversionNeeded )
			{
				dummy.StrucLength = Convert::ChangeEndian( dummy.StrucLength );	

#ifdef DEBUG_ENABLED
				dummy.CodedCharSetId = Convert::ChangeEndian( dummy.CodedCharSetId );
				dummy.Encoding = Convert::ChangeEndian( dummy.Encoding );
				dummy.Flags = Convert::ChangeEndian( dummy.Flags );
				dummy.NameValueCCSID = Convert::ChangeEndian( dummy.NameValueCCSID );
				dummy.Version = Convert::ChangeEndian( dummy.Version );
#endif
			}

			DEBUG( "\tCCSID [" << dummy.CodedCharSetId << "]" );
			DEBUG( "\tEncoding [" << dummy.Encoding << "]" );
			DEBUG( "\tFlags [" << dummy.Flags << "]" );
			DEBUG( "\tFormat [" << dummy.Format << "]" );
			DEBUG( "\tNameValueCCSID [" << dummy.NameValueCCSID << "]" );
			DEBUG( "\tStrucId [" << dummy.StrucId << "]" );
			DEBUG( "\tStucture length [" << dummy.StrucLength << "]" );
			DEBUG( "\tVersion [" << dummy.Version << "]" );

			msgFormat = dummy.Format;

			returnValue = dummy.StrucLength;
			delete[] headerBuffer;

			// read remaining headers
			headerBuffer = new char[ ( returnValue - headerLength ) + 1 ];
			char headerStrucId[ 5 ];
			unsigned int headerOffset = 0;

			// read next headers
			if ( msg.read( returnValue - headerLength, headerBuffer ) )
			{
				while( headerOffset < returnValue - headerLength )
				{
					nextHeaderLength = *( reinterpret_cast< MQLONG* >( headerBuffer + headerOffset ) );
					if( conversionNeeded )
						nextHeaderLength = Convert::ChangeEndian( nextHeaderLength );
					memcpy( headerStrucId, headerBuffer + headerOffset + sizeof( MQLONG ), 4 );
					headerStrucId[ 4 ] = 0;

					DEBUG( "Next header has a total length of [" << nextHeaderLength << 
						"] : [" << string( headerBuffer + headerOffset + sizeof( MQLONG ), nextHeaderLength ) << "]" );
					headerOffset += nextHeaderLength + sizeof( MQLONG );
				}
			}
			else
			{
				DEBUG( "Unable to read all headers ... [" << msg.reasonCode() << "]" );
			}
		}
		else
		{
			stringstream errorMessage;
			errorMessage << "RFH2 header couldn't be read. Reason code [" << msg.reasonCode() << "]";
			TRACE( errorMessage.str() );
			throw runtime_error( errorMessage.str() );
		}
		if ( headerBuffer != NULL )
			delete[] headerBuffer;
		headerBuffer = NULL;
	}
	catch( ... )
	{
		if ( headerBuffer != NULL )
			delete[] headerBuffer;
		throw;
	}

	DEBUG( "RFH2 header removed from message." );
	return returnValue;
}

unsigned int WMqHelper::InjectJMSHeader( ImqMessage& msg )
{
	msg.setFormat( MQFMT_RF_HEADER_2 );
	MQRFH2 rf2Header = {MQRFH2_DEFAULT};

	if ( m_QueueManagerCCSID == 0 )
	{
		MQLONG qmgrCCSID = 0;
		m_QueueManager.characterSet( qmgrCCSID );
		m_QueueManagerCCSID = qmgrCCSID;
		
		DEBUG( "Queue manager CCSID is [" << m_QueueManagerCCSID << "]" );
	}

	if( m_QueueManagerPlatform == MQPL_AIX )
	{
		rf2Header.Encoding = 273;
		msg.setEncoding( 273 );
		msg.setCharacterSet( 819 );
	}
	else if( m_QueueManagerPlatform == MQPL_UNIX )
	{
		//TODO : check encodings for LINUX
		rf2Header.Encoding = 546;
		msg.setEncoding( 546 );
		msg.setCharacterSet( 437 );
	}
	else
	{
		rf2Header.Encoding = 546;
		msg.setEncoding( 546 );
		msg.setCharacterSet( 437 );
	}

	rf2Header.CodedCharSetId = 1208;
	
	// setup MCD header in HRF2 header
	string mcdData( "<mcd><Msd>jms_text</Msd></mcd>  " );
	MQLONG mcdLength = mcdData.length();
	
	// setup JMS header in HRF2 header
	time_t ltime;
	time( &ltime ); 

	stringstream jmsDataStr;
	jmsDataStr << "<jms><Dst>queue://" << m_QueueManager.name() << "/" << m_Queue.name() << "</Dst><Tms>" 
		<< ltime << "</Tms><Dlv>2</Dlv></jms>";
	MQLONG jmsLength = jmsDataStr.str().length();
	unsigned int remainder = jmsLength % sizeof( MQLONG );
	if ( remainder )
	{
		jmsDataStr << string( 4 - remainder, ' ' );
		jmsLength += ( 4 - remainder );
	}
	string jmsData = jmsDataStr.str();

	// setup USR header in HRF2 header	
	stringstream usrDataStr;
	MQLONG usrLength = 0;

	int headerCount = 2;

	if( m_ReplyUsrData.length() > 0 )
	{
		usrDataStr << "<usr>" << m_ReplyUsrData << "</usr>";
		usrLength = usrDataStr.str().length();

		unsigned int remainderUL = usrLength % sizeof( MQLONG );
		if ( remainderUL )
		{
			usrDataStr << string( 4 - remainderUL, ' ' );
			usrLength += ( 4 - remainderUL );
		}
		headerCount++;
	}
	string usrData = usrDataStr.str();

	// write to message
	memcpy( rf2Header.Format, MQFMT_STRING, 8 );

	unsigned int returnValue = sizeof( MQRFH2 ) + mcdLength + jmsLength + usrLength + headerCount*sizeof( MQLONG );
	rf2Header.StrucLength = returnValue;

	if ( m_ConversionNeeded )
	{
		rf2Header.StrucLength = Convert::ChangeEndian( rf2Header.StrucLength );	
		rf2Header.CodedCharSetId = Convert::ChangeEndian( rf2Header.CodedCharSetId );
#ifdef MQ_64_BIT
		rf2Header.Encoding = Convert::ChangeEndian( (MQINT64)rf2Header.Encoding );
#else 
		rf2Header.Encoding = Convert::ChangeEndian( rf2Header.Encoding );
#endif
		rf2Header.Flags = Convert::ChangeEndian( rf2Header.Flags );
		rf2Header.NameValueCCSID = Convert::ChangeEndian( rf2Header.NameValueCCSID );
		rf2Header.Version = Convert::ChangeEndian( rf2Header.Version );	
	
		msg.write( sizeof( rf2Header ), ( const char* )&rf2Header );

		mcdLength = Convert::ChangeEndian( mcdLength );
		msg.write( sizeof( MQLONG ), ( const char * )&mcdLength );
		msg.write( mcdData.length(), mcdData.c_str() );

		jmsLength = Convert::ChangeEndian( jmsLength );
		msg.write( sizeof( MQLONG ), ( const char * )&jmsLength );
		msg.write( jmsData.length(), jmsData.c_str() );

		if ( usrLength > 0 )
		{
			usrLength = Convert::ChangeEndian( usrLength );
			msg.write( sizeof( MQLONG ), ( const char * )&usrLength );
			msg.write( usrData.length(), usrData.c_str() );
		}
	}
	else
	{
		msg.write( sizeof( rf2Header ), ( const char* )&rf2Header );
		msg.write( sizeof( MQLONG ), ( const char * )&mcdLength );
		msg.write( mcdData.length(), mcdData.c_str() );
		msg.write( sizeof( MQLONG ), ( const char * )&jmsLength );
		msg.write( jmsData.length(), jmsData.c_str() );
		if ( usrLength > 0 )
		{
			msg.write( sizeof( MQLONG ), ( const char * )&usrLength );
			msg.write( usrData.length(), usrData.c_str() );
		}
	}

	return returnValue;
}

void WMqHelper::setChannel( )
{
	DEBUG( "Setting channel" );
	
	// no connection string passed = server mode
	if( m_ConnectionString.length() <= 0 )
	{
		DEBUG( "Channel empty set" );
		return;
	}
		
	ImqString strParse( m_ConnectionString.data() );
	ImqString strToken ;

	m_Channel.setHeartBeatInterval( ( int )1 );
	m_Channel.setMaximumMessageLength( 26214400 );

    // Break down the channel definition,
    // which is of the form "channel-name/transport-type/connection-name".
	if ( strParse.cutOut( strToken, '/' ) ) 
	{
		m_Channel.setChannelName( strToken );
		DEBUG( "Channel name : [" << strToken << "]" );
		
		if ( strParse.cutOut( strToken, '/' ) ) 
		{
			// Interpret the transport type.
			if ( strToken.upperCase() == ( ImqString )"LU62" ) 
			{
				m_Channel.setTransportType( MQXPT_LU62 );
			}
			if ( strToken.upperCase() == ( ImqString )"NETBIOS" ) 
			{
				m_Channel.setTransportType( MQXPT_NETBIOS );
			}
			if ( strToken.upperCase() == ( ImqString )"SPX" ) 
			{
				m_Channel.setTransportType( MQXPT_SPX );
			}
			if ( strToken.upperCase() == ( ImqString )"TCP" ) 
			{
				DEBUG( "Transport type : TCP" );
				m_Channel.setTransportType( MQXPT_TCP );
			}

			// Establish the connection name.
			if ( strParse.cutOut( strToken ) ) 
			{
				DEBUG( "Connection name : [" << strToken << "]" );
				m_Channel.setConnectionName( strToken );
			}
		}
	}
	if( m_SSLCypherSpec.length() != 0 )
	{
		DEBUG( "Setting SslCipherSpecification to : [" << m_SSLCypherSpec << "]" );
		m_Channel.setSslCipherSpecification( m_SSLCypherSpec.c_str() );
	}
	if( m_SSLPeerName.length() != 0 )
	{
		DEBUG( "Setting SslPeerName to : [" << m_SSLPeerName << "]" );
		m_Channel.setSslPeerName( m_SSLPeerName.c_str() );
	}	
    m_QueueManager.setChannelReference( m_Channel );
    
    DEBUG( "Channel set" );
}

void WMqHelper::setOpenQueueOptions( const long& openOptions )
{
	m_OpenOption = openOptions | MQOO_INQUIRE;
}

bool WMqHelper::commit()
{
	DEBUG( "Commit" );
	return ( m_QueueManager.commit() > 0 );
}

bool WMqHelper::rollback()
{
	DEBUG( "Rollback" );
	return ( m_QueueManager.backout() > 0 );
}

void WMqHelper::clearSSLOptions()
{
	m_SSLCypherSpec = "";
	m_SSLKeyRepository = "";
	m_SSLPeerName = "";

#if !defined( AIX ) && !defined( LINUX )
	if( m_QueueManager.keyRepository().length() > 0 )
		m_QueueManager.setKeyRepository( "" );
#endif
}

void WMqHelper::clearMessages()
{
	ImqGetMessageOptions gmo;		

	// wait for new messages          
	gmo.setOptions( MQGMO_NO_WAIT | MQGMO_FAIL_IF_QUIESCING | MQGMO_ACCEPT_TRUNCATED_MSG );
	gmo.setSyncPointParticipation( false );

	// get first 10 bytes of the message. 
	// this call returns 0 if a message is available, sets the last message id, 
	// last correlid and the size of the message 
	unsigned char buffer[ 10 ];
	long result = -1;
	
	do
	{
		ImqMessage msg;
		WorkItem< ManagedBuffer > managedBuffer( new ManagedBuffer( buffer, ManagedBuffer::Ref, 10 ) );
		result = getOne( managedBuffer.get(), gmo, msg );
	} while ( result == 0 );

}

void WMqHelper::setSyncPointParticipation( ImqPutMessageOptions& pmo, unsigned char value )
{
	pmo.setSyncPointParticipation( value );
}
		
void WMqHelper::setSyncPointParticipation( ImqGetMessageOptions& gmo, unsigned char value )
{
	gmo.setSyncPointParticipation( value );
}

ImqBinary WMqHelper::createBinary( void* value )
{
	return ImqBinary( ( unsigned char* )value, 24 );
}

//XA coordination
void WMqHelper::beginXATransaction()
{
	if ( m_QueueManager.begin() == false )
	{
		stringstream errorMessage;
		errorMessage << "Begin failed. Reason code : " << m_QueueManager.reasonCode();

		throw runtime_error( errorMessage.str() );
	}
}

long WMqHelper::getQueueDepth( const string& queueName )
{
	long crtDepth = -1;
	crtDepth = m_Queue.currentDepth();

	if ( m_Queue.reasonCode() != MQRC_NONE )
	{
		TRACE( "Failed to get queue depth [" << m_Queue.reasonCode() << "]" );
	}
	return crtDepth;
}

long WMqHelper::getOpenInputCount( string queueName )
{
	long crtOpenInputCount = -1;
#ifdef WMQ_7
	crtOpenInputCount = m_Queue.openInputCount();
#else
	if( !m_Queue.openInputCount( crtOpenInputCount ) )
	{
		stringstream errorMessage;
		errorMessage << "Get open Input Count ended with code : [" << m_Queue.reasonCode() << "]";
		throw runtime_error( errorMessage.str() );
	}
#endif
	return crtOpenInputCount;
}

long WMqHelper::getOpenOutputCount( string queueName )
{
	long crtOpenOutputCount = -1;
#ifdef WMQ_7
	crtOpenOutputCount = m_Queue.openInputCount();
#else
	if( !m_Queue.openOutputCount( crtOpenOutputCount ) )
	{
		stringstream errorMessage;
		errorMessage << "Get open Output Count ended with code : [" << m_Queue.reasonCode() << "]";
		throw runtime_error( errorMessage.str() );
	}
#endif
	return crtOpenOutputCount;
}

long WMqHelper::ParseReplyOptions( const TransportReplyOptions& options )
{
	long returnedOptions = MQRO_NONE;

	// empty string = no options
	if ( options.getSize() == 0 || options[0] == NONE )
		return returnedOptions;

	for( int i = 0; i < options.getSize(); i++ )
	{
		// match agains known options
		if ( options[ i ] == TransportReplyOptions::RO_COD )
			returnedOptions += MQRO_COD;
		else if ( options[ i ] == TransportReplyOptions::RO_COA )
			returnedOptions += MQRO_COA;
		else if ( options[ i ] == TransportReplyOptions::RO_NAN )
			returnedOptions += MQRO_NAN;
		else if ( options[ i ] == TransportReplyOptions::RO_PAN )
			returnedOptions += MQRO_PAN;
		else if ( options[ i ] == TransportReplyOptions::RO_COPY_MSG_ID_TO_CORREL_ID )
			returnedOptions += MQRO_COPY_MSG_ID_TO_CORREL_ID;
		else if ( options[ i ] == TransportReplyOptions::RO_PASS_CORREL_ID )
			returnedOptions += MQRO_PASS_CORREL_ID;
	}

	return returnedOptions;
}

TransportReplyOptions WMqHelper::getLastReplyOptions() const
{
	TransportReplyOptions replyOptions;
	if( ( m_ReplyOptions & MQRO_COD ) == MQRO_COD )
		replyOptions.addReplyOption( TransportReplyOptions::RO_COD );
	else if( ( m_ReplyOptions & MQRO_COA ) == MQRO_COA )
		replyOptions.addReplyOption( TransportReplyOptions::RO_COA );
	else if( ( m_ReplyOptions & MQRO_NAN ) == MQRO_NAN )
		replyOptions.addReplyOption( TransportReplyOptions::RO_NAN );
	else if( ( m_ReplyOptions & MQRO_PAN )== MQRO_PAN )
		replyOptions.addReplyOption( TransportReplyOptions::RO_PAN );
	else if( ( m_ReplyOptions & MQRO_COPY_MSG_ID_TO_CORREL_ID ) == MQRO_COPY_MSG_ID_TO_CORREL_ID )
		replyOptions.addReplyOption( TransportReplyOptions::RO_COPY_MSG_ID_TO_CORREL_ID );
	else if( ( m_ReplyOptions & MQRO_PASS_CORREL_ID ) == MQRO_PASS_CORREL_ID )
		replyOptions.addReplyOption( TransportReplyOptions::RO_PASS_CORREL_ID );

	return replyOptions;
}

TransportHelper::TRANSPORT_MESSAGE_TYPE WMqHelper::ToTransportMessageType( long messageType )
{
	if( messageType == MQMT_DATAGRAM )
		return TransportHelper::TMT_DATAGRAM;
	if( messageType == MQMT_REPLY )
		return TransportHelper::TMT_REPLY;
	if( messageType == MQMT_REQUEST )
		return TransportHelper::TMT_REQUEST;
	if( messageType == MQMT_REPORT )
		return TransportHelper::TMT_REPORT;
	throw logic_error(" Invalid message type" );
}

void WMqHelper::setMessageFormat( const string& format )
{
	if ( format == TMT_STRING )
		m_MessageFormat = MQFMT_STRING; 
	else
		if ( format == TMT_RF_HEADER_2 )
			m_MessageFormat == MQFMT_RF_HEADER_2;
		else
			if ( format == TMT_NONE )
				m_MessageFormat = MQFMT_NONE;
			else
				throw logic_error( "Unknown message format" );
}

string WMqHelper::getLastMessageFormat() const
{
	if ( m_MessageFormat == MQFMT_RF_HEADER_2 )
		return TMT_RF_HEADER_2;
	else
		if ( m_MessageFormat == MQFMT_NONE )
			return TMT_NONE;
		else
			return TMT_STRING;
}

void WMqHelper::putSAAmessage( const TransportReplyOptions& replyOptions, const string& replyQueue, ManagedBuffer* buffer, const string& batchId, long messageSequence, bool isLast )
{
	DEBUG( "Putting one message in group" );
	ImqPutMessageOptions pmo;
	pmo.setOptions( MQPMO_NEW_MSG_ID | MQPMO_FAIL_IF_QUIESCING );

	WMqHelper::setSyncPointParticipation( pmo, 1 );

	ImqMessage msg;
	MQBYTE24 MY_GROUP_ID;

	memset( MY_GROUP_ID, 0, sizeof( MY_GROUP_ID ) );
	string::size_type grpIdLen = batchId.length();
	if ( grpIdLen > sizeof( MY_GROUP_ID ) )
		grpIdLen = sizeof( MY_GROUP_ID );

	memcpy( MY_GROUP_ID, batchId.c_str(), grpIdLen );

	ImqBinary grpId = WMqHelper::createBinary( ( void* )MY_GROUP_ID );
	msg.setMessageType( MQMT_DATAGRAM );

	//TODO Check if setSequenceNumber is neccessary
	msg.setSequenceNumber( messageSequence );
	if ( isLast )
	{
		DEBUG( "Last message in group" );
		msg.setMessageFlags( MQMF_LAST_MSG_IN_GROUP );
	}
	else
	{
		msg.setMessageFlags( MQMF_MSG_IN_GROUP );
	}

	if ( msg.setGroupId( grpId ) )
	{
		stringstream grpIdStr;
		grpIdStr << string( ( char* )( msg.groupId().dataPointer() ) );

		DEBUG( "Group ID set successfully to [" << grpIdStr.str() << "]" );
	}
	else
	{
		DEBUG( "Group ID could not be set" );
	}

	if ( buffer->size() == 0 )
		throw runtime_error( "Enqueing empty payload to WMQ." );

	//SAA-MQHA way, MQMT_DATAGRAM message for ACK, NACK request
	if( replyOptions.optionsSet( ) )
	{
		msg.setMessageType( MQMT_REQUEST );
		msg.setReport( ParseReplyOptions( replyOptions ) );
		msg.setReplyToQueueManagerName( m_QueueManager.name());
		msg.setReplyToQueueName( m_ReplyQueue.data() );
	}//SAA-MQHA way

	putOne( ( unsigned char * )buffer->buffer(), buffer->size(), pmo, msg );
}

void WMqHelper::setConnectOptions( MQLONG connectOptions )
{
	m_ConnectOptions = connectOptions;
}