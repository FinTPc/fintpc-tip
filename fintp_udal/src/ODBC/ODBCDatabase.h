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

#ifndef ODBCDATABASE_H
#define ODBCDATABASE_H

#include "../Database.h"
#include "../ConnectionString.h"
#include "ODBCParameter.h"
#include "ODBCColumn.h"
#include <pthread.h>

#include <map>
#include <string>

#ifdef DB2_ONLY
#include <SQLLIB\include\sqlcli.h>
#include <SQLLIB\include\sqlext.h>
#include <SQLLIB\include\sqlcli1.h>
#elif defined INFORMIX_ONLY
#ifdef WIN32
#include <Informix\Client-SDK\incl\cli\sql.h>
#include <Informix\Client-SDK\incl\cli\sqlext.h>
#else
#include <infxcli.h>
#include <infxsql.h>
#endif
#else
#include <sql.h>
#include <sqlext.h>
#endif

using namespace std;

namespace FinTP
{
	/**
	 * Derived ODBCDatabase class to implement specific operations for ODBC Databases
	 * 1. First a new ODBCDatabase instace is created  
	 * 2. Then, the client can create a new transaction by calling BeginTransaction(readonly) method  
	 * 3. Transactions comprise one ore more calls to "ExecuteQuery" methods. Every database query is mapped to a Command instance and  
	 * can be cached in m_StatementCache member of Database instance  
	 * 4. A transaction ends when the client calls EndTransaction(transactionType) performing commit or rollback on current transaction  
	 * 5. The database connection is released when Disconect() is called.  
	 * 
	 * Database declare two types of "ExecuteQuery" methods
	 * - ExecuteQuery - Create a DataCommand from passed parameters and execute it 
	 * - ExecuteQueryChached - Additionally, attempts to retrieve the params and resultset columns from DataCommand cache or adds them if not available
	 *
	 * Database, define four library specific exception types to be used by its implementations
	 * - DBConnectionLostException - should be used when connection is lost and cannot be recovered
	 * - DBNoUpdatesException - signal to the clients that update command with no updated database field was performed
	 * - DBWarningException - should be used when current command returned a warning code
	 * - DBErrorException - should be used whe command returned an error code
	 * 
	**/
	class ExportedUdalObject ODBCDatabase : public Database
	{
		public :

			ODBCDatabase();
			virtual ~ODBCDatabase();

			// ODBC overrides for transaction support
			virtual void BeginTransaction( const bool readonly );
			virtual void EndTransaction( const TransactionType::TRANSACTION_TYPE transactionType, const bool throwOnError );

			/**
			 * Connects using the given connection string.
			 * \param connectionString. Connection information nedeed to connect to DBMS.
			**/
			void Connect( const ConnectionString& connectionString );

			/**
			 * Disconnects and release connection resources
			**/
			void Disconnect();

			// Gets the connected/disconnected status
			virtual bool IsConnected() {
				return (isDbcAllocated() & m_IsConnected);
			};

			/**
			 * Execute NonQuery SQL statements or stored procedures ( without params )
			 * \param commType type COMMAND_TYPE.	The command type.
			 * \param stringStatement type string.  The statement.
			 * \param onCursor type bool.			Determine if the function will use an Oracle cursor to iterate through the results.									  
			**/
			void ExecuteNonQuery( const DataCommand::COMMAND_TYPE commType, const string& stringStatement, bool onCursor )
			{
				DataCommand nonquery( commType, DataCommand::NONQUERY, stringStatement, false );
				ParametersVector tempParams;
				(void)innerExecuteCommand( nonquery, tempParams, onCursor, 0 );
			}

			/**
			 * Execute NonQueryCached SQL statements or stored procedures ( without params ) and use caching
			 * \param commType type COMMAND_TYPE.	The command type
			 * \param stringStatement type string.  The statement.
			 * \param onCursor type bool.			Determine if the function will use an Oracle cursor to iterate through the results.										  
			**/
			void ExecuteNonQueryCached( const DataCommand::COMMAND_TYPE commType, const string& stringStatement, bool onCursor )
			{
				DataCommand nonquery( commType, DataCommand::NONQUERY, stringStatement, true );
				ParametersVector tempParams;
				(void)innerExecuteCommand( nonquery, tempParams, onCursor, 0 );
			}

			 /**
			 * Execute NonQuery SQL statements or stored procedures ( with params )
			 * \param commType type COMMAND_TYPE.               The command type .
			 * \param stringStatement type string.              The statement.
			 * \param vectorOfParameters type ParametersVector. The vector of parameters. Used when multiple parameters are needed.					  
			 * \param onCursor type bool.                       Determine if the function will use an Oracle cursor to iterate through the results.		
			**/
			void ExecuteNonQuery( const DataCommand::COMMAND_TYPE commType, const string& stringStatement, const ParametersVector& vectorOfParameters, bool onCursor )
			{
				DataCommand nonquery( commType, DataCommand::NONQUERY, stringStatement, false );
				(void)innerExecuteCommand( nonquery, vectorOfParameters, onCursor, 0 );
			}

			/**
			 * Execute NonQueryCached SQL statements or stored procedures ( with params ) and use caching
			 * \param commType type COMMAND_TYPE.               The command type. 
			 * \param stringStatement type string.              The statement.
			 * \param vectorOfParameters type ParametersVector. The vector of parameters. Used when multiple parameters are needed.
			 * \param onCursor type bool.                       Determine if the function will use an Oracle cursor to iterate through the results.		
			**/
			void ExecuteNonQueryCached( const DataCommand::COMMAND_TYPE commType, const string& stringStatement, const ParametersVector& vectorOfParameters, bool onCursor )
			{
				DataCommand nonquery( commType, DataCommand::NONQUERY, stringStatement, true );
				(void)innerExecuteCommand( nonquery, vectorOfParameters, onCursor, 0 );
			}

