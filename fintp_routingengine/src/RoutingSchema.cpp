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

#include <iomanip>
#include <omp.h>

#include "StringUtil.h"
#include "Trace.h"
#include "LogManager.h"
#include "DataSet.h"
#include "XSD/XSDFilter.h"
#include "XSD/XSDValidationException.h"

#include "WSRM/SequenceFault.h"
#include "WSRM/SequenceAcknowledgement.h"

//#include "PayloadEvaluators/SwiftXmlPayload.h"
//#include "PayloadEvaluators/AchBlkAccPayload.h"
#include "RoutingSchema.h"
#include "RoutingDbOp.h"
#include "RoutingExceptions.h"
#include "RoutingStructures.h"
#include "RoutingEngine.h"

#define PLAN_MAX_DEPTH 10

#if defined( WIN32 ) && defined( _OPENMP ) && !defined(__ICL) && ( _MSC_VER < 1500 )
#pragma comment(linker, "\"/manifestdependency:type='Win32' name='Microsoft.VC80.DebugOpenMP' version='8.0.50727.42' processorArchitecture='X86' publicKeyToken='1fc8b3b9a1e18e3b' language='*'\"")
#endif

RoutingSchema::RoutingSchema() : m_Dirty( true ), m_Copy( false )
{
#if defined( WIN32 ) && defined ( _DEBUG )
	m_OldStateAvailable = false;
#endif
}

RoutingSchema::~RoutingSchema()
{
	// leave the destruction part to the original schema
	//if ( m_Copy )
	//	return;

	/*try
	{
		map< long, RoutingPlan* >::iterator planWalker;	
		for( planWalker = m_UsablePlans.begin(); planWalker != m_UsablePlans.end(); planWalker++ )
		{
			if ( planWalker->second != NULL )
			{
				delete planWalker->second;
				planWalker->second = NULL;
			}
		}
		DEBUG2( "All usable plans deleted" );
	} catch( ... )
	{
		try
		{
			TRACE( "An error occured in ~RoutingSchema" ); 
		} catch( ... ){}
	}*/
	
	try
	{
		map< string, RoutingPlan* >::iterator planWalker;	
		for( planWalker = m_Plans.begin(); planWalker != m_Plans.end(); planWalker++ )
		{
			DEBUG( "Deleting plan [" << planWalker->first << "]" );
			if ( planWalker->second != NULL )
			{
				delete planWalker->second;
				planWalker->second = NULL;
			}
		}
		DEBUG2( "All plans deleted" );
	} catch( ... )
	{
		try
		{
			TRACE( "An error occured in ~RoutingSchema" ); 
		} catch( ... ){}
	}
	m_Plans.clear();
	m_UsablePlans.clear();
}

void RoutingSchema::LoadSchema( const string& schemaName, long schemaId, const string& sessionCode )
{
#if defined( WIN32 ) && defined ( _DEBUG )
	m_OldStateAvailable = false;
#endif

	//string schemaName = StringUtil( schemaNameFull ).Trim();
	if ( schemaName.size() == 0 )
		return;
		
	DataSet* rules = NULL;
	
	// read routing rules
	try
	{
		rules = RoutingDbOp::ReadRoutingRules( schemaName );
		
		// no rules ? return
		if ( rules == NULL )
		{
			TRACE( "No routing rules defined" );
			return;
		}
			
		// create rules	
		for( unsigned int i=0; i<rules->size(); i++ ) 
		{
			DEBUG( "Parsing rule #" << i );
			
			long crtSequence = rules->getCellValue( i, "SEQUENCE" )->getLong();
			DEBUG( "Sequence : " << crtSequence );
			
			long ruleType = rules->getCellValue( i, "RULETYPE" )->getLong();
			DEBUG( "Rule type : " << ruleType << " ( where 0 - normal, 1 - init rule, 2 - tear rule )" );
			
			//bool isInitRule = false;
			map< long, vector< RoutingRule > > *ruleStorage = NULL;
			
			switch( ruleType )
			{
				case 0 :
					ruleStorage = &m_RulesByQueue;
					break;
				
				case 1 :
					ruleStorage = &m_InitRulesByQueue;
					break;
					
				case 2 : 
					ruleStorage = &m_TearRulesByQueue;
					break;

				default :
					stringstream errorMessage;
					errorMessage << "Unknown rule type [" << ruleType << "]";
					throw invalid_argument( errorMessage.str() );
			}

			long crtRuleId = rules->getCellValue( i, "ID" )->getLong();
			DEBUG( "Rule id : " << crtRuleId );
			
			RoutingRule crtRule( crtRuleId, schemaId, crtSequence, sessionCode );
			
			DEBUG( "About to insert rule #" << i );
			
			// get rules for queue
			long queueId = crtRule.getQueueId();
			if ( ruleStorage == NULL )
				throw logic_error( "Unable to insert rule because the storage is not defined" );

			map< long, vector< RoutingRule > >::iterator queueFinder = ruleStorage->find( queueId );				
			
			// append it if not found
			if ( queueFinder == ruleStorage->end() )
			{
				DEBUG( "Creating sequence for queue [" << queueId << "]" );
				( void )ruleStorage->insert( pair< long, vector< RoutingRule > >( queueId, vector< RoutingRule >() ) );
			}
				
			DEBUG( "Finding a place for the rule with sequence " << crtSequence );
			// find a place in sequence
			vector< RoutingRule >::iterator rulesForQueue = ( *ruleStorage )[ queueId ].begin();
			while( ( rulesForQueue != ( *ruleStorage )[ queueId ].end() ) && ( rulesForQueue->getSequence() < crtSequence ) )
			{
				rulesForQueue++;
			}
			
			if ( ( rulesForQueue != ( *ruleStorage )[ queueId ].end() ) )
			{
				// skip duplicate tear rules and report possible clashes for normal rules if next sequence == sequence of this rule
				if ( rulesForQueue->getSequence() == crtSequence )
				{
					// not a duplicate tear rule
					if ( ruleType != 2 )
					{
						stringstream errorMessage;
						errorMessage << "Duplicate rule with same sequence found on queue [" << queueId << "] will not be inserted. Check routing definitions.";

						TRACE( errorMessage.str() );

						AppException aex( errorMessage.str(), EventType::Warning );
						LogManager::Publish( aex );
					}
					else
					{
						// else do nothing
						TRACE( "Duplicate tear rule skipped..." );
					}
				}
				else
				{
					DEBUG( "Inserting rule at index." );
					( void )( ( *ruleStorage )[ queueId ] ).insert( rulesForQueue, crtRule );
				}
			}
			else
			{
				DEBUG( "Inserting rule at end." );
				( *ruleStorage )[ queueId ].push_back( crtRule );
			}
			
			//Explain();
		}
	}
	catch( const RoutingException& rex )
	{
		TRACE( rex.what() );
		if ( rules != NULL )
			delete rules;
		
		throw RoutingException::EmbedException( "Unable to read routing rules.",
			RoutingExceptionLocation, rex ); 
	}
	catch( const std::exception& ex )
	{
		TRACE( ex.what() );
		
		if ( rules != NULL )
			delete rules;
		throw;
	}
	catch( ... )
	{
		TRACE( "Unknown error while reading rules" );
			
		if ( rules != NULL )
			delete rules;
		throw RoutingException( "Unable to read routing rules.", RoutingExceptionLocation );
	}
	if ( rules != NULL )
		delete rules;
}

void RoutingSchema::Load()
{
	m_Dirty = true;

	// read active schema
	DataSet* schemas = NULL;

	stringstream fullSchemaName;
	
	m_RulesByQueue.clear();
	m_InitRulesByQueue.clear();
	
	map< long, int >::iterator schemaWalker = m_InnerSchemas.begin();
	while( schemaWalker != m_InnerSchemas.end() )
	{
		// mark all "old" [0]
		schemaWalker->second = 0; 
		schemaWalker++;
	}
	
	try
	{
		// if the time on the server != db2, the returned schemas may not be accurate
		schemas = RoutingDbOp::GetActiveRoutingSchemas();
		if( ( schemas == NULL ) || ( schemas->size() == 0 ) )
			throw runtime_error( "No routing schema defined" );
			
		for( unsigned int i=0; i<schemas->size(); i++ )
		{
			string schemaName = StringUtil::Trim( schemas->getCellValue( i, "NAME" )->getString() );
			DEBUG( "Found active schema #" << i << " [" << schemaName << "]" );
			long schemaId = schemas->getCellValue( i, "ID" )->getLong();
			
			// the schema is still active, mark as "active" [1]
			if ( m_InnerSchemas.find( schemaId ) != m_InnerSchemas.end() )
			{
				m_InnerSchemas[ schemaId ] = 1;
			}
			else // a new schema was loaded, mark as "new" [2]
			{
				m_InnerSchemas.insert( pair< long, int >( schemaId, 2 ) );
			}
			
			string schemaSessionCode = StringUtil::Trim( schemas->getCellValue( i, "SESSIONCODE" )->getString() );
			LoadSchema( schemaName, schemaId, schemaSessionCode );
			fullSchemaName << schemaName << "+";
		}
		
		schemaWalker = m_InnerSchemas.begin();
		vector< long > deletableSchemas;
		
		while( schemaWalker != m_InnerSchemas.end() )
		{
			DEBUG( "Schema [" << schemaWalker->first << "] is [" << schemaWalker->second << "] ( 0=old, 1=still active, 2=new )" );
			
			switch( schemaWalker->second )
			{
				// perform init routine on new schemas
				case 2 :
					PerformInitRoutine( schemaWalker->first );
					schemaWalker++;
					break;
					
				case 0 :
					// perform tear routine on old schemas
					PerformTearRoutine( schemaWalker->first );

					// erase
#if defined( __GNUC__ )
					m_InnerSchemas.erase( schemaWalker );
					deletableSchemas.push_back( schemaWalker->first );
#else
					schemaWalker = m_InnerSchemas.erase( schemaWalker );
#endif
					break;
					
				default :
					schemaWalker++;
					break;
			}
		}

#if defined( __GNUC__ )
		for ( int i=0; i<deletableSchemas.size(); i++ )
		{
			m_InnerSchemas.erase( deletableSchemas[ i ] );
		}
#endif

		/*for ( int i=0; i<deletableSchemas.size(); i++ )
		{
			m_InnerSchemas.erase( deletableSchemas[ i ] );
			
			if ( m_TearRulesByQueue.size() <= 0 )
				continue;

			map< long, vector< RoutingRule > >::iterator queueIterator = m_TearRulesByQueue.begin();
			while( queueIterator != m_TearRulesByQueue.end() )
			{
				vector< RoutingRule >::iterator rulesInQueueIterator = queueIterator->second.first
				int initialSize = queueIterator->second.size(), skipCount = 0;
				 
				for ( int j=0; j<initialSize; j++ )
				{
					if ( queueIterator->second[ j ].getSchemaId() == deletableSchemas[ i ] )
					{
						DEBUG( "Removing tear rule [" << queueIterator->second[ j ].getAction().ToString() << "] from queue [" << queueIterator->first << "]" );
						m_TearRulesByQueue[ ;
					}
				} 
				queueIterator++;
			}
		}*/
		
		m_SchemaName = fullSchemaName.str();
		
		if( m_SchemaName.length() > 0 )
			m_SchemaName.erase( m_SchemaName.length() - 1, 1 );
	}	
	catch( const std::exception& ex )
	{
		if( schemas != NULL )
			delete schemas;
		throw AppException( "Unable to retrieve default routing schema from DB", ex );
	}
	catch( ... )
	{
		if( schemas != NULL )
			delete schemas;
		throw AppException( "Unable to retrieve default routing schema from DB [unknonw error]" );
	}

	if( schemas != NULL )
	{
		delete schemas;
		schemas = NULL;
	}

	//CreatePlans();
}

