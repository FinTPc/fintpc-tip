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

#include "DatabaseProvider.h"
#include <sstream>

#ifdef WITH_ORACLE
#include "Oracle/OracleDatabaseProvider.h"
#endif

#ifdef WITH_ODBC
#include "ODBC/DB2/Db2DatabaseProvider.h"
#include "ODBC/Informix/InformixDatabaseProvider.h"
#include "ODBC/SqlServer/SqlServerDatabaseProvider.h"
#include "ODBC/Postgres/PostgresDatabaseProvider.h"
#endif

#include "Base64.h"
#include "Trace.h"

using namespace FinTP;
// DatabaseProvider implementation
DatabaseProviderFactory* DatabaseProvider::GetFactory( const PROVIDER_TYPE& provider )
{
	switch ( provider )
	{
#ifdef WITH_ORACLE
		case DatabaseProvider::Oracle :
			return new OracleDatabaseFactory();
#endif
#ifdef WITH_ODBC
		case DatabaseProvider::Informix :
			return new InformixDatabaseFactory();

		case DatabaseProvider::SqlServer :
			return new SqlServerDatabaseFactory();

		case DatabaseProvider::Postgres :
			return new PostgresDatabaseFactory();
#endif
		case DatabaseProvider::None :
		{
			stringstream errorMessage;
			errorMessage << "Database provider not set [None]";
			throw logic_error( errorMessage.str() );
		}

		default :
			break;
	}

	stringstream errorMessage;
	errorMessage << "Database provider [" << provider << "] not found.";
	throw invalid_argument( errorMessage.str() );
}

DatabaseProviderFactory* DatabaseProvider::GetFactory( const string& provider )
{
	return GetFactory( Parse( provider ) );
}

DatabaseProvider::PROVIDER_TYPE DatabaseProvider::Parse( const string& providerName )
{
	if( providerName == "Oracle" )
		return DatabaseProvider::Oracle;
	if( providerName == "DB2" )
		return DatabaseProvider::DB2;
	if( providerName == "Informix" )
		return DatabaseProvider::Informix;
	if( providerName == "SqlServer" )
		return DatabaseProvider::SqlServer;
	if( providerName == "Postgres" )
		return DatabaseProvider::Postgres;

	stringstream errorMessage;
	errorMessage << "Database provider [" << providerName << "] not found.";
	throw invalid_argument( errorMessage.str() );
}

string DatabaseProvider::ToString( PROVIDER_TYPE providerType )
{
	switch ( providerType )
	{
		case DatabaseProvider::Oracle :
			return "Oracle";
		case DatabaseProvider::DB2 :
			return "DB2";
		case DatabaseProvider::Fox :
			return "Fox";
		case DatabaseProvider::Informix :
			return "Informix";
		case DatabaseProvider::SqlServer :
			return "SqlServer";
		case DatabaseProvider::Postgres :
			return "Postgres";
		case DatabaseProvider::None :
			return "None";
	}

	stringstream errorMessage;
	errorMessage << "Unknown provider definition [" << providerType << "]";
	return errorMessage.str();
}
