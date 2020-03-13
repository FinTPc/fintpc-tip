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

#include <ctime>
#include <iostream>
#include <sstream>

#include <boost/scoped_ptr.hpp>

#include <activemq/library/ActiveMQCPP.h>
#include <activemq/core/ActiveMQConnection.h>
#include <activemq/core/MessageDispatchChannel.h>
#include <activemq/core/ActiveMQConnectionFactory.h>
#include <activemq/core/ActiveMQSession.h>
#include <activemq/core/ActiveMQProducer.h>
#include <activemq/core/ActiveMQSession.h>
#include <activemq/commands/ActiveMQQueue.h>
#include <activemq/commands/ActiveMQMessage.h>
#include <activemq/commands/ActiveMQTextMessage.h>
#include <activemq/commands/ActiveMQBytesMessage.h>
#include <activemq/core/PrefetchPolicy.h>
#include <activemq/core/RedeliveryPolicy.h>
#include <cms/InvalidSelectorException.h>

#include "AmqHelper.h"
#include "Base64.h"
#include "Collections.h"
#include "StringUtil.h"

#ifndef WIN32
	#include <unistd.h>
#else
	#include <windows.h>
	#define sleep(x) Sleep( (x)*1000 )
#endif

using namespace cms;
using namespace activemq::core;
using namespace FinTP;

using boost::scoped_ptr;
using activemq::commands::ActiveMQQueue;
using activemq::commands::ActiveMQTextMessage;
using activemq::commands::ActiveMQBytesMessage;

#define NO_THROW( X ) do { try { X; } catch ( ... ) {} } while (0);
#define TEST_SESSION  do { if ( m_Session == NULL ) throw logic_error("NULL Session at " + string(__FILE__) + ":" + StringUtil::ToString(__LINE__)); } while (0);
#define MQRO_NONE 0

const string AmqHelper::FINTPGROUPID = "FinTPGroupId";
const string AmqHelper::FINTPGROUPSEQ = "FinTPGroupSeq";
const string AmqHelper::FINTPLASTINGROUP = "FinTPLastInGroup";

class ExportedTransportObject ActiveMQCPPLibraryManager
{
	private:
		ActiveMQCPPLibraryManager()
		{
			activemq::library::ActiveMQCPP::initializeLibrary();
		}
		~ActiveMQCPPLibraryManager()
		{
			activemq::library::ActiveMQCPP::shutdownLibrary();
		}
		static ActiveMQCPPLibraryManager instance;
};

ActiveMQCPPLibraryManager ActiveMQCPPLibraryManager::instance;

AmqHelper::AmqHelper( const string& connectionString ): TransportHelper( 0 ),
	m_Connection( NULL ), m_Session( NULL ), m_AutoAcknowledgeSession( NULL ), m_Consumer ( NULL ), m_AutoAcknowledgeConsumer( NULL ), m_Producer( NULL ), m_AutoAcknowledgeProducer( NULL ),
	m_QueueBrowser( NULL ), m_ConnectionString( connectionString ), m_ReplyBrokerURI( "" ), m_MessageFormatPointer( &TMT_STRING ),
	m_BrokerURIOpenRefCount( 0 ), m_MessageCMSTimestamp ( 0 ), m_ReplyOptions( MQRO_NONE ), m_Timeout( 100 )
{
	m_MessageType = TMT_DATAGRAM;
	m_LastInGroup = false;
}

AmqHelper::~AmqHelper()
{
	NO_THROW( DEBUG2( "Destructor" ) )
	disconnect();
	NO_THROW( DEBUG2( "Destructor done." ) )
}

void AmqHelper::setConnectionBrokerURI()
{
	ActiveMQConnectionFactory connectionFactory( m_ConnectionString );
	try
	{
		m_Connection = dynamic_cast <ActiveMQConnection*> ( connectionFactory.createConnection() );
		if ( m_Connection == NULL )
			throw runtime_error( "Connection is not an ActiveMQConnection." );

		PrefetchPolicy* prefetchPolicy = m_Connection->getPrefetchPolicy();
		prefetchPolicy->setQueuePrefetch( 0 );

		RedeliveryPolicy* redeliveryPolicy = m_Connection->getRedeliveryPolicy();
		redeliveryPolicy->setMaximumRedeliveries( -1 );
		redeliveryPolicy->setInitialRedeliveryDelay( 0 );
		redeliveryPolicy->setRedeliveryDelay( 0 );

		m_Connection->start();
		m_Session = m_Connection->createSession( Session::SESSION_TRANSACTED );
		m_Producer = m_Session->createProducer( NULL );
		m_Producer->setDeliveryMode( DeliveryMode::PERSISTENT );
		m_AutoAcknowledgeSession = m_Connection->createSession( Session::AUTO_ACKNOWLEDGE );
		m_AutoAcknowledgeProducer = m_AutoAcknowledgeSession->createProducer( NULL );
	}
	catch( const CMSException& e )
	{
		TRACE( "Connecting to brokerURI [" + m_ConnectionString + "] failed. Reason : " + e.getMessage() );
		disconnect();
		throw;
	}
}

void AmqHelper::connect( const string& queueManagerName, const string& transportUri, bool force )
{
	doConnect( transportUri, force );
}

