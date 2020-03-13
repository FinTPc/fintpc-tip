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

#include <xalanc/XercesParserLiaison/XercesDocumentWrapper.hpp>

#include "WSRM/SequenceAcknowledgement.h"

#include "SwiftXmlPayload.h"
#include "Trace.h"
#include "Currency.h"
#include "StringUtil.h"

const string SwiftXmlPayload::m_MessageTypePath = "//sg:ApplicationHeader/@MessageType";
 
SwiftXmlPayload::SwiftXmlPayload( const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* document ) :
	RoutingMessageEvaluator( document, RoutingMessageEvaluator::SWIFTXML )
{
	m_SequenceResponse = NULL;
	DEBUG( "Initialized SwiftXmlPayload evaluator with message type: " << getField( InternalXmlPayload::MESSAGETYPE ) );
}

SwiftXmlPayload::~SwiftXmlPayload()
{
	try
	{
		if ( m_SequenceResponse != NULL )
			delete m_SequenceResponse;
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while deleting sequence response" );
		} catch( ... ) {}
	}
}

string SwiftXmlPayload::internalToString()
{
	return XmlUtil::SerializeToString( m_Document );
}

string SwiftXmlPayload::internalGetField( const string& field )
{
	if( field == InternalXmlPayload::getFieldName( InternalXmlPayload::MESSAGETYPE ) )
	{
		return getMessageType();
	}
	else if( field == InternalXmlPayload::getFieldName( InternalXmlPayload::OBATCHID ) )
	{
		string messageType = getField( InternalXmlPayload::MESSAGETYPE );
		return ( ( messageType == "RPN" ) || ( messageType == "RCQ" ) || ( messageType == "RBE" ) ) ? getField( InternalXmlPayload::RELATEDREF ) : "";
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


const vector<string> SwiftXmlPayload::getKeywordNames()
{
	vector<string> keywords = RoutingMessageEvaluator::getKeywordNames();
	
	// add internal overidden keywords from internalGetFields
	// keywords.push_back( InternalXmlPayload::getFieldName( InternalXmlPayload::OBATCHID ) );
	
	return keywords;
}

const RoutingAggregationCode& SwiftXmlPayload::getAggregationCode( const RoutingAggregationCode& feedback )
{
	string messageType = getField( InternalXmlPayload::MESSAGETYPE );
	// TFD codes
	if ( messageType == "012" )
	{
		m_AggregationCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_TFDCODE, RoutingMessageEvaluator::FEEDBACKFTP_ACK );

		switch( m_TFDACKMethod )
		{
			case CorrelationOptions::TMETHOD_MQID :
			{
				m_AggregationCode.setCorrelToken( RoutingMessageEvaluator::AGGREGATIONTOKEN_MQID );
				m_AggregationCode.setCorrelId( feedback.getCorrelId() );

				DEBUG( "Using MQID to match FIN ack to original message" );
			}
			break;

			case CorrelationOptions::TMETHOD_CORRELID :
			{
				m_AggregationCode.setCorrelToken( RoutingMessageEvaluator::AGGREGATIONTOKEN_FTPID );
				m_AggregationCode.setCorrelId( feedback.getCorrelId() );

				DEBUG( "Using CORRELID to match FIN ack to original message" );
			}
			break;

			default :
			{
				string mir = getCustomXPath( "//smt:MessageText/smt:tag106/@tagValue" );
				if ( mir.length() > 6 )
				{
					m_AggregationCode.setCorrelToken( RoutingMessageEvaluator::AGGREGATIONTOKEN_MIR );

					mir = mir.substr( 6 );
					DEBUG( "Message MIR is [" << mir << "]" );

					m_AggregationCode.setCorrelId( mir );
					m_AggregationCode.addAggregationCondition( RoutingMessageEvaluator::AGGREGATIONTOKEN_TFDCODE + " IS NULL", "" );
				}
				else
				{
					TRACE( "Message MIR could not be obtained. Falling back to using WMQ id" );
				}
			}
			break;
		}

		return m_AggregationCode;
	}

	if ( messageType == "019" )
	{
		string errCode = getCustomXPath( "//smt:MessageText/smt:tag432/@tagValue" );
		DEBUG( "TFD error code is [" << errCode << "]" );

		m_AggregationCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_TFDCODE, errCode );

		switch( m_TFDACKMethod )
		{
			case CorrelationOptions::TMETHOD_MQID :
			{
				m_AggregationCode.setCorrelToken( RoutingMessageEvaluator::AGGREGATIONTOKEN_MQID );
				m_AggregationCode.setCorrelId( feedback.getCorrelId() );

				DEBUG( "Using MQID to match FIN nack to original message" );
			}
			break;

			case CorrelationOptions::TMETHOD_CORRELID :
			{
				m_AggregationCode.setCorrelToken( RoutingMessageEvaluator::AGGREGATIONTOKEN_FTPID );
				m_AggregationCode.setCorrelId( feedback.getCorrelId() );

				DEBUG( "Using CORRELID to match FIN nack to original message" );
			}
			break;

			default :
			{
				string mir = getCustomXPath( "//smt:MessageText/smt:tag106/@tagValue" );
				if ( mir.length() > 6 )
				{
					m_AggregationCode.setCorrelToken( RoutingMessageEvaluator::AGGREGATIONTOKEN_MIR );

					mir = mir.substr( 6 );
					DEBUG( "Message MIR is [" << mir << "]" );

					m_AggregationCode.setCorrelId( mir );
					m_AggregationCode.addAggregationCondition( RoutingMessageEvaluator::AGGREGATIONTOKEN_TFDCODE + " IS NULL", "" );
				}
				else
				{
					TRACE( "Message MIR could not be obtained. Falling back to using WMQ id" );
				}
			}
			break;
		}
		return m_AggregationCode;
	}

	if ( feedback.getCorrelToken() == RoutingMessageEvaluator::AGGREGATIONTOKEN_MQID )
	{
		// find ack/nack code and who sent it
		string acknack;
		string IOIdentifier = getCustomXPath( "//sg:ApplicationHeader//@IOIdentifier" );
		// only look for acks in input messages and messages matched by correlid
		if ( ( IOIdentifier == "I" ) || ( m_SwiftACKMethod == CorrelationOptions::METHOD_CORRELID ) )
			acknack = getCustomXPath( "//sg:AckNack/sg:AckNackMessageText/sg:tag451/@tagValue" );
		DEBUG( "Message is [" << acknack << "] ( 0=ACK, 1=NACK, empty=neither )" );

		m_AggregationCode.setCorrelToken( RoutingMessageEvaluator::AGGREGATIONTOKEN_MQID );
		m_AggregationCode.setCorrelId( feedback.getCorrelId() );

		// test for ack/nack
		if( acknack.size() > 0 )
		{
			switch ( m_SwiftACKMethod )
			{
				case CorrelationOptions::METHOD_TRN :
				{
					m_AggregationCode.setCorrelToken( RoutingMessageEvaluator::AGGREGATIONTOKEN_TRN );

					string trn = getField( InternalXmlPayload::TRN );
					string issuer = getField( InternalXmlPayload::SENDER );
					DEBUG( "Using ACK matching method [TRN] value is [" << trn << "]" );
					m_AggregationCode.setCorrelId( trn );
					m_AggregationCode.addAggregationCondition( RoutingMessageEvaluator::AGGREGATIONTOKEN_SWIFTCODE + " IS NULL", "" );
					m_AggregationCode.addAggregationCondition( RoutingMessageEvaluator::AGGREGATIONTOKEN_ISSUER, issuer.substr( 0,8 ) );

				}
				break;

				case CorrelationOptions::METHOD_CORRELID :
				{
					m_AggregationCode.setCorrelToken( RoutingMessageEvaluator::AGGREGATIONTOKEN_FTPID );
					DEBUG( "Using ACK matching method [CORRELID] value is [" << feedback.getCorrelId() << "]" );
				}
				break;

				default:
					break;
			}

			// swift codes
			if( acknack == "0" )
				m_AggregationCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_SWIFTCODE, "0" );
			else
			{
				string nackCode = getCustomXPath( "//sg:AckNack/sg:AckNackMessageText/sg:tag405/@tagValue" );
				m_AggregationCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_SWIFTCODE, nackCode );
			}

			string mir = getCustomXPath( "//sg:AckNack/sg:AckNackBasicHeader/@AckNackBasicHeaderText" );
			m_AggregationCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_MIR, mir );

			return m_AggregationCode;
		}

		// must be local SAA ack / ( incoming message ? )
		string mqCode = feedback.getAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_MQCODE );

		// if it is a reply from SAA
		if ( mqCode != "0" )
		{
			switch( m_SAAACKMethod )
			{
				case CorrelationOptions::SAA_METHOD_TRN :
				{
					// save MQID
					m_AggregationCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_MQID, feedback.getCorrelId() );
					m_AggregationCode.setCorrelToken( RoutingMessageEvaluator::AGGREGATIONTOKEN_TRN );
					string trn = getField( InternalXmlPayload::TRN );

					DEBUG( "Using SAAACK matching method [TRN] value is [" << trn << "]" );
					m_AggregationCode.setCorrelId( trn );
				}
				break;

				case CorrelationOptions::SAA_METHOD_CORRELID :
				{
					// save MQID ( for the record = correlid )
					m_AggregationCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_MQID, feedback.getCorrelId() );

					// just trick aggregationmanager into making the update "where ftpid='...'"
					m_AggregationCode.setCorrelToken( RoutingMessageEvaluator::AGGREGATIONTOKEN_FTPID );

					DEBUG( "Using SAAACK method [CORRELID] value is [" << feedback.getCorrelId() << "]" );
				}
				break;

				default:
					break;
			}

			m_AggregationCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_SAACODE, mqCode );
		}

		return m_AggregationCode;
	}

	return m_AggregationCode;
}

