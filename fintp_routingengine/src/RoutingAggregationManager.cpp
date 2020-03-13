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

#include "RoutingAggregationManager.h"
#include "RoutingDbOp.h"
#include "RoutingExceptions.h"

// RoutingAggregationManager implementation
RoutingAggregationManager::RoutingAggregationManager()
{
}

RoutingAggregationManager::~RoutingAggregationManager()
{
}

string RoutingAggregationManager::GetAggregationField( RoutingAggregationCode& request, string requestedToken, string aggregationTable )
{
	RoutingAggregationCode newRequest( request.getCorrelToken(), request.getCorrelId() );
	
	newRequest.addAggregationField( requestedToken, "" );
	GetAggregationFields( newRequest, aggregationTable );

	RoutingAggregationFieldArray fields = newRequest.getFields();
	DEBUG_GLOBAL( "Aggregation field [" << requestedToken << "] = [" << fields[ requestedToken ] << "]" );
	
	return fields[ requestedToken ];
}

bool RoutingAggregationManager::GetAggregationFields( RoutingAggregationCode& request, string aggregationTable )
{
	return RoutingDbOp::GetAggregationFields( aggregationTable, request );
}

void RoutingAggregationManager::AddRequest( const RoutingAggregationCode& request, const REQUEST_MODE requestMode,
	string aggregationTable )
{
	switch( requestMode )
	{
		case RoutingAggregationManager::OptimisticInsert :
			try
			{
				RoutingDbOp::InsertAggregationRequest( aggregationTable, request );
			}
			catch( ... )
			{
				stringstream errorMessage;
				errorMessage << "Unable to insert aggregation table [" << aggregationTable << "] using optimistic insert. Falling back to non-optimistic update.";
				
				TRACE_GLOBAL( errorMessage.str() );
				AddRequest( request, RoutingAggregationManager::InsertOrUpdate, aggregationTable );
			}
			break;

		case RoutingAggregationManager::OptimisticUpdate :
			try
			{
				RoutingDbOp::UpdateAggregationRequest( aggregationTable, request );
			}
			catch( const DBNoUpdatesException& ex )
			{
				//update performed but no rows were updated
				throw ex;
			}	
			catch( ... )
			{
				stringstream errorMessage;
				errorMessage << "Unable to update aggregation table [" << aggregationTable << "] using optimistic update. Falling back to non-optimistic update.";
				
				TRACE_GLOBAL( errorMessage.str() );
				AddRequest( request, RoutingAggregationManager::InsertOrUpdate, aggregationTable );
			}
			break;

		case RoutingAggregationManager::OptimisticUpdateTrimmed :
			try
			{
				RoutingDbOp::UpdateAggregationRequest( aggregationTable, request, true );
			}
			catch( const DBNoUpdatesException& ex )
			{
				//update performed but no rows were updated
				throw ex;
			}	
			catch( ... )
			{
				stringstream errorMessage;
				errorMessage << "Unable to update aggregation table [" << aggregationTable << "] using optimistic update trimmed. Falling back to non-optimistic update.";
				
				TRACE_GLOBAL( errorMessage.str() );
				AddRequest( request, RoutingAggregationManager::InsertOrUpdate, aggregationTable );
			}
			break;

		case RoutingAggregationManager::InsertOrUpdate :
			{
				bool trimmedMatch = false;
				bool recordExists = RoutingDbOp::AggregationRequestOp( aggregationTable, request, trimmedMatch );
				
				if ( recordExists )
					RoutingDbOp::UpdateAggregationRequest( aggregationTable, request, trimmedMatch );
				else
				{
					// this may fail if another constraint is violated or some 
					RoutingDbOp::InsertAggregationRequest( aggregationTable, request );
				}
			}
			break;

		case RoutingAggregationManager::UpdateOrFail :
			{
				bool trimmedMatch = false;
				bool recordExists = RoutingDbOp::AggregationRequestOp( aggregationTable, request, trimmedMatch );
				
				if ( recordExists )
					RoutingDbOp::UpdateAggregationRequest( aggregationTable, request, trimmedMatch );
				else
				{
					throw RoutingExceptionAggregationFailure( request.getCorrelToken(), request.getCorrelId() );
				}
			}
			break;
	}
}