void AmqHelper::connect( const string& queueManagerName, const string& transportUri, const string& keyRepository , const string& sslCypherSpec , const string& sslPeerName , bool force )
{
	TRACE( "SSL connection not implemented. Perform unsecured connection" );
	connect( queueManagerName, transportUri, force );
}

void AmqHelper::doConnect( const string& brokerURIName, bool force )
{
	DEBUG( "Connecting to brokerURI: [" << brokerURIName <<"]" )

	if ( !brokerURIName.empty() )
	{
		if ( m_ConnectionString != brokerURIName )
		{
			if ( !m_ConnectionString.empty() )
				DEBUG( "Connection string changed from: [" << m_ConnectionString << "]" )
			m_ConnectionString = brokerURIName;
		}
		else
		{
			if ( m_Connection && !force )
			{
				DEBUG( "Already connected" )
				return;
			}
			else
				DEBUG( "Forced reconnection" )
		}
	}
	else
		if ( m_ConnectionString.empty() )
		throw logic_error( "No broker URI to connect to" );

	if ( m_Connection != NULL )
	{
		DEBUG( "Disconnecting from current connection" )
		disconnect();
	}
	setConnectionBrokerURI();
	if ( m_BackoutQueueName.empty() )
		m_BackoutQueueName = "ActiveMQ.DLQ";

	m_BrokerURIOpenRefCount++;
	DEBUG( "Connection refcount = " << m_BrokerURIOpenRefCount );
	DEBUG( "Connected to brokerURI: [" << m_ConnectionString << "]" );
}

void AmqHelper::reconnect()
{
	disconnect();
	doConnect( m_ConnectionString, true);
	openQueue();
}

void AmqHelper::disconnect()
{
	NO_THROW( DEBUG( "Disconnecting from brokerURI: [" << m_ConnectionString << "]" ) )

	if( m_ConnectionString.empty() )
	{
		NO_THROW( DEBUG( "BrokerURI not set... disconnect shortcut" ) );
		return;
	}

	closeQueue();

	if ( m_Producer !=  NULL )
	{
		NO_THROW( DEBUG( "Closing producer." ) )
		try
		{
			m_Producer->close();
		}
		catch ( CMSException& e )
		{
			NO_THROW( TRACE( "Closing producer failed. Reason : " << e.getMessage() ) )
		}
	}

	if ( m_AutoAcknowledgeProducer !=  NULL )
	{
		NO_THROW( DEBUG( "Closing auto acknowledge producer." ) )
		try
		{
			m_AutoAcknowledgeProducer->close();
		}
		catch ( CMSException& e )
		{
			NO_THROW( TRACE( "Closing auto acknowledge failed. Reason : " << e.getMessage() ) )
		}
	}

	try
	{
		NO_THROW( DEBUG( "Closing session.") )
		if( m_Session != NULL)
		{
			m_BrokerURIOpenRefCount--;
			m_Session->close();
		}
	}
	catch ( const CMSException& e )
	{
		NO_THROW( TRACE( "Closing session failed. Reason : " << e.getMessage() ) )
	}
	
	try
	{
		NO_THROW( DEBUG( "Closing auto acknowledge session.") )
		if( m_AutoAcknowledgeSession != NULL)
		{
			m_AutoAcknowledgeSession->close();
		}
	}
	catch ( const CMSException& e )
	{
		NO_THROW( TRACE( "Closing auto acknowledge session failed. Reason : " << e.getMessage() ) )
	}

	try
	{
		NO_THROW( DEBUG( "Closing connection." ) )
		if( m_Connection != NULL)
		{
			m_Connection->stop();
			m_Connection->close();
		}
	}
	catch ( const CMSException& e )
	{
		NO_THROW( TRACE( "Closing connection failed. Reason : " << e.getMessage() ) )
	}

	delete m_Producer;
	m_Producer = NULL;
	delete m_AutoAcknowledgeProducer;
	m_AutoAcknowledgeProducer = NULL;
	delete m_Session;
	m_Session = NULL;
	delete m_AutoAcknowledgeSession;
	m_AutoAcknowledgeSession = NULL;
	delete m_Connection;
	m_Connection = NULL;

	NO_THROW( DEBUG( "Disconnected from brokerURI: [" << m_ConnectionString << "]" ) )
}

void AmqHelper::openBackoutQueue( const string& queueName )
{
	DEBUG( "Opening backout queue [" << queueName << "]" );

	if ( !queueName.empty() )
	{
		if ( m_BackoutQueueName != queueName )
		{
			DEBUG( "Backout queue name changed" );
			m_BackoutQueueName = queueName;
		}
		else
		{
			DEBUG( "Backout queue [" << queueName << "] already open" );
			return;
		}
	}
	else
		if ( m_BackoutQueueName.empty() )
			m_BackoutQueueName = "ActiveMQ.DLQ";
}

void AmqHelper::openQueue( const string& queueName )
{
	openQueue( true, queueName );
}