bool SwiftXmlPayload::isReply()
{
	string messageType = getField( InternalXmlPayload::MESSAGETYPE );
	// TFD codes
	if ( ( messageType == "012" ) || ( messageType == "019" ) )
		return true;

	// if this is an output message, it cannot be an ack/nack
	if ( getCustomXPath( "//sg:ApplicationHeader//@IOIdentifier" ) == "O" )
		return false;

	// find ack/nack code and who sent it
	string acknack = getCustomXPath( "//sg:AckNack/sg:AckNackMessageText/sg:tag451/@tagValue" );
	if( acknack.size() > 0 )
		return true;

	return false;
}

bool SwiftXmlPayload::isAck()
{
	string messageType = getField( InternalXmlPayload::MESSAGETYPE );
	// TFD codes
	if ( messageType == "012" )
		return true;

	if ( messageType == "019" )
		return false;

	// if this is an output message, it cannot be an ack/nack
	if (getCustomXPath( "//sg:ApplicationHeader//@IOIdentifier" ) == "O" )
		return false;

	// find ack/nack code and who sent it
	string acknack = getCustomXPath( "//sg:AckNack/sg:AckNackMessageText/sg:tag451/@tagValue" );
	if( acknack.size() == 0 )
		return false;

	if( acknack == "0" )
		return true;

	return false;
}

