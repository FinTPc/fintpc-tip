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
#ifndef SWIFTXMLPAYLOAD_H
#define SWIFTXMLPAYLOAD_H

#include "WSRM/SequenceResponse.h"
#include "RoutingMessageEvaluator.h"

using namespace FinTP;

class SwiftXmlPayload : public RoutingMessageEvaluator
{
	public :

		SwiftXmlPayload( const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* document );
		~SwiftXmlPayload();

	private :

		static const string m_MessageTypePath;
		wsrm::SequenceResponse* m_SequenceResponse;
		
		string getMessageType();
		string internalGetField( const string& field );
		const vector<string> getKeywordNames(); 

	protected :

		string internalToString();

	public:

		const RoutingAggregationCode& getAggregationCode( const RoutingAggregationCode& feedback );

		bool isReply();
		bool isAck();
		bool isNack();
		bool isBatch();

		string getOverrideFeedback();

		// standards version
		wsrm::SequenceResponse* getSequenceResponse();

		// Visitor method on message
		void UpdateMessage( RoutingMessageBase* message );
		bool updateRelatedMessages();
		RoutingAggregationCode getBusinessAggregationCode();
		bool delayReply();
};

#endif // SWIFTXMLPAYLOAD_H
