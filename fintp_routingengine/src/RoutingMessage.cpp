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
	#define __MSXML_LIBRARY_DEFINED__
	#include "windows.h"
#else
	#include <unistd.h>
#endif

#include <vector>
#include <deque>

#include <xalanc/PlatformSupport/XSLException.hpp>
#include <xalanc/XPath/NodeRefList.hpp>
#include <xalanc/XalanDOM/XalanDOMString.hpp>
#include <xalanc/XercesParserLiaison/XercesDocumentWrapper.hpp>

#include "WorkItemPool.h"
#include "PlatformDeps.h"
#include "XmlUtil.h"
#include "StringUtil.h"
#include "Trace.h"
#include "Base64.h"

#include "RoutingMessage.h"
#include "RoutingDbOp.h"
#include "RoutingEngine.h"

const string RoutingMessage::MESSAGE_OUT = "O";

//RoutingMessagePayload implementation
RoutingMessagePayload::RoutingMessagePayload( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument& doc ) : 
	m_Document( NULL ), m_Text( "" ), m_IsTextValid( false ), m_IsDocValid( false )
{
	DEBUG2( "Payload CONSTRUCTOR" );
	setDoc( doc );
}

RoutingMessagePayload::RoutingMessagePayload( const string& doc ) :
	m_Document( NULL ),  m_Text( "" ), m_IsTextValid( false ), m_IsDocValid( false )
{
	DEBUG2( "Payload CONSTRUCTOR" );	
	setText( doc, RoutingMessagePayload::AUTO );
}

RoutingMessagePayload::RoutingMessagePayload( const RoutingMessagePayload& source ) : m_Document( NULL ),  m_Text( "" )
{
	DEBUG2( "Payload CONSTRUCTOR" );	
	if( source.IsTextValid() )
	{
		m_Text = source.getTextConst();
		m_IsTextValid = true;
	}
	else
		m_IsTextValid = false;
		
	if( source.IsDocValid() )
	{
		const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* const doc = source.getDocConst();
		if ( doc == NULL )
			m_IsDocValid = false;
		else
		{
			m_Document = dynamic_cast< XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* >( doc->cloneNode( true ) );
			m_IsDocValid = ( m_Document != NULL );
		}
	}
	else
		m_IsDocValid = false;
		
	m_Format = source.getFormat();
}

RoutingMessagePayload& RoutingMessagePayload::operator=( const RoutingMessagePayload& source )
{
	if ( this == &source )
		return *this;

	DEBUG2( "Payload CONSTRUCTOR=" );	
	if( source.IsTextValid() )
	{
		m_Text = source.getTextConst();
		m_IsTextValid = true;
	}
	else
		m_IsTextValid = false;
		
	if( m_Document != NULL )
	{
		m_Document->release();
		m_Document = NULL;
	}
		
	if( source.IsDocValid() )
	{
		const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* const doc = source.getDocConst();
		if ( doc == NULL )
			m_IsDocValid = false;
		else
		{
			m_Document = dynamic_cast< XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* >( doc->cloneNode( true ) );
			m_IsDocValid = ( m_Document != NULL );
		}
	}
	else
		m_IsDocValid = false;
		
	m_Format = source.getFormat();
	return *this;
}

RoutingMessagePayload::~RoutingMessagePayload()
{
	try
	{
		DEBUG2( "~RoutingMessagePayload" );
	}catch( ... ){};

	try
	{
	if ( m_Document != NULL )
	{
		try
		{
			if( m_IsDocValid )
			{
				DEBUG2( "Releasing valid payload doc [" << m_Document << "]" );
				m_Document->release();
			}
			else
			{
				DEBUG( "Releasing invalid payload doc [" << m_Document << "]" );
				m_Document->release();
			}
		}
		catch( ... )
		{
			TRACE( "An error occured while destroying DOM payload" );
		}
	}
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while destroying m_DocumentWrapper" );
		}catch( ... ){};
	}
	m_Document = NULL;
		
	//DEBUG( "DESTRUCTOR done" );
}

RoutingMessagePayload::PayloadFormat RoutingMessagePayload::convert( RoutingMessagePayload::PayloadFormat sourceFormat, RoutingMessagePayload::PayloadFormat destFormat, string& text )
{
	if ( sourceFormat == destFormat )
		return destFormat;

	switch( destFormat )
	{
		case RoutingMessagePayload::AUTO :
			// keep format
			break;
			
		case RoutingMessagePayload::BASE64 :
		
			text.assign( Base64::encode( text ) );
			break;
			
		case RoutingMessagePayload::PLAINTEXT :
		case RoutingMessagePayload::XML :
			
			if( sourceFormat == RoutingMessagePayload::BASE64 )
				text.assign( Base64::decode( text ) );
			break;
	}
	return destFormat;
}

void RoutingMessagePayload::convert( RoutingMessagePayload::PayloadFormat payloadformat )
{
	m_Format = convert( m_Format, payloadformat, m_Text );
}

