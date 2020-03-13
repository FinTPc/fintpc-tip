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

#ifndef ROUTINGPLAN_H
#define ROUTINGPLAN_H

#include "RoutingRule.h"
#include "RoutingStructures.h"

/* 
Eligible rules for a plan :
- no condition on message / function condition from start to end

A RoutingPlan is defined by :
- start queue, end queue
- start message seq, end message seq
- routing actions in between
- conditions
*/
class RoutingPlan
{
	private :

		long m_StartQueue, m_EndQueue;
		long m_StartSequence, m_EndSequence;
		
		vector< RoutingRule > m_Rules;
		
		vector< RoutingCondition > m_ForkConditions;
		vector< RoutingCondition > m_FailConditions;
		vector< RoutingCondition > m_SuccessConditions;

		RoutingExitpoint m_Exitpoint;

		RoutingPlan *m_SuccessPlan, *m_FailPlan, *m_RootPlan;

		bool m_IsTerminal, m_IsNormalized;
		string m_Name;

	public:

		RoutingPlan( const long startQueue, const long endQueue, const long startSeq, const long endSeq, const string& name = "unnamed" );
		~RoutingPlan( void );

		long startQueue() const { return m_StartQueue; }
		long endQueue() const { return m_EndQueue; }

		void setEndQueue( const long eQueue ) { m_EndQueue = eQueue; }

		void setExitpoint( const RoutingExitpoint& exitpoint ) { m_Exitpoint = exitpoint; }
		const RoutingExitpoint& getExitpoint() const { return m_Exitpoint; }

		void setTerminal( const bool value ) { m_IsTerminal = value; }
		const bool isTerminal() const { return m_IsTerminal; }

		unsigned long size() const { return m_Rules.size(); }
		const vector< RoutingRule >& getRules() const { return m_Rules; }

		void addRule( const RoutingRule& rule ) { m_Rules.push_back( rule ); }

		void setForkConditions( const vector< RoutingCondition >& conditions );
		const vector< RoutingCondition >& getForkConditions() const { return m_ForkConditions; }

		RoutingPlan* getSuccessPlan() { return m_SuccessPlan; }
		RoutingPlan* getFailPlan() { return m_FailPlan; }

		const string name() const { return m_Name; }
		void setName( const string& planname ) { m_Name = planname; }

		// combines conditions and flattens branches with plansize = 0 
		RoutingPlan* normalize( const string& name );
		RoutingPlan* normalizeRules( const string& name );
		void normalizeConditions( RoutingPlan* plan ) const;
		const bool normalized() const { return m_IsNormalized; }

		void displayNested( const unsigned level );
		void displayNormalized( const unsigned level );
		void display( const unsigned int level );

		bool usable( RoutingMessage * message ) const;
		RoutingPlan* getRootPlan() { return m_RootPlan; }
};

#endif //ROUTINGPLAN_H
