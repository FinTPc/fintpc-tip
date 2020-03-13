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

#include "ODBCDatabase.h"
#include "ODBCDatabaseProvider.h"
#include "StringUtil.h"
#include "Trace.h"
#include "Base64.h"

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

// use it like ALLOC_ODBC_HANDLE( tempHandle, "statement", SQL_HANDLE_STMT, m_Hdbc, SQL_HANDLE_DBC )
#define ALLOC_ODBC_HANDLE( odbc_handle, odbc_handle_name, odbc_handle_type, odbc_input_handle, odbc_input_handle_type ) \
{\
	DEBUG2( "Allocating " << odbc_handle_name << " handle ..." );\
	SQLRETURN odbc_alloc_status = SQLAllocHandle( ( odbc_handle_type ), ( odbc_input_handle ), &( odbc_handle ) );\
	if ( odbc_alloc_status != SQL_SUCCESS )\
	{\
		stringstream odbc_alloc_message; \
		odbc_alloc_message << "Alloc [" << odbc_handle_name << "] handle failed [" << getErrorInformation( ( odbc_input_handle_type ), ( odbc_input_handle ) ) << "]";\
		TRACE( odbc_alloc_message.str() );\
		throw runtime_error( odbc_alloc_message.str() );\
	}\
}

// use it like FREE_ODBC_HANDLE( m_HoldCursorHandle, "cursor", SQL_HANDLE_STMT, m_Hdbc, SQL_HANDLE_DBC )
#define FREE_ODBC_HANDLE( odbc_handle, odbc_handle_name, odbc_handle_type, odbc_input_handle, odbc_input_handle_type ) \
{\
	DEBUG2( "Freeing " << odbc_handle_name << " handle ..." );\
	SQLRETURN odbc_free_status = SQLFreeHandle( ( odbc_handle_type ), ( odbc_handle ) );\
	if ( odbc_free_status != SQL_SUCCESS )\
	{\
		stringstream odbc_free_message; \
		odbc_free_message << "Free [" << odbc_handle_name << "] handle failed [" << getErrorInformation( ( odbc_input_handle_type ), ( odbc_input_handle ) ) << "]";\
		TRACE( odbc_free_message.str() );\
	}\
}

ODBCDatabase::ODBCDatabase() : Database(), m_HenvAllocated( false ), m_HdbcAllocated( false ), m_HoldCursorName( "bad_cursor" ), m_HoldCursorInvoked( false ), m_IsConnected( false )
{
	// Alocate environment handle
	DEBUG2( "CONSTRUCTOR " );

	SQLRETURN cliRC = AllocateEnv();
	if ( cliRC != SQL_SUCCESS )
	{
		// may be able to continue and connect later
		TRACE( "Alloc environment error [" << getErrorInformation( SQL_HANDLE_ENV, SQL_NULL_HANDLE ) << "]" );
	}
}

ODBCDatabase::~ODBCDatabase( )
{
	// Free environment handle
	try
	{
		SQLRETURN cliRC = SQLFreeHandle( SQL_HANDLE_ENV, m_Henv );
		if ( cliRC != SQL_SUCCESS )
			getErrorInformation( SQL_HANDLE_ENV, m_Henv );
	}
	catch( ... )
	{
		try
		{
			TRACE( "Error occured during database termination" );
		} catch( ... ) {}
	}
}

//----------------------------
// Allocate environment
//-----------------------------
SQLRETURN ODBCDatabase::AllocateEnv()
{
	SQLRETURN cliRC = SQLAllocHandle( SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_Henv );
	m_HenvAllocated = ( cliRC == SQL_SUCCESS );
	return cliRC;
}