void AmqHelper::openQueue( bool syncpoint, const string& queueName )
{
	TEST_SESSION

	DEBUG( "Opening queue [" << queueName << "]" );
	if ( !queueName.empty() )
	{
		if ( m_QueueName != queueName )
		{
			DEBUG( "Queue name changed" );
			m_QueueName = queueName;
		}
		else
			if ( syncpoint )
			{
				if ( m_Consumer != NULL && m_Consumer->getMessageSelector() == m_Selector )
					return;
			}
			else
				if ( m_AutoAcknowledgeConsumer != NULL && m_AutoAcknowledgeConsumer->getMessageSelector() == m_Selector )
					return;
	}
	else
		if ( m_QueueName.empty() )
			throw logic_error( "Queue name not specified." );
		else
		{
				DEBUG( "Queue [" << m_QueueName << "] already open" );
				if ( syncpoint )
				{
				if ( m_Consumer != NULL && m_Consumer->getMessageSelector() == m_Selector )
					return;
				}
				else
					if ( m_AutoAcknowledgeConsumer != NULL && m_AutoAcknowledgeConsumer->getMessageSelector() == m_Selector )
						return;
				DEBUG ( "Selector is different. Closing and reopening current queue." )
		}
	try
	{
		closeQueue();
		ActiveMQQueue queue ( m_QueueName );
		if ( syncpoint )
			m_Consumer = m_Session->createConsumer( &queue , m_Selector );
		else
			m_AutoAcknowledgeConsumer = m_AutoAcknowledgeSession->createConsumer( &queue , m_Selector );
	}
	catch( const InvalidDestinationException& e )
	{
		closeQueue();
		TRACE( "Open queue [" + m_QueueName + "] failed. Queue is not a valid destination. " + e.getMessage() );
		throw;
	}
	catch( const InvalidSelectorException& e )
	{
		closeQueue();
		TRACE( "Open queue [" + m_QueueName + "] failed. Message selector is not valid. " + e.getMessage() );
		throw;
	}
	catch( const CMSException& e )
	{
		closeQueue();
		TRACE( "Open queue [" + m_QueueName + "] failed with reason code : " + e.getMessage() );
		throw;
	}
	DEBUG( "Queue [" << m_QueueName << "] opened. Refcount = " );
}

void AmqHelper::closeQueue()
{
	if ( m_Consumer !=  NULL )
	{
		NO_THROW( DEBUG( "Closing consumer." ) )
		try
		{
			m_Consumer ->close();
		}
		catch ( CMSException& e )
		{
			NO_THROW( TRACE( "Closing consumer failed. Reason : " << e.getMessage() ) )
		}
	}

	if ( m_QueueBrowser !=  NULL )
	{
		NO_THROW( DEBUG( "Closing queue browser." ) )
		try
		{
			m_QueueBrowser ->close();
		}
		catch ( CMSException& e )
		{
			NO_THROW( TRACE( "Closing queue browser failed. Reason : " << e.getMessage() ) )
		}
	}

	if ( m_AutoAcknowledgeConsumer!= NULL )
	{
		NO_THROW( DEBUG( "Closing auto acknowledge consumer." ) )
		try
		{
			m_AutoAcknowledgeConsumer ->close();
		}
		catch ( CMSException& e )
		{
			NO_THROW( TRACE( "Closing auto acknowledge consumer failed. Reason : " << e.getMessage() ) )
		}
	}

	delete m_Consumer;
	m_Consumer = NULL;
	delete m_QueueBrowser;
	m_QueueBrowser = NULL;
	delete m_AutoAcknowledgeConsumer;
	m_AutoAcknowledgeConsumer = NULL;
}

time_t AmqHelper::getMessagePutTime()
{
	time_t putTime;

	putTime = m_MessageCMSTimestamp / 1000;
	return putTime;
}

void AmqHelper::setMessageId( const string& passedMessageId )
{
	m_MessageId = Base64::decode( passedMessageId );
	m_UsePassedMessageId = true;
	updateSelector ( m_Selector );
}

void AmqHelper::setCorrelationId( const string& passedCorrelId )
{
	m_CorrelationId =  Base64::decode( passedCorrelId );
	m_UsePassedCorrelId = true;
	updateSelector ( m_Selector );
}

void AmqHelper::setGroupId( const string& passedGroupId )
{
	m_GroupId = Base64::decode( passedGroupId );
	m_UsePassedGroupId = true;
	updateSelector ( m_Selector );
}

void AmqHelper::setApplicationName( const string& passedAppName )
{
	m_ApplicationName = passedAppName;
	m_UsePassedAppName = true;
}

void AmqHelper::setBackupQueue( const string& queueName )
{
	if ( queueName.empty() )
	{
		m_BackupQueueName = "";
		m_SaveBackup = false;
		return ;
	}
	else
	{
		m_BackupQueueName = queueName;
		m_SaveBackup = true;
	}
}

void AmqHelper::updateSelector( string& selector )
{
	selector = "";
	if ( m_UsePassedMessageId && !m_MessageId.empty() )
	{
		selector = "JMSMessageID='" + m_MessageId +"'";
	}
	if ( m_UsePassedCorrelId && !m_CorrelationId.empty() )
	{
		if ( !selector.empty() )
			selector = selector + " AND ";
		selector = selector + "JMSCorrelationID='" + m_CorrelationId +"'";
	}
	if ( m_UsePassedGroupId && !m_GroupId.empty() )
	{
		if ( !selector.empty() )
			selector = selector + " AND ";
		selector = selector + FINTPGROUPID + "='" + m_GroupId + "'";
	}
}