void RoutingMessagePayload::setText( const string& text, RoutingMessagePayload::PayloadFormat payloadformat )
{	
	m_IsTextValid = true;
	m_IsDocValid = false;
	
	if( m_Document != NULL )
	{
		DEBUG( "Deleting old doc" );
		m_Document->release();
	}

	m_Document = NULL;
	
	DEBUG2( "Checking payload format ..." );
	m_Format = payloadformat;
	
	// check if it is base64 or xml
	if( payloadformat == RoutingMessagePayload::AUTO )
	{
		if( Base64::isBase64( text ) )
			m_Format = RoutingMessagePayload::BASE64;
		else
			m_Format = RoutingMessagePayload::PLAINTEXT;
	}
	m_Text = StringUtil::Trim( text );
}

void RoutingMessagePayload::setDoc( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument& doc )
{			
	m_Text = "";
	
	m_IsDocValid = true;	
	m_IsTextValid = false;
	
	if( m_Document != NULL )
		m_Document->release();
	m_Document = NULL;
		
	//m_Document = doc.cloneNode( true );

	// WARN if the original doc is release, this pointer will be dangling			
	m_Document = &doc;
}

void RoutingMessagePayload::setDoc( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc )
{			
	m_Text = "";
	m_IsDocValid = true;	
	m_IsTextValid = false;
	
	if( m_Document != NULL )
		m_Document->release();
	m_Document = NULL;
	
	//m_Document = doc.cloneNode( true );
	// WARN if the original doc is release, this pointer will be dangling
	m_Document = doc;
}

const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* const RoutingMessagePayload::getDocConst() const
{
	if( m_IsDocValid )
		return m_Document;
	return NULL;
}

string RoutingMessagePayload::getTextConst() const
{
	if( m_IsTextValid )
		return m_Text;
	return "";
}

XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* RoutingMessagePayload::getDoc( const bool throwOnDeserialize )
{
	if( m_IsDocValid )
		return m_Document;
		
	m_Document = NULL;
	
	if( m_IsTextValid )
	{
		try
		{
			convert( RoutingMessagePayload::XML );
			m_Document = XmlUtil::DeserializeFromString( m_Text );	
			DEBUG( "Document deserialized" );
			if ( m_Document == NULL )
				throw logic_error( "Empty document received" );
		}
		catch( const AppException& ex )
		{
			DEBUG( "Document cannot be deserialized to an XML [" << ex.getMessage() << "]" );
			DEBUG( "Text payload is [" << m_Text << "]" );
			
			if ( m_Document != NULL )
			{
				m_Document->release();
				m_Document = NULL;
			}
			
			if( throwOnDeserialize )
				throw;
		}
		catch( const std::exception& ex )
		{
			DEBUG( "Document cannot be deserialized to an XML [" << ex.what() << "]" );
			DEBUG( "Text payload is [" << m_Text << "]" );
			
			if ( m_Document != NULL )
			{
				m_Document->release();
				m_Document = NULL;
			}
			
			if( throwOnDeserialize )
				throw;
		}
		catch( ... )
		{
			DEBUG( "Document cannot be deserialized to an XML [unknown reason]" );
			DEBUG( "Text payload is [" << m_Text << "]" );
			
			if ( m_Document != NULL )
			{
				m_Document->release();
				m_Document = NULL;
			}
			
			if( throwOnDeserialize )
				throw;
		}
	}
	
	if( ( m_Document == NULL ) && throwOnDeserialize )
	{
		TRACE( "Attempt to deserialize resulted in an empty message" );
		throw runtime_error( "Attempt to deserialize resulted in an empty message" );
	}
	
	// now we know it's an XML
	m_Format = RoutingMessagePayload::XML;
	m_IsDocValid = true;
	return m_Document;
}

string RoutingMessagePayload::getText( const RoutingMessagePayload::PayloadFormat payloadformat, const bool throwOnSerialize )
{
	if ( m_IsTextValid )
	{
		convert( payloadformat );
		return m_Text;
	}
		
	if( m_IsDocValid )
	{
		try
		{
			m_Text = XmlUtil::SerializeToString( m_Document );
			m_Format = RoutingMessagePayload::XML;
			convert( payloadformat );
			
			DEBUG( "Document serialized" );
			if ( m_Text.length() == 0 )
				throw logic_error( "Empty document received" );
		}
		catch( ... )
		{
			DEBUG( "XML document cannot be serialized" );
			if( throwOnSerialize )
				throw;
		}
	}
	
	if( ( m_Text.length() == 0 ) && throwOnSerialize )
		throw runtime_error( "Attempt to serialize resulted in an empty message" );
	
	m_IsTextValid = true;
	return m_Text;
}

//RoutingMessage implementation
string RoutingMessage::getOriginalPayload( const RoutingMessagePayload::PayloadFormat payloadformat ) const
{
	string returnValue = RoutingDbOp::GetOriginalPayload( m_MessageId );
	RoutingMessagePayload::convert( RoutingMessagePayload::BASE64, payloadformat, returnValue );
	return returnValue;
}

RoutingMessage::REQUEST_TYPE RoutingMessage::ParseRequestType( const string& request )
{
	if ( ( request.size() == 0 ) || ( request == "SingleMessage" ) )
		return RoutingMessage::SingleMessage;
		
	if ( request == "Batch" )
		return RoutingMessage::Batch;
		
	stringstream errorMessage;
	errorMessage << "Invalid request type [" << request << "]";
	throw runtime_error( errorMessage.str() );
}