bool SwiftXmlPayload::isNack()
{
	string messageType = getField( InternalXmlPayload::MESSAGETYPE );
	// TFD codes
	if ( messageType == "012" )
		return false;

	if ( messageType == "019" )
		return true;

	// if this is an output message, it cannot be an ack/nack
	if ( getCustomXPath( "//sg:ApplicationHeader//@IOIdentifier" ) == "O" )
		return false;

	// find ack/nack code and who sent it
	string acknack = getCustomXPath( "//sg:AckNack/sg:AckNackMessageText/sg:tag451/@tagValue" );
	if( acknack.size() == 0 )
		return false;

	if( acknack != "0" )
		return true;

	return false;
}

bool SwiftXmlPayload::isBatch()
{
	return false;
}

string SwiftXmlPayload::getOverrideFeedback()
{
	string messageType = getField( InternalXmlPayload::MESSAGETYPE );
	// TFD codes
	if ( messageType == "012" )
		return RoutingMessageEvaluator::FEEDBACKFTP_ACK;

	if ( messageType == "019" )
		return getCustomXPath( "//smt:MessageText/smt:tag432/@tagValue" );

	// if this is an output message, it cannot be an ack/nack
	if ( getCustomXPath( "//sg:ApplicationHeader//@IOIdentifier" ) == "O" )
		return RoutingMessageEvaluator::FEEDBACKFTP_MSG;

	return "";
}

