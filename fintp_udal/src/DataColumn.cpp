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

#include "DataColumn.h"
#include "Trace.h"
#include "StringUtil.h"

#include <exception>
#include <stdexcept>

using namespace FinTP;

namespace FinTP
{
	//DataColumn<string> implementation
	template <>
	DataColumn< string >::DataColumn( unsigned int dimension, int scale, const string& name ) :
		DataColumnBase( dimension, scale, name ), m_Value( dimension, 0 )
	{
		m_StoragePointer = NULL;
		if ( dimension > 0 )
			m_StoragePointer = new char[ dimension + 1 ];
		m_Type = DataColumn< string >::getColType();
	}

	template <>
	DataColumn< string >::DataColumn( const DataColumn< string >& source ) :
		DataColumnBase( source.m_Dimension, source.m_Scale, source.m_Name )
	{
		m_StoragePointer = NULL;
		m_Type = source.m_Type;

		if ( source.m_StoragePointer != NULL )
			m_Value = string( ( char* )source.m_StoragePointer );
		else
			m_Value = source.m_Value;
	}

	template <>
	DataColumn< string >& DataColumn< string >::operator=( const DataColumn< string >& source )
	{
		if ( this == &source )
			return *this;

		DataColumnBase::operator=( source );

		if ( m_StoragePointer != NULL )
			delete[] ( char* )m_StoragePointer;
		m_StoragePointer = NULL;
		m_Type = source.m_Type;
		if ( source.m_StoragePointer != NULL )
			m_Value = string( ( char* )source.m_StoragePointer );
		else
			m_Value = source.m_Value;

		return *this;
	}

	template <>
	DataColumn< string >::~DataColumn()
	{
		if ( m_StoragePointer != NULL )
			delete[] ( char* )m_StoragePointer;
		m_StoragePointer = NULL;
	}

	template <>
	void DataColumn< string >::Sync()
	{
		if ( m_StoragePointer != NULL )
			m_Value = string( ( char* )m_StoragePointer );
	}

	template <>
	void DataColumn< string >::setValue( string value )
	{
		m_Value = value;
		if ( m_StoragePointer != NULL )
			delete[] ( char* )m_StoragePointer;
		m_StoragePointer = NULL;
	}

	template <>
	DataColumnBase* DataColumn< string >::Clone()
	{
		Sync();
		DataColumn< string >* copy = new DataColumn< string >( *this );
		return copy;
	}

	template <>
	void DataColumn< string >::Clear()
	{
		if ( m_StoragePointer != NULL)
			memset( m_StoragePointer, 0, m_Dimension+1 );
		m_Value = "";
	}

	template <>
	string DataColumn< string >::Dump()
	{
		Sync();
		return "$"+m_Value;
	}

	template<>
	inline void DataColumn< string >::setDimension( const unsigned int newValue )
	{
		DataColumnBase::setDimension( newValue );

		if ( m_StoragePointer != NULL)
		{
			delete[] ( char* )m_StoragePointer;
			m_StoragePointer = new char[ m_Dimension+1 ];
		}

		m_Value.resize( newValue, 0 );
	}
}

// DataColumn implementation

template < class T >
DataColumn< T >::DataColumn( const DataColumn< T >& source ) :
	DataColumnBase( source.m_Dimension, source.m_Scale, source.m_Name ), m_Value( source.m_Value )
{
	m_StoragePointer = &m_Value;
	m_Type = source.m_Type;
}

template < class T >
DataColumn< T >::DataColumn( unsigned int dimension, int scale, const string& name ) :
	DataColumnBase( dimension, scale, name ), m_Value( 0 )
{
	m_StoragePointer = &m_Value;
	m_Type = DataColumn< T >::getColType();
}

template < class T >
DataColumn< T >& DataColumn< T >::operator=( const DataColumn< T >& source )
{
	if ( this == &source )
		return *this;

	DataColumnBase::operator=( source );

	m_Value = source.m_Value;
	m_StoragePointer = &m_Value;
	m_Type = source.m_Type;

	return *this;
}

template < class T >
DataColumn< T >::~DataColumn()
{
}

template < class T >
void DataColumn< T >::setValue( T value )
{
	m_Value = value;
	m_StoragePointer = &m_Value;
}

template < class T >
void DataColumn< T >::Sync()
{
	// do nothing, only string has some issues
}

template < class T >
T DataColumn< T >::getValue()
{
	Sync();
	return m_Value;
}

template < class T >
DataColumnBase* DataColumn< T >::Clone()
{
	DataColumn< T >* copy = new DataColumn< T >( *this );
	return copy;
}

template < class T >
void DataColumn< T >::Clear()
{
	m_Value = 0;
}

template class FinTP::DataColumn< string >;
template class FinTP::DataColumn< short >;
template class FinTP::DataColumn< long >;

//DataColumnBase implementation

string DataColumnBase::getString()
{
	if ( m_BaseType == DataColumn< string >::getColType() )
		return ( static_cast< DataColumn< string > * >( this ) )->getValue();
	else
		throw runtime_error( "Attempted cast to DataColumn< string > on a non string column." );
}

int DataColumnBase::getInt()
{
	return getLong();
}

short DataColumnBase::getShort()
{
	if ( m_BaseType == DataColumn< short >::getColType() )
		return ( static_cast< DataColumn< short > * >( this ) )->getValue();
	else
		throw runtime_error( "Attempted cast to DataColumn< short > on a non short column." );
}

long DataColumnBase::getLong()
{
	// Ugly Oracle hack
	DataType::DATA_TYPE longType = DataColumn< long >::getColType();
	if ( m_BaseType ==  DataColumn< long >::getColType() )
		return ( static_cast< DataColumn< long > * >( this ) )->getValue();
	if ( m_BaseType ==  DataColumn< string >::getColType() )
		return StringUtil::ParseLong( ( static_cast< DataColumn< string > * >( this ) )->getValue() );

	throw runtime_error( "Attempted cast to DataColumn< long > on a non long column." );
}