void ODBCDatabase::Connect( const ConnectionString& connectionString )
{
	DEBUG( "Thread [" << pthread_self() << "] start to connect [" << connectionString.getDatabaseName() << "] ... " );
	// if environment not allocated then allocate environment
	if ( !isEnvAllocated() )
	{
		SQLRETURN cliRC = AllocateEnv();
		if ( cliRC != SQL_SUCCESS )
		{
			stringstream errorMessage;
			errorMessage << "Alloc environment failed [" << getErrorInformation( SQL_HANDLE_ENV, m_Henv ) << "]";
			throw runtime_error( errorMessage.str() );
		}
	}

	setSpecificEnvAttr();

	// Allocate  connection handle
	m_HdbcAllocated = false;
	ALLOC_ODBC_HANDLE( m_Hdbc, "connection", SQL_HANDLE_DBC, m_Henv, SQL_HANDLE_ENV );
	m_HdbcAllocated = true;

	setSpecificConnAttr();

	// Connect
	short numberOfTries = 0;
	DEBUG( "About to connect ... " );
	SQLRETURN cliRC = SQLConnect( m_Hdbc,
	                              ( SQLCHAR * )connectionString.getDatabaseName().data(), SQL_NTS,
	                              ( SQLCHAR * )connectionString.getUserName().data(), SQL_NTS,
	                              ( SQLCHAR * )connectionString.getUserPassword().data(), SQL_NTS );

	switch ( cliRC )
	{
		case SQL_SUCCESS_WITH_INFO :
			DEBUG( "ODBCDatabase::Connect - connect : " << getErrorInformation( SQL_HANDLE_DBC, m_Hdbc ) );
			break;

		case SQL_SUCCESS:
			break;

		default:

			while ( ( numberOfTries < 3 ) && ( ( cliRC != SQL_SUCCESS ) && ( cliRC != SQL_SUCCESS_WITH_INFO ) ) )
			{
				stringstream errorMessage;
				errorMessage << "ODBCDatabase::Connect - connect " << getErrorInformation( SQL_HANDLE_DBC, m_Hdbc );

				TRACE( "Connection failed [" << errorMessage.str() << "]. Wait 30 seconds and will try again" );;
				//Wait and try to connect again
				sleep( 30 );
				cliRC = SQLConnect( m_Hdbc,
				                    (SQLCHAR *)connectionString.getDatabaseName().data(), SQL_NTS,
				                    (SQLCHAR *)connectionString.getUserName().data(), SQL_NTS,
				                    (SQLCHAR *)connectionString.getUserPassword().data(), SQL_NTS );

				numberOfTries++;
			}

			if ( ( numberOfTries == 3 ) && ( ( cliRC != SQL_SUCCESS ) && ( cliRC != SQL_SUCCESS_WITH_INFO ) ) )
			{
				// Deallocate connection handle
				cliRC = SQLFreeHandle( SQL_HANDLE_DBC, m_Hdbc );
				stringstream errorMessage;
				if ( cliRC != SQL_SUCCESS )
				{
					errorMessage << "ODBCDatabase::Disconnect - free connection handle : " << getErrorInformation( SQL_HANDLE_ENV, m_Henv );

					// nonfatal err
					TRACE( errorMessage.str() );
				}
				throw  DBErrorException( errorMessage.str() );
			}
			break;
	}
	m_IsConnected = true;
	DEBUG( "Connected to database server." );
}

void ODBCDatabase::ReleaseCursor( const bool checkConn)
{
	if ( !m_HoldCursorInvoked )
		return;

	//Free cursor handle
	FREE_ODBC_HANDLE( m_HoldCursorHandle, "cursor", SQL_HANDLE_STMT, m_Hdbc, SQL_HANDLE_DBC );
	m_HoldCursorInvoked = false;
}

void ODBCDatabase::RewindCursor()
{
	if ( !m_HoldCursorInvoked )
		return;

	//If error free statement handle
	SQLRETURN cliRC = SQLSetPos( m_HoldCursorHandle, 1, SQL_POSITION, SQL_LOCK_NO_CHANGE );
	if ( cliRC != SQL_SUCCESS )
	{
		stringstream errorMessage;
		errorMessage << "ODBCDatabase::RewindCursor - set cursor position : " << getErrorInformation( SQL_HANDLE_STMT, m_HoldCursorHandle );

		throw runtime_error( errorMessage.str() );
	}

	DEBUG( "Cursor rewind to first row" );
}

void ODBCDatabase::Disconnect()
{
	ReleaseCursor( true );

	// Disconnect
	SQLRETURN cliRC = SQLDisconnect( m_Hdbc );
	m_HdbcAllocated = false;

	if ( cliRC != SQL_SUCCESS )
	{
		stringstream errorMessage;
		errorMessage << "ODBCDatabase::Disconnect - disconnect : " << getErrorInformation( SQL_HANDLE_DBC, m_Hdbc );

		// nonfatal err
		TRACE( errorMessage.str() );
	}

	// Deallocate connection handle
	FREE_ODBC_HANDLE( m_Hdbc, "connection", SQL_HANDLE_DBC, m_Henv, SQL_HANDLE_ENV );
}

