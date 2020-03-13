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

#include "Db2DatabaseProvider.h"
#include "Db2Database.h"
#include "Trace.h"

#include <iostream>
#include <string>
#include <sstream>

using namespace std;
using namespace FinTP;

#ifdef WIN32
/*	template class DataParameter< string >;
	template class DataParameter< short >;
	template class DataParameter< long >;
	template class DataParameter< int >;
	template class DataParameter< DbTimestamp >;*/
#endif

Database* Db2DatabaseFactory::createDatabase()
{
	DEBUG( "Creating new DB2 database definition..." );
	Database* retDatabase = NULL;
	try
	{
		retDatabase = new Db2Database();
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

short int Db2DatabaseFactory::getDb2SqlType( const DataType::DATA_TYPE db2type, const int dimension )
{
	switch ( db2type )
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
#ifdef DB2_ONLY
		case DataType::LARGE_CHAR_TYPE:
			DEBUG_GLOBAL2( "Type is SQL CLOB" );
			return SQL_CLOB;


		case DataType::BINARY:
			DEBUG_GLOBAL2( "Type is SQL LOB" );
			return SQL_BLOB;
#endif
		default:
			break;
	}

	stringstream errorMessage;
	errorMessage << "Invalid data type specified for mapping to DB2 SQL type [" << db2type << "] = [" << DataType::ToString( db2type ) << "]";

	TRACE( errorMessage.str() );
	throw invalid_argument( errorMessage.str() );
}

short int Db2DatabaseFactory::getDb2DataType( const DataType::DATA_TYPE db2type )
{
	switch ( db2type )
	{
		case DataType::CHAR_TYPE :
			DEBUG_GLOBAL2( "SQL DB2 Type is SQL C CHAR" );
			return SQL_C_CHAR;

		case DataType::DATE_TYPE :
			DEBUG_GLOBAL2( "SQL DB2 Type is SQL C DATE" );
			return SQL_C_DATE;

		case DataType::TIMESTAMP_TYPE :
			DEBUG_GLOBAL2( "SQL DB2 Type is SQL C TIMESTAMP" );
			return SQL_C_TIMESTAMP;

		case DataType::SHORTINT_TYPE :
			DEBUG_GLOBAL2( "SQL DB2 Type is SQL C SHORT" );
			return SQL_C_SHORT;

		case DataType::LONGINT_TYPE :
			DEBUG_GLOBAL2( "SQL DB2 Type is SQL C LONG" );
			return SQL_C_LONG;

		case DataType::LARGE_CHAR_TYPE :
			DEBUG_GLOBAL2( "SQL DB2 Type is SQL C (CHAR)BINARY" );
			return SQL_C_BINARY;

		default:
			break;
	}

	stringstream errorMessage;
	errorMessage << "Invalid data type specified for mapping to DB2 type [" << db2type << "] = [" << DataType::ToString( db2type ) << "]";

	TRACE_GLOBAL( errorMessage.str() );
	throw invalid_argument( errorMessage.str() );
}

DataType::DATA_TYPE Db2DatabaseFactory::getDataType( const short int db2type, const int dimension )
{
	switch ( db2type )
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

#ifdef AS400
		case SQL_NUMERIC :
			return DataType::CHAR_TYPE;
#else //AS400
		case SQL_DECIMAL :
			return DataType::CHAR_TYPE;
#endif //AS400
#ifdef DB2_ONLY
		case SQL_CLOB :
			return DataType::LARGE_CHAR_TYPE;
#endif
		default:
			break;
	}

	stringstream errorMessage;
	errorMessage << "Invalid DB2 SQL parameter data type specified for mapping to generic data type [" << db2type << "]";

	TRACE_GLOBAL( errorMessage.str() );
	throw invalid_argument( errorMessage.str() );
}

#ifdef WIN32
#pragma warning( default : 4661 )
#endif
