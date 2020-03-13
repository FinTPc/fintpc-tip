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

#ifdef WIN32
	#include "windows.h"
	#define sleep(x) Sleep( (x)*1000 )
#endif

#include "Trace.h"
#include "DataSet.h"
#include "StringUtil.h"

#include "RoutingJob.h"
#include "RoutingDbOp.h"
#include "RoutingExceptions.h"

const string RoutingJob::PARAM_ROUTINGPOINT = "ROUTINGPOINT";
const string RoutingJob::PARAM_ROUTINGKEY = "ROUTINGKEY";
const string RoutingJob::PARAM_FUNCTION = "FUNCTION";
const string RoutingJob::PARAM_USERID = "USERID";
const string RoutingJob::PARAM_DESTINATION = "Destination";
const string RoutingJob::PARAM_FEEDBACK = "Feedback";
const string RoutingJob::PARAM_GROUPORDER = "GroupOrder";
const string RoutingJob::PARAM_GROUPCOUNT = "GroupCount";
const string RoutingJob::PARAM_BATCHID = "BatchID";
const string RoutingJob::PARAM_BATCHREF = "BatchRef";
const string RoutingJob::PARAM_GROUPAMOUNT = "BatchSum";
const string RoutingJob::PARAM_USERCOMMENT = "UserComment";

RoutingJob::RoutingJob() : m_BackoutCount( 0 ), m_DeferedQueue( 0 ),
	m_JobId( "" ), m_RoutingPoint( "" ), m_Function( "" ), m_UserId( 0 ), m_BatchId( "" ), m_BatchType( "" ),
	m_IsBatch( false ), m_BatchStatus( BatchManagerBase::BATCH_FAILED ),
	m_HasUnhold( -1 ), m_IsMove( -1 ), m_IsRoute( -1 ), m_IsComplete( -1 ), m_IsReply( -1),
	m_Destination( "?" ), m_IsParallel( true )
{
}

RoutingJob::RoutingJob( const string& table, const string& jobId, const string& function, const int userId ) : 
	m_BackoutCount( 0 ), m_DeferedQueue( 0 ),
	m_JobId( jobId ), m_RoutingPoint( table ), m_Function( function ), m_UserId( 0 ), m_BatchId( "" ), m_BatchType( "" ),
	m_IsBatch( false ), m_BatchStatus( BatchManagerBase::BATCH_FAILED ),
	m_HasUnhold( -1 ), m_IsMove( -1 ), m_IsRoute( -1 ), m_IsComplete( -1 ), m_IsReply( -1),
	m_Destination( "?" ), m_IsParallel( true )
{
}

RoutingJob::RoutingJob( const string& jobId ) : m_BackoutCount( 0 ), m_DeferedQueue( 0 ),
	m_JobId( "" ), m_RoutingPoint( "" ), m_Function( "" ), m_UserId( 0 ), m_BatchId( "" ), m_BatchType( "" ),
	m_IsBatch( false ), m_BatchStatus( BatchManagerBase::BATCH_FAILED ),
	m_HasUnhold( -1 ), m_IsMove( -1 ), m_IsRoute( -1 ), m_IsComplete( -1 ), m_IsReply( -1),
	m_Destination( "?" ), m_IsParallel( true )
{
	ReadNextJob( jobId );
}

RoutingJob::~RoutingJob()
{
	try
	{
		m_RoutingActions.clear();
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while clearing routing actions" );
		}catch( ... ){}
	}
	try
	{
		m_Params.clear();
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while clearing params" );
		}catch( ... ){}
	}
}

void RoutingJob::ReadNextJob( const string& jobId )
{
	m_BackoutCount = 0;
	m_JobId = "";
	m_RoutingPoint = "";
	m_Function = "";
	m_UserId = 0;
	m_BatchId = "";
	m_BatchType = "";
	m_HasUnhold = -1;
	m_IsMove = -1;
	m_IsRoute = -1;
	m_IsComplete = -1;
	m_IsReply = -1;
	m_DeferedQueue = 0;
	
	m_BatchStatus = BatchManagerBase::BATCH_FAILED;
	m_IsBatch = false;
	m_IsParallel = true;
	m_Destination = "?";

	DataSet *myDS = NULL;
	
	// TODO setup private members
	// get backout count
	try
	{
		myDS = RoutingDbOp::GetRoutingJob( jobId );
		if ( myDS == NULL )
			throw runtime_error( "Failed to read job [dataset empty]" );
			
		m_BackoutCount = myDS->getCellValue( 0, "BACKOUT" )->getLong();
		//m_DeferedQueue = myDS->getCellValue( 0, "STATUS" )->getLong();
		
		// job id
		m_JobId = StringUtil::Trim( myDS->getCellValue( 0, "ID" )->getString() );
		
		DEBUG( "Backout for job [" << m_JobId << "] = " << m_BackoutCount );
	
		m_RoutingPoint = StringUtil::Trim( myDS->getCellValue( 0, "ROUTINGPOINT" )->getString() );
		m_Function = StringUtil::Trim( myDS->getCellValue( 0, "FUNCTION" )->getString() );
		m_UserId = myDS->getCellValue( 0, "USERID" )->getInt();

		// if we tried 3 times to undertake this job .. dump it
		if ( m_BackoutCount > ROUTINGJOB_MAX_BACKOUT )
		{
			TRACE( "Job [" << jobId << "] exceded backout count and will be ignored." );
			throw RoutingExceptionJobAttemptsExceded();
		}

		// here we churn parallel from non parallel
		//if ( hasFunctionParam( RoutingJob::PARAM_BATCHID ) )
		//	m_IsParallel = false;
				
		if ( myDS != NULL )
		{
			delete myDS;
			myDS = NULL;
		}		
	}
	catch( const RoutingExceptionJobAttemptsExceded& ex )
	{
		if ( myDS != NULL )
		{
			delete myDS;
			myDS = NULL;
		}

		TRACE( ex.what() );			
		throw;
	}
	catch( const DBErrorException& )
	{
		if ( myDS != NULL )
		{
			delete myDS;
			myDS = NULL;
		}

		/*// HACK .. code 20000 is thrown by the proc to signal "no jobs"
		// sleep here as the watcher constantly fires the notification
		if ( ex.code() == "20000" )
		{
			DEBUG( "Sleeping 5 seconds.. " );
			sleep( 5 );
		}*/

		TRACE( "DBException reading next job." );
		throw;
	}
	catch( ... )
	{
		if ( myDS != NULL )
		{
			delete myDS;
			myDS = NULL;
		}
						
		TRACE( "Exception reading next job." );
		throw;
	}
}