			/**
			 * Execute Query SQL statements ( without params )
			 * \param commType type COMMAND_TYPE.               The command type.
			 * \param stringStatement type string.              The statement.
			 * \param onCursor type bool.                       Determine if the function will use an Oracle cursor to iterate through the results.		
			 * \param fetchRows type int.                       The row number used in where clause.
			**/
			DataSet* ExecuteQuery( const DataCommand::COMMAND_TYPE commType, const string& stringStatement, bool onCursor, const unsigned int fetchRows )
			{
				DataCommand query( commType, DataCommand::QUERY, stringStatement, false );
				ParametersVector tempParams;
				return innerExecuteCommand( query, tempParams, onCursor, fetchRows );
			}

			/**
			 * Execute Query SQL statements ( with params )
			 * \param commType type COMMAND_TYPE.               The command type.
			 * \param stringStatement type string.              The statement.
			 * \param vectorOfParameters type ParametersVector. The vector of parameters. Used when multiple parameters are needed.
			 * \param onCursor type bool.                       Determine if the function will use a cursor to iterate through the results.		
			 * \param fetchRows type int.                       The row number used in where clause.
			**/
			DataSet* ExecuteQuery( const DataCommand::COMMAND_TYPE commType, const string& stringStatement, const ParametersVector& vectorOfParameters, bool onCursor, const unsigned int fetchRows )
			{
				DataCommand query( commType, DataCommand::QUERY, stringStatement, false );
				return innerExecuteCommand( query, vectorOfParameters, onCursor, fetchRows );
			}

			/**
			 * \Execute Query SQL statements ( without params ) and use caching
			 * \param commType type COMMAND_TYPE.               The command type.
			 * \param stringStatement type string.              The statement.
			 * \param onCursor type bool.                       Determine if the function will use a cursor to iterate through the results.		
			 * \param fetchRows type int.                       The row number used in where clause.
			**/
			DataSet* ExecuteQueryCached( const DataCommand::COMMAND_TYPE commType, const string& stringStatement, const bool onCursor, const unsigned int fetchRows )
			{
				DataCommand queryCached( commType, DataCommand::QUERY, stringStatement, true );
				ParametersVector tempParams;
				return innerExecuteCommand( queryCached, tempParams, onCursor, fetchRows );
			}

			/**
			 * Execute Query SQL statements ( with params ) and use caching
			 * \param commType type COMMAND_TYPE.               The command type.
			 * \param stringStatement type string.              The statement.
			 * \param vectorOfParameters type ParametersVector. The vector of parameters. Used when multiple parameters are needed.
			 * \param onCursor type bool.                       Determine if the function will use a cursor to iterate through the results.		
			 * \param fetchRows type int.                       The row number used in where clause.
			**/
			DataSet* ExecuteQueryCached( const DataCommand::COMMAND_TYPE commType, const string& stringStatement, const ParametersVector& vectorOfParameters, const bool onCursor, const unsigned int fetchRows )
			{
				DataCommand queryCached( commType, DataCommand::QUERY, stringStatement, true );
				return innerExecuteCommand( queryCached, vectorOfParameters, onCursor, fetchRows );
			}

			void ReleaseCursor( const bool checkConn );
			void RewindCursor();
			bool CursorHeld() const {
				return m_HoldCursorInvoked;
			}

		protected :

			// ODBC specific Environment helper functions

			SQLHANDLE m_Henv;
			bool m_HenvAllocated;
			bool isEnvAllocated() const {
				return m_HenvAllocated;
			}
			bool m_IsConnected;

			SQLHANDLE m_Hdbc;
			bool m_HdbcAllocated;
			SQLHANDLE m_Hstmt;
			SQLHANDLE m_HoldCursorHandle;
			string m_HoldCursorName;
			bool m_HoldCursorInvoked;
			
			SQLRETURN AllocateEnv();
			bool isDbcAllocated() const {
				return m_HdbcAllocated;
			}

			void BindParams( const ParametersVector& vectorOfParameters, SQLHANDLE* statementHandle = NULL, const unsigned int startIndex = 1 );

			DataSet* innerExecuteCommand( const DataCommand& command, const ParametersVector& vectorOfParameters, const bool useCursor = false, const unsigned int fetchRows = 0 );

			// fetches data from the database, using an already created statement handle
			DataSet* executeQuery( DataCommand& command, const bool isCommandCached, SQLHANDLE* statementHandle, const bool useCursor, const unsigned int fetchRows = 0 );
			DataSet* getDataSet( DataCommand& command, const bool isCommandCached, SQLHANDLE* statementHandle, const bool useCursor, const unsigned int fetchRows = 0 );

			void executeNonQuery( DataCommand& command, const bool isCommandCached, SQLHANDLE* statementHandle, const bool useCursor );

			// translates transaction type to a DB2 specific value
			static int getODBCTransactionType( TransactionType::TRANSACTION_TYPE type );

			// get detailed error information
			virtual string getErrorInformation( SQLSMALLINT htype, SQLHANDLE handle );

			virtual void setSpecificConnAttr() {};
			virtual void setSpecificEnvAttr();
			virtual string callFormating( const string& statementString, const ParametersVector& vectorOfParameters );
	};
}

#endif
