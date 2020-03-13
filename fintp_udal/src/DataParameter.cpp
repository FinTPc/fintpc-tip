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

#include "Trace.h"
#include "DataParameter.h"

using namespace FinTP;
//ParametersVector implementation
ParametersVector::ParametersVector() : std::vector< DataParameterBase* >(), m_Copy( false )
{
	DEBUG2( ".ctor" );
}

ParametersVector::ParametersVector( const ParametersVector& source ) : std::vector< DataParameterBase* >( source ), m_Copy( true )
{
	DEBUG2( "Copy .ctor" );
}

ParametersVector::~ParametersVector()
{
	DEBUG2( ".dtor" );

	// lazy copy ctor doesn't copy elements
	if( m_Copy )
		return;

	try
	{
		std::vector< DataParameterBase* >::iterator finder;
		for( finder = this->begin(); finder != this->end(); finder++ )
		{
			if ( *finder != NULL )
			{
				delete *finder;
				*finder = NULL;
			}
		}
		DEBUG2( "All parameters deleted" );
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured when deleting parameters" );
		} catch( ... ) {}
	}

	try
	{
		while( this->size() > 0 )
			this->pop_back();
		DEBUG2( "All parameters erased" );
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured when erasing parameters" );
		} catch( ... ) {}
	}
}

void ParametersVector::Dump() const
{
	try
	{
		std::vector< DataParameterBase* >::const_iterator finder;

		TRACE( "----------- Dump of parameters begin ---------- " );
		for( finder = this->begin(); finder != this->end(); finder++ )
		{
			switch( ( *finder )->getType() )
			{
				case DataType::BINARY:
					/*
					//blob parameter implementation related
					TRACE( "\t[" << ( *finder )->getName() << "]; size [" << ( *finder )->getDimension() << "] = [] [binary]" );
					break;
					*/
				case DataType::LARGE_CHAR_TYPE :
				case DataType::CHAR_TYPE :
				case DataType::TIMESTAMP_TYPE :
					if ( ( *finder )->getDirection() == DataParameterBase::PARAM_IN )
					{
						TRACE( "\t[" << ( *finder )->getName() << "]; size [" << ( *finder )->getDimension() << "] = [" << ( *finder )->getString() << "]" );
					}
					else
					{
						TRACE( "\t[" << ( *finder )->getName() << "]; size [" << ( *finder )->getDimension() << "] = [] [output]" );
					}
					break;

				case DataType::SHORTINT_TYPE :
					TRACE( "\t[" << ( *finder )->getName() << "] = [" << ( *finder )->getShort() << "]" );
					break;

				case DataType::LONGINT_TYPE :
				case DataType::NUMBER_TYPE :
					TRACE( "\t[" << ( *finder )->getName() << "] = [" << ( *finder )->getLong() << "]" );
					break;
				case DataType::ARRAY :
					TRACE( "Array parameter dump. Name: [" << ( *finder )->getName() << "]" );
					for ( size_t i=0; i<( *finder )->getDimension(); i++ )
						TRACE( "\t[Element[" << i << "]"  << "] = [" << ( *finder )->getElement(i) << "]" );
					break;

				default :
					TRACE( "\t[" << ( *finder )->getName() << "]" );
					break;
			}
		}
		TRACE( "------------ Dump of parameters end ---------- " );
	}
	catch( ... )
	{
		//Do nothing
	}
}

// DataType implementation
string DataType::ToString( DATA_TYPE type )
{
	switch( type )
	{
		case CHAR_TYPE :
			return "CHAR_TYPE";

		case DATE_TYPE :
			return "DATE_TYPE";

		case TIMESTAMP_TYPE :
			return "TIMESTAMP_TYPE";

		case SHORTINT_TYPE :
			return "SHORTINT_TYPE";

		case LONGINT_TYPE :
			return "LONGINT_TYPE";

		case NUMBER_TYPE :
			return "NUMBER_TYPE";

		case LARGE_CHAR_TYPE :
			return "LARGE_CHAR_TYPE";

		case BINARY :
			return "BINARY";

		default :
			return "undefined data type";
	}
}

//DataParameterBase implementation
void DataParameterBase::setString( string value )
{
	dynamic_cast< DataParameter< string > * >( this )->setValue( value );
}

void DataParameterBase::setInt( int value )
{
	dynamic_cast< DataParameter< long > * >( this )->setValue( value );
}

void DataParameterBase::setLong( long value )
{
	dynamic_cast< DataParameter< long > * >( this )->setValue( value );
}

void DataParameterBase::setShort( short value )
{
	dynamic_cast< DataParameter< short > * >( this )->setValue( value );
}

// getters
int DataParameterBase::getInt()
{
	return dynamic_cast< DataParameter< long > * >( this )->getValue();
}

long DataParameterBase::getLong()
{
	return dynamic_cast< DataParameter< long > * >( this )->getValue();
}

short DataParameterBase::getShort()
{
	return dynamic_cast< DataParameter< short > * >( this )->getValue();
}

string DataParameterBase::getString()
{
	return dynamic_cast< DataParameter< string > * >( this )->getValue();
}

//DbTimestamp implementation
ostream& operator<< ( ostream& os, const DbTimestamp& value )
{
	os << "timestamp";
	return os;
}

DbTimestamp& DbTimestamp::operator=( const DbTimestamp& source )
{
	if ( this == &source )
		return *this;
	return *this;
}

void DbTimestamp::Clear() const
{
}

template class FinTP::DataParameter< string >;
template class FinTP::DataParameter< short >;
template class FinTP::DataParameter< long >;
template class FinTP::DataParameter< DbTimestamp >;
