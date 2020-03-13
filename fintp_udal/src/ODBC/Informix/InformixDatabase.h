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

#ifndef INFORMIXDATABASE_H
#define INFORMIXDATABASE_H

#include "../ODBCDatabase.h"

#include <pthread.h>

#include <map>
#include <string>
using namespace std;

namespace FinTP
{
	 /** Derived ODBCDatabase class to implement specific operations for Informix Server
		 * 1. First a new InformixDatabase database is created,  
		 * 2. Then, the client can create a new database transaction by calling the BeginTransaction(readonly) method. The connection can be tested any time during  
		 * instace lifecycle using ConnecitonLost() method
		 * 3. Transactions comprise one ore more calls to "ExecuteQuery" methods. Every database query is mapped to a Command instance and  
		 * can be cached in m_StatementCache member of Database instance . The CallFormating(statement,params) function is used for formatting the 
		 * query string when stored procedures are called.
		 * 4. A transaction ends when the client calls EndTransaction(transactionType) performing commit or rollback on the current transaction  
		 *
		 *Detailed Error Information is obtained by calling getErrorInformation( handletype,handle );
		**/
	class ExportedUdalObject InformixDatabase : public ODBCDatabase
	{
		public :

			InformixDatabase():ODBCDatabase() {};
			~InformixDatabase() {};

			void BeginTransaction( const bool readonly );
			void EndTransaction ( const TransactionType::TRANSACTION_TYPE trType, const bool throwOnError );
			//void Connect( const ConnectionString& connectionString );
			void ConnectionLost() {
				m_IsConnected = false;
				m_IsReconecting = true;
			}
			bool IsConnected();

		private :

			//Informix specific
			ConnectionString m_ConnectionString;

			bool m_IsReconecting;

			//DataSet* innerExecuteCommand( const DataCommand& command, ParametersVector vectorOfParameters, const bool useCursor = false, const unsigned int fetchRows = 0 );

			//DataSet* getDataSet( DataCommand& command, const bool isCommandCached, SQLHANDLE* statementHandle, const bool useCursor, const unsigned int fetchRows = 0 );

			/**  
			*  get detailed error information
			**/
			string getErrorInformation( SQLSMALLINT htype, SQLHANDLE handle );

			/**
			* Set Sql environment attribute for ODBC version
			**/
			void setSpecificEnvAttr();
			string callFormating( const string& statementString, const ParametersVector& vectorOfParameters );
	};
}

#endif //INFORMIXDATABASE_H

