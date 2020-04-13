#pragma once
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

#ifndef APPINTERFACESTSPAYLOAD_H
#define APPINTERFACESTSPAYLOAD_H

#include "WSRM/SequenceResponse.h"

#include "RoutingMessageEvaluator.h"

#define XPATH_INTERFACEFEEDBACK "//x:AppInterfaceInfo/x:InterfaceFeedback/text()"
#define XPATH_DESTFILENAME "//x:AppInterfaceInfo/x:InterfaceDetails/x:Filename/text()"
#define XPATH_BATHCID "//x:MessageInfo/x:GrpHdr/x:BatchId/text()"

class AppInterfaceStsPayload : public RoutingMessageEvaluator
{
public:
	AppInterfaceStsPayload( const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* document );
	~AppInterfaceStsPayload();

private:

	wsrm::SequenceResponse* m_SequenceResponse;

	string m_BatchID;

	/** 
	m_TrxNo = 0 for Batch Message
	m_TrxNo = 1 for SingelMessage
	m_TrxNo > 1 for Aggregate Message*/
	int m_TrxNo;

	string internalGetField(const string& field);
	string getMessageField(string xpath);

	string getMessageType() { return "APPINTERFACEREPLY"; }

protected:

	string internalToString();

public:

	const RoutingAggregationCode& getAggregationCode( const RoutingAggregationCode& feedback );
	RoutingMessageEvaluator::FeedbackProvider getOverrideFeedbackProvider();
	bool isReply() { return true; }
	bool isAck();
	bool isNack();
	bool isBatch() { return ( m_TrxNo == 0 ); }
	bool isDD() { return false; }

	string getOverrideFeedback();

	// standards version
	wsrm::SequenceResponse* getSequenceResponse();
};

#endif // APPINTERFACESTSPAYLOAD_H