/**
 * Saves the message id, correlation id, group id, group sequence number, type and timestamp
 * @param msg Reference to a CMS::Message
 */
void AmqHelper::setLastMessageIds( const Message& msg )
{
	resetUsePassedIds();
	const string& messageId = msg.getCMSMessageID();
	m_MessageId = Base64::encode( messageId );
	DEBUG( "Message id : [" << m_MessageId << "]" );

	const string& correlationId = msg.getCMSCorrelationID();
	m_CorrelationId = Base64::encode( correlationId );
	DEBUG( "Correlation id : [" << m_CorrelationId << "]" );

	if ( msg.propertyExists( FINTPGROUPID ) && msg.propertyExists( FINTPGROUPSEQ )  )
	{
		const string& groupId = msg.getStringProperty( FINTPGROUPID );
		m_GroupId = Base64::encode( groupId );
		DEBUG( "Group id : [" << m_GroupId << "]" );
		m_GroupSequence =  msg.getIntProperty( FINTPGROUPSEQ );
		DEBUG( "Sequence id : [" << m_GroupSequence << "]" );
	}
	else
	{
		m_GroupId = "";
		m_GroupSequence = 0;
	}
}

void AmqHelper::setLastMessageReplyData ( const Message& msg )
{
	const Destination* replyDestination = msg.getCMSReplyTo();
	if ( replyDestination != NULL )
	{
		const Queue* replyQueue =  dynamic_cast <const Queue*> ( replyDestination );
		if ( replyQueue != NULL )
		{
			m_ReplyQueue = replyQueue->getQueueName();
			if ( msg.propertyExists( "ReplyBrokerURI" ) )
				m_ReplyBrokerURI = msg.getStringProperty( "ReplyBrokerURI" );
			else
				m_ReplyBrokerURI = "";
		}
		else
		{
			m_ReplyQueue = "";
			m_ReplyBrokerURI = "";
		}
	}
}

void AmqHelper::writeToBuffer( ManagedBuffer& buffer, const unsigned char* rawBuffer )
{
	DEBUG( "Message length : " << m_MessageLength << " / Buffer size : " << buffer.size() );

	switch ( buffer.type() )
	{
		case ManagedBuffer::Ref:
		{
			if  ( m_MessageLength > buffer.size() )
				throw runtime_error("Buffer size not big enough for message data.");
			memcpy( buffer.buffer(), rawBuffer, m_MessageLength );
			break;
		}
		case ManagedBuffer::Adopt:
		{
			buffer.copyFrom( rawBuffer, m_MessageLength );
			break;
		}
	default:
		throw runtime_error("Can't write to buffer");
	}
}

int AmqHelper::noMessage()
{
	if ( m_UsePassedMessageId )
		DEBUG( "No message matching passed messageid [" << Base64::encode( m_MessageId ) << "]" );
	if ( m_UsePassedCorrelId )
		DEBUG( "No message matching passed correlid [" << Base64::encode( m_CorrelationId ) << "]" );
	if ( m_UsePassedGroupId )
		DEBUG( "No message matching passed groupid [" << Base64::encode( m_GroupId) << "]" );
	return -1;
}

int AmqHelper::setLastMessageInfo( Message* msg, ManagedBuffer* buffer, bool browse, bool get, bool syncpoint )
{
	if ( msg != NULL )
	{
		if ( const TextMessage* textMessage = dynamic_cast<const TextMessage*>( msg ) )
		{
			const string& textData = textMessage->getText();
			m_MessageLength = textData.size();
			m_MessageFormatPointer = &TMT_STRING;
			if ( !browse && buffer != NULL )
				writeToBuffer ( *buffer, reinterpret_cast<const unsigned char*>(textData.c_str()) );
		}
		else
		{
			if ( const BytesMessage* bytesMessage = dynamic_cast<const BytesMessage*>( msg ) )
			{
				m_MessageFormatPointer = &TMT_NONE;
				if ( !browse && buffer != NULL )
				{
					const unsigned char* const bytesData = bytesMessage->getBodyBytes();
					m_MessageLength = bytesMessage->getBodyLength();
					writeToBuffer( *buffer, bytesData );
				}
			}
			else
				return noMessage();
		}
		m_Selector.clear();
		setLastMessageIds ( *msg );
		setLastMessageReplyData ( *msg );
		m_MessageType = ToTransportMessageType( msg->getCMSType() );
		if ( msg->propertyExists( "feedback" ) )
			m_Feedback = msg->getLongProperty( "feedback" );
		else
			m_Feedback = 0;
		if ( msg->propertyExists("AppName") )
			m_ApplicationName = msg->getStringProperty("AppName");
		else
			m_ApplicationName.clear();
		DEBUG( "Message Type : [" << msg->getCMSType() << "]" );
		m_MessageCMSTimestamp = msg->getCMSTimestamp();
		m_LastInGroup = msg->propertyExists( FINTPGROUPID ) &&\
						msg->propertyExists( FINTPLASTINGROUP ) &&\
						msg->getBooleanProperty( FINTPLASTINGROUP ) ;
		if ( m_SaveBackup && !browse && get )
			send( m_BackupQueueName, *msg, syncpoint );
		return 0;
	}
	return noMessage();
}

