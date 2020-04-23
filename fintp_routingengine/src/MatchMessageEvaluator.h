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

#ifndef MATCHMESSAGEEVALUATOR_H
#define MATCHMESSAGEEVALUATOR_H

#include <boost/regex.hpp>
#include "DataParameter.h"
#include "RoutingMessage.h"

class MatchXmlPayload
{
	
	public :

		static MatchXmlPayload* getMatchPayload( RoutingMessageEvaluator* evaluator, const string& messageId, const string& correlationId );
		virtual void insertMessage( const string& location, const string& hash, const string& hashId ) = 0;
		virtual ~MatchXmlPayload()
		{
			m_InternalFields.clear();
		}


	protected :

		MatchXmlPayload( const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc, const string& messageId, const string& correlationId, const string& messageType ) :
				m_MessageId( messageId ), m_CorrelationId( correlationId ), m_MessageType( messageType ), m_ConfirmedSP( "" ), m_UnconfirmedSP( "" ), 
				m_DocumentWrapper( NULL ), m_XalanDocument( NULL ), m_Document( doc )
		{

		}

		typedef pair< string, string > Field; // pair of <name, path> 
		typedef pair< Field, string > MatchKeyword; // each Field with his regex selector

		vector< string > m_InternalFields;
		static const MatchKeyword m_InternalFieldsMeta []; 

		string m_MessageId, m_CorrelationId, m_Reference, m_MessageType;
		string m_ConfirmedSP, m_UnconfirmedSP;

		XALAN_CPP_NAMESPACE_QUALIFIER XercesDocumentWrapper *m_DocumentWrapper;
		XALAN_CPP_NAMESPACE_QUALIFIER XalanDocument *m_XalanDocument;
		const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument *m_Document;

		void setInternalFields();
		void loadConfigs();

};


class TreasuryMatch : public MatchXmlPayload
{
	private :
		
		enum TREASURYFIELDS
		{
			BIC = 0,
			TRADEDATE = 1,
			VALUEDATE = 2
		};

	public :

		TreasuryMatch( const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc, const string& messageId, const string& correlationId, const string& messageType );
		void insertMessage( const string& location, const string& hash, const string& hashId );
};

#endif MATCHMESSAGEEVALUATOR_H
