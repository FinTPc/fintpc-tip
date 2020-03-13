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

/**
 *\file
 *\brief MQ Transport Helper classes
 */
#ifndef TRANSPORTHELPER_H
#define TRANSPORTHELPER_H

#ifdef __GNUC__
#define DEPRECATED(func) func __attribute__ ((deprecated))
#elif defined(_MSC_VER)
#define DEPRECATED(func) __declspec(deprecated) func
#else
#define DEPRECATED(func) func
#endif

#include "DllMainTransport.h"

#include "WorkItemPool.h"
#include "Trace.h"
#include "AbstractLogPublisher.h"

#include <string>
#include <vector>

using namespace std;

namespace FinTP
{
	/**
	 * \class TransportReplyOptions
	 * \brief Define MQ reply options implemented in FinTP
	 * \details Stores all used "report options" needed to processe MQ report message.
	 * Report options can be transferd to the client instances by 'long TransportHelper::getLastReplyOptions()'
	 *
	 */
#define  MAX_REPLY_OPTIONS 7


	class ExportedTransportObject TransportReplyOptions
	{
		public :

			typedef enum
				{
					NONE,
					RO_COD,
					RO_COA,
					RO_NAN,
					RO_PAN,
					RO_COPY_MSG_ID_TO_CORREL_ID,
					RO_PASS_CORREL_ID
				} ReplyOption;

			typedef vector<ReplyOption> ReplyOptions;

			explicit TransportReplyOptions( const string& options = "NONE" ){ Parse( options ); }
			//TransportReplyOptions& operator=( const TransportReplyOptions& replyOptions );
			TransportReplyOptions( const TransportReplyOptions& replyOptions );
			int getSize()const { return m_Options.size(); }
			ReplyOption operator[]( int i )const { return m_Options[ i ]; }
			void addReplyOption( const ReplyOption& option) { m_Options.push_back( option ); }
			bool isReplyOption( const string& replyValue ) const;
			bool optionsSet() const;
			string ToString() const;
			void Parse( const string& options);

		private :

			ReplyOptions m_Options;
			static const string m_ReplyOptionNames[ MAX_REPLY_OPTIONS ];

	};
	class ExportedTransportObject TransportHelper
	{
		public :

			enum TRANSPORT_HELPER_TYPE
			{
				WMQ = 1,
				AMQ = 2,
				AQ = 3,
				NONE = 0
			};

			enum TRANSPORT_MESSAGE_TYPE
			{
				TMT_DATAGRAM = 0,
				TMT_REPLY = 1,
				TMT_REQUEST = 2,
				TMT_REPORT = 3
			};

		protected :

			const unsigned int m_MessageIdLength;

			string m_QueueName;

			string m_MessageId, m_CorrelationId, m_GroupId;
			int m_GroupSequence;

			bool m_UsePassedMessageId, m_UsePassedCorrelId, m_UsePassedGroupId, m_UsePassedAppName;

			string m_MessagePutDate, m_MessagePutTime;

			string m_ReplyQueue;

			int m_QueueOpenRefCount;

			long long m_Feedback;
			bool m_LastInGroup;
			string m_ReplyUsrData;

			string m_ApplicationName;
			// SSL options
			string m_SSLKeyRepository, m_SSLCypherSpec, m_SSLPeerName;

			unsigned int m_MessageLength;
			int m_AutoAbandon;
			string m_BackupQueueName;
			bool m_SaveBackup;

			//NameValueCollection m_Settings;

			/**
			 * \brief Private ctor. used to initialise all data members of the TransportHelper interface
			 * \details Every TransportHelper inplementation should provide its own specific parameters
			 * \param[in] 'const string& messageFormat': specific value provided by each implementation to specify string or byte message format
			 * \param[in] 'unsigned int messageIdLength': message ID length used by the core FinTP current implementation of TransportHelper
			 */
			TransportHelper( unsigned int messageIdLength );

		public :

			const static string TMT_STRING;
			const static string TMT_RF_HEADER_2;
			const static string TMT_NONE;

			/**
			 * \brief Factory method used to instantiate propper TransportHelper
			 * \details Every FinTP component that communicate with MQ Server, has a configuration key that specify its TransportHelper type
			 * The component only call 'CreateHelper( const string& helperType )' and pass configured helperType in order to instatiate its TransportHelper
			 * \param[in] 'const string& helperType'
			 */
			static TransportHelper* CreateHelper( const TransportHelper::TRANSPORT_HELPER_TYPE& helperType );
			static TRANSPORT_HELPER_TYPE parseTransportType( const string& transportType );
			virtual ~TransportHelper() { DEBUG( "TransportHelper dtor." ) };