void AmqHelper::resetUsePassedIds ()
{
	m_UsePassedMessageId = false;
	m_UsePassedCorrelId = false;
	m_UsePassedGroupId = false;
	m_UsePassedAppName = false;
}

bool AmqHelper::send( const string& queueName, Message& msg, bool syncpoint )
{
	if ( m_Producer == NULL )
		throw logic_error("NULL producer can't send messages.");

	try
	{
		ActiveMQQueue queue( queueName );
		if ( syncpoint )
			m_Producer->send( &queue, &msg  );
		else
			m_AutoAcknowledgeProducer->send( &queue, &msg );
		return true;
	}
	catch ( const UnsupportedOperationException& e )
	{
		TRACE( "Error sending message. Producer does not have a destination. " << e.getMessage() );
		return false;
	}
	catch ( const InvalidDestinationException& e )
	{
		TRACE( "Error sending message. Producer does not have a valid destination. " << e.getMessage() );
		return false;
	}
	catch ( const MessageFormatException& e )
	{
		TRACE( "Error sending message. Message is invalid. " << e.getMessage() )
		return false;
	}
	catch ( const CMSException& e )
	{
		TRACE( "ActiveMQ internal error occured while sending message. " << e.getMessage() )
		return false;
	}
}

void AmqHelper::putOneReply( const ManagedBuffer& buffer, long feedback )
{
	DEBUG( "Putting one reply - wrapper" );

	m_MessageType = TMT_REPLY;
	scoped_ptr <Message> msg ( createMsg() );
	msg->setLongProperty( "feedback", feedback );
	putOne( buffer.buffer(), buffer.size(), *msg.get() );

	DEBUG( "One reply put - wrapper" );
}

void AmqHelper::putOneReply( unsigned char* buffer, size_t bufferSize, long feedback, TRANSPORT_MESSAGE_TYPE messageType )
{
	if ( buffer == NULL )
		throw logic_error("NULL buffer");

	DEBUG( "Putting one reply - wrapper" );

	m_MessageType = messageType;
	scoped_ptr <Message> msg ( createMsg() );
	msg->setLongProperty( "feedback", feedback );
	putOne( buffer, bufferSize, *msg.get() );

	DEBUG( "One reply put - wrapper" );
}

void AmqHelper::putOneRequest( unsigned char* buffer, size_t bufferSize, const string& rtqName, const string& rtbName, TransportReplyOptions& replyOptions )
{
	if ( buffer == NULL )
		throw logic_error("NULL buffer");

	DEBUG( "Putting one request - wrapper" );

	m_ReplyBrokerURI = rtbName;
	m_ReplyQueue = rtqName;
	m_MessageType = TMT_REQUEST;

	ManagedBuffer managedBuffer(buffer, ManagedBuffer::Ref, bufferSize);
	putOne( managedBuffer );

	DEBUG( "One request put - wrapper" );
}

void AmqHelper::putOne( unsigned char* buffer, size_t bufferSize, bool syncpoint )
{
	if ( buffer == NULL )
		throw logic_error("NULL buffer");

	DEBUG( "Putting one message - wrapper" );

	ManagedBuffer managedBuffer( buffer, ManagedBuffer::Ref, bufferSize );
	putOne( managedBuffer, syncpoint );

	DEBUG( "One message put - wrapper" );
}

void AmqHelper::putToDeadLetterQueue( Message& msg, bool syncpoint )
{
	DEBUG( "Putting one message to deadletter queue" );

	if ( m_QueueName == m_BackoutQueueName )
		return;

	if ( !send( m_BackoutQueueName, msg, syncpoint ) )
	{
		string errorMessage = "Unable to put the message to backout/dead letter queue.";
		TRACE( errorMessage );
		throw runtime_error( errorMessage );
	}
	else
		DEBUG( "One message put to deadletter queue [" << m_BackoutQueueName << "]" );
}

void AmqHelper::putOne( unsigned char* buffer, size_t bufferSize, Message& msg )
{
	if ( buffer == NULL )
		throw logic_error("NULL buffer");

	if ( m_MessageFormatPointer == &TMT_STRING )
	{
		TextMessage& textMsg = static_cast<TextMessage&>( msg );
		textMsg.setText( reinterpret_cast<const char*>( buffer ) );
		putOne( textMsg );
	}
	else
	{
		BytesMessage& byteMsg = static_cast<BytesMessage&>( msg );
		byteMsg.setBodyBytes( buffer, bufferSize );
		putOne( byteMsg );
	}
	m_MessageLength = bufferSize;
}

