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

#include "InformixDatabase.h"
#include "InformixDatabaseProvider.h"
#include "StringUtil.h"

#include "Trace.h"

#ifdef IFX_TRAILING_WHITESPACES
#include <infxcli.h>
#endif

#include <iostream>
#include <exception>
#include <sstream>

#ifdef WIN32
#define __MSXML_LIBRARY_DEFINED__
#include <windows.h>
#define sleep(x) Sleep( (x)*1000 )
#else
#include <unistd.h>
#endif

using namespace std;
using namespace FinTP;

void InformixDatabase::setSpecificEnvAttr()
{
	SQLRETURN cliRC = SQLSetEnvAttr( m_Henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, 0);
	if ( cliRC != SQL_SUCCESS )
	{
		stringstream errorMessage;
		errorMessage << "InformixDatabase::Connect - set SQLSetEnvAttr : " << getErrorInformation( SQL_ATTR_ODBC_VERSION, m_Henv );

		// nonfatal err
		TRACE( errorMessage.str() );
	}
}

//DataSet* InformixDatabase::getDataSet( DataCommand& command, const bool isCommandCached, SQLHANDLE* statementHandle, const bool useCursor, const unsigned int fetchRows )
//{
//	SQLRETURN cliRC;
//	short nResultCols = 0;
//
//	if ( isCommandCached )
//	{
//		nResultCols = command.getResultColumnCount();
//		DEBUG( "Cached no of columns : " << nResultCols );
//	}
//	else
//	{
//		//Get number of result columns
//		DEBUG( "Getting the number of result set columns ..." );
//
//		cliRC = SQLNumResultCols( *statementHandle, &nResultCols );
//		if ( ( cliRC != SQL_SUCCESS ) && ( cliRC != SQL_SUCCESS_WITH_INFO ) )
//		{
//			stringstream errorMessage;
//			errorMessage << "Getting the number of result columns failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";
//			throw runtime_error( errorMessage.str() );
//		}
//		DEBUG( "No of columns : " << nResultCols );
//	}
//
//	//Define InformixRow as map of result columns
//	DataRow InformixRow;
//
//	//#region For each column in the result set, describe result and alloc buffers
//	for ( short i=0; i<nResultCols; i++ )
//	{
//		string columnName = "";
//		DataType::DATA_TYPE columnType = DataType::INVALID_TYPE;
//		unsigned int columnDimension = 0;
//		int columnScale = 0;
//
//		if ( isCommandCached )
//		{
//			columnName = command.getResultColumn( i ).getName();
//			columnType = command.getResultColumn( i ).getType();
//			columnDimension = command.getResultColumn( i ).getDimension();
//			columnScale = command.getResultColumn( i ).getScale();
//		}
//		else
//		{
//			SQLCHAR colName[ MAX_ODBC_COLUMN_NAME ];
//			SQLSMALLINT colNameLen;
//			SQLSMALLINT colType;
//			SQLUINTEGER colSize;
//			SQLSMALLINT colScale;
//
//			DEBUG( "Getting column [" << i << "] description ..." );
//
//			// Get each column description
//			cliRC = SQLDescribeCol( *statementHandle, ( SQLSMALLINT )( i + 1 ), colName, MAX_ODBC_COLUMN_NAME,
//				&colNameLen, &colType, &colSize, &colScale,	NULL );
//
//			if ( ( cliRC != SQL_SUCCESS ) && ( cliRC != SQL_SUCCESS_WITH_INFO ) )
//			{
//				stringstream errorMessage;
//				errorMessage << "Get column description failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";
//				throw runtime_error( errorMessage.str() );
//			}
//
//			DEBUG( "Column Name : " << colName << "; Column SQL Type : " << colType <<
//				"; Column Size : " << colSize << "; Column Scale : " << colScale );
//
//			columnName = StringUtil::ToUpper( string( ( char* )colName ) );
//
//			/*if( ( columnName.length() == 0 ) || !( StringUtil::IsAlphaNumeric( columnName ) ) )
//			{
//				columnName = ODBCColumnName::getName( i, "ODBCColumn.dat" );
//				DEBUG( "Setting column name : [" << columnName << "] for column [" << i << "]" );
//			}*/
//
//			columnType = InformixDatabaseFactory::getDataType( colType, colSize );
//			columnDimension = colSize + 1;
//			columnScale = colScale;
//
//			if ( colType == SQL_DECIMAL  )
//				columnDimension = colSize + 2;
//		}
//
//		// Allocate data buffer for every column, fetching will fill this data
//		DataColumnBase* ODBCColumn = NULL;
//		try
//		{
//			ODBCColumn = InformixDatabaseFactory::internalCreateColumn( columnType, columnDimension, columnScale, columnName );
//
//			// add it to cache ( if it is cacheable )
//			if ( !isCommandCached && command.isCacheable() )
//			{
//				DEBUG( "Command is not cached, but cacheable. Adding result column ..." );
//				command.addResultColumn( i, ODBCColumn );
//			}
//
//			//SQLINTEGER buffLen;
//			//Bind each Column to the result Set
//			DEBUG2( "Bind columns..." );
//			cliRC = SQLBindCol( *statementHandle, ( SQLSMALLINT )( i + 1 ), ODBCDatabaseFactory::getODBCDataType( columnType ),
//				ODBCColumn->getStoragePointer(), columnDimension, ( SQLINTEGER* )ODBCColumn->getBufferIndicator() );
//
//			if ( ( cliRC != SQL_SUCCESS ) && ( cliRC != SQL_SUCCESS_WITH_INFO ) )
//			{
//				stringstream errorMessage;
//				errorMessage << "Bind column failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) <<
//					"] for column " << i + 1 << " column name [" << columnName << "]";
//				throw runtime_error( errorMessage.str() );
//			}
//
//			DEBUG2( "Bind column by position successful." );
//
//			// Insert in the InformixRow the current InformixResultColumn pair : columnName, ODBCColumn (column data and description)
//			// InformixRow will be the buffer that will receive data after fetch
//			InformixRow.insert( makeColumn( columnName, ODBCColumn ) );
//		}
//		catch( ... )
//		{
//			if ( ODBCColumn != NULL )
//			{
//				delete ODBCColumn;
//				ODBCColumn = NULL;
//			}
//			throw;
//		}
//	}
//	//#endregion describe columns
//
//  	DEBUG2( "Creating result dataset ..." );
//	DataSet* InformixDataSet = NULL;
//
//	try
//	{
//		InformixDataSet = new DataSet();
//		DEBUG2( "Fetching first row ..." );
//
//		//Fetch First Row
//		cliRC = SQLFetch( *statementHandle );
//		if ( cliRC == SQL_NO_DATA_FOUND )
//		{
//			DEBUG( "No result data " );
//		}
//		else if ( ( cliRC != SQL_SUCCESS ) && ( cliRC != SQL_SUCCESS_WITH_INFO ) )
//		{
//			stringstream errorMessage;
//			errorMessage << "Fetch data failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";
//
//			DBErrorException errorEx( errorMessage.str() );
//			errorEx.addAdditionalInfo( "statement", command.getModifiedStatementString() );
//			errorEx.addAdditionalInfo( "location", "InformixDatabase::ExecuteQuery" );
//
//			TRACE( errorMessage.str() << " in [" << command.getModifiedStatementString() << "]" );
//			throw errorEx;
//		}
//
//		//While more data in the result set
//		while ( cliRC != SQL_NO_DATA_FOUND )
//		{
//			DEBUG2( "Copying row to dataset" );
//			//Copy the InformixRow buffer into a InformixNewRow
//			/*ODBCColumn< string >* tmpCol = dynamic_cast< ODBCColumn< string >* > (InformixRow["bicsursa"]);
//			DEBUG( tmpCol->getString() );*/
//			DataRow* InformixNewRow = new DataRow( InformixRow );
//
//
//  			DEBUG2( "Appending row to dataset" );
//  			InformixDataSet->push_back( InformixNewRow );
//
//  			InformixRow.Clear();
//
//			if ( fetchRows > 0 )
//				break;
//
//			// Fetch next row in the InformixRow buffer
//			DEBUG2( "Fetching another row" );
//			cliRC = SQLFetch( *statementHandle );
//			if ( ( cliRC != SQL_SUCCESS ) && ( cliRC != SQL_NO_DATA_FOUND ) && ( cliRC != SQL_SUCCESS_WITH_INFO ) )
//			{
//				stringstream errorMessage;
//				errorMessage << "Fetch data failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";
//
//				DBErrorException errorEx( errorMessage.str() );
//				errorEx.addAdditionalInfo( "statement", command.getModifiedStatementString() );
//				errorEx.addAdditionalInfo( "location", "InformixDatabase::ExecuteQuery" );
//
//				TRACE( errorMessage.str() << " in [" << command.getModifiedStatementString() << "]" );
//				throw errorEx;
//			}
//		}
//		//if ( InformixDataSet->size() != 0 )
//		//DisplayDataSet( InformixDataSet );
//
//		DEBUG( "Statement executed successfully." );
//	}
//	catch( ... )
//	{
//		if ( InformixDataSet != NULL )
//		{
//			try
//			{
//				delete InformixDataSet;
//				InformixDataSet = NULL;
//			} catch( ... ){}
//		}
//		throw;
//	}
//
//	return InformixDataSet;
//}