RoutingMessage::RoutingMessage() : 
	m_MessageId( "" ), m_BatchId( "" ), m_CorrelationId( "" ), m_SessionId( "" ), m_InputSessionId( "" ), m_UserId( "" ), m_Priority( 5 ), m_BatchSequence( 0 ), 
	m_BatchCount( 0 ), m_BatchAmount( "" ), m_RoutingSequence( 0 ), m_HoldStatus( false ), m_DelayedTransform( "" ), 
	m_DelayedTransformSessionCode( "" ), m_MessageOptions( RoutingMessageOptions::MO_NONE ), m_RequestorService( "dummyService" ), 
	m_ResponderService( "dummyService" ), m_OriginalRequestorService( "dummyService" ), m_RequestType( RoutingMessage::SingleMessage ), 
	m_Fastpath( false ), m_Bulk( false ), m_Virtual( false ), m_TableName( "" ), m_Payload( NULL ), m_PayloadEvaluator( NULL ), m_OriginalMessageType( "" ),
	m_DelayedReplyId( "" ), m_IsOutgoing( false )
{
}

RoutingMessage::RoutingMessage( const string& source ) : 
	m_MessageId( "" ), m_BatchId( "" ), m_CorrelationId( "" ), m_SessionId( "" ), m_InputSessionId( "" ), m_UserId( "" ), m_Priority( 5 ), m_BatchSequence( 0 ), 
	m_BatchCount( 0 ), m_BatchAmount( "" ), m_RoutingSequence( 0 ), m_HoldStatus( false ), m_DelayedTransform( "" ), 
	m_DelayedTransformSessionCode( "" ), m_MessageOptions( RoutingMessageOptions::MO_NONE ), m_RequestorService( "dummyService" ), 
	m_ResponderService( "dummyService" ),	m_OriginalRequestorService( "dummyService" ), m_RequestType( RoutingMessage::SingleMessage ), 
	m_Fastpath( false ), m_Bulk( false ), m_Virtual( false ), m_TableName( "" ), m_Payload( NULL ), m_PayloadEvaluator( NULL ), m_OriginalMessageType( "" ),
	m_DelayedReplyId( "" ),  m_IsOutgoing( false )
{
	try
	{
		m_Payload = new RoutingMessagePayload( source );
	}
	catch( ... )
	{
		if ( m_Payload != NULL )
		{
			delete m_Payload;
			m_Payload = NULL;
		}
		throw;
	}
}

RoutingMessage::RoutingMessage( const string& tableName, const string& messageId, bool delayRead ) : 
	m_MessageId( messageId ), m_BatchId( "" ), m_CorrelationId( "" ), m_SessionId( "" ), m_UserId( "" ), m_Priority( 5 ), 
	m_BatchSequence( 0 ), m_BatchCount( 0 ), m_BatchAmount( "" ), m_RoutingSequence( 0 ), m_HoldStatus( false ),
	m_DelayedTransform( "" ), m_DelayedTransformSessionCode( "" ), m_MessageOptions( RoutingMessageOptions::MO_NONE ),
	m_RequestorService( "dummyService" ), m_ResponderService( "dummyService" ),	m_OriginalRequestorService( "dummyService" ),
	m_RequestType( RoutingMessage::SingleMessage ), m_Fastpath( false ), m_Bulk( false ), m_Virtual( false ),
	m_TableName( tableName ), m_Payload( NULL ), m_PayloadEvaluator( NULL ), m_OriginalMessageType( "" ), m_DelayedReplyId( "" ),  m_IsOutgoing( false )
{
	if ( delayRead )
		return;
	else
	{
		Read();
	}
}

RoutingMessage::RoutingMessage( const string& tableName, const string& messageId, RoutingMessage ( *messageProviderCallback )(), bool fastpath ) :
	m_MessageId( messageId ), m_BatchId( "" ), m_CorrelationId( "" ), m_SessionId( "" ), m_UserId( "" ), m_Priority( 5 ), 
	m_BatchSequence( 0 ), m_BatchCount( 0 ), m_BatchAmount( "" ), m_RoutingSequence( 0 ), m_HoldStatus( false ),
	m_DelayedTransform( "" ), m_DelayedTransformSessionCode( "" ), m_MessageOptions( RoutingMessageOptions::MO_NONE ),	
	m_RequestorService( "dummyService" ), m_ResponderService( "dummyService" ),	m_OriginalRequestorService( "dummyService" ),
	m_RequestType( RoutingMessage::SingleMessage ), m_Fastpath( fastpath ), m_Bulk( false ), m_Virtual( false ), 
	m_TableName( tableName ), m_Payload( NULL ), m_PayloadEvaluator( NULL ), m_OriginalMessageType( "" ), m_DelayedReplyId( "" ), m_IsOutgoing( false )
{
	if ( messageProviderCallback == NULL )
	{
		if ( tableName.length() > 0 )
			Read();
		// else... return an empty message
	}
	else
	{
		DEBUG( "Requesting message" );
		RoutingMessage source = messageProviderCallback();
		
		//DEBUG( "Message received" );
		string payload = "";
		RoutingMessagePayload *sourcePayload = source.getPayload();
		if ( sourcePayload != NULL )
			payload = sourcePayload->getText( RoutingMessagePayload::AUTO );
		
		try
		{
			m_Payload = new RoutingMessagePayload( payload ); 
		
			m_MessageId = source.getMessageId();
			m_BatchId = source.getBatchId();
			m_CorrelationId = source.getCorrelationId();
			m_SessionId = source.getSessionId();
			m_Priority = source.getPriority();
			m_RoutingSequence = source.getRoutingSequence();
			m_HoldStatus = source.isHeld();
			setFeedback( source.getFeedback() );
			
			m_RequestorService = source.getRequestorService();
			m_ResponderService = source.getResponderService();
			m_OriginalRequestorService = source.getOriginalRequestorService();

			m_RequestType = source.getRequestType();
		}
		catch( ... )
		{
			if ( m_Payload == NULL )
			{
				delete m_Payload;
				m_Payload = NULL;
			}
			throw;
		}
	}
}