void AmqHelper::putOne( Message& msg, bool syncpoint )
{
	TEST_SESSION
	DEBUG( "Putting one message" );

	int attempts = 0;
	bool msgSend = false;
	try
	{
		do
		{
			if ( attempts++ > 0 )
			{
				DEBUG( "Retry #" << attempts << " in 10 seconds ... " );
				sleep( 10 );
				reconnect();
			}

			msg.setCMSType( ToString( m_MessageType ) );

			if ( m_UsePassedMessageId )
				msg.setStringProperty( "MessageID", m_MessageId );

			if ( m_UsePassedCorrelId )
				msg.setCMSCorrelationID( m_CorrelationId );

			if ( m_UsePassedGroupId )
				msg.setStringProperty( FINTPGROUPID, m_GroupId );

			if ( m_UsePassedAppName )
				msg.setStringProperty( "AppName", m_ApplicationName ); 

			if ( m_MessageType == TMT_REQUEST )
			{
				ActiveMQQueue queue ( m_ReplyQueue );
				msg.setCMSReplyTo( &queue );
				msg.setStringProperty( "ReplyBrokerURI", m_ReplyBrokerURI );
			}

			msgSend = send( m_QueueName, msg, syncpoint );

			if ( !msgSend )
			{
				string errorMessage = "Put message to [" + m_QueueName + "] failed." ;
				if ( attempts >= 2 )
				{
					resetUsePassedIds();
					throw runtime_error( errorMessage );
				}
				else
					TRACE( errorMessage );
			}
			else
				setLastMessageInfo( &msg, NULL, false, false, syncpoint );

		} while ( ( attempts < 3 ) && ( !msgSend ) );
	}
	catch ( const CMSException& e )
	{
		TRACE ( e.getMessage() )
		throw	e.getCause();
	}
	// reset format & type
	m_MessageFormatPointer = &TMT_STRING;
	m_MessageType = TMT_DATAGRAM;

	DEBUG( "One message put" );
}

void AmqHelper::putOne( const ManagedBuffer& buffer, bool syncpoint )
{
	DEBUG( "Putting one message" );

	if ( m_MessageFormatPointer == &TMT_STRING  )
	{
		ActiveMQTextMessage textMsg;
		textMsg.setText( buffer.str() );
		putOne( textMsg, syncpoint );
	}
	else
	{
		ActiveMQBytesMessage byteMsg;
		byteMsg.setBodyBytes( buffer.buffer(), buffer.size() );
		putOne( byteMsg, syncpoint );
	}
	m_MessageLength = buffer.size();
}

Message* AmqHelper::createMsg()
{
	if ( m_MessageFormatPointer == &TMT_STRING  )
		return new ActiveMQTextMessage();
	else
		return new ActiveMQBytesMessage();
}

long AmqHelper::getOne( unsigned char* buffer, size_t maxSize, bool syncpoint )
{
	if ( buffer == NULL )
		throw logic_error( "NULL buffer" );

	DEBUG( "Getting one message - wrapper" );

	ManagedBuffer managedBuffer( buffer, ManagedBuffer::Ref, maxSize );
	long result = getOne( &managedBuffer, syncpoint );
	DEBUG( "Got one message - wrapper" );
	return result;
}

long AmqHelper::peek( const string& queueName, bool first )
{
	TEST_SESSION

	if ( /*first ||*/ m_QueueBrowser == NULL || queueName != m_QueueBrowser->getQueue()->getQueueName() )
	{
		delete m_QueueBrowser;
		m_QueueBrowser = NULL;

		ActiveMQQueue queue( queueName );
		m_QueueBrowser = m_Session->createBrowser( &queue, m_Selector );
	}

	DEBUG( "Peeking queue [" << queueName << "]" )

	scoped_ptr <Message> msg;
	MessageEnumeration* enumeration = m_QueueBrowser->getEnumeration();

	if ( enumeration->hasMoreMessages() )
		msg.reset( enumeration->nextMessage() );

	DEBUG( "Peeked" );

	return setLastMessageInfo( msg.get(), NULL, true );
}

void AmqHelper::putGroupMessage( ManagedBuffer* buffer, const string& batchId, long messageSequence, bool isLast )
{
	if ( !buffer )
		throw logic_error("Empty payload");

	openQueue();

	scoped_ptr<Message> msg;

	if ( m_MessageFormatPointer == &TMT_STRING )
	{
		msg.reset( new ActiveMQTextMessage );
		ActiveMQTextMessage& textMsg = static_cast<ActiveMQTextMessage&>(*msg.get());
		textMsg.setText( buffer->str() );
	}
	else
	{
		msg.reset( new ActiveMQBytesMessage );
		ActiveMQBytesMessage& bytesMsg = static_cast<ActiveMQBytesMessage&>(*msg.get());
		bytesMsg.setBodyBytes( buffer->buffer(), buffer->size() );
	}

	cms::Message& theMsg = *msg.get();
	theMsg.setCMSType("MQMT_DATAGRAM");

	if ( isLast )
		theMsg.setBooleanProperty( FINTPLASTINGROUP, true );
	theMsg.setIntProperty( FINTPGROUPSEQ, messageSequence );
	theMsg.setStringProperty( FINTPGROUPID, batchId );

	putOne( theMsg );
}

