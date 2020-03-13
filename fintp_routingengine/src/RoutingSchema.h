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

#ifndef ROUTINGSCHEMA_H
#define ROUTINGSCHEMA_H

#ifdef WIN32
	/*#ifdef _DEBUG
		//#define _CRTDBG_MAP_ALLOC
		#include <stdlib.h>
		#include <crtdbg.h>
	#endif*/
#endif

#include "RoutingEngineMain.h"
#include "RoutingJob.h"
#include "RoutingRule.h"
#include "RoutingPlan.h"
#include "RoutingStructures.h"

class ExportedTestObject RoutingSchema
{
	private : 
		
		bool m_Dirty;
		bool m_Copy;

		string m_SchemaName;
		string m_Description;
		
		// the vector is ordered by sequence
		map< long, vector< RoutingRule > > m_RulesByQueue;
		map< long, vector< RoutingRule > > m_InitRulesByQueue;
		map< long, vector< RoutingRule > > m_TearRulesByQueue;

		map< string, RoutingPlan* > m_Plans;
		map< long, RoutingPlan* > m_UsablePlans;

		vector< RoutingMessage > m_RBatchMessages;
		
#if defined( WIN32 ) && defined( _DEBUG )
		_CrtMemState m_NextMemoryState, m_OldMemoryState;
		bool m_OldStateAvailable;
#endif // WIN32 && _DEBUG
	
		map< long, int > m_InnerSchemas;
		
	public:
	
		//default .ctor
		RoutingSchema();
		
		void Load();
		void LoadSchema( const string& schemaName, long schemaId, const string& sessionCode );

		RoutingSchema* Duplicate( bool createPlans = false );
		
		// destructor
		~RoutingSchema();
		
		string getName() const { return m_SchemaName; }
				
		//methods
		void Explain( void );
		void DisplayPlan( RoutingPlan* plan, unsigned int level );
	
		bool ApplyPlanRouting( RoutingJob* job, RoutingMessage* theMessage, const RoutingPlan* plan, const int userId, const bool isBulk, const bool fastpath = false ) const;
		bool ApplyQueueRouting( RoutingJob* job, RoutingMessage* theMessage, const long queueId, const int userId, const bool isBulk, const bool fastpath = false );

		void ApplyRouting( RoutingJob* job, RoutingMessage* theMessage, RoutingMessage ( *messageProviderCallback )() = NULL, bool fastpath  = false );
		void ApplyRouting( RoutingJob* job, RoutingMessage( *messageProviderCallback )() = NULL, bool fastpath = false );
		/*
		 * Process batch messages
		 * 1. Batch acks with/no transaction
		 * 2. Batch nacks with/no transactions
		 * 3. Batch not acks/ not nacks
		 * Constraint every ack/nack batch with no transactions should have a not NULL SequenceResponse
		 */
		bool RouteBatchReply( RoutingJob* job, RoutingMessage* theMessage, bool fastpath = false );
		bool RouteBatch( RoutingJob* job, RoutingMessage* theMessage, bool fastpath = false );
		
		void PerformInitRoutine( long schemaId );
		void PerformTearRoutine( long schemaId );

		// plans
		void DeletePlans();
		void CreatePlans();
		RoutingPlan* CreatePlan( RoutingPlan* plan, const long queue, const long sequence, const unsigned char depth );
		void InsertPlan( RoutingPlan* plan );

		const map< string, RoutingPlan* >& Plans() const { return m_Plans; }
		RoutingPlan* Plan( const string& name );
		void UsePlan( const string& name );
		RoutingPlan* getPlan( long queue, RoutingMessage* theMessage );

		const vector< RoutingMessage >& GetRBatchItems() const { return m_RBatchMessages; }

		bool isDirty() const { return m_Dirty; }
		void setDirty( const bool value ) { m_Dirty = value; }

		bool RouteDelayedReply( RoutingJob* job, RoutingMessage* theMessage, bool fastpath = false );
};

#endif
