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

#ifndef ROUTINGMESSAGE_H
#define ROUTINGMESSAGE_H

#include <map>
#include <vector>
#include <string>

using namespace std;

#include "XmlUtil.h"
#include "XPathHelper.h"
#include "Collections.h"

#include "WSRM/SequenceResponse.h"

#include "RoutingEngineMain.h"
#include "RoutingAggregationManager.h"
#include "RoutingMessageEvaluator.h"

class RoutingMessage;

class ExportedTestObject RoutingMessagePayload
{
	public :
	
		enum PayloadFormat
		{
			PLAINTEXT,
			XML,
			BASE64,
			AUTO
		};
	
		bool IsTextValid() const { return m_IsTextValid; }
		bool IsDocValid() const { return m_IsDocValid; }
	
		RoutingMessagePayload( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument& doc );
		RoutingMessagePayload( const string& doc );
		
		// copy ops
		RoutingMessagePayload( const RoutingMessagePayload& source );
		RoutingMessagePayload& operator=( const RoutingMessagePayload& source );
		
		~RoutingMessagePayload();
		
		// set
		void setDoc( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument& doc );
		void setDoc( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc );
		void setText( const string& text, RoutingMessagePayload::PayloadFormat format );
				
		XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* getDoc( const bool throwOnDeserialize = true );
		const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* const getDocConst() const;
		
		string getText( const RoutingMessagePayload::PayloadFormat format, const bool throwOnSerialize = true );
		string getTextConst() const;
		
		RoutingMessagePayload::PayloadFormat getFormat() const { return m_Format; }

		static RoutingMessagePayload::PayloadFormat convert( RoutingMessagePayload::PayloadFormat sourceFormat, RoutingMessagePayload::PayloadFormat destFormat, string& text );
		void convert( RoutingMessagePayload::PayloadFormat format );
		
	private :

		XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* m_Document;
		string m_Text;

		// control payload conversion
		bool m_IsTextValid;
		bool m_IsDocValid;
		RoutingMessagePayload::PayloadFormat m_Format;	
};

class ExportedTestObject RoutingMessageOptions 
{
	public :

		typedef enum
		{
			MO_NONE = 0,
			MO_BYPASSMP = 1,
			MO_BATCH = 2,
			MO_GENERATEID = 4, 
			MO_NOSEQ = 8,
			MO_COMPLETE = 16,
			MO_RBATCH = 32,
			/**
			 * Option set on exitpoint that don't support reactivation
			 * Effect : "good" messages in a batch will not be reactivated, but sent to BO just like "bad" messages
			 */
			MO_NOREACTIVATE = 64,
			/**
			 * Option set on exitpoint that don't need headers to pass through
			 * Effect : header messages ( first logical seq ) will not leave to the exitpoint queue
			 */
			MO_NOHEADERS = 128,
			/**
			 * Option set on exitpoint to insert information for incoming batch in table batchjobsinc
			 */
			MO_INSERTBATCH = 256,
			/**
			 * Option set on exit point that support correlation of good messages rejected batch reply
			*/
			MO_MARKNOTREPLIED = 512,
			/**
			 * Option set on exitpoint so that RE send out MT_REPLY as MT_DATAGRAM wmq messages
			*/
			MO_REPLYDATAGRAM = 1024
		} MESSAGE_OPTIONS;
			
		RoutingMessageOptions( const string& options ){ m_Options = Parse( options ); }
		RoutingMessageOptions( int options = RoutingMessageOptions::MO_NONE ){ m_Options = options; }
		
		static int Parse( const string& options );
		static string ToString( int options );
		
		int getValue() const { return m_Options; }

	private :
		
		int m_Options;
};

class ExportedTestObject RoutingMessage : public RoutingMessageBase
{
	public :
		
		typedef enum 
		{
			Batch,
			SingleMessage
		} REQUEST_TYPE;
		
	private :
	
		string m_MessageId, m_BatchId, m_CorrelationId, m_SessionId, m_InputSessionId, m_UserId;
		unsigned long m_Priority, m_BatchSequence, m_BatchCount;

		string m_BatchAmount;
		string m_OriginalMessageType;
		long m_RoutingSequence;
		bool m_HoldStatus;
		string m_DelayedTransform;
		string m_DelayedTransformSessionCode;
		string m_DelayedReplyId;

		RoutingAggregationCode m_Feedback;
		
		RoutingMessageOptions::MESSAGE_OPTIONS m_MessageOptions;
		
		string m_RequestorService;
		string m_ResponderService;
		string m_OriginalRequestorService;
		
		REQUEST_TYPE m_RequestType;
		bool m_Fastpath, m_Bulk, m_Virtual;
				
		string m_TableName;
		
		RoutingMessagePayload* m_Payload;
		RoutingMessageEvaluator* m_PayloadEvaluator;

		bool m_IsOutgoing;
		static const string MESSAGE_OUT;
				
	public:
		
		//.ctor & dtor
		RoutingMessage();
		RoutingMessage( const string& source );
		RoutingMessage( const string& tableName, const string& messageId, bool delayRead );
		RoutingMessage( const string& tableName, const string& messageId, RoutingMessage ( *messageProviderCallback )() = NULL, bool fastpath = false );
		
		// copy ops
		RoutingMessage( const RoutingMessage& source );
		RoutingMessage& operator=( const RoutingMessage& source );
		