void RoutingSchema::DeletePlans()
{
	if( !m_Plans.empty() )
	{
		map< string, RoutingPlan* >::iterator planWalker;	
		for( planWalker = m_Plans.begin(); planWalker != m_Plans.end(); planWalker++ )
		{
			if ( planWalker->second != NULL )
			{
				delete planWalker->second;
				planWalker->second = NULL;
			}
		}
		m_Plans.clear();
		DEBUG( "All plans deleted" );
	}
}
void RoutingSchema::CreatePlans()
{
	//Delete all plans before creating others
	DeletePlans();
	
	vector< long > inputQueues, outputQueues;

	// first, find input queues ( not a target for other moves )
	map< long, vector< RoutingRule > >::const_iterator queueIterator = m_RulesByQueue.begin();
	while( queueIterator != m_RulesByQueue.end() )
	{
		for( unsigned int i=0; i<queueIterator->second.size(); i++ )
		{
			switch( queueIterator->second[ i ].getAction().getRoutingAction() )
			{
				case RoutingAction::MOVETO :
					outputQueues.push_back( RoutingDbOp::GetQueueId( queueIterator->second[ i ].getAction().getParam() ) );
					break;
				default :
					break;
			}
		} 
		
		queueIterator++;
	}

	// fill input queues
	map< long, RoutingQueue >* queues = RoutingDbOp::GetQueueCache();
	for( map< long, RoutingQueue >::const_iterator queueWalker = queues->begin(); queueWalker != queues->end(); queueWalker++ )
	{
		if ( outputQueues.end() == find( outputQueues.begin(), outputQueues.end(), queueWalker->first ) )
			inputQueues.push_back( queueWalker->first );
	}

	// for each input queue, see how far can we get
	for( vector< long >::const_iterator iqWalker = inputQueues.begin(); iqWalker != inputQueues.end(); iqWalker++ )
	{
		// start a plan with the same start, end queue, both sequences 0
		stringstream planName;
		planName << "Plan-" << *iqWalker;

		RoutingPlan *plan = new RoutingPlan( *iqWalker, *iqWalker, 0, 0, planName.str() );

		// backtrack throgh rules
		DEBUG( "Building plan for queue [" << RoutingDbOp::GetQueueName( *iqWalker ) << "]" );
		CreatePlan( plan, *iqWalker, 0, 0 );

		// add current plan
		InsertPlan( plan );
	}
}

RoutingPlan* RoutingSchema::CreatePlan( RoutingPlan* plan, const long queue, const long sequence, const unsigned char depth )
{
	if ( plan == NULL ) 
		return plan;

	if ( depth >= PLAN_MAX_DEPTH )
		return plan;

	plan->setEndQueue( queue );

	// start with rules for this queue
	for( vector< RoutingRule >::const_iterator ruleWalker = m_RulesByQueue[ queue ].begin(); ruleWalker != m_RulesByQueue[ queue ].end(); ruleWalker++ )
	{
		if ( ruleWalker->getSequence() <= sequence )
			continue;

		const vector< RoutingCondition >& conditions = ruleWalker->getConditions();

		// this is HARD to happen ( at least one condition is on every rule )
		if ( conditions.size() == 0 )
			return plan;
		
		RoutingAction::ROUTING_ACTION action = ruleWalker->getAction().getRoutingAction();

		// we just check if this is an always condition, so any condition will do
		RoutingCondition::ROUTING_CONDITION condition = conditions[ 0 ].getConditionType();
			
		// fork on conditions 
		if( condition != RoutingCondition::ALWAYS )
		{
			// create a plan with this condition satisfied
			plan->setForkConditions( conditions );

			// create a plan with this condition not satisfied ( next sequence )
			CreatePlan( plan->getFailPlan(), queue, ruleWalker->getSequence(), depth + 1 );

			// add rule to success plan ( if the condition is satisfied, the rule may be stopping )
			plan = plan->getSuccessPlan();
		}
		plan->addRule( *ruleWalker );

		switch( action )
		{
			case RoutingAction::MOVETO :
				// for moves, continue the plan to the next queue
				// get move param ( destination queue )
				CreatePlan( plan, RoutingDbOp::GetQueueId( ruleWalker->getAction().getParam() ), 0, depth + 1 );
				break;

			case RoutingAction::CHANGEHOLDSTATUS :
				// change hold status stops us ( message needs auth )
			case RoutingAction::COMPLETE :
				// complete stops us ( message will be deleted )
			case RoutingAction::SENDREPLY :
				// complete stops us ( message will be deleted )
				// add current plan

				// terminal means the message is no longer available to this plan
				plan->setTerminal( true );
				return plan;

			default:
				break;
		}
	}

	if ( !plan->getExitpoint().isValid() )
	{
		// set queue's exitpoint to finish plan
		const RoutingQueue& crtQueue = RoutingDbOp::GetQueue( queue );
		plan->setExitpoint( crtQueue.getExitpoint() );
		plan->setTerminal( false );
	}

	return plan;
}

void RoutingSchema::InsertPlan( RoutingPlan* plan )
{
	m_Plans.insert( pair< string, RoutingPlan* >( plan->name(), plan ) );
}

RoutingPlan* RoutingSchema::getPlan( long queue, RoutingMessage* theMessage )
{
	map< long, RoutingPlan* >::const_iterator planFinder = m_UsablePlans.find( queue );
	if ( ( planFinder != m_UsablePlans.end() ) && ( planFinder->second->usable( theMessage ) ) )
		return planFinder->second;
	return NULL;
}

void RoutingSchema::Explain( void )
{
	DEBUG( "****************************** Routing definitions ******************************" );
	DEBUG( "Schema name : " << m_SchemaName );

	map< long, vector< RoutingRule > >::const_iterator queueIterator;
	
	if ( m_InitRulesByQueue.size() > 0 )
	{
		queueIterator = m_InitRulesByQueue.begin();	
		while( queueIterator != m_InitRulesByQueue.end() )
		{
			DEBUG( "Init rules for queue [" << queueIterator->first << "] : " );
			for ( unsigned int i=0; i<queueIterator->second.size(); i++ )
			{
				DEBUG( "\t" << queueIterator->second[ i ].getSequence() << 
					" : " << queueIterator->second[ i ].getAction().ToString() );
			} 
			queueIterator++;
		}
	}
	else
	{
		DEBUG( "Not init rules defined." );
	}
	
	if ( m_RulesByQueue.size() > 0 )
	{
		queueIterator = m_RulesByQueue.begin();
		while( queueIterator != m_RulesByQueue.end() )
		{
			DEBUG( "Rules for queue [" << queueIterator->first << "] : " );
			for ( unsigned int i=0; i<queueIterator->second.size(); i++ )
			{
				DEBUG( "\t" << queueIterator->second[ i ].getSequence() << 
					" : " << queueIterator->second[ i ].getAction().ToString() );
			} 
			queueIterator++;
		}
	}
	else
	{
		DEBUG( "No rules defined" );
	}
	
	if ( m_TearRulesByQueue.size() > 0 )
	{
		queueIterator = m_TearRulesByQueue.begin();
		while( queueIterator != m_TearRulesByQueue.end() )
		{
			DEBUG( "Tear rules for queue [" << queueIterator->first << "] : " );
			for ( unsigned int i=0; i<queueIterator->second.size(); i++ )
			{
				DEBUG( "\t" << queueIterator->second[ i ].getSequence() << 
					" : " << queueIterator->second[ i ].getAction().ToString() );
			} 
			queueIterator++;
		}
	}
	else
	{
		DEBUG( "No tear rules defined." );
	}

	DEBUG( "****************************** End of routing definitions ******************************" );
	if( m_Plans.empty() )
	{
		DEBUG( "Plan list is empty" ); 
	}
	DEBUG( "****************************** Routing plans ******************************" );

	for( map< string, RoutingPlan* >::const_iterator planWalker = m_Plans.begin(); planWalker != m_Plans.end(); planWalker++ )
	{
		RoutingPlan* plan = planWalker->second;

		DEBUG( planWalker->first << " from queue [" << RoutingDbOp::GetQueueName( plan->startQueue() ) << "]. Plan size " << plan->size() );
		plan->display( 1 );
	}

	DEBUG( "****************************** End of routing plans ******************************" );
}

RoutingPlan* RoutingSchema::Plan( const string& name )
{
	map< string, RoutingPlan* >::const_iterator planFinder = m_Plans.find( name );
	if ( planFinder != m_Plans.end() )
		return planFinder->second;

	stringstream errorMessage;
	errorMessage << "No plan with name [" << name << "] was found.";
	throw runtime_error( errorMessage.str() );
}

