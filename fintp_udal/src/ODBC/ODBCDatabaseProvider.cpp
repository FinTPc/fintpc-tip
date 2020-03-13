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

#include "ODBCDatabaseProvider.h"
#include "ODBCDatabase.h"
#include "Trace.h"

#include <iostream>
#include <string>
#include <sstream>

#include <sqlext.h>
#include <sqlucode.h>

#if defined SQLSERVER_ONLY
#include "SqlServer/SqlServerDatabaseProvider.h"
#elif defined DB2_ONLY
#include "DB2/DB2DatabaseProvider.h"
#elif defined INFORMIX_ONLY
#include "Informix/InformixDatabaseProvider.h"
#endif

using namespace std;
using namespace FinTP;

DataColumnBase* ODBCDatabaseFactory::createColumn( DataType::DATA_TYPE columnType, unsigned int dimension, int scale, const string& colname )
{
	return internalCreateColumn( columnType, dimension, scale, colname );
}

DataColumnBase* ODBCDatabaseFactory::internalCreateColumn( DataType::DATA_TYPE columnType, unsigned int dimension, int scale, const string& colname )
{
	DataColumnBase* returnCol = NULL;
	switch ( columnType )
	{
		case DataType::SHORTINT_TYPE :
			returnCol = new ODBCColumn< short >( dimension, scale );
			break;

		case DataType::LONGINT_TYPE :
			returnCol = new ODBCColumn< long >( dimension, scale );
			break;

		case DataType::CHAR_TYPE :
		case DataType::DATE_TYPE :
		case DataType::TIMESTAMP_TYPE :
		case DataType::LARGE_CHAR_TYPE :
		case DataType::BINARY :
			returnCol = new ODBCColumn< string >( dimension, scale );
			break;

		default:
			break;
	}

	if( returnCol != NULL )
	{
		returnCol->setName( colname );
		return returnCol;
	}

	stringstream errorMessage;
	errorMessage << "Invalid data type specified for CreateColumn [" << columnType << "] = [" << DataType::ToString( columnType ) << "]";

	TRACE( errorMessage.str() );
	throw invalid_argument( errorMessage.str() );
}

DataParameterBase* ODBCDatabaseFactory::createParameter( DataType::DATA_TYPE paramType, DataParameterBase::PARAMETER_DIRECTION parameterDirection )
{
	switch ( paramType )
	{
		case DataType::DATE_TYPE :
		case DataType::CHAR_TYPE :
			return new ODBCParameter< string >( parameterDirection );

		case DataType::TIMESTAMP_TYPE:
			return new ODBCParameter< DbTimestamp >( parameterDirection );

		case DataType::SHORTINT_TYPE:
			return new ODBCParameter< short >( parameterDirection );

		case DataType::LONGINT_TYPE:
			return new ODBCParameter< long >( parameterDirection );

		case DataType::BINARY :
		case DataType::LARGE_CHAR_TYPE:
		{
			DataParameterBase *charParam = new ODBCParameter< string >( parameterDirection );
			charParam->setType( paramType );
			return charParam;
		}

		case DataType::ARRAY:
		{
			DataParameterBase *arrayParam = new ODBCParameter< vector<string> >( parameterDirection );
			arrayParam->setType( paramType );
			return arrayParam;
		}
		/*case DataType::DOUBLE_TYPE:
			return new ODBCParameter< double >( parameterDirection );*/

		default:
			break;
	}

	stringstream errorMessage;
	errorMessage << "Invalid data type specified for CreateParameter [" << paramType << "] = [" << DataType::ToString( paramType ) << "]";

	TRACE( errorMessage.str() );
	throw invalid_argument( errorMessage.str() );
}

short int ODBCDatabaseFactory::getODBCSqlType( const DataType::DATA_TYPE ODBCtype, const int dimension )
{
#if defined SQLSERVER_ONLY
	return SqlServerDatabaseFactory::getSqlServerSqlType( ODBCtype, dimension );
#elif defined DB2_ONLY
	return Db2DatabaseFactory::getDb2SqlType( ODBCtype, dimension );
#elif defined INFORMIX_ONLY
	return InformixDatabaseFactory::getInformixSqlType( ODBCtype, dimension );
#else

	switch ( ODBCtype )
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

		case DataType::LARGE_CHAR_TYPE:
			DEBUG_GLOBAL2( "Type is SQL LONGVARCHAR" );
			return SQL_LONGVARCHAR;

		case DataType::BINARY:
			DEBUG_GLOBAL2( "Type is SQL BINARY" );
			return SQL_BINARY;

		default:
			break;
	}

	stringstream errorMessage;
	errorMessage << "Invalid data type specified for mapping to ODBC SQL type [" << ODBCtype << "] = [" << DataType::ToString( ODBCtype ) << "]";

	TRACE( errorMessage.str() );
	throw invalid_argument( errorMessage.str() );
