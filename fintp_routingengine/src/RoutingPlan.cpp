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

#include "RoutingPlan.h"

void RoutingPlan::setForkConditions( const vector< RoutingCondition >& conditions ) 
{ 
	m_ForkConditions = conditions;

	if ( m_SuccessPlan != NULL ) 
		delete m_SuccessPlan;

	if ( m_FailPlan != NULL ) 
		delete m_FailPlan;

	// create left/right trees
	string subPlanName = m_Name + string( "-1" );
	m_SuccessPlan = new RoutingPlan( m_EndQueue, m_EndQueue, m_EndSequence, m_EndSequence, subPlanName );
	m_SuccessPlan->m_RootPlan = this;

	subPlanName = m_Name + string( "-2" );
	m_FailPlan = new RoutingPlan( m_EndQueue, m_EndQueue, m_EndSequence, m_EndSequence, subPlanName );
	m_FailPlan->m_RootPlan = this;
}

RoutingPlan::RoutingPlan( const long planStartQueue, const long planEndQueue, const long startSeq, const long endSeq, const string& planname ) :
	m_StartQueue( planStartQueue ), m_EndQueue( planEndQueue ), m_StartSequence( startSeq ), m_EndSequence( endSeq ), 
	m_Exitpoint( "dummyService", -1, "" ), m_SuccessPlan( NULL ), m_FailPlan( NULL ), m_RootPlan( NULL ), m_IsTerminal( false ),
	m_IsNormalized( false ), m_Name( planname )
{
}

RoutingPlan::~RoutingPlan( void )
{
	try
	{
		if ( m_SuccessPlan != NULL ) 
		{
			delete m_SuccessPlan;
			m_SuccessPlan = NULL;
		}
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while deleting the success plan" );
		} catch( ... ) {}
	}

	try
	{
		if ( m_FailPlan != NULL ) 
		{
			delete m_FailPlan;
			m_FailPlan = NULL;
		}
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while deleting the fail plan" );
		} catch( ... ) {}
	}
}

// a 0 sized plan can be split 2 ways on its condition
RoutingPlan* RoutingPlan::normalize( const string& planname )
{
	RoutingPlan* plan = normalizeRules( planname );
	normalizeConditions( plan );

	return plan;
}

void RoutingPlan::normalizeConditions( RoutingPlan* plan ) const
{
	if ( plan == NULL )
		throw runtime_error( "Undefined plan being normalized" );

	// display conditions
	DEBUG( "Normalizing fail conditions ..." );

	vector< RoutingCondition > removeConditions;
	removeConditions.swap( plan->m_FailConditions );

	for ( unsigned int i=0; i<removeConditions.size(); i++ )
	{
		bool found = false;
		for ( unsigned int j=i+1; j<removeConditions.size(); j++ )
		{
			// ugly way to check is two conditions are the same
			if ( ( removeConditions[ i ].getConditionType() == removeConditions[ j ].getConditionType() ) &&
				( removeConditions[ i ].ToString() == removeConditions[ j ].ToString() ) )
			{
				DEBUG( "Removing duplicate condition [" << removeConditions[ i ].ToString() << "]" );
				found = true;
				break;
			}
		}
		if ( !found )
			plan->m_FailConditions.push_back( removeConditions[ i ] );
	}

	removeConditions.clear();
	removeConditions.swap( plan->m_SuccessConditions );

	DEBUG( "Normalizing success conditions ..." );
	for ( unsigned int k=0; k<removeConditions.size(); k++ )
	{
		bool found = false;
		for ( unsigned int j=k+1; j<removeConditions.size(); j++ )
		{
			// ugly way to check is two conditions are the same
			if ( ( removeConditions[ k ].getConditionType() == removeConditions[ j ].getConditionType() ) &&
				( removeConditions[ k ].ToString() == removeConditions[ j ].ToString() ) )
			{
				DEBUG( "Removing duplicate condition [" << removeConditions[ k ].ToString() << "]" );
				found = true;
				break;
			}
		}
		if ( !found )
			plan->m_SuccessConditions.push_back( removeConditions[ k ] );
	}
}