RoutingMessage::RoutingMessage( const RoutingMessage& source ) : 
	m_MessageId( source.getMessageId() ), m_BatchId( source.getBatchId() ),	m_CorrelationId( source.getCorrelationId() ), m_SessionId( source.getSessionId() ),
	m_UserId( source.getUserId() ), m_Priority( source.getPriority() ), m_BatchSequence( source.getBatchSequence() ), m_BatchCount( source.getBatchTotalCount() ), 
	m_BatchAmount( source.getBatchTotalAmount() ), m_RoutingSequence( source.getRoutingSequence() ), m_HoldStatus( source.isHeld() ),
	m_DelayedTransform( source.getDelayedTransform() ), m_DelayedTransformSessionCode( source.getDelayedTransformSessCode() ), 
	m_Feedback( source.getFeedback() ), m_MessageOptions( source.getMessageOptions() ), m_RequestorService( source.getRequestorService() ), 
	m_ResponderService( source.getResponderService() ), m_OriginalRequestorService( source.getOriginalRequestorService() ), 
	m_RequestType( source.getRequestType() ), m_Fastpath( source.getFastpath() ), m_Bulk( source.isBulk() ), m_Virtual( source.isVirtual() ),
	m_TableName( source.getTableName() ), m_Payload( NULL ), m_PayloadEvaluator( NULL ), m_OriginalMessageType( source.getOriginalMessageType() ), 
	m_DelayedReplyId( source.getDelayedReplyId() ),  m_IsOutgoing( source.isOutgoing() )
{
	try
	{
		const RoutingMessagePayload * const sourcePayload = source.getPayload();
		if ( sourcePayload == NULL )
			m_Payload = NULL;
		else
			m_Payload = new RoutingMessagePayload( *sourcePayload );
		setFeedback( source.getFeedback() ); 
	}
	catch( ... )
	{
		if ( m_Payload != NULL )
		{
			delete m_Payload;
			m_Payload = NULL;
		}
		throw;
	}
}

RoutingMessage& RoutingMessage::operator=( const RoutingMessage& source )
{
	if ( this == &source )
		return *this;

	//DEBUG( "OP=" );
	if ( m_Payload != NULL )
	{
		delete m_Payload;
		m_Payload = NULL;
	}

	try
	{
		const RoutingMessagePayload * const sourcePayload = source.getPayload();
		if ( sourcePayload == NULL )
			m_Payload = NULL;
		else
			m_Payload = new RoutingMessagePayload( *sourcePayload );

		m_PayloadEvaluator = NULL;
		m_MessageOptions = source.getMessageOptions();

		m_MessageId = source.getMessageId();
		m_BatchId = source.getBatchId();
		m_CorrelationId = source.getCorrelationId();
		m_SessionId = source.getSessionId();
		m_DelayedTransform = source.getDelayedTransform();
		m_DelayedTransformSessionCode = source.getDelayedTransformSessCode();

		m_UserId = source.getUserId();
		m_Priority = source.getPriority();
		m_BatchSequence = source.getBatchSequence();
		m_BatchCount = source.getBatchTotalCount();		

		m_BatchAmount = source.getBatchTotalAmount();		
		m_RoutingSequence = source.getRoutingSequence();
		m_HoldStatus = source.isHeld();
		m_Bulk = source.isBulk();
		m_Virtual = source.isVirtual();
		
		m_RequestorService = source.getRequestorService();
		m_ResponderService = source.getResponderService();
		m_OriginalRequestorService = source.getOriginalRequestorService();
		m_RequestType = source.getRequestType();
		m_Fastpath = source.getFastpath();
		m_TableName = source.getTableName();

		m_OriginalMessageType = source.getOriginalMessageType();
		m_DelayedReplyId = source.getDelayedReplyId();

		m_IsOutgoing = source.isOutgoing();

		setFeedback( source.getFeedback() );
	}
	catch( ... )
	{
		if ( m_Payload != NULL )
		{
			delete m_Payload;
			m_Payload = NULL;
		}
		throw;
	}

	//DEBUG( "OP= done" );	
	return *this;
}