void RoutingSchema::UsePlan( const string& name ) 
{	
	// clear all usable plans
	map< long, RoutingPlan* >::iterator planWalker = m_UsablePlans.begin();
	while( planWalker != m_UsablePlans.end() )
	{
		if ( planWalker->second != NULL )
		{
			RoutingPlan *rootPlan = planWalker->second->getRootPlan();
			while( rootPlan != NULL )
				rootPlan = rootPlan->getRootPlan();

			delete rootPlan;
			planWalker->second = NULL;
		}
		planWalker++;
	}
	m_UsablePlans.clear();

	string::size_type indexMin = name.find_first_of( '-' );
	if ( indexMin == string::npos )
	{
		TRACE( "Unable to use plan [" << name << "] because the name is invalid" );
		return;
	}
	string::size_type indexSec = name.find_first_of( '-', indexMin + 1 );
	if ( indexSec == string::npos )
	{
		TRACE( "Unable to use plan [" << name << "] because the name is invalid" );
		return;
	}

	RoutingPlan *plan = NULL, *normalPlan = NULL;
	try
	{
		plan = Plan( name.substr( 0, indexSec ) );
		normalPlan = plan->normalize( name );
		m_UsablePlans.insert( pair< long, RoutingPlan* >( plan->startQueue(), normalPlan ) );

		DEBUG( "Queue [" << plan->startQueue() << "] will use plan [" << name << "]" );
	}
	catch( ... )
	{
		if ( plan != NULL )
		{
			delete plan;
			plan = NULL;
		}
		TRACE( "This schema can't use the requested plan" );
	}	
}

bool RoutingSchema::RouteBatchReply( RoutingJob* job, RoutingMessage* theMessage, bool fastpath )
{
	pthread_t selfId = pthread_self();
	string bulkReactivateQueue = RoutingEngine::getBulkReactivateQueue();

	bool isBulk = false;
	bool needsIndividualAttn = false;
	bool isDistinctAck = false;
	int userId = job->getUserId();
	string messageTable = job->getJobTable();
	string itemFeedback = "";
	string fullBatchId = "";
	RoutingMessageEvaluator* evaluator = theMessage->getPayloadEvaluator();
	if ( evaluator == NULL )
		throw logic_error( "Routing batch reply:[ Batch reply can't be evaluated" );
	string batchIssuer = evaluator->getIssuer();
	RoutingMessageEvaluator::FeedbackProvider feedbackProvider = ( evaluator->getOverrideFeedbackProvider() );
	wsrm::SequenceResponse* response = evaluator->getSequenceResponse();

	NameValueCollection params;

	//optimistic treatment for nacks( suppose we can perform a bulk reactivation of nacks )
	if ( evaluator->isNack() )
	{
		// failed batches with all messages rejected will have seqfault != NOSEQFAULT
		wsrm::SequenceFault* nackResp = dynamic_cast< wsrm::SequenceFault* >( response );
		if ( nackResp == NULL )
			throw logic_error( "Response sequence is not nack sequence" );
		fullBatchId = response->getIdentifier();
		itemFeedback = nackResp->getErrorCode();
		if ( itemFeedback != wsrm::SequenceFault::NOSEQFAULT )
		{
			DEBUG( "The batch [" << fullBatchId << "] was NACKed with code [" << itemFeedback << "]. Bulk reply will be performed... " );
			isBulk = true;

			// if the BulkReactivateQueue is set, we must reactivate all messages in said q
			if ( ( bulkReactivateQueue.length() > 0 ) && evaluator->checkReactivation() )
			{
				DEBUG( "All messages in batch [" << fullBatchId << "] will be reactivated in queue [" << bulkReactivateQueue << "]" );
				needsIndividualAttn = true;

				RoutingDbOp::UpdateBatchCode( fullBatchId, itemFeedback );
			}
		}
		else
			needsIndividualAttn = true;
	}
	else if ( evaluator->isAck() )
	{
		// comentariu irelevant ? : sequence response for for DI SETTLED BlkAcc with transactions inside
		wsrm::SequenceAcknowledgement* ackResp = dynamic_cast< wsrm::SequenceAcknowledgement* >( response );
		if ( ackResp == NULL )
			throw logic_error( "Response sequence is not ack sequence" );
		fullBatchId = response->getIdentifier();
		itemFeedback = evaluator->getOverrideFeedback();
		if ( ackResp->HasAcks() )
		{
			needsIndividualAttn = true;
			isDistinctAck = true ;
			evaluator->UpdateMessage( theMessage );
			DEBUG( "The batch [" << fullBatchId << "] was ACKed with individual transactions. Individual reply will be performed... " );
		}
		//optimistic treatment for acks( suppose we can perform a bulk reactivation of acks )
		else
		{
			isBulk = true;
			DEBUG( "The batch [" << fullBatchId << "] was ACKed. Bulk routing will be performed... " );
		}
	}
	//optimistic treatment for all batch reply neither isAck neither isNack
	else
	{
		isBulk = true;
		itemFeedback = evaluator->getOverrideFeedback();
		DEBUG( "A batch reply [" << evaluator->getNamespace() << "] was received. Bulk routing will be performed" );
	}

	// we dealt with bulk reply
	if ( !needsIndividualAttn )
	{
		if ( response == NULL )
				throw logic_error( "Batch reply message should have a sequence response" );
		fullBatchId = response->getIdentifier();
		theMessage->setBatchId( fullBatchId );

		// set agg token to be the batch id, in effect, the call to update feedbackagg will be
		// like "update feedbackagg set tfdcode=... where batchid='....' 
		RoutingAggregationCode batchFeedback = RoutingMessageEvaluator::composeFeedback( fullBatchId, itemFeedback, feedbackProvider );
		theMessage->setFeedback( batchFeedback );

		//#pragma endregion
		return true;
	}
	else
	{
		//#pragma region Deal with individual items in reply
		DEBUG( "Getting messages in batch [" << fullBatchId << "]" );
		
		// get individual messages from db, matching the current batchId
		DataSet* batchItems = NULL;

		try
		{
			batchItems = RoutingDbOp::GetBatchMessages( response->getIdentifier(), true, batchIssuer );
			if( ( batchItems == NULL ) || ( batchItems->size() == 0 ) )
			{
				stringstream errorMessage;
				errorMessage << "Batch response for batch [" << response->getIdentifier() << "] does not match a sent batch";
				
				AppException aex = RoutingAction::internalPerformMoveToInvestigation( theMessage, errorMessage.str() );
				throw aex;
			}
			//one last chance to aggregate batch acks as bulk
			else if( ( batchItems->size() == response->seqSize() ) && evaluator->isAck() )
			{
				itemFeedback = evaluator->getOverrideFeedback();
				theMessage->setBatchId( fullBatchId );
				RoutingAggregationCode batchFeedback = RoutingMessageEvaluator::composeFeedback( fullBatchId, itemFeedback, feedbackProvider );
				theMessage->setFeedback( batchFeedback );
				DEBUG( "The batch [" << fullBatchId << "] was ACKed with all transactions in batch. Bulk reply will be performed... " );

				delete batchItems;
				batchItems = NULL;

				return true;
			}

			for( unsigned int i=0; i<batchItems->size(); i++ )
			{
				string correlationId = StringUtil::Trim( batchItems->getCellValue( i, "CORRELATIONID" )->getString() );
				LogManager::setCorrelationId( correlationId );
				
				string trn = StringUtil::Trim( batchItems->getCellValue( i, "REFERENCE" )->getString() );
				string batchItemFeedback = "0";
				if( ( evaluator->CheckPayloadType( RoutingMessageEvaluator::ACHCOREBLKDDBTRFL ) ) || ( evaluator->isOriginalIncomingMessage() ) )
				{
					DEBUG( "Message is ACHCOREBLKDDBTRFL format or is a reply for an incoming batch so we don't reactivate good mesages" );
					batchItemFeedback = "-1";
				}
				
				// guard this message 
				WorkItem< RoutingMessage > workBatchItem( new RoutingMessage() );
				RoutingMessage* batchItem = workBatchItem.get();

				batchItem->setCorrelationId( correlationId );
				
				// set fastpath, so no eval will be performed on the payload, but retain set feedback
				batchItem->setFastpath( true );

				// modified SP for RZB : GETMESSAGESINBATCH
				bool noHeaders = false;

				long batchItemSeq = batchItems->getCellValue( i, "BATCHSEQUENCE" )->getLong();
				try
				{
					string originalRequestor = StringUtil::Trim( batchItems->getCellValue( i, "REQUESTORSERVICE" )->getString() );
					messageTable = RoutingDbOp::GetReplyQueue( originalRequestor );

					if ( isBulk && ( bulkReactivateQueue.length() > 0 ) && evaluator->checkReactivation() )
						messageTable = bulkReactivateQueue;
					
					// message options for reply queue may specify "MO_NOREACTIVATE" 
					int messageOptions = RoutingDbOp::GetQueue( messageTable ).getMessageOptions();
					DEBUG( "Queue [" << messageTable << "] options : " << RoutingMessageOptions::ToString( messageOptions ) );
					if( ( messageOptions & RoutingMessageOptions::MO_NOREACTIVATE ) == RoutingMessageOptions::MO_NOREACTIVATE )
					{
						DEBUG( "The message [" << correlationId << "] with TRN [" << trn << "] was part of bulk reply, but reactivation not allowed on exitpoint [" << messageTable << "]" );
						messageTable = job->getJobTable();
						batchItemFeedback = RoutingMessageEvaluator::FEEDBACKFTP_NOREACT;
					}

					if( ( batchItemSeq == 1 ) && ( ( messageOptions & RoutingMessageOptions::MO_NOHEADERS ) == RoutingMessageOptions::MO_NOHEADERS ) )
					{
						DEBUG( "Header skipped ( MO_NOHEADERS option set on exitpoint [" << messageTable << "] )" );
						continue;
					}
				}
				catch( ... )
				{
					//the sp doesn't exist in all versions of FinTP.
				}
				
				// good message in bad batch - > return to sending queue
				// nack message in batch
				if ( response->IsNack( trn ) )//!NOSEQFAULT (is bulk nack) always return true
				{
					if ( isBulk && ( bulkReactivateQueue.length() > 0 ) && evaluator->checkReactivation() )
					{
						messageTable = bulkReactivateQueue;
						wsrm::SequenceFault* nackResponse = dynamic_cast< wsrm::SequenceFault* >( response );
						if ( nackResponse != NULL )
							batchItemFeedback = nackResponse->getErrorCode();
						else
							batchItemFeedback = "0";
					}
					else
					{
						messageTable = job->getJobTable();

						// send reply not ok
						wsrm::Nack nack = response->getNack( trn );
						batchItemFeedback = nack.getErrorCode();
					}

					DEBUG( "The message [" << correlationId << "] with TRN [" << trn << "] was rejected." );
				}
				else if ( isDistinctAck )
				{
					if ( response->IsAck( trn ) )
					{
						messageTable = job->getJobTable();
						batchItemFeedback = itemFeedback;
						// transactions specified in BlkAcc SETTLED message will be send alog with theirs amount
						if( !(evaluator->isBusinessFormat() ) )
						{
							params = evaluator->getAddParams( trn );
						}
						DEBUG( "The message [" << correlationId << "] with TRN [" << trn << "] was individual settled." );
					}
					else
					{
						if ( evaluator->isBusinessFormat() )
							continue;
					}
					/*
						else
						{
							//transactions not specified in BlkAck SETTLED message are ignored
							RoutingAggregationCode aggcode( RoutingMessageEvaluator::AGGREGATIONTOKEN_TRN, trn );
							RoutingAggregationManager::AddRequest( evaluator->getAggregationCode( aggcode ) );
							continue;
						}
					*/
				}

				else if ( batchItemFeedback == "0" )
				{
					// aggregate a good message
					int messageOptions = theMessage->getMessageOptions();
					if( ( messageOptions & RoutingMessageOptions::MO_MARKNOTREPLIED ) == RoutingMessageOptions::MO_MARKNOTREPLIED )
					{
						DEBUG( "The message [" << correlationId << "] with TRN [" << trn << "] was not rejected, but aggregation allowed on exitpoint [" << messageTable << "]" );
						RoutingAggregationCode aggcode( RoutingMessageEvaluator::AGGREGATIONTOKEN_FTPID, correlationId );
						RoutingAggregationManager::AddRequest( evaluator->getAggregationCode( aggcode ) );
						continue;
					}
					messageTable = RoutingDbOp::GetReplyQueue( theMessage->getRequestorService() );

					// reactivate good messages in originating queue
					DEBUG( "The message [" << correlationId << "] with TRN [" << trn << "] was not rejected, but part of a fault sequence. Reactivating a good message ... " );
				}
								
				if( batchItemFeedback != "-1" )
				{
					// HACK - force nack 
					RoutingAggregationCode itemFeedbackCode = RoutingMessageEvaluator::composeFeedback( correlationId, batchItemFeedback, feedbackProvider );
					batchItem->setFeedback( itemFeedbackCode );
				
					// update feedback
					RoutingAggregationCode aggcode( RoutingMessageEvaluator::AGGREGATIONTOKEN_TRN, trn );
				
					RoutingAggregationCode trnRequestedCode;
					// if the reactivation q is set, we need the code to be empty for nack messages( allow for rebatching )
					if( !( response->IsNack( trn ) && isBulk && evaluator->checkReactivation() && ( bulkReactivateQueue.length() > 0 ) ) )
					{
						trnRequestedCode = evaluator->getAggregationCode( aggcode );
						RoutingAggregationManager::AddRequest( trnRequestedCode );
					}
				
					string theNewFeedback = batchItem->getFeedbackString();
					DEBUG( "The new feedback is [" << theNewFeedback << "]" );
				
					string batchJobId = Collaboration::GenerateGuid();
					batchItem->setMessageId( batchJobId );

					batchItem->setTableName( "" );
					
					// this reactivation may insert a routingjob if the insert is done on a triggered queue
					// messages will be reactivated only if its has expected aggcode and messageTable specified
					RoutingAction::internalPerformReactivate( batchItem, messageTable, trnRequestedCode );
					string reactTableName = batchItem->getTableName(); 
					
					// tableName (queue) for reactivation is set just before reactivation. 
					// if trnRequestedCode isn't expected one, reactivation isn't made and tableName not set
					if ( reactTableName.length() > 0 )
					{
						// after reactivating, the message has no feedback, no evaluator (fastpath)
						if( isDistinctAck )
						{
							//need to assure that amount is inserted at the end of Feedback fields
							//for further evaluation fields order counts
							if( params.ContainsKey( "XSLTPARAMSTTLDAMT" ) )
							{
								RoutingAggregationCode reactivateFeedback = batchItem->getFeedback();
								reactivateFeedback.setAggregationField( "~AMOUNT", params[ "XSLTPARAMSTTLDAMT" ] );
								batchItem->setFeedback( reactivateFeedback );
							}
						}
						else
						{
							batchItem->setFeedback( theNewFeedback );
						}
									
						DEBUG_GLOBAL( "Waiting on message pool to allow inserts - thread [" << selfId << "]." );
						RoutingEngine::getMessagePool().addPoolItem( batchJobId, workBatchItem );
						DEBUG_GLOBAL( "Inserted routing message in pool [" << batchItem->getMessageId() << "]" );
		
						// route the message in the new queue
						WorkItem< RoutingJob > workBatchJob( new RoutingJob( messageTable, batchJobId, "F=Route", userId ) );				
						RoutingJob* redirectJob = workBatchJob.get();
					
						// fastpath means no re-evaluation will be performed on the message
						ApplyRouting( redirectJob, NULL, true );
						job->copyActions( *redirectJob );

						( void )RoutingEngine::getMessagePool().removePoolItem( batchJobId );
					}
				}
				try
				{
					// run this commit in the same transaction
					// TODO This is not very good ( the job will remain in routingjobs if the transaction is rolledback,
					// but the message will be rolled back )
					RoutingDbOp::CommitJob( batchItem->getMessageId(), false );
				}
				catch( ... )
				{
					TRACE( "Warning : Unable to kill job [ maybe it doesn't exist ]" );
				}
			}
			
			if ( batchItems != NULL )
			{
				delete batchItems;
				batchItems = NULL;
			}						
			// done with the batch
		}
		catch( ... )
		{
			if ( batchItems != NULL )
			{
				delete batchItems;
				batchItems = NULL;
			}
				
			throw;
		}

		// get rid of the batch message ( we've dealt with all messages in batch );
		RoutingDbOp::DeleteRoutingMessage( job->getJobTable(), theMessage->getMessageId() );

		//#pragma endregion
		return false;
	}
}