RoutingPlan* RoutingPlan::normalizeRules( const string& planname )
{
	DEBUG( "Normalizing plan [" << m_Name << "] target is [" << planname << "]" );

	// check if the name is ok
	if ( m_IsTerminal || m_Exitpoint.isValid() ) 
	{
		// if it's this plan, start normalizing
		if ( planname == m_Name )
		{
			// terminal paths need no normalization
			if ( m_IsNormalized )
			{
				DEBUG( "Plan already normalized/not terminal" );
				return NULL;
			}

			m_IsNormalized = true;
			return this;
		}
		else
			return NULL;
	}

	if ( m_Name.length() > planname.length() )
	{
		TRACE( "Unable to normalize plan [not terminal]" );
		return NULL;
	}

	string nextStr = planname.substr( m_Name.length() );

	RoutingPlan* plan = NULL;

	// success plan
	if ( nextStr.substr( 0, 2 ) == string( "-1" ) )
	{
		if ( m_SuccessPlan == NULL )
			throw runtime_error( string( "Unable to find a plan with name" ) + planname );

		plan = m_SuccessPlan->normalize( planname );
		if ( plan == NULL )
			return NULL;

		for ( unsigned int i=0; i<m_ForkConditions.size(); i++ )
		{
			plan->m_SuccessConditions.push_back( m_ForkConditions[ i ] );
		}
	}
	else // fail plan
	{
		if ( m_FailPlan == NULL )
			throw runtime_error( string( "Unable to find a plan with name" ) + planname );
		
		plan = m_FailPlan->normalize( planname );
		if ( plan == NULL )
			return NULL;

		for ( unsigned int i=0; i<m_ForkConditions.size(); i++ )
		{
			plan->m_FailConditions.push_back( m_ForkConditions[ i ] );
		}
	}

	// add all rules
	for ( unsigned int i=0; i<m_Rules.size(); i++ )
	{
		( void )plan->m_Rules.insert( plan->m_Rules.begin() + i, m_Rules[ i ] );
	}

	return plan;
}

void RoutingPlan::displayNormalized( const unsigned level )
{
	string indentation( level, '\t' );

	if ( m_Exitpoint.isValid() )
	{
		DEBUG( indentation << "Move to exitpoint [" << m_Exitpoint.getDefinition() << "]" );
	}
	else
	{
		DEBUG( indentation << "Hold message [no more rules]" );
	}

	// display conditions
	DEBUG( indentation << "Fail conditions :" );
	for ( unsigned int i=0; i<m_FailConditions.size(); i++ )
	{
		DEBUG( indentation << "#" << i << " [" << m_FailConditions[ i ].ToString() << "]" );
	}

	DEBUG( indentation << "Success conditions :" );
	for ( unsigned int j=0; j<m_SuccessConditions.size(); j++ )
	{
		DEBUG( indentation << "#" << j << " [" << m_SuccessConditions[ j ].ToString() << "]" );
	}
}

void RoutingPlan::displayNested( const unsigned level )
{
	string indentation( level, '\t' );

	if ( ( m_ForkConditions.size() > 0 ) && ( m_ForkConditions[ 0 ].getConditionType() != RoutingCondition::ALWAYS ) )
	{
		stringstream conditionStr;
		for ( unsigned int i=0; i<m_ForkConditions.size(); i++ )
		{
			if ( i > 0 )
				conditionStr << " and ";
			conditionStr << "[" << m_ForkConditions[ i ].ToString() << "]";
		}

		if ( m_SuccessPlan != NULL )
		{
			DEBUG( indentation << "if " << conditionStr.str() );
			m_SuccessPlan->display( level+1 );
		}
		if ( m_FailPlan != NULL )
		{
			DEBUG( indentation << "if not " << conditionStr.str() );
			m_FailPlan->display( level+1 );
		}
	}
	else if ( !m_IsTerminal )
	{
		if ( m_Exitpoint.isValid() )
		{
			DEBUG( indentation << "Move to exitpoint [" << m_Exitpoint.getDefinition() << "]" );
		}
		else
		{
			DEBUG( indentation << "Hold message [no more rules]" );
		}
	}
}

void RoutingPlan::display( const unsigned int level )
{
	string indentation( level, '\t' );

	DEBUG( indentation<< "Plan [" << m_Name << "] is " << ( m_IsNormalized ? "normalized" : "not normalized" ) << "; Plan size : " << m_Rules.size() );

	for( vector< RoutingRule >::const_iterator ruleWalker = m_Rules.begin(); ruleWalker != m_Rules.end(); ruleWalker++ )
	{
		DEBUG( indentation << ruleWalker->getSequence() << " : " << ruleWalker->getAction().ToString() );
	}

	if ( m_IsNormalized )
		displayNormalized( level );
	else
		displayNested( level );
}

bool RoutingPlan::usable( RoutingMessage * theMessage ) const
{
	// display conditions
	DEBUG( "Checking fail conditions ..." );
	for ( unsigned int i=0; i<m_FailConditions.size(); i++ )
	{
		// if any condition says no, don't route
		if ( m_FailConditions[ i ].Eval( *theMessage ) )
		{
			DEBUG( "Fail conditions not satisfied for plan routing ... falling back to queue routing" );
			return false;
		}
	}

	DEBUG( "Checking success conditions ..." );
	for ( unsigned int j=0; j<m_SuccessConditions.size(); j++ )
	{
		// if any condition says no, don't route
		if ( !m_SuccessConditions[ j ].Eval( *theMessage ) )
		{
			DEBUG( "Success conditions not satisfied for plan routing ... falling back to queue routing" );
			return false;
		}
	}

	return true;
}