long AmqHelper::getGroupMessage( ManagedBuffer* groupMessageBuffer, const string& groupId, bool& isCleaningUp )
{
	TEST_SESSION
	if ( m_QueueName.empty() )
		throw logic_error( "Queue name not specified." );
	if ( groupMessageBuffer == NULL )
		throw logic_error("NULL buffer");

	scoped_ptr<MessageConsumer> groupConsumer;

	if ( m_GroupLogicOrder.find( groupId ) != m_GroupLogicOrder.end() )
		m_GroupLogicOrder[groupId]++;
	else
		m_GroupLogicOrder.insert( pair<string, int>( groupId, 1 ) );

	const string sequence = StringUtil::ToString( m_GroupLogicOrder[groupId] );

	const string selector = FINTPGROUPID + "='" + groupId + "'" + " AND " + FINTPGROUPSEQ + "=" + sequence;

	const ActiveMQQueue queue( m_QueueName );
	groupConsumer.reset( m_Session->createConsumer( &queue, selector ) );
	scoped_ptr<Message> msg ( groupConsumer->receive( m_Timeout ) );

	if ( msg == NULL )
		return -1;

	if ( ( m_AutoAbandon > 0 ) && ( msg->getIntProperty( "JMSXDeliveryCount" ) >= m_AutoAbandon ) )
	{
			TRACE( "AutoAbandon set to " << m_AutoAbandon << "message is moved to deadletter." );
			TRACE( "Message in batch exceed backoutcount, sequence for dequeued mesage is [" << sequence << "]" );
			putToDeadLetterQueue( *(msg.get()) );
			isCleaningUp = true;
			return -2;
	}

	const TextMessage* textMsg = dynamic_cast<const TextMessage*>( msg.get() );
	if ( textMsg )
		groupMessageBuffer->copyFrom( textMsg->getText() );
	else
	{
		const BytesMessage* bytesMsg = dynamic_cast<const BytesMessage*>( msg.get() );
		if ( bytesMsg )
			groupMessageBuffer->copyFrom( bytesMsg->getBodyBytes(), bytesMsg->getBodyLength() );
		else
			throw runtime_error("Unknown message type");
	}

	setLastMessageInfo( msg.get(), NULL );

	if ( m_LastInGroup )
	{
		m_GroupLogicOrder.erase(groupId);
		DEBUG( "Message is LAST" );
	}

	return 0;
}

long AmqHelper::getOne( bool getForClean )
{
	ManagedBuffer managedBuffer( NULL, ManagedBuffer::Adopt);
	return getOne( &managedBuffer, getForClean );
}

/**
 * Get one message from the current queue and store it in a ManagedBuffer
 * @param buffer Pointer to a ManagedBuffer used for storing the message data
 * @param getForClean Bool - true if the message is to be discarded without affecting the Helper's state
 * @return 0 if succes, -1 if no message was retrieved and -2 if the message was moved to the dead letter queue
 */
long AmqHelper::doGetOne( ManagedBuffer* buffer, bool getForClean, bool syncpoint )
{
	if ( getForClean == false && buffer == NULL )
		throw logic_error("Can't write to NULL managedbuffer");

	openQueue( syncpoint );
	DEBUG( "Getting one message" );
	int attempts = 0;
	long result = 0;
	do
	{
		if ( getForClean == true )
			DEBUG( "Calling get message... Message data will be discarded" )
		else
			DEBUG( "Calling get message... Buffer size : " << buffer->size() )
		result = 0;
		scoped_ptr <Message> msg;
		try
		{
			if ( !syncpoint )
				msg.reset( m_AutoAcknowledgeConsumer->receive( m_Timeout ) );
			else
				msg.reset( m_Consumer->receive( m_Timeout ) );
		}
		catch( const CMSException& e )
		{
			throw runtime_error( "Error geting one message. Reason : " + e.getMessage() );
		}
		if ( getForClean )
			if ( msg == NULL )
				return -1;
			else
				return 0;
		result = setLastMessageInfo( msg.get(), buffer, false, true, syncpoint );
#ifdef NDEBUG
		if ( result != 0 )
		{
			DEBUG( "No message found on attempt :" << attempts );

			if( attempts < 3 )
			{
				DEBUG( "Attempting reopen of queue" );
				rollback();
				reconnect();
				openQueue();
			}
			else
			{
				result = -1;
				resetUsePassedIds();
			}
		}
#endif
		if ( ( msg != NULL ) && ( ( ( m_AutoAbandon > 0 ) && ( msg->getIntProperty( "JMSXDeliveryCount" ) >= m_AutoAbandon ) ) ) )
		{
			TRACE( "AutoAbandon set to " << m_AutoAbandon << "message is moved to deadletter." );

			putToDeadLetterQueue( *(msg.get()), syncpoint );
			if ( ( !getForClean ) && m_LastInGroup && syncpoint )
				commit();
			result = -2;
		}

		if ( result == 0 || result == -1 || result == -2 )
			break;

		if ( msg == NULL )
			attempts++;

	} while ( true );

	if ( result == 0 )
		DEBUG( "Get one message DONE " )
	else
		DEBUG( "Get one message FAILED " )

	return result;
}

//void AmqHelper::setAutoAbandon( const int retries )
//{
//	if ( m_Connection )
//	{
//		RedeliveryPolicy *policy = m_Connection->getRedeliveryPolicy();
//		policy->setMaximumRedeliveries( retries );
//	}
//	else
//		throw logic_error("NULL connection");
//}

/**
 * Commits the current session
 */