bool RoutingSchema::RouteBatch( RoutingJob* job, RoutingMessage* theMessage, bool fastpath )
{		
	RoutingMessageEvaluator* evaluator = theMessage->getPayloadEvaluator();
	int userId = job->getUserId();

	//string passedBatchId = job->getFunctionParam( RoutingJob::PARAM_BATCHID ), mappedBatchId = "";
	string passedBatchId = "", mappedBatchId = "", passedBatchUid = "";

	// if it's not rapid batch
	if ( !theMessage->isVirtual() )
	{
		unsigned long passedGroupOrder = StringUtil::ParseULong( job->getFunctionParam( RoutingJob::PARAM_GROUPORDER ) );
		unsigned long passedGroupCount = StringUtil::ParseULong( job->getFunctionParam( RoutingJob::PARAM_GROUPCOUNT ) );

		passedBatchId = job->getFunctionParam( RoutingJob::PARAM_BATCHREF );
		passedBatchUid = job->getFunctionParam( RoutingJob::PARAM_BATCHID );
		
		BatchManagerBase::BATCH_STATUS batchStatus = BatchManagerBase::BATCH_INPROGRESS;
		
		if ( !job->hasFunctionParam( "Fast" ) )
		{
			batchStatus = RoutingDbOp::GetBatchStatus( passedBatchId, passedBatchUid, mappedBatchId, userId, 
				passedGroupCount, job->getFunctionParam( RoutingJob::PARAM_GROUPAMOUNT ),
				RoutingDbOp::GetQueue( job->getJobTable() ).getServiceId(), job->getJobTable() );

			DEBUG( "Batch status for batch [" << mappedBatchId << "] is [" << BatchManagerBase::ToString( batchStatus ) << "]" );
		}
		else
			mappedBatchId = passedBatchId;

		DEBUG( "Attempting to batch message in [" << passedBatchId << "] [" << 
		passedGroupOrder << "/" << passedGroupCount << "] from queue [" << job->getJobTable() << "]" );

		job->setBatchId( mappedBatchId, batchStatus );

		switch( batchStatus )
		{
			case BatchManagerBase::BATCH_NEW :
			case BatchManagerBase::BATCH_INPROGRESS :
				
				if ( passedGroupCount > 1000 )
				{
					AppException aex( "Max. number of messages in batch exceeded [1000]" );
					aex.setSeverity( EventSeverity::Fatal );

					throw aex;
				}

				if ( ( evaluator == NULL ) || !( evaluator->isBusinessFormat() ) )
				{
					AppException aex( "Unable to batch NON-SWIFT messages." );
					aex.setSeverity( EventSeverity::Fatal );

					RoutingEngine::getMessagePool().Dump();

					// test only
					//sleep( 60 );
					throw aex;
				}

				//this would envelope the first mesage with an incorrect amount
				//theMessage->setMessageOption( RoutingMessageOptions::MO_BATCH );
				//theMessage->setRequestType( RoutingMessage::Batch );

				theMessage->setBatchSequence( passedGroupOrder );

				try
				{
					// used before to sum amount in batches ( now done in UI )
					//string itemCurrency = evaluator->getField( SwiftXmlPayload::CURRENCY );
					//theMessage->setBatchTotalAmount( RoutingEngine::Keywords->Evaluate( itemCurrency, "Currency", "ammount" ).first );
					theMessage->setBatchTotalAmount( job->getFunctionParam( RoutingJob::PARAM_GROUPAMOUNT ) );
				}
				catch( const std::exception& ex )
				{
					stringstream errorMessage;
					errorMessage << "Unable to batch message. Failed to evaluate value [" << ex.what() << "]";
					TRACE( errorMessage.str() );

					RoutingExceptionMoveInvestig aex = RoutingAction::internalPerformMoveToInvestigation( theMessage, errorMessage.str(), true );
					aex.setSeverity( EventSeverity::Fatal );

					throw aex;
				}
				catch( ... )
				{
					stringstream errorMessage;
					errorMessage << "Unable to batch message. Failed to evaluate value [unknown reason]";
					TRACE( errorMessage.str() );

					RoutingExceptionMoveInvestig aex = RoutingAction::internalPerformMoveToInvestigation( theMessage, errorMessage.str(), true );
					aex.setSeverity( EventSeverity::Fatal );

					throw aex;
				}

				theMessage->setBatchId( mappedBatchId );
				break;
			
			default :
				// BatchManagerBase::BATCH_READY should not happen ( a job after batch ready )			
				// BatchManagerBase::BATCH_COMPLETED should not happen ( a job after batch completion )
				// BatchManagerBase::BATCH_FAILED - a job has arrived, but the batch is compromised
				TRACE( "Unable to perform routing job : Batch [" << mappedBatchId << "] was terminated with status [" << BatchManagerBase::ToString( batchStatus ) << "]" );
				return false;
		}

		return true;
	}
	else // if it is rapid batch
	{
		unsigned int messagesInBatch = 0;
		// get individual messages from db, matching the current batchId
		DataSet* batchItems = NULL;

		try
		{
			do
			{
				m_RBatchMessages.clear();
				batchItems = RoutingDbOp::GetBatchPart( userId, job->getFunctionParam( RoutingJob::PARAM_BATCHID ),
						RoutingDbOp::GetQueue( job->getJobTable() ).getExitpoint().getServiceId(),job->getJobTable() );
				if( batchItems == NULL ) 
					return false;
				
				messagesInBatch = batchItems->size();
				if ( messagesInBatch == 0 )
				{
					if ( batchItems != NULL )
					{
						delete batchItems;
						batchItems = NULL;
					}
					break;
				}

				string batchId = ( messagesInBatch > 0 ) ? StringUtil::Trim( batchItems->getCellValue( 0, "ID" )->getString() ) : "";
				string batchRef = ( messagesInBatch > 0 ) ? StringUtil::Trim( batchItems->getCellValue( 0, "BATCHREFERENCE" )->getString() ) : "";

				for( unsigned int j=0; j<messagesInBatch; j++ )
				{
					string messageId = StringUtil::Trim( batchItems->getCellValue( j, "ID" )->getString() );
					RoutingMessage dummyMessage( theMessage->getTableName(), messageId );

					m_RBatchMessages.push_back( dummyMessage );
				}

				int bSucceeded = true;
				vector< string > threadErrors;

#if defined( _OPENMP_SKIP )
				bool wasConnected;

				//omp_set_num_threads( 2 );
#pragma omp parallel private ( wasConnected )
{
#pragma omp single
{
				int noOfThreads = omp_get_num_threads();
				int maxNoOfThreads = omp_get_max_threads(); 
				DEBUG( "Start of parallel section - rapid batch [parallel] - [" << noOfThreads << " out of " << maxNoOfThreads << "] threads" );
				for( unsigned int threadCount = 0; threadCount < noOfThreads; threadCount++ )
					threadErrors.push_back( "" );
}
#ifdef AIX

				// block SIGINT every thread
				sigset_t signalSet;
				sigemptyset( &signalSet );
				sigaddset( &signalSet, SIGINT );

				if ( pthread_sigmask( SIG_BLOCK, &signalSet, NULL ) != 0 )
				{
					TRACE( "Can't catch SIGINT" );
				}
#endif //AIX
				int threadNum = omp_get_thread_num();
				wasConnected = RoutingDbOp::isConnected();
				if ( !wasConnected )
					RoutingDbOp::getData()->BeginTransaction();
#pragma omp for reduction( && : bSucceeded )
#else 
				size_t threadNum = 0;
				DEBUG( "Start of parallel section - rapid batch [non-parallel]" );
				threadErrors.push_back( "" );

#endif //defined( _OPENMP )
				for( unsigned int i=0; i<messagesInBatch; i++ )
				{
					if ( !bSucceeded )
						continue;
#if !defined(__ICL)
					try
					{
#endif
						stringstream jobFunction;
						long batchSequence = batchItems->getCellValue( i, "BATCHSEQUENCE" )->getLong();
						string batchAmount = StringUtil::Trim( batchItems->getCellValue( 0, "BATCHAMOUNT" )->getString() );
						jobFunction << "F=Route,F=Unhold,P=" << RoutingJob::PARAM_GROUPORDER << 
							"(" << batchSequence << "),P=" << RoutingJob::PARAM_GROUPCOUNT << "(" << messagesInBatch << 
							"),P=BatchID(" << batchId << "),P=" << RoutingJob::PARAM_GROUPAMOUNT << "(" << batchAmount <<
							"),P=" << RoutingJob::PARAM_BATCHREF << "(" << batchRef << "),P=Fast(true)";
						
						m_RBatchMessages[i].setMessageOption( RoutingMessageOptions::MO_RBATCH );
						m_RBatchMessages[i].setMessageOption( RoutingMessageOptions::MO_BATCH);
						m_RBatchMessages[i].setBatchId( batchId );
						m_RBatchMessages[i].setBatchSequence( batchSequence );
						m_RBatchMessages[i].setBatchTotalAmount( batchAmount );
						m_RBatchMessages[i].setBatchTotalCount( messagesInBatch );

						RoutingJob dummyJob( theMessage->getTableName(), m_RBatchMessages[i].getMessageId(), jobFunction.str(), userId );
						ApplyRouting( &dummyJob, &m_RBatchMessages[i] );
#if !defined(__ICL)
					}
					catch( const AppException& aex )
					{
						bSucceeded = 0;
						if ( threadErrors.size() > threadNum )
							threadErrors[ threadNum ] = aex.getMessage();
					}
					catch( const std::exception& ex )
					{
						bSucceeded = 0;
						if ( threadErrors.size() > threadNum )
							threadErrors[ threadNum ] = ex.what();
					}
					catch( ... )
					{
						bSucceeded = 0;
						if ( threadErrors.size() > threadNum )
							threadErrors[ threadNum ] = "Unknown reason";
					}
#endif
				}
#if defined( _OPENMP_SKIP )
				// sync all threads for commit/rollback
				if ( !wasConnected )
				{
					RoutingDbOp::getData()->EndTransaction( TransactionType::COMMIT );	
					RoutingDbOp::getData()->Disconnect();
				}
#pragma omp master
				{
#endif //defined( _OPENMP )
					const RoutingMessagePayload *firstPayload = m_RBatchMessages[0].getPayload();
					if ( firstPayload != NULL )
					{
						const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* const thePayload = m_RBatchMessages[0].getPayload()->getDocConst();
						if ( thePayload != NULL )
						{
							job->setBatchType( XmlUtil::getNamespace( thePayload ) );
							DEBUG( "Batch type : [" << job->getBatchType() << "]" );
						}
						else
						{
							DEBUG( "Unknown batch type : [payload is null]" );
						}
					}
					else
					{
						DEBUG( "Unknown batch type : [payload is null]" );
					}

					theMessage->setBatchId( batchRef );
					job->setBatchId( batchRef, ( bSucceeded ) ? BatchManagerBase::BATCH_COMPLETED : BatchManagerBase::BATCH_FAILED );

					DEBUG( "The batch [" << theMessage->getBatchId() << "] is ready and will be sent to the next rule." );
					if ( !theMessage->isHeld() && bSucceeded )
					{
						DEBUG( "About to process message in queue ..." );
						RoutingQueue queue = RoutingDbOp::GetQueue( job->getJobTable() );

						theMessage->setMessageOption( RoutingMessageOptions::MO_RBATCH );
						string destination = queue.ProcessMessage( theMessage, userId, true, job->getBatchType() );
						
						DEBUG( "Destination = [" << destination << "]" );
						 
						job->setDestination( destination );
						// TODO remove it from the queue
					}
#if defined( _OPENMP_SKIP )
				}
}
#endif //defined( _OPENMP )

				if ( !bSucceeded )
				{
					stringstream errorMessages;
					int errCounter = 0;
					for ( unsigned int errIter = 0; errIter < threadErrors.size(); errIter++ )
					{
						if ( threadErrors[ errIter ].length() > 0 )
						{
							if ( ++errCounter > 1 )
								errorMessages << ";";
							errorMessages << threadErrors[ errIter ];
						}
					}

					string errMessage = "";
					switch( errCounter )
					{
						case 0 :
							errMessage = "The rapid batch could not complete";
							break;
						case 1 :
							errMessage = errorMessages.str();
							break;
						default:
							errMessage = "Multiple reasons : " + errorMessages.str(); 
							break;
					}
					
					TRACE( errMessage );
					AppException aex( errMessage ) ;
					aex.setSeverity( EventSeverity::Fatal );
					throw aex;
				}

				if ( batchItems != NULL )
				{
					delete batchItems;
					batchItems = NULL;
				}

				RoutingDbOp::TerminateRapidBatch( batchRef, userId, job->getJobTable(), RoutingDbOp::GetQueue( job->getJobTable() ).getExitpoint().getServiceName() );

				// done with the batch part
				DEBUG( "First " << messagesInBatch << " messages batched." );
				
				stringstream partialBatch;
				partialBatch << messagesInBatch << " messages successfully processed in rapid batch";
				AppException aex( partialBatch.str(), EventType::Info );
				LogManager::Publish( aex );
				
				m_RBatchMessages.clear();				
			} while( messagesInBatch == 1000 );
		}
		catch( ... )
		{
			m_RBatchMessages.clear();
			if ( batchItems != NULL )
			{
				delete batchItems;
				batchItems = NULL;
			}
				
			throw;
		}
		return false;
	}
}

