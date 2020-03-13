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

#ifndef DATACOMMAND_H
#define DATACOMMAND_H

#include "DllMainUdal.h"
#include "CacheManager.h"
#include "DataParameter.h"
#include "DataColumn.h"

namespace FinTP
{
	/**
	 * Command class implements objectual mapping of a database query.
	 * Every database query is parsed and stored in data member of this class
	 * If Command is cachable it will be retained in a Database cache for subsequent use 
	**/
	class ExportedUdalObject DataCommand
	{
		public :

			/**
			 * Database query type passed as a command type to be executed
			**/
			typedef enum
			{
			    /** An enum constant representing the Stored Procedure option
			    * Command statement string is a stored procedure name
			    **/
			    SP,
				/** An enum constant representing Query Statement option
			    * Command statement string  is the actual query ( ex : select * from ... ) 
			    **/
			    INLINE,

			    /** An enum constant representing the Cursor option
			    * Informix open a Cursor to execute Query Statement
			    * \note Why not use QUERY with SP irespectiv INLINE options
				**/
			    CURSOR

			} COMMAND_TYPE;
 
			typedef enum
			{
			    /** Enum constant representing option of executing a query command ( select ) **/
			    QUERY,

			    /** Enum constant representing the option of executing a nonquery command ( update, delete, insert ) **/
			    NONQUERY
			} QUERY_TYPE;

			/**
			 * Returns a friendly textual name of the COMMAND_TYPE values.
			 * \param type The type of the command as defined by COMMAND_TYPE type
			 * \return COMMAND_TYPE as a string.
			**/
			static string ToString( COMMAND_TYPE type );

		public:

			DataCommand();

			/**
			 * Constructor.
			 * \param commandType		 Type of the command.
			 * \param queryType			 Type of the query.
			 * \param statementString    The statement string.
			 * \param cacheable			 (optional) true to cache Command.
			 * \param modStatementString (optional) the modifier statement string.
			**/
			DataCommand( const DataCommand::COMMAND_TYPE commandType, const DataCommand::QUERY_TYPE queryType,
			             const string& statementString, const bool cacheable = false, const string& modStatementString = "" );
			DataCommand( const DataCommand& source );

			/**
			 * Assign operator.
			 * \param source Source for the data command.
			 * \return A shallow copy of this object.
			**/
			DataCommand& operator=( const DataCommand& source );

			/**
			* DataCommand destructor
			**/
			~DataCommand();

		private :

			DataCommand::COMMAND_TYPE m_CommandType;
			DataCommand::QUERY_TYPE m_QueryType;

			/** The SQL statement string of a Command**/
			string m_StatementString;
			/** The modified SQL statement string of a Command  
			 *  m_Statement string ca be modified to reflect additional option sent along with the SQL statement to ExecuteQuery methods.
			 *  See Database::ExecuteQuery() Database::ExecuteNonQuery()
			**/
			string m_ModifiedStatementString;

			/** True if command is cacheable. Default value set by .ctor is false**/
			bool m_Cacheable;

			/**
			 * Data member provided to store result set columns description.
			**/
			CacheManager< unsigned int, DataColumnBase > m_ResultColumns;

		public :
			/** 
			 * \Return the command type
			 **/
			DataCommand::COMMAND_TYPE getCommandType() const {
				return m_CommandType;
			}
			
			/** 
			 * Set Command Type Method
			**/
			void setCommandType( const DataCommand::COMMAND_TYPE commandType ) {
				m_CommandType = commandType;
			}

			/**
			* \Return the query type
			 **/ 
			DataCommand::QUERY_TYPE getQueryType() const {
				return m_QueryType;
			}
			/**
			*\Return true if the DataCommand is a select query and false otherwise
			**/
			bool isQuery() const {
				return ( m_QueryType == DataCommand::QUERY );
			}

			/**
			*Set query type
			**/
			void setQueryType( const DataCommand::QUERY_TYPE queryType ) {
				m_QueryType = queryType;
			}

			/**
			* \Return the modified statement string
			**/
			string getModifiedStatementString() const {
				return m_ModifiedStatementString;
			}

			/**
			*Set the modified statement string
			**/
			void setModifiedStatementString( const string& modStatementString ) {
				m_ModifiedStatementString = modStatementString;
			}

			/**
			* \return statement string
			**/
			string getStatementString() const {
				return m_StatementString;
			}

			/**
			* \Return the cacheable option
			**/
			bool isCacheable() const {
				return m_Cacheable;
			}

			/** 
			* \Set the cacheable option
			**/
			void setCacheable( const bool cacheable ) {
				m_Cacheable = cacheable;
			}

			/** 
			* Get result set indexed column 
			* \param int index is the column position
			* \return the result column on the idex position
			**/
			const DataColumnBase& getResultColumn( unsigned int index ) const {
				return m_ResultColumns[ index ];
			}

			/**
			* \return result set column number
			**/
			unsigned int getResultColumnCount() const {
				return m_ResultColumns.size();
			}

			/**
			* Add a result column to m_ResultColumns
			* \param type int index position of the resultcolumn
			* \param type DataColumnBase column to be added  
			**/
			void addResultColumn( const unsigned int index, const DataColumnBase* column )
			{
				m_ResultColumns.Add( index, DataColumnBase( column->getDimension(), column->getScale(), column->getName(), column->getType() ) );
			}
	};
}

#endif // DATACOMMAND_H
