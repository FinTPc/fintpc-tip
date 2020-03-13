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

#include "DataRow.h"
#include "Trace.h"

#include <string>
using namespace std;
using namespace FinTP;

DataRow::DataRow() : std::map< std::string, DataColumnBase* >()
{
	DEBUG2( ".ctor" );
}

DataRow::DataRow( const DataRow& source ) : std::map< std::string, DataColumnBase* >()
{
	DEBUG2( "Copy .ctor" );
	std::map< std::string, DataColumnBase* >::const_iterator myIterator = source.begin();

	for ( ; myIterator != source.end(); myIterator++ )
	{
		this->insert( makeColumn( myIterator->first,
		                          ( myIterator->second )->Clone() ) );
	}
}

DataRow::~DataRow()
{
	DEBUG2( ".dtor" );

	try
	{
		std::map< std::string, DataColumnBase* >::iterator finder;
		for( finder = this->begin(); finder != this->end(); finder++ )
		{
			try
			{
				if ( finder->second != NULL )
				{
					delete finder->second;
					finder->second = NULL;
				}
			} catch( ... ) {}
		}
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured when deleting columns" );
		} catch( ... ) {}
	}
	try
	{
		this->clear();
		DEBUG2( "All columns erased" );
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured when erasing columns" );
		} catch( ... ) {}
	}
}

void DataRow::Clear()
{
	map< std::string, DataColumnBase* >::iterator myIterator = this->begin();

	for ( ; myIterator != this->end(); myIterator++ )
	{
		( myIterator->second )->Clear();
	}
}

void DataRow::Sync()
{
	for ( map< std::string, DataColumnBase* >::const_iterator myIterator = this->begin(); myIterator != this->end(); myIterator++ )
		myIterator->second->Sync();
}

void DataRow::DumpHeader() const
{
	map< std::string, DataColumnBase* >::const_iterator myIterator = this->begin();
	int cellDimension = 0;
	string cellType = "";

	DEBUG( "------ start of header -----" )
	for ( ; myIterator != this->end(); myIterator++ )
	{
		if( myIterator->second != NULL )
		{
			cellType = DataType::ToString( myIterator->second->getType() );
			cellDimension = myIterator->second->getDimension();
		}

		DEBUG( "[" << myIterator->first << "] type : " << cellType << " size : " << cellDimension );

		if( myIterator->second != NULL )
			myIterator->second->Sync();
	}
	DEBUG( "------- end of header ------" )
}
