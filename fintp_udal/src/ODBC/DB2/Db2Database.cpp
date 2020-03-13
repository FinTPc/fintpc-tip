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

#include "Db2Database.h"
#include "Db2DatabaseProvider.h"

#include "StringUtil.h"
#include "Trace.h"

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

void Db2Database::setSpecificConnAttr()
{
#ifdef DB2_ONLY
	DEBUG( "Setting connection attribute: SQL_CONCURRENT_TRANS" );
	SQLRETURN cliRC = SQLSetConnectAttr( m_Hdbc, SQL_ATTR_CONNECTTYPE, ( SQLPOINTER )SQL_CONCURRENT_TRANS, SQL_NTS );
	if ( cliRC != SQL_SUCCESS )
	{
		stringstream errorMessage;
		errorMessage << "Db2Database::Connect - set SQL_CONCURRENT_TRANS : " << getErrorInformation( SQL_HANDLE_DBC, m_Hdbc );

		// nonfatal err
		TRACE( errorMessage.str() );
	}
#endif
}

//void Db2Database::Disconnect()
//{
//	ReleaseCursor( true );
//
//	// Disconnect
//	SQLRETURN cliRC = SQLDisconnect( m_Hdbc );
//	m_HdbcAllocated = false;
//
//	if ( cliRC != SQL_SUCCESS )
//	{
//  		stringstream errorMessage;
//		errorMessage << "Db2Database::Disconnect - disconnect : " << getErrorInformation( SQL_HANDLE_DBC, m_Hdbc );
//
//		// nonfatal err
//		TRACE( errorMessage.str() );
//	}
//
//	// Deallocate connection handle
//	FREE_DB2_HANDLE( m_Hdbc, "connection", SQL_HANDLE_DBC, m_Henv, SQL_HANDLE_ENV );
//}
//
//void Db2Database::BindParams( ParametersVector& vectorOfParameters, SQLHANDLE* statementHandle, const unsigned int startIndex )
//{
//	if ( statementHandle == NULL )
//		statementHandle = &m_Hstmt;
//
//	DEBUG( "Number of parameters : " << vectorOfParameters.size() );
//
//	SQLRETURN cliRC;
//	for ( unsigned int i=0; i<vectorOfParameters.size(); i++ )
//	{
//		DataType::DATA_TYPE paramType = vectorOfParameters[i]->getType();
//
//		DEBUG2( "Param #" << i <<
//		" Direction: " << vectorOfParameters[i]->getDirection() <<
//		"; Dimension: " << vectorOfParameters[i]->getDimension() <<
//		"; Value: " << ( char* )( ( vectorOfParameters[i]->getStoragePointer() ) ) <<
//		"; Type: " << paramType );
//
//		DEBUG2( "Binding parameter " << i + 1 << " ... " );
//		short int sqlType = Db2DatabaseFactory::getDb2SqlType( paramType, vectorOfParameters[ i ]->getDimension() );
//
//		cliRC = SQLBindParameter( *statementHandle, i + 1,
//			Db2DatabaseFactory::getDb2ParameterDirection( vectorOfParameters[i]->getDirection() ),
//			Db2DatabaseFactory::getDb2DataType( paramType ),
//			sqlType, vectorOfParameters[i]->getDimension(),	0,
//			vectorOfParameters[i]->getStoragePointer(),
//			vectorOfParameters[i]->getDimension(), NULL );
//		if ( ( cliRC != SQL_SUCCESS ) && ( cliRC != SQL_SUCCESS_WITH_INFO ) )
//		{
//		  	stringstream errorMessage;
//			errorMessage << "Bind parameter failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";
//			throw runtime_error( errorMessage.str() );
//		}
//	}
//}
//
//DataSet* Db2Database::innerExecuteCommand( const DataCommand& command, ParametersVector vectorOfParameters, const bool onCursor, const unsigned int fetchRows )
//{
//	// Determine Statement Type : Select or Store Procedure
//	string modStatementString = "", statementString = command.getStatementString();
//	DataCommand cachedCommand = command;
//	bool isCommandCached = m_StatementCache.Contains( statementString );
//
//	// paramCount is +1 for queries
//	unsigned int paramCount = vectorOfParameters.size();
//
//	// return comand from cache
//	if ( isCommandCached )
//	{
//		cachedCommand = m_StatementCache[ statementString ];
//		modStatementString = cachedCommand.getModifiedStatementString();
//	}
//	else
//	{
//		stringstream paramBuffer;
//		switch( command.getCommandType() )
//		{
//			case DataCommand::SP :
//
//				// Compose the Stored Procedure CALL statement
//				// TODO:  Verify the SP CALL format
//				paramBuffer << "CALL " << statementString << "( ";
//				for ( unsigned int i=0; i<paramCount; i++ )
//				{
//					paramBuffer << " ? ";
//					if ( i < paramCount - 1 )
//						paramBuffer << ",";
//				}
//				paramBuffer << " )";
//				break;
//
//			case DataCommand::INLINE :
//
//				paramBuffer << statementString;
//				if ( fetchRows != 0 )
//				{
//					paramBuffer << " FETCH FIRST " << fetchRows << " ROWS ONLY";
//				}
//				else if ( !command.isQuery() && onCursor )
//				{
//					paramBuffer << " WHERE CURRENT OF " << m_HoldCursorName;
//				}
//				break;
//
//			default :
//				throw runtime_error( "Command types supported by ExecuteQuery : SP|TEXT" );
//		}
//
//		modStatementString = paramBuffer.str();
//		cachedCommand.setModifiedStatementString( modStatementString );
//	}
//
//	DEBUG( "Executing statement [" << modStatementString << "]" );
//
//	SQLRETURN cliRC;
//	SQLHANDLE *statementHandle = NULL;
//
//	try
//	{
//		if ( onCursor && command.isQuery() )
//		{
//			// Allocate statement handle
//			if ( m_HoldCursorInvoked )
//				throw runtime_error( "Multiple hold cursor invocation not supported" );
//
//			m_HoldCursorInvoked = true;
//
//			ALLOC_DB2_HANDLE( m_HoldCursorHandle, "statement", SQL_HANDLE_STMT, m_Hdbc, SQL_HANDLE_DBC );
//
//			// set it scrollable
//			cliRC = SQLSetStmtAttr( m_HoldCursorHandle, SQL_ATTR_CURSOR_TYPE, ( SQLPOINTER )SQL_CURSOR_KEYSET_DRIVEN, 0 );
//			if ( ( cliRC != SQL_SUCCESS ) && ( cliRC != SQL_SUCCESS_WITH_INFO ) )
//			{
//				stringstream errorMessage;
//				errorMessage << "Set cursor attribute failed [" << getErrorInformation( SQL_HANDLE_STMT, m_HoldCursorHandle ) << "]";
//
//				throw runtime_error( errorMessage.str() );
//			}
//			statementHandle = &m_HoldCursorHandle;
//		}
//		else
//		{
//			// allocate statement handle
//			ALLOC_DB2_HANDLE( m_Hstmt, "statement", SQL_HANDLE_STMT, m_Hdbc, SQL_HANDLE_DBC );
//			statementHandle = &m_Hstmt;
//		}
//
//		DEBUG( "Preparing statement ... " );
//		cliRC = SQLPrepare( *statementHandle, ( SQLCHAR * )modStatementString.data(), SQL_NTS );
//
//		if ( ( cliRC != SQL_SUCCESS ) && ( cliRC != SQL_SUCCESS_WITH_INFO ) )
//	  	{
//	  		stringstream errorMessage;
//			errorMessage << "Prepare statement failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";
//
//			DBErrorException errorEx( errorMessage.str() );
//			errorEx.addAdditionalInfo( "statement", modStatementString );
//			errorEx.addAdditionalInfo( "location", "Db2Database::Prepare" );
//
//			TRACE( errorMessage.str() << " in [" << modStatementString << "]" );
//			throw errorEx;
//	  	}
//
//		// bind a cursor on the first position for queries of SP type
//		if ( vectorOfParameters.size() > 0 )
//			BindParams( vectorOfParameters );
//	}
//	catch( ... )
//	{
//		vectorOfParameters.Dump();
//		if( statementHandle != NULL )
//			FREE_DB2_HANDLE( *statementHandle, "statement", SQL_HANDLE_STMT, m_Hdbc, SQL_HANDLE_DBC );
//		throw;
//	}
//
//	//Get Data
//	DataSet* returnValue = NULL;
//	try
//	{
//		if ( command.isQuery() )
//			returnValue = executeQuery( cachedCommand, isCommandCached, statementHandle, onCursor, fetchRows );
//		else
//			executeNonQuery( cachedCommand, isCommandCached, statementHandle, onCursor );
//
//		// add it to cache if cacheable
//		if( !isCommandCached && command.isCacheable() )
//			m_StatementCache.Add( statementString, cachedCommand );
//	}
//	catch( ... )
//	{
//		vectorOfParameters.Dump();
//
//		DEBUG2( "Freeing statement" );
//		if ( statementHandle != NULL )
//		{
//			cliRC = SQLFreeStmt( *statementHandle, SQL_CLOSE );
//			if ( cliRC != SQL_SUCCESS )
//			{
//				stringstream freeErrorMessage;
//				freeErrorMessage << "Free statement handle failed [" << getErrorInformation( SQL_HANDLE_DBC, m_Hdbc ) << "]";
//
//				//nonfatal err
//				TRACE( freeErrorMessage.str() );
//			}
//			FREE_DB2_HANDLE( *statementHandle, "statement", SQL_HANDLE_STMT, m_Hdbc, SQL_HANDLE_DBC );
//		}
//
//		if ( returnValue != NULL )
//		{
//			try
//			{
//				delete returnValue;
//				returnValue = NULL;
//			} catch( ... ){}
//		}
//		throw;
//	}
//
//	DEBUG( "Statement executed successfully." );
//
//	if ( command.isQuery() && onCursor )
//	{
//		DEBUG( "Not releasing statement handle ( on cursor )" );
//	}
//	else
//	{
//		if ( statementHandle != NULL )
//		{
//			DEBUG2( "Freeing statement" );
//			cliRC = SQLFreeStmt( *statementHandle, SQL_CLOSE );
//			if ( cliRC != SQL_SUCCESS )
//			{
//				stringstream freeErrorMessage;
//				freeErrorMessage << "Free statement handle failed [" << getErrorInformation( SQL_HANDLE_DBC, m_Hdbc ) << "]";
//
//				//nonfatal err
//				TRACE( freeErrorMessage.str() );
//			}
//			FREE_DB2_HANDLE( *statementHandle, "statement", SQL_HANDLE_STMT, m_Hdbc, SQL_HANDLE_DBC );
//		}
//	}
//
//	return returnValue;
//}
//
//void Db2Database::executeNonQuery( DataCommand& command, const bool isCommandCached, SQLHANDLE* statementHandle, const bool holdCursor )
//{
//	SQLRETURN cliRC;
//
//	// Execute Statement
//	DEBUG( "Calling execute on non-query ... " );
//	cliRC = SQLExecute( *statementHandle );
//
//	switch( cliRC )
//	{
//		case SQL_SUCCESS :
//			break;
//
//		case SQL_SUCCESS_WITH_INFO :
//			{
//				// statement executed, but no data was retrieved, or another warning
//				stringstream errorMessage;
//				errorMessage << "Execute statement failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";
//
//				DBWarningException warningEx( errorMessage.str() );
//				warningEx.addAdditionalInfo( "statement", command.getModifiedStatementString() );
//				warningEx.addAdditionalInfo( "location", "Db2Database::ExecuteNonQuery" );
//
//				TRACE( errorMessage.str() << " in [" << command.getModifiedStatementString() << "]" );
//
//				throw warningEx;
//			}
//			break;
//
//		default :
//			{
//	  			stringstream errorMessage;
//				errorMessage << "Execute statement failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";
//
//				DBErrorException errorEx( errorMessage.str() );
//				errorEx.addAdditionalInfo( "statement", command.getModifiedStatementString() );
//				errorEx.addAdditionalInfo( "location", "Db2Database::ExecuteNonQuery" );
//
//				TRACE( errorMessage.str() << " in [" << command.getModifiedStatementString() << "]" );
//
//				throw errorEx;
//		  	}
//			break;
//	}
//}
//
//DataSet* Db2Database::executeQuery( DataCommand& command, const bool isCommandCached, SQLHANDLE* statementHandle, const bool holdCursor, const unsigned int fetchRows )
//{
//	DataSet *db2DataSet = NULL;
//	SQLRETURN cliRC;
//
//	if ( holdCursor )
//	{
//		cliRC = SQLSetCursorName( *statementHandle, ( SQLCHAR * )"CURSNAME", SQL_NTS );
//		if ( cliRC != SQL_SUCCESS )
//		{
//			stringstream errorMessage;
//			errorMessage << "Set cursor name failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";
//			throw runtime_error( errorMessage.str() );
//		}
//	}
//
//	// Execute Statement
//	DEBUG( "Calling execute on query ... " );
//	cliRC = SQLExecute( *statementHandle );
//	switch( cliRC )
//	{
//		case SQL_SUCCESS :
//			break;
//
//		case SQL_SUCCESS_WITH_INFO :
//			{
//				// statement executed, but no data was retrieved, or another warning
//				stringstream errorMessage;
//				errorMessage << "Execute statement warning [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";
//				TRACE( errorMessage.str() << " in [" << command.getModifiedStatementString() << "]" );
//			}
//			break;
//
//		default :
//			{
//	  			stringstream errorMessage;
//				errorMessage << "Execute statement failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";
//
//				DBErrorException errorEx( errorMessage.str() );
//				errorEx.addAdditionalInfo( "statement", command.getModifiedStatementString() );
//				errorEx.addAdditionalInfo( "location", "Db2Database::ExecuteQuery" );
//
//				TRACE( errorMessage.str() << " in [" << command.getModifiedStatementString() << "]" );
//
//				throw errorEx;
//		  	}
//			break;
//	}
//
//	db2DataSet = getDataSet( command, isCommandCached, statementHandle, holdCursor, fetchRows );
//	if ( db2DataSet == NULL )
//		throw runtime_error( "Empty ( NULL ) resultset returned." );
//
//	if( holdCursor )
//	{
//		SQLCHAR cursorName[ 20 ];
//		SQLSMALLINT cursorLen;
//		cliRC = SQLGetCursorName( *statementHandle, cursorName, 20, &cursorLen );
//		if ( cliRC != SQL_SUCCESS )
//	  	{
//			stringstream errorMessage;
//			errorMessage << "Get cursor name failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";
//	  		throw runtime_error( errorMessage.str() );
//	  	}
//		m_HoldCursorName = ( char* )( cursorName );
//		db2DataSet->setHoldReference( m_HoldCursorName );
//	}
//
//	return db2DataSet;
//}
//
//DataSet* Db2Database::getDataSet( DataCommand& command, const bool isCommandCached, SQLHANDLE* statementHandle, const bool useCursor, const unsigned int fetchRows )
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
//	//Define db2Row as map of result columns
//	DataRow db2Row;
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
//			SQLCHAR colName[ MAX_DB2_COLUMN_NAME ];
//			SQLSMALLINT colNameLen;
//			SQLSMALLINT colType;
//			SQLUINTEGER colSize;
//			SQLSMALLINT colScale;
//
//			DEBUG( "Getting column [" << i << "] description ..." );
//
//			// Get each column description
//			cliRC = SQLDescribeCol( *statementHandle, ( SQLSMALLINT )( i + 1 ), colName, MAX_DB2_COLUMN_NAME,
//				&colNameLen, &colType, &colSize, &colScale,	NULL );
//
//			if ( ( cliRC != SQL_SUCCESS ) && ( cliRC != SQL_SUCCESS_WITH_INFO ) )
//			{
//				stringstream errorMessage;
//				errorMessage << "Get column description failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";
//				throw runtime_error( errorMessage.str() );
//			}
//
//			DEBUG2( "Column Name : " << colName << "; Column SQL Type : " << colType <<
//				"; Column Size : " << colSize << "; Column Scale : " << colScale );
//
//			columnName = string( ( char* )colName );
//			columnType = Db2DatabaseFactory::getDataType( colType, colSize );
//			columnDimension = colSize + 1;
//			columnScale = colScale;
//
//#ifdef AS400
//			if ( colType == SQL_NUMERIC  )
//		 		columnDimension = colSize + 2;
//#else
//			if ( colType == SQL_DECIMAL  )
//				columnDimension = colSize + 2;
//#endif
//		}
//
//		// Allocate data buffer for every column, fetching will fill this data
//		DataColumnBase* db2Column = NULL;
//		try
//		{
//			db2Column = Db2DatabaseFactory::internalCreateColumn( columnType, columnDimension, columnScale, columnName );
//
//			// add it to cache ( if it is cacheable )
//			if ( !isCommandCached && command.isCacheable() )
//			{
//				DEBUG( "Command is not cached, but cacheable. Adding result column ..." );
//				command.addResultColumn( i, db2Column );
//			}
//
//			//SQLINTEGER buffLen;
//			//Bind each Column to the result Set
//			DEBUG2( "Bind columns..." );
//			cliRC = SQLBindCol( *statementHandle, ( SQLSMALLINT )( i + 1 ), Db2DatabaseFactory::getDb2DataType( columnType ),
//				db2Column->getStoragePointer(), columnDimension, db2Column->getBufferIndicator() );
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
//			// Insert in the db2Row the current Db2ResultColumn pair : columnName, db2Column (column data and description)
//			// db2Row will be the buffer that will receive data after fetch
//			db2Row.insert( makeColumn( columnName, db2Column ) );
//		}
//		catch( ... )
//		{
//			if ( db2Column != NULL )
//			{
//				delete db2Column;
//				db2Column = NULL;
//			}
//			throw;
//		}
//	}
//	//#endregion describe columns
//
//  	DEBUG2( "Creating result dataset ..." );
//	DataSet* db2DataSet = NULL;
//
//	try
//	{
//		db2DataSet = new DataSet();
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
//			errorEx.addAdditionalInfo( "location", "Db2Database::ExecuteQuery" );
//
//			TRACE( errorMessage.str() << " in [" << command.getModifiedStatementString() << "]" );
//			throw errorEx;
//		}
//
//		//While more data in the result set
//		while ( cliRC != SQL_NO_DATA_FOUND )
//		{
//			DEBUG2( "Copying row to dataset" );
//			//Copy the db2Row buffer into a db2NewRow
//			DataRow* db2NewRow = new DataRow( db2Row );
//
//  			DEBUG2( "Appending row to dataset" );
//  			db2DataSet->push_back( db2NewRow );
//
//  			db2Row.Clear();
//
//			if ( fetchRows > 0 )
//				break;
//
//			// Fetch next row in the db2Row buffer
//			DEBUG2( "Fetching another row" );
//			cliRC = SQLFetch( *statementHandle );
//			if ( ( cliRC != SQL_SUCCESS ) && ( cliRC != SQL_NO_DATA_FOUND ) && ( cliRC != SQL_SUCCESS_WITH_INFO ) )
//			{
//				stringstream errorMessage;
//				errorMessage << "Fetch data failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";
//
//				DBErrorException errorEx( errorMessage.str() );
//				errorEx.addAdditionalInfo( "statement", command.getModifiedStatementString() );
//				errorEx.addAdditionalInfo( "location", "Db2Database::ExecuteQuery" );
//
//				TRACE( errorMessage.str() << " in [" << command.getModifiedStatementString() << "]" );
//				throw errorEx;
//			}
//		}
//		//if ( db2DataSet->size() != 0 )
//		//DisplayDataSet( db2DataSet );
//
//		DEBUG( "Statement executed successfully." );
//	}
//	catch( ... )
//	{
//		if ( db2DataSet != NULL )
//		{
//			try
//			{
//				delete db2DataSet;
//				db2DataSet = NULL;
//			} catch( ... ){}
//		}
//		throw;
//	}
//
//	return db2DataSet;
//}
//
//
////-----------------------------------------------------------
////Display Error Information after each DB2 CLI - SQL function
////-----------------------------------------------------------
//string Db2Database::getErrorInformation( SQLSMALLINT htype, SQLHANDLE handle )
//{
//	SQLINTEGER sqlcode;
//	SQLCHAR sqlstate[ SQL_SQLSTATE_SIZE + 1 ];
//	SQLCHAR message[ SQL_MAX_MESSAGE_LENGTH + 1 ];
//	SQLSMALLINT realLength;
//	int i = 1;
//	stringstream errorBuffer;
//
//
//	while ( SQLGetDiagRec( htype, handle, i, sqlstate, &sqlcode, message, SQL_MAX_MESSAGE_LENGTH, &realLength ) == SQL_SUCCESS )
//	{
//		errorBuffer << "Detailed DB2 error information : ";
//		errorBuffer << " SQLSTATE: " << sqlstate;
//		errorBuffer << " SQLCODE: "  << sqlcode;
//		errorBuffer << " SQLMESSAGE: " << message;
//		i++;
//	}
//
//	TRACE( errorBuffer.str( ) );
//	return errorBuffer.str();
//}
//
////-------------------------------
////Begin Transaction
////-------------------------------
//void Db2Database::BeginTransaction( const bool readonly )
//{
//	if ( m_HdbcAllocated )
//	{
//		DEBUG( "Setting connection attribute " );
//		SQLRETURN  cliRC = SQLSetConnectAttr( m_Hdbc, SQL_ATTR_AUTOCOMMIT, ( SQLPOINTER )SQL_AUTOCOMMIT_OFF, SQL_NTS );
//		if ( cliRC != SQL_SUCCESS )
//		{
//			stringstream errorMessage;
//			errorMessage << "Db2Database::BeginTransaction - set SQL_ATTR_AUTOCOMMIT : " << getErrorInformation( SQL_HANDLE_DBC, m_Hdbc );
//
//			TRACE( errorMessage.str() )
//		}
//	}
//}
//
////-------------------------------
////End Transaction
////-------------------------------
//void Db2Database::EndTransaction ( TransactionType::TRANSACTION_TYPE trType, const bool throwOnError )
//{
//	DEBUG( "Ending transaction ..." );
//
//	int transactionType = getDb2TransactionType( trType );
//
//	SQLRETURN cliRC = SQLEndTran( SQL_HANDLE_DBC, m_Hdbc, transactionType );
//	if ( cliRC != SQL_SUCCESS )
//	{
//		stringstream errorMessage;
//		errorMessage << "Db2Database::EndTransaction - end transaction : " << getErrorInformation( SQL_HANDLE_DBC, m_Hdbc );
//
//		TRACE( errorMessage.str() );
//	}
//
//	cliRC = SQLSetConnectAttr( m_Hdbc, SQL_ATTR_AUTOCOMMIT, ( SQLPOINTER )SQL_AUTOCOMMIT_ON, SQL_NTS );
//	if ( cliRC != SQL_SUCCESS )
//	{
//		stringstream errorMessage;
//		errorMessage << "Db2Database::EndTransaction - set SQL_ATTR_AUTOCOMMIT : " << getErrorInformation( SQL_HANDLE_DBC, m_Hdbc );
//
//		TRACE( errorMessage.str() );
//	}
//}
//
//int Db2Database::getDb2TransactionType( TransactionType::TRANSACTION_TYPE type )
//{
//	switch( type )
//	{
//		case TransactionType::COMMIT :
//			return SQL_COMMIT;
//		case TransactionType::ROLLBACK :
//			return SQL_ROLLBACK;
//	}
//	throw invalid_argument( "type" );
//}