void ODBCDatabase::BindParams( const ParametersVector& vectorOfParameters, SQLHANDLE* statementHandle, const unsigned int startIndex )
{
	if ( statementHandle == NULL )
		statementHandle = &m_Hstmt;

	DEBUG( "Number of parameters : " << vectorOfParameters.size() );

	SQLRETURN cliRC;
	int boundIndex = 0;
	for ( unsigned int i=0; i<vectorOfParameters.size(); i++ )
	{
		DataType::DATA_TYPE paramType = vectorOfParameters[i]->getType();

		DEBUG2( "Param #" << i <<
		        " Direction: " << vectorOfParameters[i]->getDirection() <<
		        "; Dimension: " << vectorOfParameters[i]->getDimension() <<
		        "; Value: " << ( char* )( ( vectorOfParameters[i]->getStoragePointer() ) ) <<
		        "; Type: " << paramType );

		DEBUG2( "Binding parameter " << i + 1 << " ... " );

		if ( vectorOfParameters[i]->getType() != DataType::ARRAY )
		{
			short int sqlType = ODBCDatabaseFactory::getODBCSqlType( paramType, vectorOfParameters[ i ]->getDimension() );

			SQLLEN* StrLen_or_IndPtr = reinterpret_cast<SQLLEN*>(vectorOfParameters[i]->getIndicatorValue());

			cliRC = SQLBindParameter( *statementHandle, ++boundIndex,
									  ODBCDatabaseFactory::getODBCParameterDirection( vectorOfParameters[i]->getDirection() ),
									  ODBCDatabaseFactory::getODBCDataType( paramType ),
									  sqlType, vectorOfParameters[i]->getDimension(),	0,
									  vectorOfParameters[i]->getStoragePointer(),
									  vectorOfParameters[i]->getDimension(), StrLen_or_IndPtr );
			if ( ( cliRC != SQL_SUCCESS ) && ( cliRC != SQL_SUCCESS_WITH_INFO ) )
			{
				stringstream errorMessage;
				errorMessage << "Bind parameter failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";
				throw runtime_error( errorMessage.str() );
			}
		}
		else
			for ( unsigned int j=0; j<vectorOfParameters[i]->getDimension(); j++ )
			{
				const string& arrayElement = vectorOfParameters[i]->getElement(j);
				cliRC = SQLBindParameter( *statementHandle, ++boundIndex,
							ODBCDatabaseFactory::getODBCParameterDirection( vectorOfParameters[i]->getDirection() ),
							SQL_C_CHAR,
							SQL_CHAR, arrayElement.size(),	0,
							(SQLPOINTER)arrayElement.c_str(),
							arrayElement.size(), NULL );
				if ( ( cliRC != SQL_SUCCESS ) && ( cliRC != SQL_SUCCESS_WITH_INFO ) )
				{
					stringstream errorMessage;
					errorMessage << "Bind parameter failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";
					throw runtime_error( errorMessage.str() );
				}
			}
	}
}

