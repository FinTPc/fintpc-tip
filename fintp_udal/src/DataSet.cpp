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

#include "DataSet.h"
#include "Trace.h"

#include <exception>
#include <stdexcept>

using namespace FinTP;

DataSet::DataSet() : std::vector< DataRow* >(), m_Copy( false )
{
	DEBUG2( ".ctor" );
}

DataSet::DataSet( const DataSet& source ) : std::vector< DataRow* >( source ), m_Copy( true )
{
	DEBUG2( "Copy .ctor" );
}

DataSet::~DataSet()
{
	DEBUG2( ".dtor" );

	// Shallow copy instance doesn't own copy elements
	if( m_Copy )
		return;

	try
	{
		std::vector< DataRow* >::iterator finder;
		for( finder = this->begin(); finder != this->end(); finder++ )
		{
			try
			{
				if ( *finder != NULL )
				{
					delete *finder;
					*finder = NULL;
				}
			}
			catch ( ... )
			{
				try
				{
					TRACE( "An error occured while deleting a row in the dataset" );
				} catch( ... ) {}
			}
		}
		DEBUG2( "All rows deleted" );
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured during dataset termination" );
		} catch( ... ) {};
	}

	try
	{
		while( this->size() > 0 )
			this->pop_back();
		DEBUG2( "All rows erased" );
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while erasing rows" );
		} catch( ... ) {};
	}
}

DataColumnBase* DataSet::getCellValue( const unsigned int row, const string& columnName )
{
	DataRow* myRow = NULL;

	if ( row >= this->size() )
		throw invalid_argument( "Row index out of bounds" );

	myRow = ( *this )[ row ];
	if ( myRow == NULL )
		throw runtime_error( "Unable to access row" );

	DataColumnBase* result = NULL;
	try
	{
		result = ( *myRow )[ columnName ];

		if ( result == NULL )
		{
			DEBUG( "Warning : cell empty for request : " << row << ", [" << columnName << "]" );
			myRow->DumpHeader();
		}
	}
	catch( ... )
	{
		TRACE( "Invalid name for column : [" << columnName << "]" );
		throw invalid_argument( "columnName" );
	}

	return result;
}

DataColumnBase* DataSet::getCellValue( const unsigned int row, const unsigned int columnIndex )
{
	DataRow* myRow = NULL;

	if ( row >= this->size() )
		throw invalid_argument( "Row index out of bounds" );

	myRow = ( *this )[ row ];
	if ( myRow == NULL )
		throw runtime_error( "Unable to access row" );

	if ( columnIndex > myRow->size() )
		throw invalid_argument( "Column index out of bounds" );

	string columnName = "";
	map< std::string, DataColumnBase* >::iterator myIterator = myRow->begin();
	for ( unsigned int j = 0; j<=columnIndex; myIterator++, j++ )
	{
		columnName = myIterator->first;
	}

	DataColumnBase* result = NULL;
	try
	{
		result = ( *myRow )[ columnName ];

		if ( result == NULL )
		{
			DEBUG( "Warning : cell empty for request : " <<
			       row << ", " << columnIndex << " (" << columnName << ")" );
			myRow->DumpHeader();
		}
	}
	catch( ... )
	{
		throw invalid_argument( "columnIndex" );
	}

	return result;
}
