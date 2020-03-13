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

#ifndef MQHELPER_H
#define MQHELPER_H

#include <imqi.hpp>
#include <cmqbc.h>
#include <cmqcfc.h>
#include <string>

#include "../TransportHelper.h"

using namespace std;

namespace FinTP
{
	class ExportedTransportObject WMqHelper : public TransportHelper
	{
		private :
		
			// WMQ objects
			ImqQueueManager m_QueueManager;
			ImqQueue m_Queue, m_BackupQueue, m_BackoutQueue;
			ImqChannel m_Channel;

			long m_QueueManagerCCSID, m_QueueManagerEncoding;
			bool m_ConversionNeeded;
			long m_QueueManagerPlatform;

			long m_OpenOption;
			string m_ConnectionString;

			long m_MessageType;

			long m_DataOffset;

			// reply/report options
			long m_ReplyOptions;
			string m_ReplyQueueManager;
			string m_MessageFormat;
			MQLONG m_ConnectOptions;
		
			bool m_UseSyncpoint;
					
			int m_QueueManagerOpenRefCount;

			void setOpenQueueOptions( const long& openOptions );
			void setChannel();
			TransportHelper::TRANSPORT_MESSAGE_TYPE ToTransportMessageType( long messageType );
			unsigned int StripJMSHeader( ImqMessage& msg, string& msgFormat ) const;
			unsigned int InjectJMSHeader( ImqMessage& msg );
			long getLastDataOffset() const { return m_DataOffset; }
			long ParseReplyOptions( const TransportReplyOptions& options);

		public :
			WMqHelper();
			~WMqHelper();

			/*
			 Function: connect
			 Connects to the queue manager
			 
			 This function will not reconnect to the queue manager unless the connection was broken/closed or the queue manager name has changed.
		
			 Parameters:
			 
			 queueManagerName - Queue manager name that the helper will connect to
			 channelDefinition - WMQ Channel definition ( when running as client ) which is of the form "channel-name/transport-type/connection-name(port)"
			 force - Force reconnect even if already connected. It will first close the existing connection.
			 */
			void connect( const string& queueManagerName = "", const string& channelDefinition = "", bool force = false );
			void connect( const string& queueManagerName, const string& channelDefinition, const string& keyRepository , const string& sslCypherSpec , const string& sslPeerName , bool force = false );

			void openQueue( const string& queueName = "" );
			void openBackupQueue();
			void openBackoutQueue( string queueName );

			void closeQueue();
			void closeBackupQueue();
			void closeBackoutQueue();
			
			void disconnect();
			
			/**
			 * interface declared gets messages
			 */
			long getOne( unsigned  char* buffer, size_t maxSize, bool syncpoint = true );
			long getOne( ManagedBuffer* buffer, bool syncpoint = true, bool keepJMSHeader = false );
			/**
			 * overloads
			 */
			long getOne( unsigned  char* buffer, size_t maxSize, ImqGetMessageOptions& gmo );
			long getOne( unsigned  char* buffer, size_t maxSize, ImqGetMessageOptions& gmo, ImqMessage& msg );
			long getOne( ManagedBuffer* buffer, ImqGetMessageOptions& gmo );
			long getOne( ManagedBuffer* buffer, ImqGetMessageOptions& gmo, ImqMessage& msg, bool getForClean = false, bool keepJMSHeader = false );

			/**
			* interface declared put messages
			*/
			void putOne( unsigned char* buffer, size_t bufferSize, bool syncpoint = true );
			/**
			 * overloads
			 */
			void putOne( unsigned char* buffer, size_t bufferSize, ImqPutMessageOptions& pmo );
			void putOne( unsigned char* buffer, size_t bufferSize, ImqPutMessageOptions& pmo, ImqMessage& msg );
			void putOne( ManagedBuffer* buffer, ImqPutMessageOptions& pmo, ImqMessage& msg );
			
			void putToDeadLetterQueue( ImqMessage& msg );
			void clearMessages();

			// syncpoint = true, messagetype = MQMT_REQUEST, 
			// setReplyToQueueManagerName, setReplyToQueueName, setReport
			
			/**
			* interface declared, handling group messages methods
			*/
			long getGroupMessage( ManagedBuffer* groupMessageBuffer, const string& groupId, bool& isCleaningUp );
			void putGroupMessage( ManagedBuffer* buffer,const string& batchId, long messageSequence, bool isLast );

			void putSAAmessage( const TransportReplyOptions& replyOptions, const string& replyQueue, ManagedBuffer* buffer, const string& batchId, long messageSequence, bool isLast );

			/**
			* interface declared request/reply
			*/
			void putOneRequest( unsigned char* buffer, size_t bufferSize, const string& rtqName, const string& rtqmName, TransportReplyOptions& replyOptions );
			void putOneReply( unsigned char* buffer, size_t bufferSize = 0, long feedback = 0, TRANSPORT_MESSAGE_TYPE replyType = TMT_REPLY );
			/**
			 * overloads
			 */
			void putOneReply( ManagedBuffer* buffer, long feedback );
			
			/**
			* interface declared peek
			*/
			long peek( const string& queue = "", bool first = true );

			string getApplicationName() const { return m_ApplicationName; }
			void setApplicationName( const string& applicationName );

			void setMessageFormat( const string& format );
			string getLastMessageFormat() const;

			TRANSPORT_MESSAGE_TYPE getLastMessageType() { return ToTransportMessageType( m_MessageType ); }

			TransportReplyOptions getLastReplyOptions() const ;
			string getLastReplyQueueManager() const { return m_ReplyQueueManager; }

			void setConnectOptions( MQLONG connectOptions );

			void clearSSLOptions();
			bool commit();
			bool rollback();

			long getQueueDepth( const string& queueName = "");
			long getOpenOutputCount( string queueName = "" );
			long getOpenInputCount( string queueName = "" );
			
			static void setSyncPointParticipation( ImqPutMessageOptions& pmo, unsigned char value );		
			static void setSyncPointParticipation( ImqGetMessageOptions& gmo, unsigned char value );
			
			static ImqBinary createBinary( void* value );

			// XA resource coordination
			void beginXATransaction();
	};
}

#endif
