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

#ifndef ABSTRACTDATABASE_H
#define ABSTRACTDATABASE_H

#include <string>
#include <xercesc/dom/DOM.hpp>

using namespace std;

#include "AppExceptions.h"

#include "ConnectionString.h"
#include "DataSet.h"
#include "DataCommand.h"

namespace FinTP
{
	/**
	 * Used when the current command returned a warning code
	**/
	class ExportedUdalObject DBWarningException : public AppException
	{
		public:
			explicit DBWarningException( const string& argumentName = "No argument", const EventType::EventTypeEnum eventType = EventType::Warning, const NameValueCollection* additionalInfo = NULL );
			~DBWarningException() throw() {}
	};

	/**
	 * Used when the current command returned an error code
	**/
	class ExportedUdalObject DBErrorException : public AppException
	{
		private :
			string m_Code;

		public:

			explicit DBErrorException( const string& argumentName = "No argument", const EventType::EventTypeEnum eventType = EventType::Error, const NameValueCollection* additionalInfo = NULL );
			~DBErrorException() throw() {}

			string code() const;
			void setCode( const string& code );
	};

	/**
	 * Used when connection is lost and cannot be recovered
	**/
	class ExportedUdalObject DBConnectionLostException : public AppException
	{
		public:

			explicit DBConnectionLostException( const string& argumentName = "No argument", const EventType::EventTypeEnum eventType = EventType::Error, const NameValueCollection* additionalInfo = NULL );
			~DBConnectionLostException() throw() {};
	};

	/**
	 * Signal to the clients that update command with no updated database field was performed
	 * \note Not thrown by Udal
	 * 
	**/
	class ExportedUdalObject DBNoUpdatesException : public AppException
	{
		public:
			explicit DBNoUpdatesException( const string& argumentName = "No argument",const EventType::EventTypeEnum eventType = EventType::Warning, const NameValueCollection* additionalInfo = NULL );
			~DBNoUpdatesException() throw() {}
	};

	/**
	 * Contains an enum of all the internal transaction types used by udal and a static helper function that returns a friendly name of the transaction type
	**/
	class ExportedUdalObject TransactionType
	{
		public :

			/**
			 * Two value type used as transaction end options. See EndTransaction() method 
			**/
			typedef enum
			{
			    /** Commit transaction **/
			    COMMIT,
			    /** Rollback transaction **/
			    ROLLBACK
			} TRANSACTION_TYPE;

			/**
			 * Returns a friendly name of the TransactionType.
			 * \param type The TRANSACTION_TYPE value to be converted as string.
			 * \return string representation of TRANSACTION_TYPE value.
			**/
			static string ToString( TRANSACTION_TYPE type );
	};

	/**
	 * Main database functional class. Its purpose is to provide a common interface to operate over different database specific APIs.
	 * Database usage consists on making the following sequenced calls to Database class methods
	 * 1. First a new Database instace is obtained from factory,  
	 * 2. Then, the client can easily connect to database storage by calling the Connect() method. The connection can be tested any time during the 
	 * instace lifecycle using isConnected() method
	 * 3. If database connection is up the client needs to spawn a new database transaction by calling BeginTransaction(readonly) method  
	 * 4. Transactions comprise one ore more calls to "ExecuteQuery" methods. Every database query is mapped to a Command instance and  
	 * can be cached in m_StatementCache member of Database instance  
	 * 5. A transaction ends when the client calls EndTransaction(transactionType) performing commit or rollback on the current transaction  
	 * 6. The database connection is released when Disconect()  
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
	class ExportedUdalObject Database
	{
		protected :
			/** Collection to hold already executed Command instances **/
			CacheManager< string, DataCommand > m_StatementCache;
			/** The last error code returned from command execution. **/
			string m_LastErrorCode;
			/** The number of updates last command performed. **/
			unsigned int m_LastNumberofAffectedRows;
			
		public:

			Database();
			virtual ~Database();

			/**
			 * Starts a transaction.
			 * \param readonly = true if no Inserts/Updates are intended
			**/
			virtual void BeginTransaction( const bool readonly ) = 0;
			void BeginTransaction() {
				BeginTransaction( false );
			}

