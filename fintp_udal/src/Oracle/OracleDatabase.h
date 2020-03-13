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

#ifndef ORACLEDATABASE_H
#define ORACLEDATABASE_H

#include "../Database.h"
#include "../ConnectionString.h"

#ifdef WIN32
#define __MSXML_LIBRARY_DEFINED__
#include <windows.h>
#define sleep(x) Sleep( (x)*1000 )
#else
#include <unistd.h>
#endif

#include <string>
#include <oci.h>
using namespace std;

#define MAX_DOCUMENT_LENGTH 8000

namespace FinTP
{
	/**
	 * Derived OracleDatabase class to implement specific operations for Oracle Databases
	 * 1. First a new OracleDatabase instace is created  
	 * 2. The, the client can create a new transaction by calling BeginTransaction(readonly) method  
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
	class OracleDatabase : public Database
	{
		public :

			OracleDatabase();
			~OracleDatabase();

			/**
			 * Oracle overrides for transaction support.
			 * \param readonly type bool.
			**/
			void BeginTransaction( const bool readonly );
			void EndTransaction( const TransactionType::TRANSACTION_TYPE transactionType, const bool throwOnError  );

			/**
			 * Connect to database
			 *  - allocate environment handle
			 *  - allocate database connection handle
			 *  - connect to database.
			 * \param connectionString The connection string to connect.
			**/
			void Connect( const ConnectionString& connectionString );

			/**
			 * Disconnect from database
			 *  - deallocate environment handle
			 *  - deallocate database connection handle
			 *  - disconnect to database.
			**/
			void Disconnect();

			// Gets the connected/disconnected status
			bool IsConnected();

			/**
			 * Execute NonQuery SQL statements or stored procedures ( without params )
			 * \param commType type COMMAND_TYPE.	The command type.
			 * \param stringStatement type string.  The statement.
			 * \param onCursor type bool.			Determine if the function will use an Oracle cursor to iterate through the results.
			 * \note Cursor description ?							  
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
			 * \param onCursor type bool.			Determine if the function will use an Oracle cursor to iterate throught the results.
			 * \note Cursor description ?								  
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
			 * \param onCursor type bool.                       Determine if the function will use an Oracle cursor to iterate throught the results.		
			 * \note Cursor description ?
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
			 * \param onCursor type bool.                       Determine if the function will use an Oracle cursor to iterate throught the results.
			 * \note Cursor description ?
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
			 * \param onCursor type bool.                       Determine if the function will use an Oracle cursor to iterate throught the results.		
			 * \param fetchRows type int.                       The row number used in where clause.
			 * \note Cursor description and fetchrows descr?
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
			 * \param onCursor type bool.                       Determine if the function will use an Oracle cursor to iterate throught the results.
			 * \param fetchRows type int.                       The row number used in where clause.
			 * \note Cursor description and fetchrows descr?
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
			 * \param onCursor type bool.                       Determine if the function will use an Oracle cursor to iterate throught the results.
			 * \param fetchRows type int.                       The row number used in where clause.
			 * \note Cursor description and fetchrows descr?
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
			 * \param onCursor type bool.                       Determine if the function will use an Oracle cursor to iterate throught the results.
			 * \param fetchRows type int.                       The row number used in where clause.
			 * \note Cursor description and fetchrows descr?
			**/
			DataSet* ExecuteQueryCached( const DataCommand::COMMAND_TYPE commType, const string& stringStatement, const ParametersVector& vectorOfParameters, const bool onCursor, const unsigned int fetchRows )
			{
				DataCommand queryCached( commType, DataCommand::QUERY, stringStatement, true );
				return innerExecuteCommand( queryCached, vectorOfParameters, onCursor, fetchRows );

			/**
			 * Releases the statement.
			 * \param isCommandCached The is command cached.
			 * \param key			  The key associated with the statement in the cache.
			**/
			}

			void ReleaseStatement( const bool isCommandCached, const string& key );
			void ReleaseCursor( const bool checkConn );
			void RewindCursor() {}

		private :

			bool m_IsConnected, m_IsReconnecting;

			ConnectionString m_ConnectionString;

			OCIEnv 		*m_hEnv;			// environment handle
			OCIError 	*m_hError;			// error handle
			OCISvcCtx 	*m_hServiceContext;	// ServiceContext handle
			OCISession 	*m_hSession;		// session handle
			OCIServer 	*m_hServer;			// server handle
			OCITrans 	*m_hTransaction;	// transaction handle
			OCIStmt		*m_StatementHandle;	// statement handle
			OCIStmt		*m_hCursor;			// cursor handle
			OCIRowid	*m_HoldCursorRowId;	// held rowid
			xmlctx		*m_XmlContext;		// XML context required by XDB

			OCIClobLocator *m_ClobLocator;
			OCIBlobLocator *m_BlobLocator;
			OCIClobLocator *m_FetchClobLocator;
			OCIClobLocator *m_FetchBlobLocator;
			OCIType *m_ArrayType;
			// binds for date/timestamp columns in a returned dataset
			vector< OCIDate** > m_Date;
			vector< OCIDateTime** > m_Timestamp;

			CacheManager< string, OCIType* > m_TypeCache;

			/**
			 * Bind parameters.
			 * \param vectorOfParameters Options for controlling the vector of parameters.
			 * \param startIndex		 the start index.
			**/
			void BindParams( const ParametersVector& vectorOfParameters, const unsigned int startIndex = 1 );

			DataSet* innerExecuteCommand( const DataCommand& command, const ParametersVector& vectorOfParameters, const bool useCursor = false, const unsigned int fetchRows = 0 );

			/**
			 * fetches data from the database, using an already created statement handle.
			 * \param [in,out] command The command. Can be query or stored procedure.
			 * \param isCommandCached  Determines if the command is cached.
			 * \param useCursor		   Determines if a cursor is going to be used.
			 * \return null if the query execution fails .
			**/
			DataSet* executeQuery( DataCommand& command, const bool isCommandCached, const bool useCursor );
			DataSet* getDataSet( DataCommand& command, const bool isCommandCached, OCIStmt *statementHandle, const bool useCursor );

			/**
			 * Executes the non query operation.
			 * \param [in,out] command The command. Can be nonquery or stored procedure.
			 * \param isCommandCached  Determines if the command is cached
			 * \param useCursor		   Determines if a cursor is going to be used.
			**/
			void executeNonQuery( DataCommand& command, const bool isCommandCached, const bool useCursor );

			// translates transaction type to a Oracle specific value
			static int getOracleTransactionType( TransactionType::TRANSACTION_TYPE type );

			/**
			 * get detailed error information.
			 * \param [in,out] errorHandle If non-null, handle of the error.
			 * \param status			   The status.
			 * \param handleType		   The type of the handle.
			 * \param connCheck			   The connection check.
			 * \return The error information.
			**/
			string getErrorInformation( dvoid *errorHandle, sword status, ub4 handleType = OCI_HTYPE_ERROR, const bool connCheck = true );

			OCIType* getMessageType( const string& typeName, const string& userName = "" );

			void freeOCIArrays( const ParametersVector& vectorOfParameters );

			void initXmlContext();

			string RawToString( const OCIRaw* value );
			OCIRaw* StringToRaw( const string& value );

			// clears the m_Date, m_Timestamp vectors
			void ClearDateTimestampBinds();
	};
}

#endif // ORACLEDATABASE_H