DataSet* ODBCDatabase::innerExecuteCommand( const DataCommand& command, const ParametersVector& vectorOfParameters, const bool onCursor, const unsigned int fetchRows )
{
	// Determine Statement Type : Select or Store Procedure
	string modStatementString = "", statementString = command.getStatementString();
	DataCommand cachedCommand = command;
	bool isCommandCached = m_StatementCache.Contains( statementString );

	// paramCount is +1 for queries
	unsigned int paramCount = vectorOfParameters.size();

	// return comand from cache
	if ( isCommandCached )
	{
		cachedCommand = m_StatementCache[ statementString ];
		modStatementString = cachedCommand.getModifiedStatementString();
	}
	else
	{
		stringstream paramBuffer;
		switch( command.getCommandType() )
		{
			case DataCommand::SP :

				// Compose the Stored Procedure CALL statement
				// TODO:  Verify the SP CALL format
				paramBuffer << callFormating( statementString, vectorOfParameters );
				break;

			case DataCommand::INLINE :

				paramBuffer << statementString;
				if ( fetchRows != 0 )
				{
					paramBuffer << " FETCH FIRST " << fetchRows << " ROWS ONLY";
				}
				else if ( !command.isQuery() && onCursor )
				{
					paramBuffer << " WHERE CURRENT OF " << m_HoldCursorName;
				}
				break;

			default :
				throw runtime_error( "Command types supported by ExecuteQuery : SP|TEXT" );
		}

		modStatementString = paramBuffer.str();
		cachedCommand.setModifiedStatementString( modStatementString );
	}

	DEBUG( "Executing statement [" << modStatementString << "]" );

	SQLRETURN cliRC;
	SQLHANDLE *statementHandle = NULL;

	try
	{
		if ( onCursor && command.isQuery() )
		{
			// Allocate statement handle
			if ( m_HoldCursorInvoked )
				throw runtime_error( "Multiple hold cursor invocation not supported" );

			m_HoldCursorInvoked = true;

			ALLOC_ODBC_HANDLE( m_HoldCursorHandle, "statement", SQL_HANDLE_STMT, m_Hdbc, SQL_HANDLE_DBC );

			// set it scrollable
			cliRC = SQLSetStmtAttr( m_HoldCursorHandle, SQL_ATTR_CURSOR_TYPE, ( SQLPOINTER )SQL_CURSOR_KEYSET_DRIVEN, 0 );
			if ( ( cliRC != SQL_SUCCESS ) && ( cliRC != SQL_SUCCESS_WITH_INFO ) )
			{
				stringstream errorMessage;
				errorMessage << "Set cursor attribute failed [" << getErrorInformation( SQL_HANDLE_STMT, m_HoldCursorHandle ) << "]";

				throw runtime_error( errorMessage.str() );
			}
			statementHandle = &m_HoldCursorHandle;
		}
		else
		{
			// allocate statement handle
			ALLOC_ODBC_HANDLE( m_Hstmt, "statement", SQL_HANDLE_STMT, m_Hdbc, SQL_HANDLE_DBC );
			statementHandle = &m_Hstmt;
		}

#ifdef SQLSERVER_ONLY
		if( command.isQuery() && ( command.getCommandType() == DataCommand::SP ) )
		{
			cliRC = SQLSetCursorName( *statementHandle, ( SQLCHAR* )"retCursor", SQL_NTS );
			if ( ( cliRC != SQL_SUCCESS ) && ( cliRC != SQL_SUCCESS_WITH_INFO ) )
			{
				stringstream errorMessage;
				errorMessage << "SetCursorName with name=[retCursor] failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";

				DBErrorException errorEx( errorMessage.str() );
				errorEx.addAdditionalInfo( "statement", modStatementString );
				errorEx.addAdditionalInfo( "location", "ODBCDatabase::Prepare" );

				TRACE( errorMessage.str() << " in [" << modStatementString << "]" );
				throw errorEx;
			}
		}
#endif

		DEBUG( "Preparing statement ... " );
		cliRC = SQLPrepare( *statementHandle, ( SQLCHAR * )modStatementString.data(), SQL_NTS );

		if ( ( cliRC != SQL_SUCCESS ) && ( cliRC != SQL_SUCCESS_WITH_INFO ) )
		{
			stringstream errorMessage;
			errorMessage << "Prepare statement failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";

			DBErrorException errorEx( errorMessage.str() );
			errorEx.addAdditionalInfo( "statement", modStatementString );
			errorEx.addAdditionalInfo( "location", "ODBCDatabase::Prepare" );

			TRACE( errorMessage.str() << " in [" << modStatementString << "]" );
			throw errorEx;
		}

		// bind a cursor on the first position for queries of SP type for SQL_SERVER

		if ( vectorOfParameters.size() > 0 )
			BindParams( vectorOfParameters );
	}
	catch( ... )
	{
		vectorOfParameters.Dump();
		if( statementHandle != NULL )
			FREE_ODBC_HANDLE( *statementHandle, "statement", SQL_HANDLE_STMT, m_Hdbc, SQL_HANDLE_DBC );
		throw;
	}

	//Get Data
	DataSet* returnValue = NULL;
	try
	{
		if ( command.isQuery() )
			returnValue = executeQuery( cachedCommand, isCommandCached, statementHandle, onCursor, fetchRows );
		else
			executeNonQuery( cachedCommand, isCommandCached, statementHandle, onCursor );

		// add it to cache if cacheable
		if( !isCommandCached && command.isCacheable() )
			m_StatementCache.Add( statementString, cachedCommand );
	}
	catch( ... )
	{
		vectorOfParameters.Dump();

		DEBUG2( "Freeing statement" );
		if ( statementHandle != NULL )
		{
			cliRC = SQLFreeStmt( *statementHandle, SQL_CLOSE );
			if ( cliRC != SQL_SUCCESS )
			{
				stringstream freeErrorMessage;
				freeErrorMessage << "Free statement handle failed [" << getErrorInformation( SQL_HANDLE_DBC, m_Hdbc ) << "]";

				//nonfatal err
				TRACE( freeErrorMessage.str() );
			}
			FREE_ODBC_HANDLE( *statementHandle, "statement", SQL_HANDLE_STMT, m_Hdbc, SQL_HANDLE_DBC );
		}

		if ( returnValue != NULL )
		{
			try
			{
				delete returnValue;
				returnValue = NULL;
			} catch( ... ) {}
		}
		throw;
	}

	DEBUG( "Statement executed successfully." );

	if ( command.isQuery() && onCursor )
	{
		DEBUG( "Not releasing statement handle ( on cursor )" );
	}
	else
	{
		if ( statementHandle != NULL )
		{
#ifdef SQLSERVER_ONLY
			if( command.isQuery() && ( command.getCommandType() == DataCommand::SP ) )
			{
				cliRC = SQLCloseCursor( *statementHandle );
				if ( ( cliRC != SQL_SUCCESS ) && ( cliRC != SQL_SUCCESS_WITH_INFO ) )
				{
					stringstream errorMessage;
					errorMessage << "SetCloseCursor with name=[retCursor] failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";

					DBErrorException errorEx( errorMessage.str() );
					errorEx.addAdditionalInfo( "statement", modStatementString );
					errorEx.addAdditionalInfo( "location", "ODBCDatabase::Prepare" );

					TRACE( errorMessage.str() << " in [" << modStatementString << "]" );
					throw errorEx;
				}
			}
#endif
			DEBUG2( "Freeing statement" );
			cliRC = SQLFreeStmt( *statementHandle, SQL_CLOSE );
			if ( cliRC != SQL_SUCCESS )
			{
				stringstream freeErrorMessage;
				freeErrorMessage << "Free statement handle failed [" << getErrorInformation( SQL_HANDLE_DBC, m_Hdbc ) << "]";

				//nonfatal err
				TRACE( freeErrorMessage.str() );
			}
			FREE_ODBC_HANDLE( *statementHandle, "statement", SQL_HANDLE_STMT, m_Hdbc, SQL_HANDLE_DBC );
		}
	}

	return returnValue;
}