			/**
			 * Ends a transaction.
			 * \param transactionType TransactionType option to close transaction 
			 * \param throwOnError    Bool flag to inhibit exception throwing on error.
			 * \note why wish not to throw exception
			**/
			virtual void EndTransaction( const TransactionType::TRANSACTION_TYPE transactionType, const bool throwOnError ) = 0;
			void EndTransaction( const TransactionType::TRANSACTION_TYPE transactionType ) {
				EndTransaction( transactionType, true );
			}

			/**
			 * Connects using the given connection string.
			 * \param connectionString. Connection information nedeed to connect to DBMS.
			**/
			virtual void Connect( const ConnectionString& connectionString ) = 0;

			/**
			 * Disconnects and release connection resources
			**/
			virtual void Disconnect() = 0;

			/**
			 * Query if Database instance is connected.
			 * \return true if connected, false if not.
			**/
			virtual bool IsConnected() = 0;

			/**
			 * ExecuteQuery method
			 * Execute NonQuery (Insert, Update, Delete) SQL statements or stored procedures. Executed statement is not parameterized
			 * \param commandType	  Type of the command.
			 * \param stringStatement The string statement.
			 * \param onCursor		  true to on cursor.
			**/
			virtual void ExecuteNonQuery( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement, bool onCursor ) = 0;
			void ExecuteNonQuery( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement )
			{
				ExecuteNonQuery( commandType, stringStatement, false );
			}

			/**
			 * ExecuteQuery method
			 * Execute NonQuery (Insert, Update, Delete) SQL statements or stored procedures, cached in Database instance.
			 * Executed statement is not parameterized
			 * \param commandType	  Type of the command.
			 * \param stringStatement The string statement.
			 * \param onCursor		  true to on cursor.
			**/
			virtual void ExecuteNonQueryCached( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement, bool onCursor ) = 0;
			void ExecuteNonQueryCached( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement )
			{
				ExecuteNonQueryCached( commandType, stringStatement, false );
			}

			/**
			 * ExecuteQuery method
			 * Execute NonQuery (Insert, Update, Delete) SQL statements or stored procedures. Executed statement is parameterized
			 * \param commandType		 Type of the command.
			 * \param stringStatement    The string statement.
			 * \param vectorOfParameters Vector of parameters that are bound to the SQL statement.
			 * \param onCursor			 true to on cursor.
			**/
			virtual void ExecuteNonQuery( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement, const ParametersVector& vectorOfParameters, bool onCursor ) = 0;
			void ExecuteNonQuery( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement, const ParametersVector& vectorOfParameters )
			{
				ExecuteNonQuery( commandType, stringStatement, vectorOfParameters, false );
			}

			/**
			 * ExecuteQuery method
			 * Execute NonQuery (Insert, Update, Delete) SQL statements or stored procedures, cached in Database instance.
			 * Executed statement is parameterized
			 * \param commandType		 Type of the command.
			 * \param stringStatement    The string statement.
			 * \param vectorOfParameters Vector of parameters that are bound to the SQL statement.
			 * \param onCursor			 true to on cursor.
			**/
			virtual void ExecuteNonQueryCached( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement, const ParametersVector& vectorOfParameters, bool onCursor ) = 0;
			void ExecuteNonQueryCached( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement, const ParametersVector& vectorOfParameters )
			{
				ExecuteNonQueryCached( commandType, stringStatement, vectorOfParameters, false );
			}

			/**
			 * ExecuteQuery method 
			 * Execute Select SQL statements or stored procedures. The statement is not parametrized nor cached in Database instance
			 * \param commandType	  Type of the command.
			 * \param stringStatement The string statement.
			 * \param holdCursor	  true to hold cursor.
			 * \param fetchRows		  The number of fetched rows.
			 * \return Fetched rows as DataSet pointer; throw runtime_error if fails
			**/
			virtual DataSet* ExecuteQuery( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement, bool holdCursor, const unsigned int fetchRows ) = 0;
			DataSet* ExecuteQuery( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement, bool holdCursor )
			{
				return ExecuteQuery( commandType, stringStatement, holdCursor, 0 );
			}
			DataSet* ExecuteQuery( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement )
			{
				return ExecuteQuery( commandType, stringStatement, false, 0 );
			}