#endif
}

short int ODBCDatabaseFactory::getODBCDataType( const DataType::DATA_TYPE ODBCtype )
{
#if defined SQLSERVER_ONLY
	return SqlServerDatabaseFactory::getSqlServerDataType( ODBCtype );
#elif defined DB2_ONLY
	return Db2DatabaseFactory::getDb2DataType( ODBCtype );
#elif defined INFORMIX_ONLY
	return InformixDatabaseFactory::getInformixDataType( ODBCtype );
#else

	switch ( ODBCtype )
	{
		case DataType::CHAR_TYPE :
			DEBUG_GLOBAL2( "SQL ODBC Type is SQL C CHAR" );
			return SQL_C_CHAR;

		case DataType::DATE_TYPE :
			DEBUG_GLOBAL2( "SQL ODBC Type is SQL C DATE" );
			return SQL_C_DATE;

		case DataType::TIMESTAMP_TYPE :
			DEBUG_GLOBAL2( "SQL ODBC Type is SQL C TIMESTAMP" );
			return SQL_C_TIMESTAMP;

		case DataType::SHORTINT_TYPE :
			DEBUG_GLOBAL2( "SQL ODBC Type is SQL C SHORT" );
			return SQL_C_SHORT;

		case DataType::LONGINT_TYPE :
			DEBUG_GLOBAL2( "SQL ODBC Type is SQL C LONG" );
			return SQL_C_LONG;

		case DataType::BINARY :
		case DataType::LARGE_CHAR_TYPE :
			DEBUG_GLOBAL2( "SQL ODBC Type is SQL C BINARY" );
			return SQL_C_BINARY;

		default:
			break;
	}

	stringstream errorMessage;
	errorMessage << "Invalid data type specified for mapping to ODBC type [" << ODBCtype << "] = [" << DataType::ToString( ODBCtype ) << "]";

	TRACE_GLOBAL( errorMessage.str() );
	throw invalid_argument( errorMessage.str() );
#endif
}

int ODBCDatabaseFactory::getODBCParameterDirection( DataParameterBase::PARAMETER_DIRECTION paramDirection )
{
	switch ( paramDirection )
	{
		case DataParameterBase::PARAM_IN :
			return SQL_PARAM_INPUT;

		case DataParameterBase::PARAM_OUT :
			return SQL_PARAM_OUTPUT;

		case DataParameterBase::PARAM_INOUT :
			return SQL_PARAM_INPUT_OUTPUT;
	}

	stringstream errorMessage;
	errorMessage << "Invalid parameter direction specified for mapping to ODBC parameter direction [" << paramDirection << "]";

	TRACE_GLOBAL( errorMessage.str() );
	throw invalid_argument( errorMessage.str() );
}

DataType::DATA_TYPE ODBCDatabaseFactory::getDataType( const short int ODBCtype, const int dimension )
{
#if defined SQLSERVER_ONLY
	return SqlServerDatabaseFactory::getDataType( ODBCtype, dimension );
#elif defined DB2_ONLY
	return Db2DatabaseFactory::getDataType( ODBCtype, dimension );
#elif defined INFORMIX_ONLY
	return InformixDatabaseFactory::getDataType( ODBCtype, dimension );
#else

	switch ( ODBCtype )
	{
		case SQL_VARCHAR :
		case SQL_CHAR :
		case SQL_LONGVARCHAR :
		case SQL_WCHAR :
		case SQL_WVARCHAR :
		case SQL_WLONGVARCHAR :
			return DataType::CHAR_TYPE;

		case SQL_SMALLINT :
		case SQL_TINYINT :
			return DataType::SHORTINT_TYPE;

		case SQL_INTEGER :
		case SQL_BIGINT :
			return DataType::LONGINT_TYPE;

		case SQL_FLOAT :
		case SQL_REAL :
		case SQL_DOUBLE :
			return DataType::CHAR_TYPE;

		case SQL_NUMERIC :
		case SQL_DECIMAL :
			return DataType::CHAR_TYPE;

		case SQL_TIME :
		case SQL_TIMESTAMP :
		case SQL_TYPE_DATE :
		case SQL_TYPE_TIMESTAMP :
			return DataType::CHAR_TYPE;

		case SQL_BINARY:
		case SQL_VARBINARY :
		case SQL_LONGVARBINARY :
			return DataType::BINARY;

		default:
			break;
	}

	stringstream errorMessage;
	errorMessage << "Invalid ODBC SQL parameter data type specified for mapping to generic data type [" << ODBCtype << "]";

	TRACE_GLOBAL( errorMessage.str() );
	throw invalid_argument( errorMessage.str() );
#endif
}