void ODBCDatabase::executeNonQuery( DataCommand& command, const bool isCommandCached, SQLHANDLE* statementHandle, const bool holdCursor )
{
	SQLRETURN cliRC;

	// Execute Statement
	DEBUG( "Calling execute on non-query ... " );
	cliRC = SQLExecute( *statementHandle );

	switch( cliRC )
	{
		case SQL_SUCCESS :
			break;

		case SQL_SUCCESS_WITH_INFO :
		{
			// statement executed, but no data was retrieved, or another warning
			stringstream errorMessage;
			errorMessage << "Execute statement failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";

			DBWarningException warningEx( errorMessage.str() );
			warningEx.addAdditionalInfo( "statement", command.getModifiedStatementString() );
			warningEx.addAdditionalInfo( "location", "ODBCDatabase::ExecuteNonQuery" );

			TRACE( errorMessage.str() << " in [" << command.getModifiedStatementString() << "]" );

			throw warningEx;
		}
		break;

		default :
		{
			stringstream errorMessage;
			errorMessage << "Execute statement failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";

			DBErrorException errorEx( errorMessage.str() );
			errorEx.addAdditionalInfo( "statement", command.getModifiedStatementString() );
			errorEx.addAdditionalInfo( "location", "ODBCDatabase::ExecuteNonQuery" );

			TRACE( errorMessage.str() << " in [" << command.getModifiedStatementString() << "]" );

			throw errorEx;
		}
		break;
	}

	SQLLEN rowCount = 0;
	cliRC = SQLRowCount( *statementHandle, &rowCount );
	switch ( cliRC )
	{
		case SQL_SUCCESS :
			break;
		case SQL_SUCCESS_WITH_INFO :
		{
			//maybe data source cannot return the number of rows in a result set before fetching them
			stringstream errorMessage;
			errorMessage << "Could not get number of affected rows [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";

			DBWarningException warningEx( errorMessage.str() );
			warningEx.addAdditionalInfo( "statement", command.getModifiedStatementString() );
			warningEx.addAdditionalInfo( "location", "ODBCDatabase::ExecuteNonQuery" );

			TRACE( errorMessage.str() << " in [" << command.getModifiedStatementString() << "]" );

			throw warningEx;
		}
		default:
		{
			stringstream errorMessage;
			errorMessage << "Could not get number of affected rows [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";
			DBErrorException errorEx( errorMessage.str() );
			errorEx.addAdditionalInfo( "statement", command.getModifiedStatementString() );
			errorEx.addAdditionalInfo( "location", "ODBCDatabase::ExecuteNonQuery" );

			TRACE( errorMessage.str() << " in [" << command.getModifiedStatementString() << "]" );

			throw errorEx;
		}
	}

	m_LastNumberofAffectedRows = rowCount;
}

DataSet* ODBCDatabase::executeQuery( DataCommand& command, const bool isCommandCached, SQLHANDLE* statementHandle, const bool holdCursor, const unsigned int fetchRows )
{
	DataSet *odbcDataSet = NULL;
	SQLRETURN cliRC;

	if ( holdCursor )
	{
		cliRC = SQLSetCursorName( *statementHandle, ( SQLCHAR * )"CURSNAME", SQL_NTS );
		if ( cliRC != SQL_SUCCESS )
		{
			stringstream errorMessage;
			errorMessage << "Set cursor name failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";
			throw runtime_error( errorMessage.str() );
		}
	}

	// Execute Statement
	DEBUG( "Calling execute on query ... " );
	cliRC = SQLExecute( *statementHandle );
	switch( cliRC )
	{
		case SQL_SUCCESS :
			break;

		case SQL_SUCCESS_WITH_INFO :
		{
			// statement executed, but no data was retrieved, or another warning
			stringstream errorMessage;
			errorMessage << "Execute statement warning [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";
			TRACE( errorMessage.str() << " in [" << command.getModifiedStatementString() << "]" );
		}
		break;

		default :
		{
			stringstream errorMessage;
			errorMessage << "Execute statement failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";

			DBErrorException errorEx( errorMessage.str() );
			errorEx.addAdditionalInfo( "statement", command.getModifiedStatementString() );
			errorEx.addAdditionalInfo( "location", "ODBCDatabase::ExecuteQuery" );

			TRACE( errorMessage.str() << " in [" << command.getModifiedStatementString() << "]" );

			throw errorEx;
		}
		break;
	}

	odbcDataSet = getDataSet( command, isCommandCached, statementHandle, holdCursor, fetchRows );
	if ( odbcDataSet == NULL )
		throw runtime_error( "Empty ( NULL ) resultset returned." );

	if( holdCursor )
	{
		SQLCHAR cursorName[ 20 ];
		SQLSMALLINT cursorLen;
		cliRC = SQLGetCursorName( *statementHandle, cursorName, 20, &cursorLen );
		if ( cliRC != SQL_SUCCESS )
		{
			stringstream errorMessage;
			errorMessage << "Get cursor name failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";
			throw runtime_error( errorMessage.str() );
		}
		m_HoldCursorName = ( char* )( cursorName );
	}

	return odbcDataSet;
}

