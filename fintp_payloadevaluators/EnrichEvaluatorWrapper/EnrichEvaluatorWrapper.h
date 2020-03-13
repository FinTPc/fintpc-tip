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

#ifndef ENRICHEVALUATORWRAPPER_H
#define ENRICHEVALUATORWRAPPER_H

#include <boost/scoped_ptr.hpp>

#include "WSRM/SequenceResponse.h"

#include "RoutingMessageEvaluator.h"
#include "RoutingMessage.h"

#include "Trace.h"

#define XPATH_SRCFIELNAME "//x:filenameEntity/x:filename/text()"

//EnrichEvaluatorWrapper implementation
// 1. If first element is !BusinessFormat return evaluator as is (EnrichedEvaluatorWrapper)
// 2. After Transformation and forced reevaluation m_WrappedPayload overtake m_Document
// 3. Finally RoutingMesage will have a 
//		m_PayloadEvaluator of type EnrichedEvaluatorWrapper that delegates everything to m_WrappedPayload
//		m_Payload with EnrichDocument envelope
// m_namespace value should always be syncronised with m_wrappedPayload
class EnrichEvaluatorWrapper: public RoutingMessageEvaluator
{
	
	private:
	
		RoutingMessageEvaluator* m_WrappedPayload;
		string m_EnrichedPayload;
		boost::scoped_ptr<XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument> m_EnrichDoc;
		boost::scoped_ptr<XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument> m_InnerDoc;

		static const string m_MessageTypePath;

		string getMessageField( string xpath );
		
	public :

		EnrichEvaluatorWrapper( const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* document );
		~EnrichEvaluatorWrapper()
		{
			try
			{
				if ( m_WrappedPayload != NULL )
				{
					delete m_WrappedPayload;
					m_WrappedPayload = NULL;
				}
			}
			catch( ... )
			{
				TRACE( "An error occured while destroying payload" );
			}
		}
	
		bool isReply(){ return false; }
		bool isAck(){ return false; }
		bool isNack(){ return false; }

		bool isBatch()
		{ 
			return m_WrappedPayload == NULL ? false : m_WrappedPayload->isBatch(); 
		}
		bool isDD()
		{
			return  m_WrappedPayload == NULL ? false : m_WrappedPayload->isDD(); 
		}
		
		void UpdateMessage( RoutingMessage* message );
		
		/**
		 * Gets sequence response after validation.
		 * \return	null if it fails, else the sequence response.
		 */
		wsrm::SequenceResponse* getSequenceResponse(){ return NULL; }
		const RoutingAggregationCode& getAggregationCode( const RoutingAggregationCode& feedback );
		void setInnerKeywordMappings() { m_WrappedPayload->setKeywordMappings( m_KeywordMappings ); };
		string getMessageType();

	protected:
		
		string internalToString();
		string internalGetFieldName( const int field ) { return InternalXmlPayload::getFieldName ( field ); }

		/**
		 * Return envelope fields if neccesary
		**/
		string internalGetField( const int field );	
		string internalGetField( const string  &field);

};
#endif // ENRICHEDEVALUATORWRAPPER_H
