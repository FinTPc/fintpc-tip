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

#ifndef AMQHELPER_H
#define AMQHELPER_H

#include <activemq/core/ActiveMQConnection.h>

#include "../TransportHelper.h"

namespace FinTP
{
	class ExportedTransportObject AmqHelper: public TransportHelper
	{
		private :

			activemq::core::ActiveMQConnection* m_Connection;
			cms::Session* m_Session, *m_AutoAcknowledgeSession;
			cms::MessageConsumer* m_Consumer, *m_AutoAcknowledgeConsumer;
			cms::QueueBrowser* m_QueueBrowser;
			cms::MessageProducer* m_Producer, *m_AutoAcknowledgeProducer;

			string m_ReplyBrokerURI;
			string m_BackoutQueueName;
			string m_ConnectionString, m_Selector;

			TRANSPORT_MESSAGE_TYPE m_MessageType;

			long m_ReplyOptions;

			int m_Timeout;
			int m_BrokerURIOpenRefCount;
			long long m_MessageCMSTimestamp;

			int noMessage();
			void resetUsePassedIds();
			void updateSelector( string& selector );

			const string* m_MessageFormatPointer;

			void writeToBuffer( ManagedBuffer& buffer, const unsigned char* rawBuffer );
			void doConnect( const string& brokerURIName, bool force );
			bool send( const string& queueName, cms::Message& msg, bool syncpoint = true );
			inline void setConnectionBrokerURI();
			void setLastMessageIds( const cms::Message& msg );
			void setLastMessageReplyData( const cms::Message& msg );
			int setLastMessageInfo( cms::Message* msg, ManagedBuffer* buffer, bool browse=false, bool get=true, bool syncpoint=true );

			TRANSPORT_MESSAGE_TYPE ToTransportMessageType( const string& messageType );
			std::string ToString( TRANSPORT_MESSAGE_TYPE messageType );

			void closeQueue();

			static const string FINTPGROUPID;
			static const string FINTPGROUPSEQ;
			static const string FINTPLASTINGROUP;

			map<string, int> m_GroupLogicOrder;

		public :

			explicit AmqHelper( const std::string& connectionstring = "" );

			~AmqHelper();

			void connect( const string& queueManagerName = "", const string& transportUri = "", bool force = false );
			void connect( const string& queueManagerName, const string& transportUri, const string& keyRepository , const string& sslCypherSpec , const string& sslPeerName , bool force = false );
						
			void openQueue( bool syncpoint, const std::string& queueName = "" );
			void openQueue( const std::string& queueName = "");
			void openBackoutQueue( const std::string& queueName );
			void setBackupQueue( const std::string& queueName );

			long getQueueDepth( const std::string& queueName = "" );
			
			long peek( const std::string& queueName = "", bool first = true );

			void setTimeout( int millisecs );

			long getOne( bool getForClean = false );
			long doGetOne( ManagedBuffer* buffer, bool getForClean, bool syncpoint = true );
			long getOne( unsigned  char* buffer, size_t maxSize, bool syncpoint = true );
			long getGroupMessage( ManagedBuffer* groupMessageBuffer, const string& groupId, bool& isCleaningUp);
			void putGroupMessage( ManagedBuffer* buffer, const string& batchId, long messageSequence, bool isLast  );

			void putSAAmessage( const TransportReplyOptions& replyOptions, const string& replyQueue, ManagedBuffer* buffer, const string& batchId, long messageSequence, bool isLast );	
			void putOne( const ManagedBuffer& buffer, bool syncpoint = true );
			void putOne( unsigned char* buffer, size_t bufferSize, bool syncpoint = true );
			void putOne( unsigned char* buffer, size_t bufferSize, cms::Message& msg );
			void putOne( cms::Message& msg, bool syncpoint = true );

			void putOneRequest( unsigned char* buffer, size_t bufferSize, const std::string& rtqName, const std::string& rtbName, TransportReplyOptions& replyOptions );
			void putOneReply( unsigned char* buffer, size_t bufferSize, long feedback, TRANSPORT_MESSAGE_TYPE messageType = TMT_REPLY );
			void putOneReply( const ManagedBuffer& buffer, long feedback );

			void putToDeadLetterQueue( cms::Message& msg, bool syncpoint = true );

			cms::Message* createMsg();

			void clearMessages();

			void setApplicationName( const std::string& applicationName );
			void setMessageId( const std::string& messageId );
			void setCorrelationId( const std::string& correlId );
			void setGroupId( const std::string& groupId );

			void setMessageFormat( const string& format );
			string getLastMessageFormat() const;

			time_t getMessagePutTime();
			std::string getLastReplyBrokerURI() const { return m_ReplyBrokerURI; }

			TRANSPORT_MESSAGE_TYPE getLastMessageType() { return m_MessageType; };

			int getLastSequenceId() const { return m_GroupSequence; }
			void setSequenceId( int sequenceId );

			void clearSSLOptions();

			void reconnect();
			void disconnect();
			bool commit();
			bool rollback();

//			void setAutoAbandon( const int retries );

			/* FIXME */
			TransportReplyOptions getLastReplyOptions() const;
			string getApplicationName() const { return m_ApplicationName; } 
			std::string getLastReplyQueueManager() const { return m_ConnectionString; }

			long getOne( ManagedBuffer* buffer, bool syncpoint = true, bool keepJMSHeader = false ) { return doGetOne(buffer, false, syncpoint); }
			/* FIXME */

	};
}

#endif //AMQHELPER_H