void InformixDatabase::BeginTransaction( const bool readonly )
{
	ODBCDatabase::BeginTransaction( readonly );
#ifdef IFX_TRAILING_WHITESPACES
	//attribute extension for informix database
	SQLRETURN cliRC = SQLSetConnectAttr( m_Hdbc, SQL_INFX_ATTR_LEAVE_TRAILING_SPACES, ( SQLPOINTER )SQL_TRUE, SQL_IS_INTEGER );
	if ( cliRC != SQL_SUCCESS )
	{
		stringstream errorMessage;
		errorMessage << "Informix::ConectionAtribute - set  : SQL_INFX_ATTR_LEAVE_TRAILING_SPACES" << getErrorInformation( SQL_HANDLE_DBC, m_Hdbc );

		TRACE( errorMessage.str() )
	}
#endif
}

void InformixDatabase::EndTransaction ( TransactionType::TRANSACTION_TYPE trType, const bool throwOnError )
{
	ODBCDatabase::EndTransaction ( trType, throwOnError );

#ifdef IFX_TRAILING_WHITESPACES
	SQLRETURN cliRC = SQLSetConnectAttr( m_Hdbc, SQL_INFX_ATTR_LEAVE_TRAILING_SPACES, ( SQLPOINTER )SQL_FALSE, SQL_IS_INTEGER );
	if ( cliRC != SQL_SUCCESS )
	{
		stringstream errorMessage;
		errorMessage << "InformixDatabase::EndTransaction - set SQL_INFX_ATTR_LEAVE_TRAILING_SPACES : " << getErrorInformation( SQL_HANDLE_DBC, m_Hdbc );

		TRACE( errorMessage.str() );
	}
#endif
}