bool RoutingSchema::ApplyPlanRouting( RoutingJob* job, RoutingMessage* theMessage, const RoutingPlan* plan, const int userId, const bool isBulk, const bool fastpath ) const
{
	DEBUG( "Using plan routing for message in [" << theMessage->getTableName() << "]" );
	const vector< RoutingRule >& rulesForPlan = plan->getRules();

	long initialSequence = theMessage->getRoutingSequence();
	bool messageHeld = false;

	// perform each rule on the message
	for( unsigned int i=0; i<rulesForPlan.size(); i++ )
	{
		// no more routing on the message
		if ( messageHeld )
			break;
			
		DEBUG( "Executing rule #" << i << " of " << rulesForPlan.size() );
		
		// skip to sequence ( start routing from active seq )
		if ( rulesForPlan[ i ].getSequence() <= initialSequence )
			continue;
		
		RoutingAction::ROUTING_ACTION resolution = RoutingAction::NOACTION;
		if ( rulesForPlan[ i ].getAction().getRoutingAction() == RoutingAction::MOVETO )
		{
			//reset sequence
			theMessage->setRoutingSequence( 0 );
	
			// change the name of the table (allow routing engine to continue applying rules)
			theMessage->setTableName( rulesForPlan[ i ].getAction().getParam() );
		}
		else
		{
			resolution = rulesForPlan[ i ].Route( theMessage, userId, isBulk );
			string outcome = rulesForPlan[ i ].getOutcome();
			if ( outcome.length() > 0 )
				job->addAction( outcome );
		}
		DEBUG( "Resolution : " << RoutingAction::ToString( resolution ) );
		
		switch( resolution )
		{
			case RoutingAction::COMPLETE :
			case RoutingAction::SENDREPLY :
			case RoutingAction::CHANGEHOLDSTATUS :

				{
					stringstream errorMessage;
					errorMessage << "Unsupported plan action [" << RoutingAction::ToString( resolution ) << "]";
					throw runtime_error( errorMessage.str() );
				}

				break;
				
			//default to continue
			default:
				DEBUG( "Proceeding to next rule." );
				break;
		}		
	}

	return true;
}