void RoutingMessage::Read()
{
	DataSet* myData = NULL;
	try
	{
		// read the message from DB
		myData = RoutingDbOp::GetRoutingMessage( m_TableName, m_MessageId );
		
		DEBUG( "Reading payload" );
		string payload = StringUtil::Trim( myData->getCellValue( 0, "PAYLOAD" )->getString() );
		
		if ( m_Payload != NULL )
			delete m_Payload;
		m_Payload = new RoutingMessagePayload( payload ); 
		DEBUG( "Payload ok" );
		
		m_MessageId = StringUtil::Trim( myData->getCellValue( 0, "ID" )->getString() );
		m_BatchId = StringUtil::Trim( myData->getCellValue( 0, "BATCHID" )->getString() );
		m_CorrelationId = StringUtil::Trim( myData->getCellValue( 0, "CORRELATIONID" )->getString() );
		m_SessionId = StringUtil::Trim( myData->getCellValue( 0, "SESSIONID" )->getString() );
		m_InputSessionId = m_SessionId;
		m_Priority = myData->getCellValue( 0, "PRIORITY" )->getLong();
		m_RoutingSequence = myData->getCellValue( 0, "SEQUENCE" )->getLong();		
		m_HoldStatus = ( myData->getCellValue( 0, "HOLDSTATUS" )->getLong() == 0 ) ? false : true;
		setFeedback( StringUtil::Trim( myData->getCellValue( 0, "FEEDBACK" )->getString() ) );
		
		m_RequestorService = StringUtil::Trim( myData->getCellValue( 0, "REQUESTORSERVICE" )->getString() );
		m_ResponderService = StringUtil::Trim( myData->getCellValue( 0, "RESPONDERSERVICE" )->getString() );
		m_OriginalRequestorService = "";

		string requestType = StringUtil::Trim( myData->getCellValue( 0, "REQUESTTYPE" )->getString() );
		m_RequestType = ParseRequestType( requestType );

		string direction = StringUtil::Trim( myData->getCellValue( 0, "IOIDENTIFIER")->getString() );
		m_IsOutgoing = ( direction == RoutingMessage::MESSAGE_OUT ) ? true : false;
		
		DEBUG( "All message fields got" );
		if( myData != NULL )
		{
			delete myData;	
			myData = NULL;
		}
	}
	catch( const std::exception& ex )
	{
		TRACE( ex.what() );
		if ( m_Payload != NULL )
		{
			delete m_Payload;
			m_Payload = NULL;
		}
		if( myData != NULL )
		{
			delete myData;	
			myData = NULL;
		}
		throw;
	}
	catch( ... )
	{
		if ( m_Payload != NULL )
		{
			delete m_Payload;
			m_Payload = NULL;
		}
		if( myData != NULL )
		{
			delete myData;	
			myData = NULL;
		}
		throw;
	}
}

RoutingMessage::~RoutingMessage()
{
	try
	{
		DEBUG2( "~RoutingMessage" );
	}
	catch( ... ){};
	try
	{
		if ( m_Payload != NULL )
		{
			delete m_Payload;
			m_Payload = NULL;
		}
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while destroying payload" );
		}catch( ... ){};
	}
	
	try
	{
		destroyPayloadEvaluator();
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while destroying payload evaluator" );
		}catch( ... ){};
	}

	//DEBUG( "Payload deleted" );
}

string RoutingMessage::ToString( REQUEST_TYPE reqType )
{
	switch( reqType )
	{
		case RoutingMessage::Batch :
			return "Batch";
		case RoutingMessage::SingleMessage :
			return "SingleMessage";
			
		default :
			throw runtime_error( "Invalid request type" );
	}
}

void RoutingMessage::DelayTransform( const string& deltransform, const string& sessionCode, const unsigned int sequence )
{
	// a bulk transform will be delayed and applied on items
	m_DelayedTransform = deltransform;
	m_DelayedTransformSessionCode = sessionCode;
}

void RoutingMessage::setFeedback( const string& feedback )
{
	DEBUG( "Setting feedback [" << feedback << "]" );
	
	StringUtil parseFeedback = StringUtil( feedback );
	parseFeedback.Split( "|" );
	
	if( parseFeedback.MoreTokens() )
	{
		string firstPart = parseFeedback.NextToken();
		string secondPart = parseFeedback.NextToken();
		string thirdPart = ( parseFeedback.MoreTokens() ? parseFeedback.NextToken() : "" );
		
		//HACK find the length of the first part; if > 24, it's WMQID
		RoutingAggregationCode feedbackCode;
		
		// W|<msgid>|code = WMQID 
		RoutingMessageEvaluator::FeedbackProvider evalProvider;
		
		if ( thirdPart.length() > 0 )
		{
			evalProvider = RoutingMessageEvaluator::getProviderByName( firstPart );
		}
		else
		{
			TRACE( "DON'T USE THIS ANYMORE!!!! feedback must be in this form : <PROVIDER>|<CORRELATIONID>|<CODE>. Current value is [" << feedback << "]" );
			
			thirdPart = secondPart;
			secondPart = firstPart;
				
			// HACK for Adi... ( old RTGS connector not setting W| in feedback )
			if ( firstPart.length() == 32 )
				evalProvider = RoutingMessageEvaluator::FEEDBACKPROVIDER_MQ;
			else
				evalProvider = RoutingMessageEvaluator::FEEDBACKPROVIDER_UNK;	
				
#ifdef WIN32
			//_CrtDbgBreak();
#endif
		}

		setFeedback( RoutingMessageEvaluator::composeFeedback( secondPart, thirdPart, evalProvider ) );
	}
	else
	{
		stringstream errorMessage;
		errorMessage << "Invalid feedback received [" << feedback << "]";
		throw runtime_error( errorMessage.str() );
	}
}