		~RoutingMessage();
	
		void Read();
		// static instance specific members
		
		// helpers
		static RoutingMessage::REQUEST_TYPE ParseRequestType( const string& request );
		bool isReply();
		bool isAck();
		bool isNack();
		
		// flatten
		string ToString( bool payloadEncoded = false );
		string ToString( const string& payload );

		void DelayTransform( const string& transform, const string& sessionCode, const unsigned int sequence );
		string getDelayedTransform() const { return m_DelayedTransform; }
		string getDelayedTransformSessCode() const { return m_DelayedTransformSessionCode; }
		
		static string ToString( REQUEST_TYPE reqType );
			
		// accessors
		void setBatchSequence( const unsigned long sequence ) { m_BatchSequence = sequence; }
		unsigned long getBatchSequence() const { return m_BatchSequence; }

		string getOriginalMessageType() const { return m_OriginalMessageType; }
		
		void setOutputSession( const string& osession ) { m_SessionId = osession; }
		string getOutputSession() const { return m_SessionId; }
		string getInputSession() const { return m_InputSessionId; }
		
		void setBatchTotalCount( const unsigned long batchcount ) { m_BatchCount = batchcount; }
		unsigned long getBatchTotalCount() const { return m_BatchCount; }
		
		void setBatchTotalAmount( const string& amount ) { m_BatchAmount = amount; }
		string getBatchTotalAmount() const { return m_BatchAmount; }
		
		RoutingMessageOptions::MESSAGE_OPTIONS getMessageOptions() const { return m_MessageOptions; }
		void setMessageOptions( const RoutingMessageOptions::MESSAGE_OPTIONS options ) { m_MessageOptions = options; }
		void setMessageOption( const RoutingMessageOptions::MESSAGE_OPTIONS option ) { m_MessageOptions = ( RoutingMessageOptions::MESSAGE_OPTIONS )( option | m_MessageOptions ); }
		
		string getMessageId() const { return m_MessageId; }
		void setMessageId( const string& messageId ) { m_MessageId = messageId; }
		
		string getCorrelationId() const { return m_CorrelationId; }
		string getSessionId() const { return m_SessionId; }	
		
		string getBatchId() const { return m_BatchId; }
		void setBatchId( const string& value ) { m_BatchId = value; }
		
		unsigned long getPriority() const { return m_Priority; }
		void setPriority( const unsigned long value ) { m_Priority = value; }
		
		string getUserId() const { return m_UserId; }
		void setUserId( const int userId ) { m_UserId = userId; }
		
		void setCorrelationId( const string& correlationId ) { m_CorrelationId = correlationId; }
		
		RoutingAggregationCode getAggregationCode();
		
		const RoutingAggregationCode& getFeedback() const { return m_Feedback; }
		void setFeedback( const RoutingAggregationCode& feedback ) { m_Feedback = feedback; }

		string getFeedbackString() const;
		void setFeedback( const string& feedback );
		
		long getRoutingSequence() const { return m_RoutingSequence; }
		void setRoutingSequence( const long value ) { m_RoutingSequence = value; }
				
		bool isHeld() const { return m_HoldStatus; }
		void setHeld( const bool value ) { m_HoldStatus = value; }

		bool isBulk() const { return m_Bulk; }
		void setBulk( const bool value ) { m_Bulk = value; }

		bool isVirtual() const { return m_Virtual; }
		void setVirtual( const bool value ) { m_Virtual = value; }

		bool isDuplicate() const;

		bool isOutgoing()const { return m_IsOutgoing; }
		
		string getRequestorService() const { return m_RequestorService; }
		void setRequestorService( const string& value ) { m_RequestorService = value; }

		string getOriginalRequestorService() const { return m_OriginalRequestorService; }
		void setOriginalRequestorService( const string& value ) { m_OriginalRequestorService = value; }
		
		string getResponderService() const { return m_ResponderService; }
		void setResponderService( const string& value ) { m_ResponderService = value; }
		
		string getTableName() const { return m_TableName; }
		void setTableName( const string& value ) { m_TableName = value; }
		
		REQUEST_TYPE getRequestType() const { return m_RequestType; }
		void setRequestType( const REQUEST_TYPE value ) { m_RequestType = value; }
		
		const RoutingMessagePayload* const getPayload() const { return m_Payload; }
		RoutingMessagePayload* getPayload() { return m_Payload; }
		void setPayload( const string& payload );
		//void setPayload( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* payload );

		RoutingMessageEvaluator* getPayloadEvaluator( const bool forceReload = false );
		void destroyPayloadEvaluator();

		void setFastpath( const bool fastpath ) { m_Fastpath = fastpath; }
		bool getFastpath() const { return m_Fastpath; }
		
		string getOriginalPayload( const RoutingMessagePayload::PayloadFormat format = RoutingMessagePayload::AUTO ) const;
		
		void setDelayedReplyId( const string& idValue ){ m_DelayedReplyId = idValue; }
		string getDelayedReplyId() const { return m_DelayedReplyId; }
		void setGenerateId(){ setMessageOptions( RoutingMessageOptions::MO_GENERATEID ); };

		friend ostream& operator << ( ostream& os, const RoutingMessage& except );
};

#endif // ROUTINGMESSAGE_H
