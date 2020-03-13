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

#include "PostgresDatabaseProvider.h"
#include "PostgresDatabase.h"
#include "Trace.h"
#include <iostream>
#include <string>
#include <sstream>

using namespace std;
using namespace FinTP;

Database* PostgresDatabaseFactory::createDatabase()
{
	DEBUG( "Creating new ODBC database definition..." );
	Database* retDatabase = NULL;
	try
	{
		retDatabase = new PostgresDatabase();
		if ( retDatabase == NULL )
			throw runtime_error( "Unable to create databse defintion" );
	}
	catch( ... )
	{
		if ( retDatabase != NULL )
			delete retDatabase;
		throw;
	}
	return retDatabase;
}