DataSet* ODBCDatabase::getDataSet( DataCommand& command, const bool isCommandCached, SQLHANDLE* statementHandle, const bool useCursor, const unsigned int fetchRows )
{
	SQLRETURN cliRC;
	short nResultCols = 0;

	if ( isCommandCached )
	{
		nResultCols = command.getResultColumnCount();
		DEBUG( "Cached no of columns : " << nResultCols );
	}
	else
	{
		//Get number of result columns
		DEBUG( "Getting the number of result set columns ..." );

		cliRC = SQLNumResultCols( *statementHandle, &nResultCols );
		if ( ( cliRC != SQL_SUCCESS ) && ( cliRC != SQL_SUCCESS_WITH_INFO ) )
		{
			stringstream errorMessage;
			errorMessage << "Getting the number of result columns failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";
			throw runtime_error( errorMessage.str() );
		}
		DEBUG( "No of columns : " << nResultCols );
	}

	//Define odbcRow as map of result columns
	DataRow odbcRow;
	bool inBlobsArea = false;
	map<SQLUSMALLINT, string> blobColumns;

	//#region For each column in the result set, describe result and alloc buffers
	for ( SQLUSMALLINT i=0; i<nResultCols; i++ )
	{
		string columnName = "";
		DataType::DATA_TYPE columnType = DataType::INVALID_TYPE, columnBaseType = DataType::INVALID_TYPE;
		unsigned int columnDimension = 0;
		int columnScale = 0;

		if ( isCommandCached )
		{
			columnName = command.getResultColumn( i ).getName();
			columnType = command.getResultColumn( i ).getType();
			columnBaseType = command.getResultColumn( i ).getBaseType();
			columnDimension = command.getResultColumn( i ).getDimension();
			columnScale = command.getResultColumn( i ).getScale();
		}
		else
		{
			SQLCHAR colName[ MAX_ODBC_COLUMN_NAME ];
			SQLSMALLINT colNameLen;
			SQLSMALLINT colType;
			SQLULEN colSize;
			SQLSMALLINT colScale;

			DEBUG( "Getting column [" << i << "] description ..." );

			// Get each column description
			cliRC = SQLDescribeCol( *statementHandle, ( SQLUSMALLINT )( i + 1 ), colName, MAX_ODBC_COLUMN_NAME,
			                        &colNameLen, &colType, &colSize, &colScale,	NULL );

			if ( ( cliRC != SQL_SUCCESS ) && ( cliRC != SQL_SUCCESS_WITH_INFO ) )
			{
				stringstream errorMessage;
				errorMessage << "Get column description failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";
				throw runtime_error( errorMessage.str() );
			}

			DEBUG2( "Column Name : " << colName << "; Column SQL Type : " << colType <<
			        "; Column Size : " << colSize << "; Column Scale : " << colScale );

			columnName = StringUtil::ToUpper( string( ( char* )colName ) );

			columnType = ODBCDatabaseFactory::getDataType( colType, colSize );
			columnDimension = colSize + 1;
			columnScale = colScale;

#ifdef AS400
			if ( colType == SQL_NUMERIC  )
				columnDimension = colSize + 2;
#else
			if ( colType == SQL_DECIMAL  )
				columnDimension = colSize + 2;
#endif
		}

		// Allocate data buffer for every column, fetching will fill this data
		DataColumnBase* odbcColumn = NULL;
		try
		{
			odbcColumn = ODBCDatabaseFactory::internalCreateColumn( columnType, columnDimension, columnScale, columnName );

			//SQLINTEGER buffLen;
			//Bind each Column to the result Set
			// Don't bind BLOBS/CLOBS at least for SqlServer..
			if ( ( columnType == DataType::BINARY ) || ( columnBaseType == DataType::BINARY ) )
			{
				blobColumns.insert( pair<SQLUSMALLINT, string>( i, columnName ) );
				odbcColumn->setType( DataType::BINARY );
			}
			else
			{
				DEBUG2( "Bind columns..." );
				cliRC = SQLBindCol( *statementHandle, ( SQLSMALLINT )( i + 1 ), ODBCDatabaseFactory::getODBCDataType( columnType ),
				                    odbcColumn->getStoragePointer(), columnDimension, (SQLLEN*)odbcColumn->getBufferIndicator() );

				if ( ( cliRC != SQL_SUCCESS ) && ( cliRC != SQL_SUCCESS_WITH_INFO ) )
				{
					stringstream errorMessage;
					errorMessage << "Bind column failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) <<
					             "] for column " << i + 1 << " column name [" << columnName << "]";
					throw runtime_error( errorMessage.str() );
				}

				DEBUG2( "Bind column by position successful." );
			}

			// add it to cache ( if it is cacheable )
			if ( !isCommandCached && command.isCacheable() )
			{
				DEBUG( "Command is not cached, but cacheable. Adding result column ..." );
				command.addResultColumn( i, odbcColumn );
			}

			// Insert in the odbcRow the current Db2ResultColumn pair : columnName, odbcColumn (column data and description)
			// odbcRow will be the buffer that will receive data after fetch
			odbcRow.insert( makeColumn( columnName, odbcColumn ) );
		}
		catch( ... )
		{
			if ( odbcColumn != NULL )
			{
				delete odbcColumn;
				odbcColumn = NULL;
			}
			throw;
		}
	}
	//#endregion describe columns

	DEBUG2( "Creating result dataset ..." );
	DataSet* odbcDataSet = NULL;

	try
	{
		odbcDataSet = new DataSet();
		DEBUG2( "Fetching first row ..." );

		//Fetch First Row
		cliRC = SQLFetch( *statementHandle );
		if ( cliRC == SQL_NO_DATA_FOUND )
		{
			DEBUG( "No result data " );
		}
		else if ( ( cliRC != SQL_SUCCESS ) && ( cliRC != SQL_SUCCESS_WITH_INFO ) )
		{
			stringstream errorMessage;
			errorMessage << "Fetch data failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";

			DBErrorException errorEx( errorMessage.str() );
			errorEx.addAdditionalInfo( "statement", command.getModifiedStatementString() );
			errorEx.addAdditionalInfo( "location", "ODBCDatabase::getDataSet" );

			TRACE( errorMessage.str() << " in [" << command.getModifiedStatementString() << "]" );
			throw errorEx;
		}

		//While more data in the result set
		while ( cliRC != SQL_NO_DATA_FOUND )
		{
			//Check to see if we need to get a blob
			map<SQLUSMALLINT, string>::const_iterator it;
			for ( it = blobColumns.begin(); it != blobColumns.end(); ++it ) 
			{
				BYTE tempBlogDummy;
				SQLLEN blobIndicator;

				// Call SQLGetData to determine the amount of data that's waiting.
				cliRC = SQLGetData( *statementHandle, ( SQLSMALLINT )( it->first + 1 ),
				                    SQL_C_BINARY, &tempBlogDummy, 0, &blobIndicator );
				if ( ( cliRC != SQL_SUCCESS ) && ( cliRC != SQL_SUCCESS_WITH_INFO ) )
				{
					stringstream errorMessage;
					errorMessage << "Get Blob length failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";

					DBErrorException errorEx( errorMessage.str() );
					errorEx.addAdditionalInfo( "statement", command.getModifiedStatementString() );
					errorEx.addAdditionalInfo( "location", "ODBCDatabase::getDataSet" );

					TRACE( errorMessage.str() << " in [" << command.getModifiedStatementString() << "]" );
					throw errorEx;
				}
				if ( blobIndicator <= 0 )
					static_cast< ODBCColumn< string >* >( odbcRow[ it->second ] )->setValue( "" );
				else
				{
					vector<BYTE> tempBlob( blobIndicator );

					cliRC = SQLGetData( *statementHandle, ( SQLSMALLINT )( it->first + 1 ),
						                SQL_C_DEFAULT, &tempBlob[0], blobIndicator, &blobIndicator );
					if ( ( cliRC != SQL_SUCCESS ) && ( cliRC != SQL_SUCCESS_WITH_INFO ) )
					{
						stringstream errorMessage;
						errorMessage << "Get Blob data failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";

						DBErrorException errorEx( errorMessage.str() );
						errorEx.addAdditionalInfo( "statement", command.getModifiedStatementString() );
						errorEx.addAdditionalInfo( "location", "ODBCDatabase::getDataSet" );

						TRACE( errorMessage.str() << " in [" << command.getModifiedStatementString() << "]" );
						throw errorEx;
					}

					const string base64EncodedBinaryData( Base64::encode( &tempBlob[0], blobIndicator ) );
					static_cast< ODBCColumn< string >* >( odbcRow[ it->second ] )->setValue( base64EncodedBinaryData );
				}
			}

			odbcRow.Sync();
			//Done with reading the blob

			DEBUG2( "Copying row to dataset" );
			//Copy the odbcRow buffer into a odbcNewRow
			DataRow* odbcNewRow = new DataRow( odbcRow );

			DEBUG2( "Appending row to dataset" );
			odbcDataSet->push_back( odbcNewRow );

			odbcRow.Clear();

			if ( fetchRows > 0 )
				break;

			// Fetch next row in the db2Row buffer
			DEBUG2( "Fetching another row" );
			cliRC = SQLFetch( *statementHandle );
			if ( ( cliRC != SQL_SUCCESS ) && ( cliRC != SQL_NO_DATA_FOUND ) && ( cliRC != SQL_SUCCESS_WITH_INFO ) )
			{
				stringstream errorMessage;
				errorMessage << "Fetch data failed [" << getErrorInformation( SQL_HANDLE_STMT, *statementHandle ) << "]";

				DBErrorException errorEx( errorMessage.str() );
				errorEx.addAdditionalInfo( "statement", command.getModifiedStatementString() );
				errorEx.addAdditionalInfo( "location", "ODBCDatabase::ExecuteQuery" );

				TRACE( errorMessage.str() << " in [" << command.getModifiedStatementString() << "]" );
				throw errorEx;
			}
		}
		//if ( odbcDataSet->size() != 0 )
		//DisplayDataSet( odbcDataSet );

		DEBUG( "Statement executed successfully." );
	}
	catch( ... )
	{
		if ( odbcDataSet != NULL )
		{
			try
			{
				delete odbcDataSet;
				odbcDataSet = NULL;
			} catch( ... ) {}
		}
		throw;
	}

	return odbcDataSet;
}