void RoutingMessage::setPayload( const string& payload )
{
	if ( m_Payload != NULL )
	{
		delete m_Payload;
		m_Payload = NULL;
	}
	m_Payload = new RoutingMessagePayload( payload );
}

/*void RoutingMessage::setPayload( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* payload )
{
	if ( m_Payload != NULL )
	{
		delete m_Payload;
		m_Payload = NULL;
	}
	m_Payload = new RoutingMessagePayload( *payload );
}*/

string RoutingMessage::getFeedbackString() const
{
	try
	{
		return RoutingMessageEvaluator::serializeFeedback( m_Feedback );
	}
	catch( const std::exception& ex )
	{
		stringstream errorMessage;
		errorMessage << "Unable to serialize feedback [" << ex.what() << "]";
		throw runtime_error( errorMessage.str() );
	}
	catch( ... )
	{
		throw runtime_error( "Unable to serialize feedback [unknown reason]" );
	}
}

RoutingAggregationCode RoutingMessage::getAggregationCode()
{
	RoutingMessageEvaluator* evaluator = getPayloadEvaluator();
	
	// case 1 : message is plain text / some undistinguishable xml :
	// evaluator is set to NULL.. nothing to aggregate
	if ( evaluator == NULL )
	{
		// if we already have a response
		RoutingAggregationCode request( RoutingMessageEvaluator::AGGREGATIONTOKEN_FTPID, getCorrelationId() );
		return request;
	}
	
	RoutingAggregationCode feedback = getFeedback();
	DEBUG( "Feedback before agregating is [" << feedback.getCorrelToken() << "] = [" << feedback.getCorrelId() << "]" );
	
	RoutingAggregationCode request = evaluator->getAggregationCode( feedback );
 
	// aggregate some fields if this is not a reply
	if ( !isReply() )
	{
		request.setCorrelToken( RoutingMessageEvaluator::AGGREGATIONTOKEN_FTPID );
		request.setCorrelId( getCorrelationId() );
		RoutingMessagePayload* crtPayload = getPayload();
		if ( crtPayload != NULL )
			request.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_PAYLOAD, crtPayload->getText( RoutingMessagePayload::AUTO ) );
		else
			request.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_PAYLOAD, "" );

		request.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_REQUESTOR, getRequestorService() );
		request.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_BATCHID, getBatchId() );
						
		if( ( evaluator != NULL ) && ( evaluator->isBusinessFormat() ) && !evaluator->isEnrichFormat() )
		{
			request.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_TRN, evaluator->getField( InternalXmlPayload::TRN ) );
			request.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_ISSUER, evaluator->getField( InternalXmlPayload::SENDER ).substr( 0, 8 ) );
			request.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_OBATCHID, evaluator->getField( InternalXmlPayload::RELATEDREF  ) );
		}
	}
	return request;
}

string RoutingMessage::ToString( bool payloadEncoded )
{
	if( payloadEncoded )
		return ToString( m_Payload->getText( RoutingMessagePayload::BASE64 ) );
	else
		return ToString( m_Payload->getText( RoutingMessagePayload::AUTO ) );
}

string RoutingMessage::ToString( const string& payload )
{
	// quick xmlize
	stringstream message;
	message << "<?xml version=\"1.0\" encoding=\"us-ascii\"?><qPCMessageSchema><Message><Guid>";
	message << m_MessageId << "</Guid><Payload>";
	message << payload;
	message << "</Payload><BatchId>" << m_BatchId << "</BatchId><CorrelationId>";
	message << m_CorrelationId << "</CorrelationId><SessionId>";
	message << m_SessionId << "</SessionId><RequestorService>";
	message << m_RequestorService << "</RequestorService><ResponderService>";
	message << m_ResponderService << "</ResponderService><RequestType>";
	message << ToString( m_RequestType ) << "</RequestType><Priority>";
	message << m_Priority << "</Priority><HoldStatus>";
	message << m_HoldStatus << "</HoldStatus><Sequence>";
	message << m_RoutingSequence << "</Sequence><Feedback>";
	message << getFeedbackString();
	message << "</Feedback></Message></qPCMessageSchema>";
	
	return message.str();
}

bool RoutingMessage::isReply() 
{
	RoutingMessageEvaluator* evaluator = getPayloadEvaluator();
	
	if( ( evaluator != NULL ) && ( evaluator->isReply() ) )
		return true;
	
	// one more chance for this to be a reply : feedback code != 0 for fastpath messages
	if ( m_Fastpath )
	{
		string feedbackcode = getFeedback().getAggregationField( 0 );

		// not an incoming message
		return ( feedbackcode != RoutingMessageEvaluator::FEEDBACKFTP_MSG );
	}

	string correlToken = getFeedback().getCorrelToken();
	
	// one more chance for this to be an ack : MQfeedback != 0 && MQfeedback != FEEDBACKFTP_MSG
	if ( correlToken == RoutingMessageEvaluator::AGGREGATIONTOKEN_MQID )
	{
		string mqCode = getFeedback().getAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_MQCODE );
		return ( ( mqCode != "0" ) && ( mqCode != RoutingMessageEvaluator::FEEDBACKFTP_MSG ) );
	}
	
	return false;
}