bool AmqHelper::commit()
{
	if ( m_Session == NULL )
		return false;

	DEBUG( "Commit" );
	bool result = true;
	try
	{
		m_Session->commit();
	}
	catch( const IllegalStateException& e )
	{
		TRACE( "Commit failed. Reason :" << e.getMessage() );
		result = false;
	}
	catch( const CMSException& e )
	{
		TRACE( "Commit failed. Reason : " << e.getMessage() );
		result = false;
	}
	return result;
}

/**
 * Rollbacks the current session
 */
bool AmqHelper::rollback()
{	
	if ( m_Session == NULL )
		return false;

	DEBUG( "Rollback" );
	bool result = true;
	try
	{
		m_Session->rollback();
	}
	catch( const IllegalStateException& e )
	{
		TRACE( "Rollback failed. Reason :" << e.getMessage() );
		result = false;
	}
	catch( const CMSException& e )
	{
		TRACE( "Rollback failed. Reason : " << e.getMessage() );
		result = false;
	}
	return result;
}

/**
 * Clears all messages in the current queue
 */
void AmqHelper::clearMessages()
{
	TEST_SESSION
	ActiveMQQueue queue( m_QueueName );
	scoped_ptr<MessageConsumer> consumer( m_AutoAcknowledgeSession->createConsumer( &queue ) );
	scoped_ptr<Message> msg;

	do
	{
		msg.reset( consumer->receive( m_Timeout ) );
	} while ( msg != NULL );
}

/**
 * Works only for small queues. For test purposes only.
 * @param queueName String containing the queue's name
 * @return The current number of messages or -1 in case of error
 */
long AmqHelper::getQueueDepth( const string& queueName )
{
	TEST_SESSION

	long crtDepth = 0;
	try
	{
		ActiveMQQueue queue;
		if ( queueName.empty() )
		{
			if ( m_QueueName.empty() )
				throw logic_error( "Queue name not specified" );
			queue.setPhysicalName( m_QueueName );
		}
		else 
			queue.setPhysicalName( queueName );
		scoped_ptr <QueueBrowser> queueBrowser ( m_Session->createBrowser( &queue ) );
		scoped_ptr <Message> msg;
		MessageEnumeration* enumeration = queueBrowser->getEnumeration();
		while ( enumeration->hasMoreMessages() )
		{
			msg.reset( enumeration->nextMessage() );
			++crtDepth;
		}
	}
	catch ( const CMSException& e )
	{
		crtDepth = - 1;
		TRACE("Error trying to get depth for queue: [" << queueName << "]. Reason: " << e.getMessage() );
	}
	return crtDepth;
}

void AmqHelper::setMessageFormat( const string& format )
{
	if ( format == TMT_STRING || format == TMT_RF_HEADER_2 )
		m_MessageFormatPointer = &TMT_STRING;
	else
	if ( format == TMT_NONE )
		m_MessageFormatPointer = &TMT_NONE;
	else
		throw logic_error( "Unknown message format" );
}

string AmqHelper::getLastMessageFormat() const
{
	if ( m_MessageFormatPointer == &TMT_STRING )
		return TMT_STRING;
	else
		return TMT_NONE;
}

TransportHelper::TRANSPORT_MESSAGE_TYPE AmqHelper::ToTransportMessageType( const string& messageType )
{
	if( messageType == "MQMT_DATAGRAM" )
		return TransportHelper::TMT_DATAGRAM;
	if( messageType == "MQMT_REPLY" )
		return TransportHelper::TMT_REPLY;
	if( messageType == "MQMT_REQUEST" )
		return TransportHelper::TMT_REQUEST;
	if( messageType == "MQMT_REPORT" )
		return TransportHelper::TMT_REPORT;

	TRACE("Unknown message type. Assuming MQMT_DATAGRAM");
	return TransportHelper::TMT_DATAGRAM;
}

string AmqHelper::ToString( TransportHelper::TRANSPORT_MESSAGE_TYPE messageType )
{
	if( messageType == TransportHelper::TMT_DATAGRAM )
		return "MQMT_DATAGRAM";
	if( messageType == TransportHelper::TMT_REPLY )
		return "MQMT_REPLY";
	if( messageType == TransportHelper::TMT_REQUEST )
		return "MQMT_REQUEST";
	if( messageType == TransportHelper::TMT_REPORT )
		return "MQMT_REPORT";
	throw runtime_error("Unknown message type");
}

void AmqHelper::clearSSLOptions()
{
	decaf::lang::System::setProperty( "decaf.net.ssl.keyStore", "" );
	decaf::lang::System::setProperty( "decaf.net.ssl.keyStorePassword", "" );
	decaf::lang::System::setProperty( "decaf.net.ssl.trustStore", "" );
}

void AmqHelper::putSAAmessage( const TransportReplyOptions& replyOptions, const string& replyQueue, ManagedBuffer* buffer, const string& batchId, long messageSequence, bool isLast )
{
	DEBUG( "Test function putSAAmessage - AmqHelper" )
	putGroupMessage( buffer, batchId, messageSequence, isLast );
}

/* FIXME */

TransportReplyOptions AmqHelper::getLastReplyOptions() const
{
	TransportReplyOptions tr;
	return tr;
}

/* FIXME */

void AmqHelper::setTimeout( int millisecs )
{
	m_Timeout = millisecs;
}