bool RoutingSchema::ApplyQueueRouting( RoutingJob* job, RoutingMessage* theMessage, const long queueId, const int userId, const bool isBulk, const bool fastpath )
{
	const vector< RoutingRule > rulesForQueue = m_RulesByQueue[ queueId ];

	long initialSequence = theMessage->getRoutingSequence();
	bool messageHeld = false;

	// perform each rule on the message
	for( unsigned int i=0; i<rulesForQueue.size(); i++ )
	{
		// no more routing on the message
		if ( messageHeld )
			break;
			
		DEBUG( "Executing rule #" << i << " of " << rulesForQueue.size() );
		
		// skip to sequence ( start routing from active seq )
		if ( rulesForQueue[ i ].getSequence() <= initialSequence )
			continue;
		
		RoutingAction::ROUTING_ACTION resolution = rulesForQueue[ i ].Route( theMessage, userId, isBulk );
		string outcome = rulesForQueue[ i ].getOutcome();
		if ( outcome.length() > 0 )
			job->addAction( outcome );
		
		DEBUG( "Resolution : " << RoutingAction::ToString( resolution ) );
		
		switch( resolution )
		{
			case RoutingAction::SENDREPLY :
			case RoutingAction::MOVETO :
				{
					// the message was moved ... create a new job and execute it. return
					WorkItem< RoutingJob > workJob( new RoutingJob( theMessage->getTableName(), job->getJobId(), "F=Route", userId ) );
					
					// prepare to redirect
					RoutingJob* redirectJob = workJob.get();
					ApplyRouting( redirectJob );
					
					// sub job 
					if ( redirectJob->isDefered() )
					{
						DEBUG( "Sub job defered. " );
						job->setJobTable( redirectJob->getJobTable() );
						job->Defer( redirectJob->getDeferedQueue(), false );
					}
					
					// copy actions from the subjob
					job->copyActions( *redirectJob );

					return false;
				}
				
			case RoutingAction::COMPLETE :

				// the message was deleted
				return false;
				
			case RoutingAction::WAITON :
			case RoutingAction::CHANGEHOLDSTATUS :

				// if the message is held, abort routing on it				
				if ( theMessage->isHeld() )
				{
					DEBUG( "The message is held now... " )
					messageHeld = true;
				}
				job->setDestination( theMessage->getTableName() );

				break;

			case RoutingAction::ASSEMBLE :

				// if the message is held, abort routing on it				
				if ( theMessage->isHeld() )
				{
					DEBUG( "The message is held now... " )
					messageHeld = true;
				}
				job->setDestination( theMessage->getTableName() );

				break;
				
			//default to continue
			default:
				DEBUG( "Proceeding to next rule." );
				break;
		}		
	}

	return true;
}

void RoutingSchema::ApplyRouting( RoutingJob* job, RoutingMessage ( *messageProviderCallback )(), bool fastpath )
{
	DEBUG( "Executing routing job [" << job->getJobId() << "]" );
	
	//obtain the message	
	WorkItem< RoutingMessage > workMessage = RoutingEngine::getMessagePool().getPoolItem( job->getMessageId() );
	RoutingMessage* theMessage = workMessage.get();
	
	int userId = job->getUserId();
	
	if ( ( userId > 0 ) && !theMessage->isVirtual() )
	//if ( ( userId.length() > 0 ) || ( userId == "-1" ) )
	{
		// this may fail on "raw" messages, but we should be able to continue
		try
		{
			RoutingDbOp::UpdateBusinessMessageUserId( theMessage->getCorrelationId(), userId );
		}
		catch( const DBWarningException& )
		{
			TRACE( "This job is performed on a non-standard message." );
		}
	}
	ApplyRouting( job, theMessage, messageProviderCallback, fastpath );
}

