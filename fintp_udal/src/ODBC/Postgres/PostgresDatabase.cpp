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

#include "PostgresDatabase.h"

namespace FinTP {

string PostgresDatabase::callFormating( const string& statementString, const ParametersVector& vectorOfParameters )
{
	const size_t paramCount = vectorOfParameters.size();
	// Compose the Stored Procedure CALL statement
	stringstream paramBuffer;
	paramBuffer << "{CALL " << statementString << "(";

	for ( size_t i=0; i<paramCount; i++ )
	{
		if ( vectorOfParameters[i]->getType() != DataType::ARRAY )
		{
			paramBuffer << " ? ";
			if ( i < paramCount - 1 )
				paramBuffer << ",";
		}
		else
		{
			paramBuffer << "ARRAY[ ";
			const size_t arrayDimension = vectorOfParameters[i]->getDimension();
			for ( size_t j=0; j < arrayDimension; j++)
			{
				paramBuffer << " ? ";
				if ( j < arrayDimension - 1 )
					paramBuffer << ",";
			}
			paramBuffer << " ]";
			if ( i < paramCount - 1 )
				paramBuffer << ",";
		}
	}

	paramBuffer << ")}";

	return paramBuffer.str();
}

DataSet* PostgresDatabase::ExecuteQuery( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement, bool holdCursor, const unsigned int fetchRows )
{
	DataCommand query( commandType, DataCommand::QUERY, stringStatement, false );
	ParametersVector tempVector;
	return ExecuteQuery( query, tempVector, holdCursor, fetchRows );
}

DataSet* PostgresDatabase::ExecuteQueryCached( const DataCommand::COMMAND_TYPE commType, const string& stringStatement, const bool onCursor, const unsigned int fetchRows )
{
	DataCommand query( commType, DataCommand::QUERY, stringStatement, true );
	ParametersVector tempVector;
	return ExecuteQuery( query, tempVector, onCursor, fetchRows );
}

DataSet* PostgresDatabase::ExecuteQuery( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement, const ParametersVector& vectorOfParameters, bool holdCursor, const unsigned int fetchRows )
{
	DataCommand query( commandType, DataCommand::QUERY, stringStatement, false );
	return ExecuteQuery( query, vectorOfParameters, holdCursor, fetchRows );
}

DataSet* PostgresDatabase::ExecuteQueryCached( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement, const ParametersVector& vectorOfParameters, const bool holdCursor, const unsigned int fetchRows )
{
	DataCommand query( commandType, DataCommand::QUERY, stringStatement, true );
	return ExecuteQuery( query, vectorOfParameters, holdCursor, fetchRows );
}

DataSet* PostgresDatabase::ExecuteQuery( const DataCommand& query, const ParametersVector& vectorOfParameters, bool holdCursor, const unsigned int fetchRows )
{
	//TODO: Find a better to way to find if a SP returned a refcursor. 
	DataSet* result = NULL;
	try
	{
		result = innerExecuteCommand( query, vectorOfParameters, holdCursor, fetchRows );

		if ( result->size() == 1 && query.getCommandType() == DataCommand::SP )
		{
			const map< std::string, DataColumnBase* >::const_iterator firstCell = result->at(0)->begin();
			const string columnName = firstCell->first;
			const string refCursorName = firstCell->second->getString();

			unsigned int foundPortalNumber = 0;
			if ( refCursorName.find( "<unnamed portal ", 0 ) == 0 && *refCursorName.rbegin() == '>'  )
			{
				const size_t portalNumberStart = sizeof("<unnamed portal ")-1;
				const size_t portalNumberStringSize = refCursorName.find( ">", portalNumberStart ) - portalNumberStart;
				if ( portalNumberStringSize != string::npos )
				{
					const string portalNumberString = refCursorName.substr( portalNumberStart, portalNumberStringSize );
					try
					{
						foundPortalNumber = boost::lexical_cast<unsigned int>( portalNumberString );
					}
					catch ( const boost::bad_lexical_cast& e )
					{
						const string& what = e.what();
						throw runtime_error( "Invalid portal number." + what );
					}
					if ( foundPortalNumber == 0 )
						throw runtime_error( "Invalid portal number" );
					delete result;
					result = NULL;
					//TODO: Find out why this statement doesn't work with bind parameters.
					return ExecuteQuery( DataCommand::INLINE, "FETCH ALL FROM \"" + refCursorName +"\"", false, 0 );
				}
			}
		}
	}
	catch( ... )
	{
		delete result;
		throw;
	}

	return result;
}

}
