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

#include "InformixDatabaseProvider.h"
#include "InformixDatabase.h"

using namespace std;
using namespace FinTP;

Database* InformixDatabaseFactory::createDatabase()
{
	DEBUG( "Creating new ODBC database definition..." );
	Database* retDatabase = NULL;
	try
	{
		retDatabase = new InformixDatabase();
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

short int InformixDatabaseFactory::getInformixSqlType( const DataType::DATA_TYPE informixType, const int dimension )
{
	switch ( informixType )
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
			return SQL_TIMESTAMP;

		case DataType::NUMBER_TYPE:
#ifdef AS400
			DEBUG_GLOBAL2( "Type is SQL NUMERIC" );
			return SQL_NUMERIC;
#else
			DEBUG_GLOBAL2( "Type is SQL DECIMAL" );
			return SQL_DECIMAL;
#endif

		case DataType::LONGINT_TYPE:
			DEBUG_GLOBAL2( "Type is SQL INTEGER" );
			return SQL_INTEGER;

			//case DataType::LARGE_CHAR_TYPE:
			//	DEBUG_GLOBAL2( "Type is SQL CLOB" );
			//	return SQL_CLOB;

		default:
			break;
	}

	stringstream errorMessage;
	errorMessage << "Invalid data type specified for mapping to Informix SQL type [" << informixType << "] = [" << DataType::ToString( informixType ) << "]";

	TRACE( errorMessage.str() );
	throw invalid_argument( errorMessage.str() );
}

short int InformixDatabaseFactory::getInformixDataType( const DataType::DATA_TYPE ifxtype )
{
	switch ( ifxtype )
	{
		case DataType::CHAR_TYPE :
			DEBUG_GLOBAL2( "SQL Informix Type is SQL C CHAR" );
			return SQL_C_CHAR;

		case DataType::DATE_TYPE :
			DEBUG_GLOBAL2( "SQL Informix Type is SQL C DATE" );
			return SQL_C_DATE;

		case DataType::TIMESTAMP_TYPE :
			DEBUG_GLOBAL2( "SQL Informix Type is SQL C TIMESTAMP" );
			return SQL_C_TIMESTAMP;

		case DataType::SHORTINT_TYPE :
			DEBUG_GLOBAL2( "SQL Informix Type is SQL C SHORT" );
			return SQL_C_SHORT;

		case DataType::LONGINT_TYPE :
			DEBUG_GLOBAL2( "SQL Informix Type is SQL C LONG" );
			return SQL_C_LONG;

		case DataType::LARGE_CHAR_TYPE :
			DEBUG_GLOBAL2( "SQL Informix Type is SQL C BINARY" );
			return SQL_C_BINARY;

		default:
			break;
	}

	stringstream errorMessage;
	errorMessage << "Invalid data type specified for mapping to ODBC type [" << ifxtype << "] = [" << DataType::ToString( ifxtype ) << "]";

	TRACE_GLOBAL( errorMessage.str() );
	throw invalid_argument( errorMessage.str() );
}

DataType::DATA_TYPE InformixDatabaseFactory::getDataType( const short int ifxType, const int dimension )
{
	switch ( ifxType )
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

			//case SQL_CLOB :
			//	return DataType::LARGE_CHAR_TYPE;

		default:
			break;
	}

	stringstream errorMessage;
	errorMessage << "Invalid Informix SQL parameter data type specified for mapping to generic data type [" << ifxType << "]";

	TRACE_GLOBAL( errorMessage.str() );
	throw invalid_argument( errorMessage.str() );
}

