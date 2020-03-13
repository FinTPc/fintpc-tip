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

#include "DataCommand.h"

//explicit template instantiation
//EXPIMP_TEMPLATE template class ExportedUdalObject CacheManager< unsigned int, DataParameterBase >;
//EXPIMP_TEMPLATE template class ExportedUdalObject CacheManager< unsigned int, DataColumnBase >;

using namespace FinTP;

DataCommand::DataCommand() : m_CommandType( DataCommand::SP ), m_QueryType( DataCommand::NONQUERY ),
	m_StatementString( "" ), m_ModifiedStatementString( "" ), m_Cacheable( false )
{
}

DataCommand::DataCommand( const DataCommand::COMMAND_TYPE commandType, const DataCommand::QUERY_TYPE queryType,
                          const string& statementString, const bool cacheable, const string& modStatementString ) :
	m_CommandType( commandType ), m_QueryType( queryType ), m_StatementString( statementString ),
	m_ModifiedStatementString( modStatementString ), m_Cacheable( cacheable )
{
}

DataCommand::DataCommand( const DataCommand& source ) :
	m_CommandType( source.m_CommandType ), m_QueryType( source.m_QueryType ),
	m_StatementString( source.m_StatementString ), m_ModifiedStatementString( source.m_ModifiedStatementString ),
	m_Cacheable( source.m_Cacheable )
{
	for ( unsigned int j=0; j<source.getResultColumnCount(); j++ )
	{
		addResultColumn( j, &( source.getResultColumn( j ) ) );
	}
}

DataCommand& DataCommand::operator=( const DataCommand& source )
{
	if ( this == &source )
		return *this;

	m_CommandType = source.m_CommandType;
	m_QueryType = source.m_QueryType;
	m_StatementString = source.m_StatementString;
	m_ModifiedStatementString = source.m_ModifiedStatementString;
	m_Cacheable = source.m_Cacheable;

	for ( unsigned int j=0; j<source.getResultColumnCount(); j++ )
	{
		addResultColumn( j, &( source.getResultColumn( j ) ) );
	}

	return *this;
}

DataCommand::~DataCommand()
{
}
