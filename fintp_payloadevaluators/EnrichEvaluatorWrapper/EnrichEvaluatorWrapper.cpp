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

#include "EnrichEvaluatorWrapper.h"
#include "XmlUtil.h"
#include <xalanc/XercesParserLiaison/XercesDocumentWrapper.hpp>

EnrichEvaluatorWrapper::EnrichEvaluatorWrapper( const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* document ) :
	RoutingMessageEvaluator( document, RoutingMessageEvaluator::FINTPENRCH )
{
	//Create inner DOMDocument
	DOMImplementation* impl = DOMImplementationRegistry::getDOMImplementation( unicodeForm( "LS" ) );
	m_InnerDoc.reset( impl->createDocument( 0, unicodeForm( "root" ), 0 ) );
	//Remove root element
	XERCES_CPP_NAMESPACE_QUALIFIER DOMNode* root = m_InnerDoc->getDocumentElement();
	if ( root != NULL )
	{
		m_InnerDoc->removeChild( root );
		root->release();
	}
	//Get inner message
	root = document->getDocumentElement();
	if ( root == NULL )
	{
		string errorMessage = "Root element of the enriched message is NULL ";
		TRACE( errorMessage );
		throw runtime_error( errorMessage );
	}
	XERCES_CPP_NAMESPACE_QUALIFIER DOMNode* key = root->getFirstChild();
	for ( ; key != 0; key = key->getNextSibling() )
	{
		if ( key->getNodeType() != DOMNode::ELEMENT_NODE )
			continue;
		else
			break;
	}
	if ( key == 0 )
	{
		stringstream errorMessage;
		errorMessage << "Bad type: Enriched root message, " << localForm( root->getNodeName() ) << " should have a DOMElement child. ";
		TRACE( errorMessage.str() );
		throw logic_error( errorMessage.str() );
	}

	//Append new root
	m_InnerDoc->appendChild( m_InnerDoc->importNode( key, true ) );

	DEBUG( "Evaluate Enriched inner message" );
	m_WrappedPayload = RoutingMessageEvaluator::getEvaluator( m_InnerDoc.get() );
	if ( m_WrappedPayload != NULL )
	{
		m_Document = m_WrappedPayload->getDocument();
		m_Namespace = m_WrappedPayload->getNamespace();

		if ( m_WrappedPayload->CheckPayloadType( RoutingMessageEvaluator::SWIFTXML ) )
			m_EvaluatorType = RoutingMessageEvaluator::SWIFTXML;
		string asdm_EnrichedPayload = XmlUtil::SerializeToString( m_Document );
		m_EnrichedPayload = XmlUtil::SerializeToString( document );
	}
}

const RoutingAggregationCode& EnrichEvaluatorWrapper::getAggregationCode( const RoutingAggregationCode& feedback )
{
	
	m_AggregationCode.Clear();
	//**Get aggregationCode from inner message*/
	m_AggregationCode = m_WrappedPayload->getAggregationCode( feedback );
	
	string crtPayload = XmlUtil::SerializeToString( m_WrappedPayload->getDocument() );
	m_AggregationCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_PAYLOAD, crtPayload );


	/*TODO
	 * Check if this is still needed for incoming messages 
	 */
	string tempEvaluateResult;

	tempEvaluateResult = XPathHelper::Evaluate( XPATH_SRCFIELNAME, m_EnrichedPayload, "urn:fintp:enrich.01" );
	m_AggregationCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_SRCFILENAME, tempEvaluateResult );

	if ( m_WrappedPayload->isBusinessFormat() )
	{ 
		m_AggregationCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_TRN, m_WrappedPayload->getField( InternalXmlPayload::TRN ) );
		m_AggregationCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_ISSUER, m_WrappedPayload->getField( InternalXmlPayload::SENDER ).substr( 0, 8 ) );
		//m_AggregationCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_OBATCHID, m_WrappedPayload->getField( InternalXmlPayload::RELATEDREF ) );
	}
	return m_AggregationCode;
}

string EnrichEvaluatorWrapper::internalGetField( const int field )
{
	return getField( field );
}

string EnrichEvaluatorWrapper::internalGetField( const string &field )
{
	if( ( m_WrappedPayload != NULL ) && ( m_WrappedPayload->getKeywordNames().size() <= 0 ) )
		setInnerKeywordMappings();

	if ( field == InternalEvaluatorWrapper::getFieldName( InternalEvaluatorWrapper::SOURCEFILENAME ) )
	{
		return getMessageField( XPATH_SRCFIELNAME );
	}

	if ( field == InternalXmlPayload::getFieldName( InternalXmlPayload::MESSAGETYPE ) )
	{
		return getMessageType();
	}
	return m_WrappedPayload->getField( field );
}

string EnrichEvaluatorWrapper::internalToString()
{
	string innerText = "";
	if ( m_WrappedPayload != NULL )
		innerText = XmlUtil::SerializeToString( m_WrappedPayload->getDocument() );
	return innerText;
}

void EnrichEvaluatorWrapper::UpdateMessage( RoutingMessage* message )
{
	if ( m_WrappedPayload != NULL )
		m_WrappedPayload->UpdateMessage( message );
}
string EnrichEvaluatorWrapper::getMessageType()
{
	const DOMElement* root = m_Document->getDocumentElement();
	for ( XERCES_CPP_NAMESPACE_QUALIFIER DOMNode* key = root->getFirstChild(); key != 0; key = key->getNextSibling() )
	{
		if ( key->getNodeType() != DOMNode::ELEMENT_NODE )
			continue;

		string messageType = localForm( key->getLocalName() );
		if ( messageType != "Message" && messageType != "Enrich" )
		{
			return messageType;
		}
	}
	
	return "FINTPENRICH";
}
string EnrichEvaluatorWrapper::getMessageField( string xpath )
{
	return XPathHelper::SerializeToString( XPathHelper::Evaluate( xpath, m_XalanDocument, m_Namespace ) );
}