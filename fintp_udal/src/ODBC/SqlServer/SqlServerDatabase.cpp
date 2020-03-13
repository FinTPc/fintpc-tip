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

#include "SqlServerDatabase.h"
#include "SqlServerDatabaseProvider.h"

using namespace FinTP;

void SqlServerDatabase::setSpecificEnvAttr()
{
	SQLRETURN cliRC = SQLSetEnvAttr( m_Henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, 0);
	if ( cliRC != SQL_SUCCESS )
	{
		stringstream errorMessage;
		errorMessage << "SqlServer::Connect - set SQLSetEnvAttr : " << getErrorInformation( SQL_ATTR_ODBC_VERSION, m_Henv );

		// nonfatal err
		TRACE( errorMessage.str() );
	}
}

void SqlServerDatabase::setSpecificConnAttr()
{
	/*	SQLRETURN cliRC = SQLSetConnectAttr( m_Hdbc, SQL_ATTR_ODBC_CURSORS, SQL_CUR_USE_ODBC, SQL_NTS );
		if ( cliRC != SQL_SUCCESS )
		{
			stringstream errorMessage;
			errorMessage << "SqlServer::Connect - set SQLSetConnectAttr : " << getErrorInformation( SQL_ATTR_ODBC_VERSION, m_Henv );

			// nonfatal err
			TRACE( errorMessage.str() );
		}*/
}

string SqlServerDatabase::callFormating( const string& statementString, const ParametersVector& vectorOfParameters )
{
	// Compose the Stored Procedure CALL statement
	// TODO:  Verify the SP CALL format
	stringstream paramBuffer;
	paramBuffer << "{call " << statementString;
	unsigned int paramCount = vectorOfParameters.size();
	if( paramCount != 0 )
		paramBuffer << "( ";

	for ( unsigned int i=0; i<paramCount; i++ )
	{
		paramBuffer << " ? ";
		if ( i < paramCount - 1 )
			paramBuffer << ",";
	}
	if( paramCount != 0 )
		paramBuffer << ")";
	paramBuffer << "}";

	return paramBuffer.str();
}

void SqlServerDatabase::BeginTransaction(const bool readonly)
{
	if ( m_HdbcAllocated )
	{
		DEBUG( "Setting connection attribute " );
		SQLRETURN  cliRC = SQLSetConnectAttr( m_Hdbc, SQL_ATTR_AUTOCOMMIT, ( SQLPOINTER )SQL_AUTOCOMMIT_OFF, SQL_NTS );
		if ( cliRC != SQL_SUCCESS )
		{
			stringstream errorMessage;
			errorMessage << "SqlServer::BeginTransaction - set SQL_ATTR_AUTOCOMMIT : " << getErrorInformation( SQL_HANDLE_DBC, m_Hdbc );

			TRACE( errorMessage.str() )
		}
	}
}

void SqlServerDatabase::EndTransaction(const FinTP::TransactionType::TRANSACTION_TYPE trType, const bool throwOnError)
{
	DEBUG( "Ending transaction ..." );

	int transactionType = getODBCTransactionType( trType );

	SQLRETURN cliRC = SQLEndTran( SQL_HANDLE_DBC, m_Hdbc, transactionType );
	if ( cliRC != SQL_SUCCESS )
	{
		stringstream errorMessage;
		errorMessage << "SqlServer::EndTransaction - end transaction : " << getErrorInformation( SQL_HANDLE_DBC, m_Hdbc );

		TRACE( errorMessage.str() );
	}
}
