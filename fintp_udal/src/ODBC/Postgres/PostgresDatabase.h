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

#ifndef POSTGRESDATABASE_H
#define POSTGRESDATABASE_H

#include "../ODBCDatabase.h"

namespace FinTP
{
	class ExportedUdalObject PostgresDatabase : public ODBCDatabase
	{
		public :
			PostgresDatabase() : ODBCDatabase() {}
			~PostgresDatabase() {};

			DataSet* ExecuteQuery( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement, bool holdCursor, const unsigned int fetchRows );
			DataSet* ExecuteQueryCached( const DataCommand::COMMAND_TYPE commType, const string& stringStatement, const bool onCursor, const unsigned int fetchRows );

			DataSet* ExecuteQuery( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement, const ParametersVector& vectorOfParameters, bool holdCursor, const unsigned int fetchRows );
			DataSet* ExecuteQueryCached( const DataCommand::COMMAND_TYPE commandType, const string& stringStatement, const ParametersVector& vectorOfParameters, const bool holdCursor, const unsigned int fetchRows );

		private :

			DataSet* ExecuteQuery( const DataCommand& query, const ParametersVector& vectorOfParameters, bool holdCursor, const unsigned int fetchRows );

			string callFormating( const string& statementString, const ParametersVector& vectorOfParameters );
	};
}

#endif // POSTGRESDATABASE_H