//-----------------------------------------------------------
//Display Error Information after each ODBC CLI - SQL function
//-----------------------------------------------------------
string ODBCDatabase::getErrorInformation( SQLSMALLINT htype, SQLHANDLE handle )
{
	SQLINTEGER sqlcode;
	SQLCHAR sqlstate[ SQL_SQLSTATE_SIZE + 1 ];
	SQLCHAR message[ SQL_MAX_MESSAGE_LENGTH + 1 ];
	SQLSMALLINT realLength;
	int i = 1;
	stringstream errorBuffer;


	while ( SQLGetDiagRec( htype, handle, i, sqlstate, &sqlcode, message, SQL_MAX_MESSAGE_LENGTH, &realLength ) == SQL_SUCCESS )
	{
		errorBuffer << "Detailed ODBC error information : ";
		errorBuffer << " SQLSTATE: " << sqlstate;
		errorBuffer << " SQLCODE: "  << sqlcode;
		errorBuffer << " SQLMESSAGE: " << message;
		i++;
	}

	//TRACE( errorBuffer.str( ) );
	return errorBuffer.str();
}

//-------------------------------
//Begin Transaction
//-------------------------------
void ODBCDatabase::BeginTransaction( const bool readonly )
{
	if ( m_HdbcAllocated )
	{
		DEBUG( "Setting connection attribute " );
		SQLRETURN  cliRC = SQLSetConnectAttr( m_Hdbc, SQL_ATTR_AUTOCOMMIT, ( SQLPOINTER )SQL_AUTOCOMMIT_OFF, SQL_IS_UINTEGER );
		if ( cliRC != SQL_SUCCESS )
		{
			stringstream errorMessage;
			errorMessage << "ODBCDatabase::BeginTransaction - set SQL_ATTR_AUTOCOMMIT : " << getErrorInformation( SQL_HANDLE_DBC, m_Hdbc );

			TRACE( errorMessage.str() )
		}
	}
}

