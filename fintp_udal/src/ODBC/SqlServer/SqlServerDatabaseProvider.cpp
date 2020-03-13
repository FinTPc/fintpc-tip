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
#pragma warning( disable : 4661 )
#endif

#include "SqlServerDatabaseProvider.h"
#include "SqlServerDatabase.h"
#include "Trace.h"
#include <iostream>
#include <string>
#include <sstream>

using namespace std;
using namespace FinTP;

Database* SqlServerDatabaseFactory::createDatabase()
{
	DEBUG( "Creating new ODBC database definition..." );
	Database* retDatabase = NULL;
	try
	{
		retDatabase = new SqlServerDatabase();
		if ( retDatabase == NULL )
			throw runtime_error( "Unable to create databse defintion" );
	}
	catch( ... )
	{
		if ( retDatabase != NULL )
			delete retDatabase;
		throw;
	}
	return retDatabase;
}

short int SqlServerDatabaseFactory::getSqlServerSqlType( const DataType::DATA_TYPE sqlServerType, const int dimension )
{
	switch ( sqlServerType )
	{
		case DataType::CHAR_TYPE :
			if ( dimension > 255 )
			{
				DEBUG_GLOBAL2( "Type is SQL VARCHAR" );
				return SQL_VARCHAR;
			}
			else
			{
				DEBUG_GLOBAL2( "Type is SQL CHAR" );
				return SQL_CHAR;
			}

		case DataType::SHORTINT_TYPE:
			DEBUG_GLOBAL2( "Type is SQL SMALLINT" );
			return SQL_SMALLINT;

		case DataType::TIMESTAMP_TYPE:
			DEBUG_GLOBAL2( "Type is SQL TIMESTAMP" );
			return SQL_TYPE_TIMESTAMP;

		case DataType::NUMBER_TYPE:
			DEBUG_GLOBAL2( "Type is SQL DECIMAL" );
			return SQL_DECIMAL;

		case DataType::LONGINT_TYPE:
			DEBUG_GLOBAL2( "Type is SQL INTEGER" );
			return SQL_INTEGER;

		case DataType::LARGE_CHAR_TYPE:
			//if ( dimension > 4000 )
		{
			DEBUG_GLOBAL2( "Type is SQL LONGVARBINARY" );
			return SQL_LONGVARBINARY;
		}
		/*else
		{
			DEBUG_GLOBAL2( "Type is SQL LONGVARCHAR" );
			return SQL_LONGVARCHAR;
		}*/

		case DataType::BINARY :
			DEBUG2( "Type is SQL LONGVARBINARY" );
			return SQL_VARBINARY;

		default:
			break;
	}

	stringstream errorMessage;
	errorMessage << "Invalid data type specified for mapping to SQLSERVER SQL type [" << sqlServerType << "] = [" << DataType::ToString( sqlServerType ) << "]";

	TRACE( errorMessage.str() );
	throw invalid_argument( errorMessage.str() );
}

short int SqlServerDatabaseFactory::getSqlServerDataType( const DataType::DATA_TYPE sqlServerType )
{
	switch ( sqlServerType )
	{
		case DataType::CHAR_TYPE :
			DEBUG_GLOBAL2( "SQL SqlServer Type is SQL C CHAR" );
			return SQL_C_CHAR;

		case DataType::DATE_TYPE :
			DEBUG_GLOBAL2( "SQL SqlServer Type is SQL C DATE" );
			return SQL_C_TYPE_DATE;

		case DataType::TIMESTAMP_TYPE :
			DEBUG_GLOBAL2( "SQL SqlServer Type is SQL C TIMESTAMP" );
			return SQL_C_TYPE_TIMESTAMP;

		case DataType::SHORTINT_TYPE :
			DEBUG_GLOBAL2( "SQL SqlServer Type is SQL C SHORT" );
			return SQL_C_SSHORT;

		case DataType::LONGINT_TYPE :
			DEBUG_GLOBAL2( "SQL SqlServer Type is SQL C LONG" );
			return SQL_C_SLONG;

		case DataType::LARGE_CHAR_TYPE :
		case DataType::BINARY:
			DEBUG_GLOBAL2( "SQL SqlServer Type is SQL C BINARY" );
			return SQL_C_BINARY;

		default:
			break;
	}

	stringstream errorMessage;
	errorMessage << "Invalid data type specified for mapping to SqlServer type [" << sqlServerType << "] = [" << DataType::ToString( sqlServerType ) << "]";

	TRACE_GLOBAL( errorMessage.str() );
	throw invalid_argument( errorMessage.str() );
}

DataType::DATA_TYPE SqlServerDatabaseFactory::getDataType( const short int sqlServerType, const int dimension )
{
	switch ( sqlServerType )
	{
		case SQL_VARCHAR :
		case SQL_CHAR :
			return DataType::CHAR_TYPE;

		case SQL_SMALLINT :
			return DataType::SHORTINT_TYPE;

		case SQL_TYPE_DATE :
		case SQL_TYPE_TIMESTAMP :
			//return DataType::TIMESTAMP_TYPE;
			return DataType::CHAR_TYPE;

		case SQL_INTEGER :
			return DataType::LONGINT_TYPE;

		case SQL_DECIMAL :
			return DataType::CHAR_TYPE;

		case SQL_LONGVARCHAR :
			return DataType::LARGE_CHAR_TYPE;

		case SQL_VARBINARY :
			return DataType::BINARY;

		default:
			break;
	}

	stringstream errorMessage;
	errorMessage << "Invalid ODBC SQL parameter data type specified for mapping to generic data type [" << sqlServerType << "]";

	TRACE_GLOBAL( errorMessage.str() );
	throw invalid_argument( errorMessage.str() );
}

#ifdef WIN32
#pragma warning( default : 4661 )
#endif
