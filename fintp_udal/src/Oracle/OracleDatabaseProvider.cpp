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

#include "OracleDatabaseProvider.h"
#include "OracleDatabase.h"
#include "OracleColumn.h"
#include "Trace.h"

#include <iostream>
#include <string>

using namespace std;
using namespace FinTP;

Database* OracleDatabaseFactory::createDatabase()
{
	DEBUG( "CreateFactory");
	return new OracleDatabase();
}

DataColumnBase* OracleDatabaseFactory::createColumn( DataType::DATA_TYPE columnType, unsigned int dimension, int scale, const string& colname )
{
	return internalCreateColumn( columnType, dimension, scale, colname );
}

DataColumnBase* OracleDatabaseFactory::internalCreateColumn( DataType::DATA_TYPE columnType, unsigned int dimension, int scale, const string& colname )
{
	DataColumnBase* returnCol = NULL;
	switch ( columnType )
	{
		case DataType::BINARY :
		case DataType::LARGE_CHAR_TYPE :
		case DataType::CHAR_TYPE :
			returnCol = new OracleColumn< string >( dimension, scale );
			returnCol->setType( columnType );
			break;

		case DataType::SHORTINT_TYPE :
			returnCol = new OracleColumn< short >( dimension, scale );
			break;

		case DataType::LONGINT_TYPE :
			returnCol = new OracleColumn< long >( dimension, scale );
			break;

		case DataType::DATE_TYPE :
		case DataType::TIMESTAMP_TYPE :
			returnCol = new OracleColumn< string >( MAX_ORACLE_DATE_DIMENSION, scale );
			break;

		case DataType::NUMBER_TYPE :
			returnCol = new OracleColumn< string >( MAX_ORACLE_NUMBER_DIMENSION, scale );
			returnCol->setType( DataType::NUMBER_TYPE );
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

DataParameterBase* OracleDatabaseFactory::createParameter( DataType::DATA_TYPE paramType, DataParameterBase::PARAMETER_DIRECTION parameterDirection )
{
	switch ( paramType )
	{
		case DataType::BINARY :
			/*
			//blob parameter implementation related
			{
				DataParameterBase *blobParam = new OracleParameter< blob >( parameterDirection );
				blobParam->setType( paramType );
				return blobParam;
			}
			*/
		case DataType::LARGE_CHAR_TYPE :
		case DataType::DATE_TYPE :
		case DataType::CHAR_TYPE :
		{
			DataParameterBase *charParam = new OracleParameter< string >( parameterDirection );
			charParam->setType( paramType );
			return charParam;
		}
		case DataType::ARRAY :
		{
			DataParameterBase *arrayParam = new OracleArrayParameter;
			arrayParam->setType( paramType );
			return arrayParam;
		}

		case DataType::TIMESTAMP_TYPE :
			return new OracleParameter< string >( parameterDirection );

		case DataType::SHORTINT_TYPE :
			return new OracleParameter< short >( parameterDirection );

		case DataType::LONGINT_TYPE :
			return new OracleParameter< long >( parameterDirection );

		case DataType::NUMBER_TYPE :
			return new OracleParameter< string >( parameterDirection );

		default:
			break;
	}

	stringstream errorMessage;
	errorMessage << "Invalid data type specified for CreateParameter [" << paramType << "] = [" << DataType::ToString( paramType ) << "]";

	TRACE( errorMessage.str() );
	throw invalid_argument( errorMessage.str() );
}

unsigned short int OracleDatabaseFactory::getOracleSqlType( const DataType::DATA_TYPE oratype, const int dimension )
{
	switch ( oratype )
	{
		case DataType::CHAR_TYPE :
			if ( dimension > 4000 )
			{
				DEBUG2( "Type is SQLT LNG" );
				return SQLT_LNG;
			}
			else
			{
				DEBUG2( "Type is SQLT STR" );
				return SQLT_STR;
			}

		case DataType::LONGINT_TYPE :
		case DataType::SHORTINT_TYPE :
			DEBUG2( "Type is SQLT INT" );
			return SQLT_INT;

		case DataType::DATE_TYPE :
			DEBUG2( "Type is SQLT DATE" );
			return SQLT_DATE;

		case DataType::TIMESTAMP_TYPE :
			DEBUG2( "Type is SQLT TIMESTAMP" );
			return SQLT_TIMESTAMP;

		case DataType::NUMBER_TYPE :
			DEBUG2( "Type is SQLT NUM" );
			return SQLT_NUM;

		case DataType::LARGE_CHAR_TYPE :
			DEBUG2( "Type is SQLT CLOB" );
			return SQLT_CLOB;

		case DataType::BINARY :
			DEBUG2( "Type is SQLT BLOB" );
			return SQLT_BLOB;

		case DataType::ARRAY:
			DEBUG2( "Type is SQLT ARRAY" );
			return SQLT_NTY;

		default:
			break;
	}

	stringstream errorMessage;
	errorMessage << "Invalid data type specified for mapping to Oracle SQL type [" << oratype << "] = [" << DataType::ToString( oratype ) << "]";

	TRACE( errorMessage.str() );
	throw invalid_argument( errorMessage.str() );
}

unsigned short int OracleDatabaseFactory::getOracleDataType( const DataType::DATA_TYPE datatype )
{
	switch ( datatype )
	{
		case DataType::NUMBER_TYPE:
		case DataType::CHAR_TYPE :
			DEBUG2( "Type is SQLT STR" );
			return SQLT_STR;

		case DataType::SHORTINT_TYPE:
			DEBUG2( "Type is SQLT INT" );
			return SQLT_INT;

		case DataType::TIMESTAMP_TYPE:
			DEBUG2( "Type is SQLT TIMESTAMP" );
			return SQLT_TIMESTAMP;

		case DataType::LONGINT_TYPE:
			DEBUG2( "Type is SQLT_LNG" );
			return SQLT_LNG;

		case DataType::LARGE_CHAR_TYPE:
			DEBUG2( "Type is SQLT CLOB" );
			return SQLT_CLOB;

		case DataType::BINARY:
			DEBUG2( "Type is SQLT BLOB" );
			return SQLT_BLOB;

		case DataType::DATE_TYPE:
			DEBUG2( "Type is SQLT DATE" );
			return SQLT_DATE;

		default:
			break;
	}

	stringstream errorMessage;
	errorMessage << "Invalid data type specified for mapping to Oracle data type [" << datatype << "] = [" << DataType::ToString( datatype ) << "]";

	TRACE( errorMessage.str() );
	throw invalid_argument( errorMessage.str() );
}

int OracleDatabaseFactory::getOracleParameterDirection( DataParameterBase::PARAMETER_DIRECTION paramDirection )
{
	/*
	switch ( paramDirection )
	{
		case DataParameterBase::PARAM_IN :
			return SQL_PARAM_INPUT;

		case DataParameterBase::PARAM_OUT :
			return SQL_PARAM_OUTPUT;

		case DataParameterBase::PARAM_INOUT :
			return SQL_PARAM_INPUT_OUTPUT;
	}

	throw invalid_argument( "paramDirection" );
	*/
	return 0;
}


DataType::DATA_TYPE OracleDatabaseFactory::getDataType( const short int oratype, const int dimension )
{
	switch ( oratype )
	{
		case SQLT_CHR :
		case SQLT_AFC :
		case SQLT_RDD :
			return DataType::CHAR_TYPE;

		case SQLT_NUM :
			return DataType::NUMBER_TYPE;

		case SQLT_TIMESTAMP :
			return DataType::TIMESTAMP_TYPE;

		case SQLT_DAT :
			return DataType::DATE_TYPE;

			/*case SQL_INTEGER :
				return DataType::LONGINT_TYPE; */
		case SQLT_LNG :
			return DataType::CHAR_TYPE;

		case SQLT_CLOB :
			return DataType::LARGE_CHAR_TYPE;

		case SQLT_BLOB :
			return DataType::BINARY;

		default:
			break;
	}

	throw invalid_argument( "type" );
}