			virtual void connect( const string& queueManagerName = "", const string& transportUri = "", bool force = false ) = 0;
			virtual void connect( const string& queueManagerName, const string& transportUri, const string& keyRepository , const string& sslCypherSpec , const string& sslPeerName , bool force = false ) {} ;

			virtual void disconnect() = 0;
			virtual bool commit() = 0;
			virtual bool rollback() = 0;

			//get messages methods
			virtual long getOne( unsigned char* buffer, size_t maxSize, bool syncpoint = true ) = 0;
			/**
			 * \brief Pull one message from MQ Server and return it in 'ManagedBuffer* buffer'
			 * \details Syncpoit participation is mandatory if transaction should be controlled by application
			 * Is method responsibility to set all ids for currently fetch message ( i.e m_MessageId, m_CorrelationId )
			 * \param ManagedBuffer* buffer:returned MQ message
			 * \param bool syncpoint: true value, make the operation part of unit of work spanned over multiple operation
			 * \param bool keepJMSHeader: true value, make MQ header part of buffer
			 */
			virtual long getOne( ManagedBuffer* buffer, bool syncpoint = true, bool keepJMSHeader = false ) = 0;

			/**
			 * \brief
			 */
			virtual long getGroupMessage( ManagedBuffer* groupMessageBuffer, const string& groupId, bool& isCleaningUp ) = 0;
			//put messages methods
			virtual void putOne( unsigned char* buffer, size_t bufferSize, bool syncpoint = true ) = 0;


			virtual void putGroupMessage( ManagedBuffer* buffer, const string& batchId, long messageSequence, bool isLast ) = 0;

			virtual void putSAAmessage( const TransportReplyOptions& replyOptions, const string& replyQueue, ManagedBuffer* buffer, const string& batchId, long messageSequence, bool isLast );

			// request/reply puts messages
			virtual void putOneRequest( unsigned char* buffer, size_t bufferSize, const string& rtqName, const string& rtqmName, TransportReplyOptions& replyOptions ) = 0;
			virtual void putOneReply( unsigned char* buffer, size_t bufferSize = 0, long feedback = 0,  TRANSPORT_MESSAGE_TYPE replyType = TMT_REPLY ) = 0;

			// browse queue
			virtual long peek( const string& queue = "", bool first = true ) = 0;

			// helper methods
			virtual long getQueueDepth( const string& queueName ) = 0;
			virtual TRANSPORT_MESSAGE_TYPE getLastMessageType() = 0;
			virtual void clearMessages() = 0;

			/**
			 * \brief Set message header value when message end up in Dead.letter.queue
			 */
			virtual string getApplicationName() const = 0;
			virtual void setApplicationName( const string& applicationName ) = 0;
			/**
			* TODO getLastReplyOptions to return ReplyOptions type
			*/
			virtual TransportReplyOptions getLastReplyOptions() const = 0;
			string getLastReplyQueue() const { return m_ReplyQueue; }
			virtual string getLastReplyQueueManager() const = 0;
			virtual void clearSSLOptions() = 0;

			/**
			 * Common interface
			 */
			DEPRECATED(virtual void openQueue( const string& queueName ) ){ m_QueueName = queueName; }
			DEPRECATED(virtual void closeQueue() ){};
			virtual long getOne( ManagedBuffer* buffer )
			{
				return getOne(buffer, true);
			}
			string getLastMessageId() const { return m_MessageId; }
			virtual void setMessageId( const string& messageId );

			string getLastCorrelId() const { return m_CorrelationId; }
			virtual void setCorrelationId( const string& correlId );

			string getLastGroupId() const { return m_GroupId; }
			virtual void setGroupId( const string& groupId );

			int getLastGroupSequence() const { return m_GroupSequence; }

			virtual void setMessageFormat( const string& format ) = 0;
			virtual string getLastMessageFormat() const = 0;

			void setReplyUserData( const string& usrData ){ m_ReplyUsrData = usrData; }
			string getReplyUserData() const { return m_ReplyUsrData; }

			time_t getMessagePutTime();
			unsigned long getLastMessageLength() const { return m_MessageLength; }

			long long getLastFeedback() const { return m_Feedback; }
			bool isLastInGroup() const { return m_LastInGroup; }

			virtual void setAutoAbandon( const int retries );

			virtual void setBackupQueue( const string& queueName );
			long getQueueDepth(){ return getQueueDepth( m_QueueName ); }

			static AbstractLogPublisher* createMqLogPublisher( const NameValueCollection& propSettings, bool& isDefault );
	};
}

#endif
