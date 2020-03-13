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

#include <sstream>
#include <deque>

#include "WSRM/SequenceAcknowledgement.h"
#include "WSRM/SequenceFault.h"

#include "IsoXmlPayload.h"
#include "XmlUtil.h"
#include "Trace.h"
#include "Currency.h"

IsoXmlPayload::IsoXmlPayload( const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* document ) : m_DDFieldXPath( "//x:OrgnlMsgNmId/text()" ), m_DDFieldValue( "pacs.003.001.02" ),
	m_DDType( 2 ), RoutingMessageEvaluator( document, RoutingMessageEvaluator::ISOXML )
{
	m_SequenceResponse = NULL;
}

IsoXmlPayload::~IsoXmlPayload()
{
	try
	{
		if ( m_SequenceResponse != NULL )
			delete m_SequenceResponse;
		m_SequenceResponse = NULL;
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while deleting sequence response" );
		} catch( ... ) {}
	}
}

string IsoXmlPayload::internalToString()
{
	return XmlUtil::SerializeToString( m_Document );
}

string IsoXmlPayload::internalGetField( const string& field )
{
	if( field == InternalXmlPayload::getFieldName( InternalXmlPayload::MESSAGETYPE ) )
	{
		return getMessageType();
	}
	else if( field == InternalXmlPayload::getFieldName( InternalXmlPayload::OBATCHID ) )
	{
		//TODO: Looks like it can be set as database keyword for specified message types
		string messageType = getField( InternalXmlPayload::MESSAGETYPE );
		return ( ( messageType == "FIToFIPmtCxlReq" ) || ( messageType == "RsltnOfInvstgtn" ) || ( messageType == "PmtRtr" ) ) ? getField( InternalXmlPayload::RELATEDREF ) : "";
	}
	else if( field == InternalXmlPayload::getFieldName( InternalXmlPayload::SEQ ) )
	{
		string messageType = getField( InternalXmlPayload::MESSAGETYPE );
		string xPath = GetKeywordXPath( messageType, field );
		return ( xPath.length() > 0 ) ? XPathHelper::SerializeToString( XPathHelper::Evaluate( xPath, m_XalanDocument, m_Namespace ) ) : "1";
	}

	else if( field == InternalXmlPayload::getFieldName( InternalXmlPayload::MAXSEQ ) )
	{
		string messageType = getField( InternalXmlPayload::MESSAGETYPE );
		string xPath = GetKeywordXPath( messageType, field );
		return ( xPath.length() > 0 ) ? XPathHelper::SerializeToString( XPathHelper::Evaluate( xPath, m_XalanDocument, m_Namespace ) ) : "1";
	}
	else
	{
		string messageType = getField( InternalXmlPayload::MESSAGETYPE );
		string xPath = GetKeywordXPath( messageType, field );
		string value = XPathHelper::SerializeToString( XPathHelper::Evaluate( xPath, m_XalanDocument, m_Namespace ) );
		return EvaluateKeywordValue( messageType, value, field );
	}

	throw invalid_argument( "Unknown field requested [" + field + "]" );
}

const RoutingAggregationCode& IsoXmlPayload::getAggregationCode( const RoutingAggregationCode& feedback )
{
	
	m_AggregationCode.Clear();
	
	string correlId = feedback.getCorrelId();
	string correlToken = feedback.getCorrelToken();
	
	m_AggregationCode.setCorrelToken( correlToken );
	m_AggregationCode.setCorrelId( correlId );
	
	/*TODO
	 * Check if this is still needed for incoming messages 
	 */
	if ( correlToken == RoutingMessageEvaluator::AGGREGATIONTOKEN_MQID )
	{
		string wmqCode = feedback.getAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_MQCODE );
		if ( wmqCode == RoutingMessageEvaluator::FEEDBACKFTP_MSG )
		{
			m_AggregationCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_SAACODE, wmqCode );
		}
	}
	
	return m_AggregationCode;
}

bool IsoXmlPayload::isReply()
{
	return false;
}

bool IsoXmlPayload::isAck()
{
	return false;
}

bool IsoXmlPayload::isNack()
{
	return false;
}

bool IsoXmlPayload::isBatch()
{
	return false;
}

bool IsoXmlPayload::isDD()
{
	if ( m_DDType != 2 )
		return ( m_DDType == 1 );
	
	string msgType = getField( InternalXmlPayload::MESSAGETYPE );
	string DDFieldValue = XPathHelper::SerializeToString( XPathHelper::Evaluate( m_DDFieldXPath, m_XalanDocument, m_Namespace ) );
	if( ( msgType == "PmtRtr" ) && ( DDFieldValue == m_DDFieldValue ) )
		m_DDType = 1;
	else
		m_DDType = 0;

	return( m_DDType == 1 );
}

string IsoXmlPayload::getOverrideFeedback()
{
	return "";
}

// standards version
wsrm::SequenceResponse* IsoXmlPayload::getSequenceResponse()
{
	//string batchId = getField( InternalXmlPayload::GROUPID );
	if ( m_SequenceResponse != NULL )
		return m_SequenceResponse;
		
	string messageType = getField( InternalXmlPayload::MESSAGETYPE );

	if ( isAck() )
	{
		m_SequenceResponse = new wsrm::SequenceAcknowledgement( "" );
		
		wsrm::AcknowledgementRange ackrange( 0, 1 );
		m_SequenceResponse->AddChild( ackrange );

		return m_SequenceResponse;
	}
	
	if ( isNack() )
	{
		m_SequenceResponse = new wsrm::SequenceAcknowledgement( "" );
		
		wsrm::AcknowledgementRange nackrange( 0, 1 );
		m_SequenceResponse->AddChild( nackrange );
		
		return m_SequenceResponse;
	}

	return NULL;
}

