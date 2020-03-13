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

#ifndef ROUTINGRULE_H
#define ROUTINGRULE_H

#include "RoutingEngineMain.h"
#include "RoutingActions.h"
#include "RoutingEvaluators.h"
#include "RoutingStructures.h"

class ExportedTestObject RoutingRule
{
	private :

		long m_Id;
		long m_Sequence;
		long m_SchemaId;
		RoutingQueue m_Queue;
		
		// conditions for this rule
		vector< RoutingCondition > m_Conditions;
		
		// action to take on the message
		RoutingAction m_Action;
		
		string m_Outcome;
		string m_Description;
		
	public:

		RoutingRule( long id, long schemaId, long sequence, string sessionCode );
		~RoutingRule();
		
		//accessors
		long getQueueId() const { return m_Queue.getId(); }
		long getSchemaId() const { return m_SchemaId; }
		string getQueueName() const { return m_Queue.getName(); }
		
		long getSequence() const { return m_Sequence; }
		
		const RoutingAction& getAction() const { return m_Action; }
		const vector< RoutingCondition >& getConditions() const { return m_Conditions; }
		
		//returns true if the rule was performed
		RoutingAction::ROUTING_ACTION Route( RoutingMessage* message, const int userId, bool bulk = false ) const;

		string getOutcome() const { return m_Outcome; }
};

#endif
