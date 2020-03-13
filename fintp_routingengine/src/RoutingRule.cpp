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

#include "Trace.h"
#include "StringUtil.h"
#include "DataSet.h"

#include "RoutingRule.h"
#include "RoutingDbOp.h"

//Routing rule implementation
RoutingRule::RoutingRule( long id, long schemaId, long sequence, string sessionCode )
{
	DEBUG( "CONSTRUCTOR" );
	
	m_Id = id;
	m_SchemaId = schemaId;
	m_Sequence = sequence;
	
	DataSet* myData = NULL;
	
	try
	{
		myData = RoutingDbOp::ReadRoutingRule( id );
		if ( ( myData == NULL ) || ( myData->size() == 0 ) )
		{
			stringstream errorMessage;
			errorMessage << "Rule with id [" << id << "] could not be found.";
			TRACE( errorMessage.str() );
			throw runtime_error( errorMessage.str() );
		}

		// read queue
		m_Queue = RoutingDbOp::GetQueue( myData->getCellValue( 0, "QUEUEID" )->getLong() );
		DEBUG( "Queue got" );
		
		// read description
		m_Description = StringUtil::Trim( myData->getCellValue( 0, "DESCRIPTION" )->getString() );
		
		// add message condition
		string condition = StringUtil::Trim( myData->getCellValue( 0, "MESSAGECONDITION" )->getString() );
		if ( condition.size() > 0 )
			m_Conditions.push_back( RoutingCondition( RoutingCondition::MESSAGE, condition ) );
			
		// add function condition
		condition = StringUtil::Trim( myData->getCellValue( 0, "FUNCTIONCONDITION" )->getString() );
		if ( condition.size() > 0 )
			m_Conditions.push_back( RoutingCondition( RoutingCondition::FUNCTION, condition ) );
		
		// add metadata condition
		condition = StringUtil::Trim( myData->getCellValue( 0, "METADATACONDITION" )->getString() );
		if ( condition.size() > 0 )
			m_Conditions.push_back( RoutingCondition( RoutingCondition::METADATA, condition ) );
			
		// if no conditions were set = always 	
		if ( m_Conditions.size() == 0 )
			m_Conditions.push_back( RoutingCondition( RoutingCondition::ALWAYS, "" ) );
		
		// set action	
		m_Action.setText( StringUtil::Trim( myData->getCellValue( 0, "ACTION" )->getString() ) );
		m_Action.setSessionCode( sessionCode );
		
		DEBUG( "Action set. Cleaning temp. data" );
		
		// cleanup
		if ( myData != NULL )
			delete myData;
			
		DEBUG( "Rule set OK." );
	}
	catch( const std::exception& e )
	{
		TRACE( e.what() );
		
		// cleanup
		if ( myData != NULL )
			delete myData;
			
		throw;
	}
	catch( ... )
	{
		TRACE( "Unhandled exception while creating routing rule with id ["  << id << "]" );
		
		// cleanup
		if ( myData != NULL )
			delete myData;
			
		throw;
	}
}

RoutingRule::~RoutingRule()
{
}

RoutingAction::ROUTING_ACTION RoutingRule::Route( RoutingMessage* message, const int userId, bool bulk ) const
{
	const_cast< RoutingRule* >( this )->m_Outcome = "";

	if( message == NULL )
		throw logic_error( "The message is NULL" );

	//evaluate conditions
	string condition = "";
	for( unsigned int i=0; i<m_Conditions.size(); i++ )
	{
		condition = m_Conditions[ i ].ToString();
		DEBUG( "Evaluating condition : " << condition );
		
		// if any condition says no, don't route
		if ( m_Conditions[ i ].Eval( *message ) == false )
		{
			DEBUG( "Routing rule [" << m_Id << "] not matched." );
			
			// mark the message as processed by this rule
			message->setRoutingSequence( m_Sequence );
			
			// say that the action was not performed
			return RoutingAction::NOACTION;
		}
		DEBUG( "Condition [" << condition << "] matched." );
	}
	
	//perform action
	condition = RoutingAction::ToString( m_Action.getRoutingAction() );
	DEBUG( "Performing action [" << condition << "] on message... " );
	
	try
	{
		// mark the message as processed by this rule
		message->setRoutingSequence( m_Sequence );

		// if the queue has an exitpoint defined, and the exitpoint has MO_GENERATEID set, add that as message flags
		if( ( m_Queue.getMessageOptions() & RoutingMessageOptions::MO_GENERATEID ) == RoutingMessageOptions::MO_GENERATEID )
			message->setMessageOption( RoutingMessageOptions::MO_GENERATEID );

		const_cast< RoutingRule* >( this )->m_Outcome = m_Action.Perform( message, userId, bulk );

		DEBUG( "Action outcome was : " << m_Outcome );
	}
	catch( const RoutingExceptionMessageDeleted& )
	{
		DEBUG( "Disposing bulk message [all parts reactivated]" );

		// HACK : return complete, so that the message is not routed any further
		return RoutingAction::COMPLETE;
	}
	catch( const AppException& ex )
	{
		TRACE( "Exception performing action [" << ex.getMessage() << "]" );
		throw;
	}
	catch( const std::exception& ex )
	{
		TRACE( "Exception performing action [" << ex.what() << "]" );
		throw;
	}
	catch( ... )
	{
		TRACE( "Unknown exception performing action" );
		throw;
	}
	
	DEBUG( "Action done" );
	
	// say that the action was performed
	return m_Action.getRoutingAction();
}

