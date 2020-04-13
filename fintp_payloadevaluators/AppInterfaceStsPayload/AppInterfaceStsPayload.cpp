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

#include "AppInterfaceStsPayload.h"
#include "XmlUtil.h"
#include "Trace.h"
#include "Currency.h"

AppInterfaceStsPayload::AppInterfaceStsPayload( const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* document ) 
	: RoutingMessageEvaluator( document, RoutingMessageEvaluator::APPINTERFACEREPLY)
{

	XERCES_CPP_NAMESPACE_QUALIFIER DOMNodeList * trxNodesList = document->getElementsByTagName( unicodeForm( "TrxInfo") );

	if( trxNodesList != NULL )
		m_TrxNo = trxNodesList->getLength();
	else
		m_TrxNo = 0;

	m_SequenceResponse = NULL;
}

AppInterfaceStsPayload::~AppInterfaceStsPayload()
{
	try
	{
		if (m_SequenceResponse != NULL)
			delete m_SequenceResponse;
		m_SequenceResponse = NULL;
	}
	catch (...)
	{
		try
		{
			TRACE("An error occured while deleting sequence response");
		}
		catch (...) {}
	}
}

string AppInterfaceStsPayload::internalToString()
{
	return XmlUtil::SerializeToString(m_Document);
}

string AppInterfaceStsPayload::internalGetField(const string& field)
{
	if ( field == InternalXmlPayload::getFieldName( InternalXmlPayload::MESSAGETYPE ) )
		return getMessageType();

	if ( field == InternalAppInterfacePayload::getFieldName( InternalAppInterfacePayload::INTERFACEFEEDBACK ) )
	{
		return getMessageField( XPATH_INTERFACEFEEDBACK );
	}

	if ( field == InternalAppInterfacePayload::getFieldName( InternalAppInterfacePayload::DESTINATIONFILENAME ) )
	{
		return getMessageField( XPATH_DESTFILENAME );
	}

	if ( field == InternalAppInterfacePayload::getFieldName( InternalAppInterfacePayload::BATCHID ) )
	{
		if ( m_BatchID.empty() )
			m_BatchID = getMessageField( XPATH_BATHCID );

		return m_BatchID;
	}
	return "";
}

const RoutingAggregationCode& AppInterfaceStsPayload::getAggregationCode(const RoutingAggregationCode& feedback)
{

	m_AggregationCode.Clear();

	string correlId = feedback.getCorrelId();
	string correlToken;

	if ( isBatch() )
	{
		correlToken = InternalAppInterfacePayload::getFieldName( InternalAppInterfacePayload::BATCHID );
		correlId = internalGetField( InternalAppInterfacePayload::getFieldName( InternalAppInterfacePayload::BATCHID ) );
	} 
	else
	{
		correlToken = feedback.getCorrelToken();
		correlId = feedback.getCorrelId();
	}
	m_AggregationCode.setCorrelToken(correlToken);
	m_AggregationCode.setCorrelId(correlId);

	m_AggregationCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_DSTFILENAME, internalGetField( InternalAppInterfacePayload::getFieldName(InternalAppInterfacePayload::DESTINATIONFILENAME ) ) );

	return m_AggregationCode;
}

string AppInterfaceStsPayload::getOverrideFeedback()
{
	return "0";
}
RoutingMessageEvaluator::FeedbackProvider AppInterfaceStsPayload::getOverrideFeedbackProvider()
{

	if ( isBatch() )
		return RoutingMessageEvaluator::FEEDBACKPROVIDER_FTP;

	return RoutingMessageEvaluator::FEEDBACKPROVIDER_MQ;;
}
bool AppInterfaceStsPayload::isAck()
{
	return false;
}

bool AppInterfaceStsPayload::isNack()
{
	return false;
}
// standards version
wsrm::SequenceResponse* AppInterfaceStsPayload::getSequenceResponse()
{
	m_SequenceResponse = new wsrm::SequenceAcknowledgement("");

	if ( m_SequenceResponse != NULL )
		m_SequenceResponse->setIdentifier( internalGetField( InternalAppInterfacePayload::getFieldName( InternalAppInterfacePayload::BATCHID ) ) );
	
	return m_SequenceResponse;
}

string AppInterfaceStsPayload::getMessageField( string xpath )
{
	return XPathHelper::SerializeToString( XPathHelper::Evaluate( xpath, m_XalanDocument, "urn:fintp:interface-reply.01" ) );
}