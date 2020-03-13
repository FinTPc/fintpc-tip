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

#include "Message.h"

#include "Trace.h"
#include "MQ/MqFilter.h"
#include "XSLT/XSLTFilter.h"
#include "StringUtil.h"
#include "XPathHelper.h"

#include <xalanc/XercesParserLiaison/XercesDocumentWrapper.hpp>

using namespace FinTPMessage;

// Metadata implementation
Metadata::Metadata() :	m_Id( "" ), m_GroupId( "" ), m_Format( "" ), m_Length( 0 )
{
}

Metadata::Metadata( const string& messageId, const string& messageGroupId, const unsigned long messageLength ) :
	m_Id( messageId ), m_GroupId( messageGroupId ), m_Format( "" ), m_Length( messageLength )
{
}

Metadata::Metadata( const Metadata& metadata )
{
	m_GroupId = metadata.groupId();
	m_Id = metadata.id();
	m_Format = metadata.format();
	m_Length = metadata.length();
}

// Message implementation
/*Message::Message( const FinTPMessage::Metadata metadata, XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* dom, const string payload , const string correlationId , const string requestor , const string responder, const string feedback ) : 
	m_Metadata( metadata ), m_Dom( dom ), m_Payload( payload ), m_CorrelationId( correlationId ), m_Requestor( requestor ), m_Responder( responder ), m_Feedback( feedback )
{	
}*/

Message::Message() : m_Dom( NULL ), m_Payload( "" ), m_CorrelationId( "" ), m_Requestor( "" ), m_Responder( "" ), m_Feedback( "" )
{
}

Message::Message( const Metadata& messageMetadata, const string& messagePayload, const string& messageCorrelId, 
	const string& messageRequestor, const string& messageResponder, const string& messageFeedback ) : 
	m_Metadata( messageMetadata ), m_Payload( messagePayload ), m_CorrelationId( messageCorrelId ), 
	m_Requestor( messageRequestor ), m_Responder( messageResponder ), m_Feedback( messageFeedback )
{
	//build the dom document 
	DOMImplementation* impl = DOMImplementationRegistry::getDOMImplementation( unicodeForm( "LS" ) );
	m_Dom = impl->createDocument( 0, unicodeForm( "root" ), 0 );	
}

Message::~Message()
{
	try
	{
		releaseDom();
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while releasing DOM" );
		} catch( ... ){}
	}
}

void Message::releaseDom()
{
	if( m_Dom != NULL )
	{
		m_Dom->release();
		m_Dom = NULL;
	}
}

void Message::getInformation( const Message::ConnectorType connType )
{
	//getting payload, requestor, ....
	try
	{
		XALAN_USING_XALAN( XercesDocumentWrapper );
		XALAN_USING_XALAN( XalanDocument );

		// map xerces dom to xalan document
#ifdef XALAN_1_9
		XALAN_USING_XERCES( XMLPlatformUtils )
		XercesDocumentWrapper docWrapper( *XMLPlatformUtils::fgMemoryManager, m_Dom, true, true, true );
#else
		XercesDocumentWrapper docWrapper( m_Dom, true, true, true );
#endif
		XalanDocument* theDocument = ( XalanDocument* )&docWrapper;		
		
		{ // Xalan required block
			DEBUG( "Connector type is : [" << connType << "]" );
			switch ( connType ) 
			{
				case Message::FILECONNECTOR :
					m_Feedback = XPathHelper::SerializeToString( XPathHelper::Evaluate( "/qPCMessageSchema/Message/Feedback/child::text()", theDocument ) ); 
					DEBUG2( "Feedback is : [" << m_Feedback << "]" );
					break;

				case Message::DBCONNECTOR :
					m_Requestor = XPathHelper::SerializeToString( XPathHelper::Evaluate( "/qPCMessageSchema/Message/RequestorService/child::text()", theDocument ) );
					DEBUG2( "Requestor is : [" << m_Requestor << "]" );
					m_Responder = XPathHelper::SerializeToString( XPathHelper::Evaluate( "/qPCMessageSchema/Message/ResponderService/child::text()", theDocument ) );
					DEBUG2( "Responder is : [" << m_Responder << "]" );
					break;

				case Message::MQCONNECTOR :
				case Message::ACHCONNECTOR :
					break; //nothing special for this connector 

				default : 
					TRACE( "Connector type is unknown [" << connType << "]" );
					break;
			}
			
			//correlationid is required by all publisher	
			m_CorrelationId = XPathHelper::SerializeToString( XPathHelper::Evaluate( "/qPCMessageSchema/Message/CorrelationId/child::text()", theDocument ) );
			DEBUG2( "CorrelationId is : [" << m_CorrelationId << "]" );
					
			//payload is required by all publisher	
			m_Payload = XPathHelper::SerializeToString( XPathHelper::Evaluate( "/qPCMessageSchema/Message/Payload/child::text()", theDocument ) );
			DEBUG2( "Payload is : [" << m_Payload << "]" );
		}
	}
	catch( const std::exception& error )
	{
		TRACE( "A " << typeid( error ).name() << " error [" << error.what() << "] occured while getting info about message" );
		
		releaseDom();			
		throw;
	}
	catch( ... )
	{
		TRACE( "An unknown error occured while getting info about message" );
		
		releaseDom();  			
		throw;
	}
}