bool RoutingMessage::isAck()
{
	RoutingMessageEvaluator* evaluator = getPayloadEvaluator();
	
	if ( evaluator != NULL )
	{
		if ( evaluator->isAck() )
			return true;

		// hack : in order to avoid untestable side-efects, test here if the achblk evaluator says ack/nack
		// should change this and if the evaluator is valid, the decision is its
		if ( evaluator->getEvaluatorType() == RoutingMessageEvaluator::ACHBLKACC )
			return false;
	}

	// if the message is fastpath, for TFD replies we have an WMQID correlation token
	if ( m_Fastpath )
	{
		string feedbackcode = getFeedback().getAggregationField( 0 );

		//not a received message, not an ack !!! the processing function must
		// override feedback to FEEDBACKFTP_MSG before setting it fastpath
		return ( feedbackcode == RoutingMessageEvaluator::FEEDBACKFTP_ACK );
	}

	// if the message is fastpath, for TFD replies we have an WMQID correlation token
	// one more chance for this to be an ack : WMQfeedback == 275
	if ( getFeedback().getCorrelToken() == RoutingMessageEvaluator::AGGREGATIONTOKEN_MQID )
	{
		return ( getFeedback().getAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_MQCODE ) == "275" );
	}

	// one more chance for this to be an ack : FINTP feedback == RoutingMessageEvaluator::FEEDBACKFTP_ACK
	string FinTPcode = getFeedback().getAggregationField( 0 );
	if ( FinTPcode == RoutingMessageEvaluator::FEEDBACKFTP_ACK )
		return true;

	return false;
}

bool RoutingMessage::isNack()
{
	RoutingMessageEvaluator* evaluator = getPayloadEvaluator();
	
	if( ( evaluator != NULL ) && ( evaluator->isNack() ) )
		return true;
	
	// if the message is fastpath, for TFD replies we have an WMQID correlation token
	if ( m_Fastpath )
	{
		string feedbackcode = getFeedback().getAggregationField( 0 );

		//not a received message, not an ack !!! the processing function must
		// override feedback to FEEDBACKFTP_MSG before setting it fastpath
		return ( ( feedbackcode != RoutingMessageEvaluator::FEEDBACKFTP_MSG ) && ( feedbackcode != RoutingMessageEvaluator::FEEDBACKFTP_ACK ) );
	}

	// one more chance for this to be a nack : WMQfeedback != 275|0
	if ( getFeedback().getCorrelToken() == RoutingMessageEvaluator::AGGREGATIONTOKEN_MQID )
	{
		string mqfeedback = getFeedback().getAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_MQCODE );
	
		if( ( mqfeedback == "275" ) || ( mqfeedback == "0" ) || ( mqfeedback == RoutingMessageEvaluator::FEEDBACKFTP_MSG ) )
			return false;
			
		return true;
	}

	// one more chance for this to be a nack : FINTP feedback == RoutingMessageEvaluator::FEEDBACKFTP_RJCT|RoutingMessageEvaluator::FEEDBACKFTP_LATERJCT
	try
	{
		string FinTPcode = getFeedback().getAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_FTPCODE );
		if ( ( FinTPcode == RoutingMessageEvaluator::FEEDBACKFTP_RJCT ) || ( FinTPcode == RoutingMessageEvaluator::FEEDBACKFTP_LATERJCT ) )
			return true;
	}
	catch( ... )
	{
		DEBUG( "FTPCODE not found. This message is not a reject." );
	}

	return false;
}

void RoutingMessage::destroyPayloadEvaluator()
{
	if ( m_PayloadEvaluator != NULL )
	{
		delete m_PayloadEvaluator;
		m_PayloadEvaluator = NULL;
	}
}