bool RoutingJob::hasUnhold() const
{
	string::size_type posUnhold = StringUtil::ToUpper( m_Function ).find( "F=UNHOLD" );
	return( posUnhold != string::npos );
}

bool RoutingJob::hasFunctionParam( const string& paramName )
{
	try
	{
		string param = getFunctionParam( paramName );
		return ( param.length() > 0 );
	}
	catch( ... )
	{
		return false;
	}
}

string RoutingJob::getFunctionParam( const string& paramName )
{
	if ( m_Params.find( paramName ) != m_Params.end() )
		return m_Params[ paramName ];
	
	stringstream param;
	param << "P=" << paramName << "(";
	string function = getFunction();
	string::size_type posParam = function.find( param.str() );
	if( posParam == string::npos )
	{
		stringstream errorMessage;
		errorMessage << "Param not found [" << paramName << "] ... looking for [" << param.str() << "] in function [" << function << "]";
		throw invalid_argument( errorMessage.str() );
	}
	string::size_type nextBracket = function.find( ")", posParam );
	if( posParam == string::npos )
		throw logic_error( "Bad function : missing ) for param definition" );
		
	string::size_type statLen = param.str().length();
	string paramValue = function.substr( posParam + statLen, ( nextBracket - posParam ) - statLen );

	DEBUG( "Param [" << paramName << "] = [" << paramValue << "]" );
	m_Params[ paramName ] = paramValue;
	
	return paramValue;
}

bool RoutingJob::isRoute()
{
	if ( m_IsRoute < 0 )
	{
		string::size_type posRoute = StringUtil::ToUpper( m_Function ).find( "F=ROUTE" );
		m_IsRoute = ( posRoute != string::npos );
	}
	return m_IsRoute;
}

bool RoutingJob::isMove()
{
	if ( m_IsMove < 0 )
	{
		string::size_type posMove = StringUtil::ToUpper( m_Function ).find( "F=DISPOSE" );
		m_IsMove = ( posMove != string::npos );
	}
	return m_IsMove;
}

bool RoutingJob::isComplete()
{
	if ( m_IsComplete < 0 )
	{
		string::size_type posComplete = StringUtil::ToUpper( m_Function ).find( "F=COMPLETE" );
		m_IsComplete = ( posComplete != string::npos );
	}
	return m_IsComplete;
}

bool RoutingJob::isReply()
{
	if ( m_IsReply < 0 )
	{
		string::size_type posReply = StringUtil::ToUpper( m_Function ).find( "F=REPLY" );
		m_IsReply = ( posReply != string::npos );
	}
	return m_IsReply;
}

void RoutingJob::Defer( const long queueId, const bool dbCommit )
{
	m_DeferedQueue = queueId;
	
	// TODO Rollback remove
	if ( dbCommit )
		RoutingDbOp::DeferJob( m_JobId, m_DeferedQueue, m_RoutingPoint, m_Function, m_UserId );
}

void RoutingJob::populateAddInfo( AppException& ex ) const
{
	for ( unsigned int i=0; i<m_RoutingActions.size(); i++ )
	{
		stringstream crtAction;
		crtAction << "Action " << i << " performed";
		ex.addAdditionalInfo( crtAction.str(), m_RoutingActions[ i ] );
	}
}

void RoutingJob::setFunction( const string& function )
{
	m_Function = function;
	m_IsRoute = -1;
	m_IsBatch = false;
	m_Params.clear();

	m_IsParallel = true;
	//m_IsParallel = !( hasFunctionParam( RoutingJob::PARAM_BATCHID ) );
}