			/**
			 * ExecuteQuery method
			 * Execute Select SQL statements or stored procedures, cached in Database instance
			 * \param commandType	  Type of the command.
			 * \param stringStatement The string statement.
			 * \param holdCursor	  The hold cursor.
			 * \param fetchRows		  The fetch rows.
			 * \return Fetched rows as DataSet pointer or throw runtime_error if fails
			**/
			virtual DataSet* ExecuteQueryCached( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement, const bool holdCursor, const unsigned int fetchRows ) = 0;
			DataSet* ExecuteQueryCached( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement, bool holdCursor )
			{
				return ExecuteQueryCached( commandType, stringStatement, holdCursor, 0 );
			}
			DataSet* ExecuteQueryCached( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement )
			{
				return ExecuteQueryCached( commandType, stringStatement, false, 0 );
			}

			/**
			 * Execute Select SQL statements or stored procedures. The statement is parametrized 
			 * \param commandType		 Type of the command.
			 * \param stringStatement    The string statement.
			 * \param vectorOfParameters Vector of parameters that are bound to the SQL statement.
			 * \param holdCursor		 true to hold cursor.
			 * \param fetchRows			 The fetch rows.
			 * \return Fetched rows as DataSet pointer; throw runtime_error if fails
			**/
			virtual DataSet* ExecuteQuery( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement, const ParametersVector& vectorOfParameters, bool holdCursor, const unsigned int fetchRows ) = 0;
			DataSet* ExecuteQuery( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement, const ParametersVector& vectorOfParameters, bool holdCursor )
			{
				return ExecuteQuery( commandType, stringStatement, vectorOfParameters, holdCursor, 0 );
			}
			DataSet* ExecuteQuery( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement, const ParametersVector& vectorOfParameters )
			{
				return ExecuteQuery( commandType, stringStatement, vectorOfParameters, false, 0 );
			}

			/**
			 * Execute Select SQL statements or stored procedures. The SQL statement is parameterized and cached 
			 * \param commandType		 Type of the command.
			 * \param stringStatement    The string statement.
			 * \param vectorOfParameters Vector of parameters that are bound to the SQL statement.
			 * \param holdCursor		 The hold cursor.
			 * \param fetchRows			 The fetch rows.
			 * \return Fetched rows as DataSet pointer; throw runtime_error if fails
			**/
			virtual DataSet* ExecuteQueryCached( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement, const ParametersVector& vectorOfParameters, const bool holdCursor, const unsigned int fetchRows ) = 0;
			DataSet* ExecuteQueryCached( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement, const ParametersVector& vectorOfParameters, bool holdCursor )
			{
				return ExecuteQueryCached( commandType, stringStatement, vectorOfParameters, holdCursor, 0 );
			}
			DataSet* ExecuteQueryCached( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement, const ParametersVector& vectorOfParameters )
			{
				return ExecuteQueryCached( commandType, stringStatement, vectorOfParameters, false, 0 );
			}

			/**
			 * If onCursor=true is passed to ExecuteQuery methods, a client is required to use these methods to release cursor resources
			 * \param checkConn true to check connection.
			**/
			virtual void ReleaseCursor( const bool checkConn ) = 0;
			void ReleaseCursor() {
				ReleaseCursor( true );
			}

			/**
			 * Determines if we can hold the cursor.
			 * \return true if cursor is held , false if not.
			**/
			virtual bool CursorHeld() const {
				return false;
			}

			/**
			 * Rewind cursor to the first position
			**/
			virtual void RewindCursor() = 0;

			/**
			 * Utility method used to pretty print DataSet objects.
			 * \param theDataSet The data set to print.
			**/
			static void DisplayDataSet( const DataSet* theDataSet );

			int getLastNumberofAffectedRows() const {
				return m_LastNumberofAffectedRows;
			}

  			/**@{*/
			/** Used to convert a date/timestamp column to a string format database specific formats. */
			static string DateFormat;
			static string TimestampFormat;
			/**@}*/

			/**
			 * Utility method that converts DataSet parameter objects to following XML format:
			 * <TableName>
			 *	<Record>
			 *	 <Column_Name_n>Column_Value_n<Column_Name_n>
			 *	</Record>
			 * </TableName>
			 *	 
			 * \param theDataSet The DataSet to be converted.
			 * \param doTrimm    (optional) Do trimming on db fields values.
			 * \return null if it fails, else the given data converted to an XML. Throws if *theDataSet=NULL
			**/
			static XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* ConvertToXML( const DataSet* theDataSet, const bool doTrimm = true );
	};
}

#endif // ABSTRACTDATABASE_H
