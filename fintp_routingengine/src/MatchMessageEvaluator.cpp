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

#include "MatchMessageEvaluator.h"
#include "RoutingDbOp.h"
#include "RoutingEngine.h"

#define TREASURY_FIELDS_COUNT 11

const MatchXmlPayload::MatchKeyword MatchXmlPayload::m_InternalFieldsMeta[TREASURY_FIELDS_COUNT] = { MatchKeyword( Field( "RREF", "//smt:Seq_15A/smt:tag21/@tagValue"), "" ),
	MatchKeyword( Field( "OPTYPE", "//smt:Seq_15A/smt:tag22A/@tagValue"), "" ),
	MatchKeyword( Field( "BIC", "//smt:Seq_15A/smt:tag87A/@tagValue"), "" ),
	MatchKeyword( Field( "TRADEDATE", "//smt:Seq_15B/smt:tag30T/@tagValue"), "" ),
	MatchKeyword( Field( "VALUEDATE", "//smt:Seq_15B/smt:tag30V/@tagValue"), "" ),
	MatchKeyword( Field( "PARTYA", "//smt:Seq_15A/smt:tag82A/@tagValue"), "" ),
	MatchKeyword( Field( "PARTYB", "//smt:Seq_15A/smt:tag87A/@tagValue"), "" ) ,
	//just MT300
	MatchKeyword( Field( "AMOUNTBOUGHT", "//smt:Seq_15B/smt:tag32B/@tagValue"), "^(N?)([A-Z]{3})([\\d\\.,]*)" ),
	MatchKeyword( Field( "AMOUNTSOLD", "//smt:Seq_15B/smt:tag33B/@tagValue"), "^(N?)([A-Z]{3})([\\d\\.,]*)" ),
	// just MT320
	MatchKeyword( Field( "PRINCIPALAMT", "//smt:Seq_15B/smt:tag32B/@tagValue"), "^(N?)([A-Z]{3})([\\d\\.,]*)" ),
	MatchKeyword( Field( "INTERESTAMT", "//smt:Seq_15B/smt:tag34E/@tagValue"), "^(N?)([A-Z]{3})([\\d\\.,]*)" )
};
	
void MatchXmlPayload::loadConfigs()
{
	
	if( RoutingEngine::TheRoutingEngine->GlobalSettings.getSettings().ContainsKey( "Match.Confirmed." + m_MessageType + ".InsertSP" ) )
		m_ConfirmedSP = ( RoutingEngine::TheRoutingEngine->GlobalSettings[ "Match.Confirmed." + m_MessageType + ".InsertSP" ] );
	else if(  RoutingEngine::TheRoutingEngine->GlobalSettings.getSettings().ContainsKey( "Match.Confirmed.InsertSP" ) )
		m_ConfirmedSP = ( RoutingEngine::TheRoutingEngine->GlobalSettings[ "Match.Confirmed.InsertSP" ] );
	else 
		m_ConfirmedSP = "";

	if( RoutingEngine::TheRoutingEngine->GlobalSettings.getSettings().ContainsKey( "Match.Unconfirmed." + m_MessageType + ".InsertSP" ) )
		m_UnconfirmedSP = ( RoutingEngine::TheRoutingEngine->GlobalSettings[ "Match.Unconfirmed." + m_MessageType + ".InsertSP" ] );
	else if(  RoutingEngine::TheRoutingEngine->GlobalSettings.getSettings().ContainsKey( "Match.Unconfirmed.InsertSP" ) )
		m_UnconfirmedSP = ( RoutingEngine::TheRoutingEngine->GlobalSettings[ "Match.Unconfirmed.InsertSP" ] );
	else 
		m_UnconfirmedSP = "";
}