void RoutingSchema::ApplyRouting( RoutingJob* job, RoutingMessage* theMessage, RoutingMessage ( *messageProviderCallback )(), bool fastpath )
{
	if ( theMessage->getPayload() != NULL )
	{
		DEBUG( "Routing message [" << theMessage->getPayload()->getTextConst() << "]" );
	}
	else
	{
		DEBUG( "Routing message [" << theMessage->getMessageId() << "] (no payload available)" );
	}

	int userId = job->getUserId();
	bool isBulk = false;

	theMessage->setUserId( userId );

	// we may not need to evaluate the message for some rules
	RoutingMessageEvaluator* evaluator = theMessage->getPayloadEvaluator();
		
	LogManager::setCorrelationId( theMessage->getCorrelationId() );	

	// check is this is a bulk reply 
	// if we're going fastpath, we're routing the reply for a piece, so the evaluator will return NULL and will not enter here
	if ( ( evaluator != NULL ) && evaluator->isBatch() && evaluator->isReply() )
	{
		// returns true if the process shouldn't continue
		if ( !RouteBatchReply( job, theMessage, fastpath ) )
			return;
		isBulk = true;
	}
		
	// only route if the function calls unhold
	if ( theMessage->isHeld() )
	{
		if ( job->hasUnhold() )
		{
			DEBUG( "HoldStatus changed on message by the job" );
			theMessage->setHeld( false );
		}
		else
		{
			DEBUG( "The message is held, and the job doesn't have unhold" );
			return;
		}
	}
	
	bool delayProcess = false, isBatchJob = false; 

	if ( job->hasFunctionParam( RoutingJob::PARAM_BATCHID ) )
	{
		isBatchJob = true;
		delayProcess = true;

		if ( !RouteBatch( job, theMessage, fastpath ) )
			return;
	}
	
	// Move from UI
	if ( job->isMove() )
	{
		stringstream actionCommand;
		string destination = job->getFunctionParam( RoutingJob::PARAM_DESTINATION );
		//actionCommand << "MoveTo(" << destination << ")";
		DEBUG( "Performing manual move of the message to [" << destination << "]" );
		
		// unhold the message 
		theMessage->setHeld( false );
		// reset sequence
		theMessage->setRoutingSequence( 0 );
		
		// perform move
		RoutingAction actionMove( RoutingAction::MOVETO, destination );
		string outcome = "";

		if ( job->hasFunctionParam( RoutingJob::PARAM_USERCOMMENT ) )
		{
			string userComment = job->getFunctionParam( RoutingJob::PARAM_USERCOMMENT );
			RoutingMessagePayload* payload = theMessage->getPayload();
			if ( payload && payload->IsDocValid() )
			{
				XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc = payload->getDoc();
				if ( doc )
				{
					DOMElement* root = doc->getDocumentElement();
					XERCES_CPP_NAMESPACE_QUALIFIER DOMNode* key = root->getFirstChild();
					for ( ; key != 0; key=key->getNextSibling() )
					{
						if ( key->getNodeType() != DOMNode::ELEMENT_NODE )
							continue;
						else
							break;
					}

					unique_ptr<XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument> newXML( XmlUtil::DeserializeFromString( "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?><root xmlns:xs=\"http://www.w3.org/2001/XMLSchema\"  xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\"  xmlns:wsrm=\"http://schemas.xmlsoap.org/ws/2005/02/rm\"  xmlns:wsu=\"http://schemas.xmlsoap.org/ws/2002/07/utility\" ></root>" ));
					DOMElement* newRoot  = newXML->getDocumentElement();

					DOMNode* sequenceFault = newXML->createElement( unicodeForm( "wsrm:SequenceFault" ) );
					DOMNode* faultCode = newXML->createElement( unicodeForm( "wsrm:FaultCode" ) );
					faultCode->setTextContent( unicodeForm( "MOVE" ) );
					DOMNode* nack = newXML->createElement( unicodeForm( "wsrm:Nack" ) );
					nack->setTextContent( unicodeForm( userComment ) );

					sequenceFault->appendChild( faultCode );
					sequenceFault->appendChild( nack );

					newRoot->appendChild( newXML->importNode( key , true ) );
					newRoot->appendChild (sequenceFault );

					theMessage->setPayload( XmlUtil::SerializeToString( newXML.get() ) );
				}
			}
		}
		outcome = actionMove.Perform( theMessage, userId );

		if ( outcome.length() > 0 )
			job->addAction( outcome );

		// route the message in the new queue		
		WorkItem< RoutingJob > workJob( new RoutingJob( destination, job->getJobId(), "F=Route", userId ) );
		RoutingJob* redirectJob = workJob.get();
		
		ApplyRouting( redirectJob, messageProviderCallback );
		
		// sub job 
		if ( redirectJob->isDefered() )
		{
			DEBUG( "Sub job defered. " );
			job->setJobTable( redirectJob->getJobTable() );
			job->Defer( redirectJob->getDeferedQueue(), false );
		}

		// copy actions from the subjob
		job->copyActions( *redirectJob );

		return;
	}

	if ( job->isReply() )
	{
		string feedback = "";
		
		if ( job->hasFunctionParam( RoutingJob::PARAM_FEEDBACK ) )
			feedback = job->getFunctionParam( RoutingJob::PARAM_FEEDBACK );

		if ( feedback.empty() )
			feedback = "FTP09";

		// unhold the message, reset sequence
		theMessage->setHeld( false );
		theMessage->setRoutingSequence( 0 );

		RoutingAction actionComplete( RoutingAction::SENDREPLY, feedback );

		string outcome = actionComplete.Perform( theMessage, userId );
		if ( outcome.length() > 0 )
			job->addAction( outcome );

		// route the response in the new queue	
		string destination = RoutingDbOp::GetReplyQueue( theMessage->getRequestorService() );
			
		WorkItem< RoutingJob > workJob( new RoutingJob( destination, job->getJobId(), "F=Route", userId ) );
		RoutingJob* redirectJob = workJob.get();
	
		ApplyRouting( redirectJob, messageProviderCallback );

		// copy actions from the subjob
		job->copyActions( *redirectJob );

		return;
	}

	//Reject from UI ?
	if ( job->isComplete() )
	{
		stringstream actionCommand;
		string feedback = "";
		
		if ( job->hasFunctionParam( RoutingJob::PARAM_FEEDBACK ) )
			feedback = job->getFunctionParam( RoutingJob::PARAM_FEEDBACK );

		if ( feedback.empty() )
			feedback = "FTP39";

		//actionCommand << "Complete(" << feedback << ")\0";
		DEBUG( "Performing manual process of the message with feedback [" << feedback << "]" );
		
		// if the message is rejected from businessmessages( not in a queue ), the tablename is empty
		if( theMessage->getTableName().length() == 0 )
		{
			// reactivating with an empty table ( no insert will be performed )
			DEBUG( "The job doesn't specify a table for this message. Performing reactivation ..."  );

			// set the correlationId for reactivation
			string correlationId = theMessage->getMessageId();
			
			theMessage->setCorrelationId( correlationId );
			theMessage->setMessageId( RoutingDbOp::GetOriginalMessageId( correlationId ) );
			RoutingAction::internalPerformReactivate( theMessage, "" );
		}

		// unhold the message, reset sequence
		theMessage->setHeld( false );
		theMessage->setRoutingSequence( 0 );
		
		// perform move
		RoutingAction actionComplete( RoutingAction::COMPLETE, feedback );

		theMessage->setFastpath( true );
		string outcome = actionComplete.Perform( theMessage, userId );
		if ( outcome.length() > 0 )
			job->addAction( outcome );

		if ( feedback.empty() )
			DEBUG( "Message completed with no feedback ( deleted with no reply )" )
		else
			DEBUG( "Message completed with feedback: " << feedback << " ( deleted with no reply )" )

		return;
	}
	// Feedback overide from UI
	if ( job->hasFunctionParam( RoutingJob::PARAM_FEEDBACK ) )
	{
		string uiFeedback = job->getFunctionParam( RoutingJob::PARAM_FEEDBACK );
		DEBUG( "Using UI feedback [" << uiFeedback << "] to override message feedback" );

		// set the feedback received from UI
		RoutingAggregationCode uiAggCode = theMessage->getAggregationCode();
		uiAggCode.Clear();
		
		uiAggCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_FTPCODE, uiFeedback );
		theMessage->setFeedback( uiAggCode );
	}

	// obtain queueid ( if a queue was provided )
	RoutingQueue queue = RoutingDbOp::GetQueue( job->getJobTable() );
	if ( queue.getHeld() )
	{
		job->Defer( queue.getId() );
		DEBUG( ( job->isDefered() ? "Job was deferred" : "Job was not deferred" ) );
		return;
	}
	
	const RoutingPlan* plan = getPlan( queue.getId(), theMessage );
	if ( plan != NULL )
	{
		if ( !ApplyPlanRouting( job, theMessage, plan, userId, isBulk, fastpath ) )
			return;

		// the plan may have changed the message table set active queue as the new queue ( for ep )
		queue = RoutingDbOp::GetQueue( theMessage->getTableName() );

		// reset original queue on the message
		theMessage->setTableName( job->getJobTable() );
	}
	else
	{
		// 102/104 ( don't route batches ) - change batchid for batched messages, so that assemble will use it
		if ( !job->isRoute() )
		{
			DEBUG( "Changing batchid for autobatch message" );
			// change batchid in feedbackagg, so that we can match replies on batchid 
			RoutingAggregationCode batchIdCode( RoutingMessageEvaluator::AGGREGATIONTOKEN_FTPID, theMessage->getCorrelationId() );
			batchIdCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_BATCHID, theMessage->getBatchId() );
			RoutingAggregationManager::AddRequest( batchIdCode, RoutingAggregationManager::UpdateOrFail );

			// execute all rules up to assemble
			ApplyQueueRouting( job, theMessage, queue.getId(), userId, isBulk, fastpath );
		}
		else
		{
			if( !ApplyQueueRouting( job, theMessage, queue.getId(), userId, isBulk, fastpath ) )
			{	
				RoutingMessageEvaluator* evaluator = theMessage->getPayloadEvaluator();
				if( evaluator != NULL && evaluator->isReply() )
				{
					RouteDelayedReply( job, theMessage );
		
				}
				return;
			}
		}
	}

	if ( isBatchJob && delayProcess )
	{
		//string batchSchema = RoutingEngine::getBatchSchema();
		string batchSchema = queue.getExitpoint().getValidationSchema();
		string batchSchemaNamespace = queue.getExitpoint().getValidationSchemaNamespace();
		string xformMessage = theMessage->getPayload()->getText( RoutingMessagePayload::PLAINTEXT );

		Currency itemAmount;

		try
		{
			// validate item to XSD to make sure the batch will be ok
			if( ( batchSchema.length() > 0 ) && job->isRoute() )
			{
				XSDFilter itemCheck;
				NameValueCollection trHeaders;
				trHeaders.Add( XSDFilter::XSDFILE, batchSchema );
				trHeaders.Add( XSDFilter::XSDNAMESPACE, batchSchemaNamespace );

				itemCheck.ProcessMessage( ( unsigned char* )( xformMessage.c_str() ), NULL, trHeaders, true );
			}

			// validate and set amount
			itemAmount.setAmount( theMessage->getBatchTotalAmount() );
		}
		catch( const XSDValidationException& ex )
		{
			stringstream errorMessage;
			RoutingExceptionMoveInvestig inex = RoutingAction::internalPerformMoveToInvestigation( theMessage, ex.getMessage(), true );

			errorMessage << ex.getMessage() << " [" << ex.getAdditionalInfo( "Invalid_reason" ) << "]";
			inex.setMessage( errorMessage.str() );

			inex.addAdditionalInfo( "Invalid_reason", ex.getAdditionalInfo( "Invalid_reason" ) );

			TRACE( errorMessage.str() );
			TRACE( "Buffer is [" << xformMessage << "]" );

			inex.setSeverity( EventSeverity::Fatal );
			throw inex;
		}
		catch( const std::exception& ex )
		{
			stringstream errorMessage;
			RoutingExceptionMoveInvestig inex = RoutingAction::internalPerformMoveToInvestigation( theMessage, ex.what(), true );

			errorMessage << "Message is invalid [" << ex.what() << "]";
			inex.setMessage( errorMessage.str() );

			inex.addAdditionalInfo( "Invalid_reason", ex.what() );

			TRACE( errorMessage.str() );
			TRACE( "Buffer is [" << xformMessage << "]" );

			inex.setSeverity( EventSeverity::Fatal );
			throw inex;
		}	

		BatchManagerBase::BATCH_STATUS batchStatus = BatchManagerBase::BATCH_INPROGRESS;
		if ( ( theMessage->getMessageOptions() & RoutingMessageOptions::MO_RBATCH ) == RoutingMessageOptions::MO_RBATCH )
			batchStatus = BatchManagerBase::BATCH_INPROGRESS;
		else
			batchStatus = job->Batch( theMessage->getBatchId(), theMessage->getCorrelationId(), theMessage->getFeedbackString() , xformMessage, itemAmount.getBDPPart(), itemAmount.getADPPart() );

		// if the batch is complete, continue
		if ( batchStatus != BatchManagerBase::BATCH_READY )
		{
			DEBUG( "The batch [" << theMessage->getBatchId() << "] has status " << BatchManagerBase::ToString( batchStatus ) );
			return;
		}

		DEBUG( "The batch [" << theMessage->getBatchId() << "] is ready and will be sent to the next rule." );

		if ( theMessage->getPayload() != NULL )
		{
			const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* const thePayload = theMessage->getPayload()->getDocConst();
			if ( thePayload != NULL )
				job->setBatchType( XmlUtil::getNamespace( thePayload ) );
		}

		isBulk = true;
		delayProcess = false;

		// 102/104 -- the function is Batch instead of Route
		if ( !job->isRoute() )
		{
			stringstream newFunc;
			newFunc << "F=Route;P=" << RoutingJob::PARAM_GROUPCOUNT << "(" << job->getFunctionParam( RoutingJob::PARAM_GROUPCOUNT ) << ")";

			job->setFunction( newFunc.str() );
			theMessage->setHeld( false );

			// so that the transform will set params like BATCHCOUNT, BATCHAMOUNT...
			theMessage->setBatchSequence( BatchItem::FIRST_IN_SEQUENCE );
			theMessage->setMessageOption( RoutingMessageOptions::MO_BATCH );

			// workaround for assemble ( the message will now be assembled, so rewind last rule )
			DEBUG( "Rewinding routing sequence [Assemble]" );
			theMessage->setRoutingSequence( theMessage->getRoutingSequence() - 1 );
			ApplyRouting( job, messageProviderCallback, false );

			// done.. the batch is done

			// terminate batch explicitly
			RoutingDbOp::TerminateBatch( theMessage->getBatchId(), job->getBatchType(), BatchManagerBase::BATCH_COMPLETED, "ok" );
			
			// delete parts (update wmqid, outputsession for all )
			// this is not needed anymore ( if the message is assembled it changes to requesttype batch, thus it updates the wmqid amd osession when outputed to mq )
			//RoutingDbOp::RemoveBatchMessages( theMessage->getBatchId(), theMessage->getTableName() );

			// rely on this : the message was put to mq and it has 
			/*if ( theMessage->getFeedback().getCorrelToken() != RoutingMessageEvaluator::AGGREGATIONTOKEN_MQID )
				throw logic_error( "Unable to obtain WMQID from message feedback" );

			string mqid = Base64::encode( theMessage->getFeedback().getCorrelId() );

			// set output session, wmqid for all messagess within the assembly
			RoutingAggregationCode request( RoutingMessageEvaluator::AGGREGATIONTOKEN_BATCHID, theMessage->getBatchId() );
			request.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_MQID, mqid );
			request.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_OSESSION, theMessage->getOutputSession() );

			// optimistic update( we know the messages are in there ;) )
			RoutingAggregationManager::AddRequest( request, RoutingAggregationManager::OptimisticUpdateTrimmed );*/

			return;
		}

		// just for fun, validate amount passed agains amount computed
	}
		
	// the message did not leave the queue 
	// if it was not held, process it in exitpoint
	if ( !theMessage->isHeld() && !delayProcess )
	{
		DEBUG( "About to process message in queue ..." );
		string destination = queue.ProcessMessage( theMessage, userId, ( isBulk || theMessage->isBulk() ), job->getBatchType() );
		
		DEBUG( "Destination = [" << destination << "]" );
		 
		job->setDestination( destination );
		// TODO remove it from the queue
	}
}

