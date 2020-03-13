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

#ifndef INFORMIXDATABASEPROVIDER_H_
#define INFORMIXDATABASEPROVIDER_H_

#include "../ODBCDatabaseProvider.h"

namespace FinTP
{
	/**
	 * InformixDatabaseFactory derived from ODBCDatabaseFactory class to implement specific operations for Informix
	**/
	class ExportedUdalObject InformixDatabaseFactory : public ODBCDatabaseFactory
	{
		public:

			InformixDatabaseFactory() : ODBCDatabaseFactory( "Informix" ) {}

			Database* createDatabase();
			/**
			* convert Db2DataType to and from another DataType
			**/
			static short int getInformixDataType( const DataType::DATA_TYPE type );
			static short int getInformixSqlType( const DataType::DATA_TYPE ifxtype, const int dimension );

			static DataType::DATA_TYPE getDataType( const short int ifxtype, const int dimension );
	};
}

#endif // INFORMIXDATABASEPROVIDER_H_
