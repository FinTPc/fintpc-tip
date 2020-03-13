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

#include "ConnectionString.h"
#include <string>
#include "Trace.h"

using namespace std;
using namespace FinTP;

ConnectionString::ConnectionString( const string& dbName, const string& userName, const string& userPassword)
{
	m_DatabaseName = dbName;
	m_UserName = userName;
	m_UserPassword = userPassword;

	if( m_DatabaseName.length() > 0 )
	{
		DEBUG( "Database name [" << m_DatabaseName << "]" );
	}
	if( m_UserName.length() > 0 )
	{
		DEBUG( "Username [" << m_UserName << "]" );
	}
	if ( m_UserPassword.length() > 0 )
	{
		DEBUG2( "Password [" << m_UserPassword << "]" );
	}
}

ConnectionString& ConnectionString::operator=( const ConnectionString &source )
{
	if ( this == &source )
		return *this;

	m_DatabaseName = source.getDatabaseName();
	m_UserName = source.getUserName();
	m_UserPassword = source.getUserPassword();

	return *this;
}