void RoutingSchema::PerformInitRoutine( long schemaId )
{
	DEBUG( "Performing init routine for schema [" << schemaId << "]" )

	string crtQueue = "";
	string crtRule = "";
	
	try
	{
		// if no init rules, nothing to do
		if ( m_InitRulesByQueue.size() == 0 )
			return;

		map< long, vector< RoutingRule > >::iterator queueIterator = m_InitRulesByQueue.begin();
		while( queueIterator != m_InitRulesByQueue.end() )
		{
			RoutingMessage message;
			
			crtQueue = RoutingDbOp::GetQueueName( queueIterator->first );
			message.setTableName( crtQueue );
			
			for ( unsigned int i=0; i<queueIterator->second.size(); i++ )
			{
				crtRule = RoutingAction::ToString( queueIterator->second[ i ].getAction().getRoutingAction() );
				if( queueIterator->second[ i ].getSchemaId() == schemaId )
				{
					DEBUG( "[" << queueIterator->second[ i ].getSchemaId() << "] Executing init rules for queue [" << queueIterator->first << "] ... " );
					queueIterator->second[ i ].Route( &message, 0 );
				}
			} 
			queueIterator++;
		}
	}
	catch( AppException& ex )
	{
		TRACE_GLOBAL( "An exception has occured during init routine : " << ex.getMessage() );
		
		ex.addAdditionalInfo( "Rule", crtRule );
		ex.addAdditionalInfo( "Queue", crtQueue );
		LogManager::Publish( ex );
		
		throw;
	}
	catch( const std::exception& ex )
	{
		TRACE_GLOBAL( "An exception has occured during init routine : " << ex.what() );
		AppException aex( "An exception has occured during init routine : ", ex );
		aex.addAdditionalInfo( "Rule", crtRule );
		aex.addAdditionalInfo( "Queue", crtQueue );
		
		LogManager::Publish( aex );
		throw;
	}
	catch( ... )
	{
		TRACE_GLOBAL( "An exception has occured during init routine : unknown error"  );
		AppException aex( "An exception has occured during init routine : unknown error" );
		aex.addAdditionalInfo( "Rule", crtRule );
		aex.addAdditionalInfo( "Queue", crtQueue );
		
		LogManager::Publish( aex );
		throw;
	}
}

void RoutingSchema::PerformTearRoutine( long schemaId )
{
	DEBUG( "Performing tear routine for schema [" << schemaId << "]" )
	
	string crtQueue = "";
	string crtRule = "";

	try
	{
		// if no tear rules, nothing to do
		if ( m_TearRulesByQueue.size() == 0 )
			return;

		map< long, vector< RoutingRule > >::iterator queueIterator= m_TearRulesByQueue.begin();	
		while( queueIterator != m_TearRulesByQueue.end() )
		{			
			RoutingMessage message;
			crtQueue = RoutingDbOp::GetQueueName( queueIterator->first );
			message.setTableName( crtQueue );
			
			for ( unsigned int i=0; i<queueIterator->second.size(); i++ )
			{
				crtRule = RoutingAction::ToString( queueIterator->second[ i ].getAction().getRoutingAction() );
				if( queueIterator->second[ i ].getSchemaId() == schemaId )
				{
					DEBUG( "[" << queueIterator->second[ i ].getSchemaId() << "] Executing tear rules for queue ["  << queueIterator->first << "] ... " );
					queueIterator->second[ i ].Route( &message, 0 );
				}
			} 
			queueIterator++;
		}
	}
	catch( AppException& ex )
	{
		TRACE_GLOBAL( "An exception has occured during tear routine : " << ex.getMessage() );
		ex.addAdditionalInfo( "Rule", crtRule );
		ex.addAdditionalInfo( "Queue", crtQueue );
		
		LogManager::Publish( ex );
				
		throw;
	}
	catch( const std::exception& ex )
	{
		TRACE_GLOBAL( "An exception has occured during tear routine : " << ex.what() );
		AppException aex( "An exception has occured during tear routine : ", ex );
		aex.addAdditionalInfo( "Rule", crtRule );
		aex.addAdditionalInfo( "Queue", crtQueue );
		
		LogManager::Publish( aex );		
		throw;
	}
	catch( ... )
	{
		TRACE_GLOBAL( "An exception has occured during tear routine : unknown error"  );
		AppException aex( "An exception has occured during tear routine : unknown error" );
		aex.addAdditionalInfo( "Rule", crtRule );
		aex.addAdditionalInfo( "Queue", crtQueue );
		
		LogManager::Publish( aex );
		throw;
	}
}

RoutingSchema* RoutingSchema::Duplicate( bool createPlans )
{
	// create a copy without init/tear rules
	RoutingSchema* schema = new RoutingSchema( *this );

	schema->m_Copy = true;

	schema->m_InitRulesByQueue.clear();
	schema->m_TearRulesByQueue.clear();
	if ( createPlans )
		schema->CreatePlans();
	schema->Explain();

	// let the schema handle it's own rapid batch messages
	schema->m_RBatchMessages.clear();
	return schema;
}

bool RoutingSchema::RouteDelayedReply( RoutingJob* job, RoutingMessage* response, bool fastpath )
{	
	DEBUG( "Getting delayed messages" );
	pthread_t selfId = pthread_self();
	int userId = job->getUserId();
	bool isFirstDelayedRouted = false;

	// get individual messages from DelayedTFDQueue that match with MIR
	DataSet* delayedItems = NULL;
	string delayedJobId ="";
	string delayId = response->getDelayedReplyId();
	string delayedTable = RoutingDbOp::GetDelayedReplyQueue( response->getRequestorService() );
	if( delayId.length() > 0 && delayedTable.length() > 0 )
	{
		try
		{
			delayedItems = RoutingDbOp::GetDelayedMessages( delayedTable, delayId );
			stringstream eventMessage;
			while( ( delayedItems != NULL ) && ( delayedItems->size() != 0 ) )
			{	
				string correlationId = StringUtil::Trim( delayedItems->getCellValue( 0, "CORRELATIONID" )->getString() );
				//correlate event with current message
				LogManager::setCorrelationId( correlationId );
				
				// guard this message 
				WorkItem< RoutingMessage > workItem( new RoutingMessage() );
				RoutingMessage* delayedItem = workItem.get();

				//set needed fields for routing
				delayedItem->setCorrelationId( correlationId );
				delayedItem->setTableName( delayedTable );
				string msgId = StringUtil::Trim( delayedItems->getCellValue( 0, "ID" )->getString() );
				delayedItem->setMessageId( msgId );
				string requestor = StringUtil::Trim( delayedItems->getCellValue( 0, "REQUESTORSERVICE" )->getString() );
				delayedItem->setRequestorService( requestor );
				string feedback = StringUtil::Trim( delayedItems->getCellValue( 0, "FEEDBACK" )->getString() );
				delayedItem->setFeedback( feedback );
				string payload = StringUtil::Trim( delayedItems->getCellValue( 0, "PAYLOAD" )->getString() );
				delayedItem->setPayload( payload );
				
				delayedJobId = msgId;

				DEBUG_GLOBAL( "Waiting on message pool to allow inserts - thread [" << selfId << "]." );
				RoutingEngine::getMessagePool().addPoolItem( delayedJobId, workItem );
				DEBUG_GLOBAL( "Inserted routing message in pool [" << delayedItem->getMessageId() << "]" );

				WorkItem< RoutingJob > workJob( new RoutingJob( delayedTable, delayedJobId, "F=Route", userId ) );				
				RoutingJob* delayedReplyJob = workJob.get();

				if( !isFirstDelayedRouted )
				{
					ApplyRouting( delayedReplyJob, NULL, false );
					
					isFirstDelayedRouted = true;
					//publish event for every message in delayed queue
					eventMessage << "Message routed from " << delayedReplyJob->getJobTable() << " to " << delayedReplyJob->getDestination();
					AppException aex( eventMessage.str(), EventType::Info );
					if ( userId > 0 )
						aex.addAdditionalInfo( "UserId", StringUtil::ToString( userId ) );
					delayedReplyJob->populateAddInfo( aex );
					LogManager::Publish( aex );
				}
				else
				{
					//move the rest to DuplicateReplies queue
					RoutingExceptionMoveInvestig iex = RoutingAction::internalPerformeMoveToDuplicateReply( delayedItem, 
						"A reply was received, but original message was already replied." );
					LogManager::Publish( iex );
				}

				( void )RoutingEngine::getMessagePool().removePoolItem( delayedJobId );

				delayedItems = RoutingDbOp::GetDelayedMessages( delayedTable, delayId );
			}
			
			if ( delayedItems != NULL )
			{
				delete delayedItems;
				delayedItems = NULL;
			}
			//set back event correlationId for master message to complete
			LogManager::setCorrelationId( response->getCorrelationId());
		}
		catch( ... )
		{		
			if( delayedItems != NULL )
			{
				delete delayedItems;
				delayedItems = NULL;
			}

			if( delayedJobId.length() > 0 )
				( void )RoutingEngine::getMessagePool().removePoolItem( delayedJobId );
			
			if( isFirstDelayedRouted )
			{
				// if first delayed reply was sent to exitpoint we need to commit
				// RE will commit all database operations so far if rethrow fatal event
				AppException aex( "Error while routing delayed duplicate reply", EventType::Error ); 
				aex.setSeverity( EventSeverity::Fatal );
				LogManager::setCorrelationId( response->getCorrelationId() );
				throw &aex;		
			}
			LogManager::setCorrelationId( response->getCorrelationId() );
			throw;
		}
	}

	return false;
}
