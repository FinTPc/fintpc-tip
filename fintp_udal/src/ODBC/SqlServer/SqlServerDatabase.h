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

#ifndef SQLSERVERDATABASE_H
#define SQLSERVERDATABASE_H

#include "../ODBCDatabase.h"

namespace FinTP
{
	/** SqlServerDatabase derived ODBCDatabase class to implement specific operations for Sql Server
		 * 1. First a new SqlServerDatabase database is created,  
		 * 2. Then, client can create a new database transaction by calling the BeginTransaction(readonly) method. The connection can be tested any time during  
		 * instace lifecycle using ConnecitonLost() method
		 * 3. Transactions comprise one ore more calls to "ExecuteQuery" methods. Every database query is mapped to a Command instance and  
		 * can be cached in m_StatementCache member of Database instance . The CallFormating(statement,params) function is used for formatting the 
		 * query string when stored procedures are called.
		 * 4. A transaction ends when the client calls EndTransaction(transactionType) performing commit or rollback on the current transaction  
		 *
		 *Detailed Error Information is obtained by calling getErrorInformation( handletype,handle );
		**/
	class ExportedUdalObject SqlServerDatabase : public ODBCDatabase
	{
		public :
			SqlServerDatabase() : ODBCDatabase() {}
			~SqlServerDatabase() {};

			/**
			*SQLServer overrides transaction support
			**/
			void BeginTransaction( const bool readonly );
			void EndTransaction( const TransactionType::TRANSACTION_TYPE transactionType, const bool throwOnError );

		private :
			/**
			* set Sql environment attribute for specific odbc version
			**/
			void setSpecificEnvAttr();
			/**
			* Set ODBC Cursors
			* \note does nothing (bc commented code)
			**/
			void setSpecificConnAttr();
			/**
			* Format input string for calling stored procedures
			**/
			string callFormating( const string& statementString, const ParametersVector& vectorOfParameters );
	};
}

#endif // SQLSERVERDATABASE_H