MatchXmlPayload* MatchXmlPayload::getMatchPayload( RoutingMessageEvaluator* evaluator, const string& messageId, const string& correlationId )
{
	const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc = evaluator->getDocument();

	MatchXmlPayload* payload = NULL;
	try
	{
		payload = new TreasuryMatch( doc, messageId, correlationId, evaluator->getField( InternalXmlPayload::MESSAGETYPE ) );
		payload->loadConfigs();

		if( ( payload->m_Reference ).empty() )
			payload->m_Reference = evaluator->getField( InternalXmlPayload::OBATCHID );
			//payload->m_Reference = evaluator->getField(InternalXmlPayload::TRN);
	}
	catch( std::exception& ex )
	{
		TRACE( "Match evaluation failed: [" << ex.what() << "]");
	}
	catch( ... )
	{
		TRACE( "Match evaluation failed");
	}

	return payload;			
}
/*
MatchXmlPayload::MatchXmlPayload( const MatchXmlPayload& source )
{
	m_MessageId = source.m_MessageId;
	m_CorrelationId = source.m_CorrelationId;
	m_Reference = source.m_Reference;
	m_MessageType = source.m_MessageType;
}
*/
void MatchXmlPayload::setInternalFields()
{
	if( m_Document == NULL )
		throw logic_error( "Document is NULL and cannot be evaluate for matching" );

	string fieldName = "";
	try 
	{
		XALAN_USING_XALAN( XercesDocumentWrapper );
		XALAN_USING_XALAN( XalanDocument );
		
		// map xerces dom to xalan document
		if ( m_XalanDocument == NULL )
		{
#ifdef XALAN_1_9
			XALAN_USING_XERCES( XMLPlatformUtils )
			m_DocumentWrapper = new XercesDocumentWrapper( *XMLPlatformUtils::fgMemoryManager, m_Document, true, true, true );
#else
			m_DocumentWrapper = new XercesDocumentWrapper( m_Document, true, true, true );
#endif
			m_XalanDocument = ( XalanDocument* )m_DocumentWrapper;
				
			if( m_XalanDocument == NULL )
				throw runtime_error( "Unable to parse payload for match evaluation." );

			string result;
			for( int i = 0; i < TREASURY_FIELDS_COUNT ; i++ )
			{
				Field fieldDesc = m_InternalFieldsMeta[ i ].first;
				fieldName = fieldDesc.first;
				string strSelector =  m_InternalFieldsMeta[ i ].second;
				try
				{
					result = XPathHelper::SerializeToString( XPathHelper::Evaluate( fieldDesc.second, m_XalanDocument ) );
				}
				catch( ... )
				{
					result = "";
					TRACE( "Cannot evaluate match field [" << fieldName << "] for message type [" << m_MessageType << "]" );
				}

				if( strSelector.empty() )
				{
					m_InternalFields.push_back( result );
					DEBUG( "Set internal match field [" << fieldName << "] = [" << result << "]" ); 
				}
				else
				{
					//TODO: generalisation for different selectors type 
					boost::regex selector( strSelector );
					DEBUG( "Match amount selector, [" << strSelector << "] to value, [" << result << "]." );
					string amountSign, currency, amount;
					if( !result.empty() )
					{
						boost::match_results<std::string::const_iterator> results;
						if( boost::regex_match( result, results, selector ) && results.size() == 4 )
						{
							amountSign = ( results[1] == "N" ) ? "-" : "";
							currency = results[2];
							amount = results[3];
						}
					}
					DEBUG( "Set internal match field [" << fieldName << "] with currency [" << currency << "] and amount [" << amountSign << amount << "]" ); 
					m_InternalFields.push_back( amountSign.append( amount ) );
					m_InternalFields.push_back( currency );
				}
			}
		}
	}
	catch( const std::exception& ex )
	{
		TRACE( "Error while evaluating internal match field [" << fieldName << "] : " << ex.what() );
		throw ex;
	}
	catch( ... )
	{
		TRACE( "Error while evaluating internal match field [" << fieldName << "] : Unknown reason" );
		throw;

	}
}

//TreasuryMatch definition
TreasuryMatch::TreasuryMatch( const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc, const string& messageId, const string& correlationId, const string& messageType ) : 
	MatchXmlPayload( doc, messageId, correlationId, messageType )
{
	this->setInternalFields();
}

void TreasuryMatch::insertMessage( const string& location, const string& hash, const string& hashId )
{
	string spName = m_UnconfirmedSP;
	if( !hashId.empty() )
		spName = m_ConfirmedSP;

	RoutingDbOp::insertMatchMessage( spName, location, m_MessageId, m_CorrelationId, m_Reference, hash, m_MessageType, hashId, m_InternalFields );
}