bool InformixDatabase::IsConnected()
{
	//SQLRETURN  s = SQLGetConnectAttr( m_Hdbc, SQL_ATTR_CONNECTION_DEAD, NULL, NULL, NULL );
	if ( m_IsConnected )
	{
		DEBUG( "Already connected ..." );
		return true;
	}
	else
	{
		if( m_IsReconecting )
		{
			DEBUG( "Sleeping 10 seconds before trying to reconnect to Informix... " );
			sleep( 10 );
			try
			{
				Connect( m_ConnectionString );
			}
			catch( const std::exception& ex )
			{
				DEBUG( "Failed to reconnect. [" << ex.what() << "]" );
			}
			catch( ... )
			{
				DEBUG( "Failed to reconnect." );
			}
		}
	}
	return m_IsConnected;
}

//-----------------------------------------------------------
//Display Error Information after each Informix CLI - SQL function
//-----------------------------------------------------------
string InformixDatabase::getErrorInformation( SQLSMALLINT htype, SQLHANDLE handle )
{
	SQLINTEGER sqlcode;
	SQLCHAR sqlstate[ SQL_SQLSTATE_SIZE + 1 ];
	SQLCHAR message[ SQL_MAX_MESSAGE_LENGTH + 1 ];
	SQLSMALLINT realLength;
	int i = 1;
	stringstream errorBuffer;


	while ( SQLGetDiagRec( htype, handle, i, sqlstate, &sqlcode, message, SQL_MAX_MESSAGE_LENGTH, &realLength ) == SQL_SUCCESS )
	{
		errorBuffer << "Detailed Informix error information : ";
		errorBuffer << " SQLSTATE: " << sqlstate;
		errorBuffer << " SQLCODE: "  << sqlcode;
		errorBuffer << " SQLMESSAGE: " << message;
		i++;
	}

	TRACE( errorBuffer.str( ) );

	if( sqlcode == -11020 )
	{
		ConnectionLost();
		throw DBConnectionLostException( errorBuffer.str( ) );
	}
	else if( sqlcode == -11017 )
	{
		if( m_IsConnected ) ConnectionLost();
	}

	return errorBuffer.str();
}

string InformixDatabase::callFormating( const string& statementString, const ParametersVector& vectorOfParameters )
{
	// Compose the Stored Procedure CALL statement
	// TODO:  Verify the SP CALL format
	stringstream paramBuffer;
	paramBuffer << "{call " << statementString << "( ";
	unsigned int paramCount = vectorOfParameters.size();
	for ( unsigned int i=0; i<paramCount; i++ )
	{
		paramBuffer << " ? ";
		if ( i < paramCount - 1 )
			paramBuffer << ",";
	}
	paramBuffer << " )}";
	return paramBuffer.str();
}
