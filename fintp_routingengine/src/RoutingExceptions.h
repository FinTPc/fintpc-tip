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

#ifndef ROUTINGEXCEPTIONS_H
#define ROUTINGEXCEPTIONS_H

#include <stdexcept>

#include "AppExceptions.h"

#include "RoutingEngineMain.h"

#define RoutingExceptionLocation "Location not available"
//string( "<ExceptionLocation file=\"" ) + __FILE__ + string( "\" line=\" __LINE__ \">")
//+ string( __FILE__) + string( """ line=""" ) + string( __LINE__ )+ string( """>") )

class RoutingException : public logic_error
{
	private :
		string m_Location;
	
	public:

		RoutingException( const string& message, const string& location );
		~RoutingException() throw() {};
	
		static RoutingException EmbedException( const string& message, const string& location, const RoutingException &rex );
	
		string getLocation() const { return m_Location; }
};

class RoutingExceptionMoveInvestig : public AppException
{
	public:

		RoutingExceptionMoveInvestig( const string& reason = "unknown reason" ) : AppException( "Message moved to investigation", EventType::Warning )
		{
			m_Message = string( "Message moved to investigation [" ) + reason + string( "]" );
			addAdditionalInfo( "Reason", reason );
		}
		~RoutingExceptionMoveInvestig() throw() {};
};

class RoutingExceptionMessageNotFound : public AppException
{
	public:

		RoutingExceptionMessageNotFound( const string& reason = "unknown reason" ) : AppException( "Message not found", EventType::Warning )
		{
			m_Message = string( "Message not found [" ) + reason + string( "]" );
			addAdditionalInfo( "Reason", reason );
		}
		~RoutingExceptionMessageNotFound() throw() {};
};

class RoutingExceptionMessageDeleted : public AppException
{
	public:

		RoutingExceptionMessageDeleted() : AppException( "Message deleted", EventType::Warning )
		{
			m_Message = string( "Message deleted" );
		}
		~RoutingExceptionMessageDeleted() throw() {};
};

class RoutingExceptionAggregationFailure : public AppException
{
	public:

		RoutingExceptionAggregationFailure( const string& token, const string& value ) : AppException( "No aggregation records found", EventType::Error )
		{
			m_Message = string( "No aggregation records found for [" ) + token + string( " = " ) + value + string( "]" );
			addAdditionalInfo( "TokenName", token );
			addAdditionalInfo( "TokenValue", value );
		}
		~RoutingExceptionAggregationFailure() throw() {};
};

class RoutingExceptionJobAttemptsExceded : public logic_error
{
	public:

		RoutingExceptionJobAttemptsExceded();
		~RoutingExceptionJobAttemptsExceded() throw() {};
};

#endif