// standards version
wsrm::SequenceResponse* SwiftXmlPayload::getSequenceResponse()
{
	//string batchId = getField( InternalXmlPayload::GROUPID );
	if ( m_SequenceResponse != NULL )
		return m_SequenceResponse;

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

bool SwiftXmlPayload::updateRelatedMessages()
{
	if ( getCustomXPath( "//sg:ApplicationHeader//@IOIdentifier" ) != "O" )
		return false;
	
	string originalBatchId = getField( InternalXmlPayload::OBATCHID );
	return ( originalBatchId.length() > 0 );
}

RoutingAggregationCode SwiftXmlPayload::getBusinessAggregationCode()
{
	RoutingAggregationCode businessReply( RoutingMessageEvaluator::AGGREGATIONTOKEN_TRN, getField( InternalXmlPayload::TRN ) );

	string originalBatchId = getField( InternalXmlPayload::OBATCHID );

	if( originalBatchId.length() > 0 )
	{
		string messageType = getField( InternalXmlPayload::MESSAGETYPE );
		string currency = getField( InternalXmlPayload::CURRENCY );
		string amount = getField( InternalXmlPayload::AMOUNT );

		businessReply.addAggregationCondition( RoutingMessageEvaluator::AGGREGATIONTOKEN_BATCHID, originalBatchId );

		Currency amountParsed( amount );
		if ( amountParsed.isZero() )
		{
			businessReply.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_TFDCODE, RoutingMessageEvaluator::FEEDBACKFTP_REFUSE );
		}
		else
		{
			businessReply.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_SWIFTCODE,	RoutingMessageEvaluator::FEEDBACKFTP_REFUSE );
		}
	}
	return businessReply;
}

void SwiftXmlPayload::UpdateMessage( RoutingMessageBase* message )
{
	DEBUG2( "Updating message of type [" << messageType << "] ... " );

	string messageType = getField( InternalXmlPayload::MESSAGETYPE );
	// gsrs set-ups
	if ( StringUtil::StartsWith( messageType, "FI" ) )
	{
		DEBUG( "Setting options for FIxxx messages" );

		message->setBatchId( getField( InternalXmlPayload::TRN ) );
		message->setBatchSequence( StringUtil::ParseULong( getField( InternalXmlPayload::SEQ ) ) );
		message->setBatchTotalCount( StringUtil::ParseULong( getField( InternalXmlPayload::MAXSEQ ) ) );

		//generate a message id
		message->setGenerateId();
		
	}
}

bool SwiftXmlPayload::delayReply()
{
	string messageType = getField( InternalXmlPayload::MESSAGETYPE );
	return ( messageType == "012" ) || ( messageType == "019" );
}

string SwiftXmlPayload::getMessageType()
{
	return XPathHelper::SerializeToString( XPathHelper::Evaluate( SwiftXmlPayload::m_MessageTypePath, m_XalanDocument ) );
}