//-------------------------------
//End Transaction
//-------------------------------
void ODBCDatabase::EndTransaction ( TransactionType::TRANSACTION_TYPE trType, const bool throwOnError )
{
	DEBUG( "Ending transaction ..." );

	int transactionType = getODBCTransactionType( trType );

	SQLRETURN cliRC = SQLEndTran( SQL_HANDLE_DBC, m_Hdbc, transactionType );
	if ( cliRC != SQL_SUCCESS )
	{
		stringstream errorMessage;
		errorMessage << "ODBCDatabase::EndTransaction - end transaction : " << getErrorInformation( SQL_HANDLE_DBC, m_Hdbc );

		TRACE( errorMessage.str() );
	}

	cliRC = SQLSetConnectAttr( m_Hdbc, SQL_ATTR_AUTOCOMMIT, ( SQLPOINTER )SQL_AUTOCOMMIT_ON, SQL_IS_UINTEGER );
	if ( cliRC != SQL_SUCCESS )
	{
		stringstream errorMessage;
		errorMessage << "ODBCDatabase::EndTransaction - set SQL_ATTR_AUTOCOMMIT : " << getErrorInformation( SQL_HANDLE_DBC, m_Hdbc );

		TRACE( errorMessage.str() );
	}
}

int ODBCDatabase::getODBCTransactionType( TransactionType::TRANSACTION_TYPE type )
{
	switch( type )
	{
		case TransactionType::COMMIT :
			return SQL_COMMIT;
		case TransactionType::ROLLBACK :
			return SQL_ROLLBACK;
	}
	throw invalid_argument( "type" );
}

void ODBCDatabase::setSpecificEnvAttr()
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

string ODBCDatabase::callFormating( const string& statementString, const ParametersVector& vectorOfParameters )
{
	// Compose the Stored Procedure CALL statement
	// TODO:  Verify the SP CALL format
	stringstream paramBuffer;
	unsigned int paramCount = vectorOfParameters.size();
	paramBuffer << "CALL " << statementString << "( ";
	for ( unsigned int i=0; i<paramCount; i++ )
	{
		paramBuffer << " ? ";
		if ( i < paramCount - 1 )
			paramBuffer << ",";
	}
	paramBuffer << " )";
	return paramBuffer.str();
}