RoutingMessageEvaluator* RoutingMessage::getPayloadEvaluator( const bool forceReload )
{
	// no evaluation if fastpath set
	if( m_Fastpath )
		return NULL;
		
	if( forceReload )
	{
		DEBUG( "Resetting payload evaluator ( forced )" );
	}
	else
	{
		if( ( m_PayloadEvaluator != NULL ) && ( !m_PayloadEvaluator->isValid() ) )
		{
			DEBUG( "Payload evaluator was invalidated" );
			return NULL;
		}
	}
	
	if( forceReload && m_PayloadEvaluator != NULL )
	{
		destroyPayloadEvaluator();
	}
	
	if( m_PayloadEvaluator == NULL )
	{
		if ( m_Payload == NULL )
			m_PayloadEvaluator = NULL;
		else
		{
			m_PayloadEvaluator = RoutingMessageEvaluator::getEvaluator( m_Payload->getDoc( false ) );
		}
	
		if ( m_PayloadEvaluator != NULL )
		{
			//TODO switch it to message type convention
			string messageType = m_PayloadEvaluator->getField( InternalXmlPayload::MESSAGETYPE );
			RoutingKeywordMappings *keywordMappings = RoutingEngine::getRoutingMappings(); 
			
			// try to find message types mappings 
			RoutingKeywordMappings::const_iterator mtFinder = keywordMappings->find( messageType );
			if ( mtFinder == keywordMappings->end() )
			{
				stringstream errorMessage;
				errorMessage << "Message type [" << messageType << "] mappings not found in configuration. Empty keyword mappings returned";
			    TRACE( errorMessage.str() );
			}
			else
				m_PayloadEvaluator->setKeywordMappings( mtFinder->second );

			m_PayloadEvaluator->setIsoType( RoutingEngine::getRoutingIsoMessageType( messageType) );

			if ( m_PayloadEvaluator->isBusinessFormat() )
				m_OriginalMessageType = messageType;
			

			string evalFeedback = m_PayloadEvaluator->getOverrideFeedback();
			RoutingMessageEvaluator::FeedbackProvider evalFeedbackProvider = m_PayloadEvaluator->getOverrideFeedbackProvider();
			
			// if feeback is overriden by evaluator ( ex : tfd 012 -> RoutingMessageEvaluator::FEEDBACKFTP_ACK )
			if ( evalFeedback.length() > 0 ) 
			{
				// correlId, errorCode, feedbackProvider )
				string overrideId = m_PayloadEvaluator->getOverrideFeedbackId();

				// if an override correlationId was not provided, use the existing one
				if ( overrideId.length() == 0 )
					overrideId = m_Feedback.getCorrelId();
				m_Feedback = RoutingMessageEvaluator::composeFeedback( overrideId, evalFeedback, evalFeedbackProvider );
				DEBUG( "Feedback was overriden to: [" << m_Feedback.getCorrelToken() << "] = [" << m_Feedback.getCorrelId() << "]" );
			}
		}
	}
	
	return m_PayloadEvaluator;
}

bool RoutingMessage::isDuplicate() const
{
	if ( !RoutingEngine::shouldCheckDuplicates( m_RequestorService ) )
		return false;
	return ( RoutingDbOp::GetDuplicates( m_RequestorService, m_MessageId ) > 1 );
}

// RoutingMessageOptions implementation
int RoutingMessageOptions::Parse( const string& options )
{
	int returnedOptions = MO_NONE;
	
	// empty string = no options
	if ( options.length() == 0 )
		return returnedOptions;

	// split the string using '+' as separator
	StringUtil myOptions( options );
	myOptions.Split( "+" );
	string crtToken = myOptions.NextToken();
	
	while( crtToken.length() > 0 )
	{
		// match agains known options
		if ( crtToken == "MO_BYPASSMP" )
			returnedOptions |= MO_BYPASSMP;
			
		if ( crtToken == "MO_BATCH" )
			returnedOptions |= MO_BATCH;
		
		if ( crtToken == "MO_GENERATEID" )
			returnedOptions |= MO_GENERATEID;

		if ( crtToken == "MO_COMPLETE" )
			returnedOptions |= MO_COMPLETE;

		if ( crtToken == "MO_NOREACTIVATE" )
			returnedOptions |= MO_NOREACTIVATE;

		if ( crtToken == "MO_NOHEADERS" )
			returnedOptions |= MO_NOHEADERS;
			
		if ( crtToken == "MO_INSERTBATCH" )
			returnedOptions |= MO_INSERTBATCH;
				
		if ( crtToken == "MO_MARKNOTREPLIED" )
			returnedOptions |= MO_MARKNOTREPLIED;

		if ( crtToken == "MO_REPLYDATAGRAM" )
			returnedOptions |= MO_REPLYDATAGRAM;
		crtToken = myOptions.NextToken();		
	}
	
	return returnedOptions;
}

string RoutingMessageOptions::ToString( int options )
{
	stringstream message;
	
	// confirmation of delivery
	if ( ( options & MO_BYPASSMP ) == MO_BYPASSMP )
		message << "MO_BYPASSMP+";
		
	if ( ( options & MO_BATCH ) == MO_BATCH )
		message << "MO_BATCH+";

	if ( ( options & MO_GENERATEID ) == MO_GENERATEID )
		message << "MO_GENERATEID+";

	if ( ( options & MO_COMPLETE ) == MO_COMPLETE )
		message << "MO_COMPLETE+";

	if ( ( options & MO_NOREACTIVATE ) == MO_NOREACTIVATE )
		message << "MO_NOREACTIVATE+";

	if ( ( options & MO_NOHEADERS ) == MO_NOHEADERS )
		message << "MO_NOHEADERS+";
		
	if ( ( options & MO_INSERTBATCH ) == MO_INSERTBATCH )
		message << "MO_INSERTBATCH+";

	if ( ( options & MO_MARKNOTREPLIED ) == MO_MARKNOTREPLIED )
			message << "MO_MARKNOTREPLIED+";

	return message.str();
}

ostream& operator << ( ostream& os, const RoutingMessage& item )
{
	os << endl << "RoutingMessage" << endl << "--------------------------------------------" << endl;
	os << "\tGuid [" << item.getMessageId() << "]" << endl;
	const RoutingMessagePayload* const payload = item.getPayload();
	if ( payload != NULL )
		os << "\tText [" << payload->getTextConst() << "]" << endl;
	else
		os << "\tText [N/A-paylod is NULL]" << endl;
	os << "--------------------------------------------" << endl;

	return os;
}
