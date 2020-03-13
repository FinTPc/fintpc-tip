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

#include "OracleColumn.h"
#include <iostream>
#include <exception>
#include <string>

using namespace std;

#include "Trace.h"

namespace FinTP
{
/// OracleColumn Implementation
//
//  - OracleColumnBase Class
//	- OracleColumn 	Class

/// OracleColumn implementation implementation
template < class T >
OracleColumn< T >::OracleColumn( unsigned int dimension, int scale, const string& name ) :
	DataColumn< T >( dimension, scale, name )
{
	DataColumn< T >::m_BaseType = DataType::INVALID_TYPE;
}

template <>
OracleColumn< short >::OracleColumn( unsigned int dimension, int scale, const string& name ) :
	DataColumn< short >( dimension, scale, name )
{
	m_BaseType = DataColumn< short >::getColType();
}

template <>
OracleColumn< long >::OracleColumn( unsigned int dimension, int scale, const string& name ) :
	DataColumn< long >( dimension, scale, name )
{
	m_BaseType = DataColumn< long >::getColType();
}

template <>
OracleColumn< string >::OracleColumn( unsigned int dimension, int scale, const string& name ) :
	DataColumn< string >( dimension, scale, name )
{
	m_BaseType = DataColumn< string >::getColType();
}

template < class T >
OracleColumn< T >::~OracleColumn()
{

}

template < class T >
void OracleColumn< T >::setValue( T value )
{
	DataColumn< T >::setValue( value );
}

template < class T >
T OracleColumn< T >::getValue()
{
	return DataColumn< T >::getValue();
}

template < class T >
DataColumnBase* OracleColumn< T >::Clone()
{
	OracleColumn< T >* copy = new OracleColumn< T >( DataColumn< T >::m_Dimension, DataColumn< T >::m_Scale, DataColumn< T >::m_Name );

	copy->setType( DataColumn< T >::m_Type );
	copy->setValue( DataColumn< T >::m_Value );

	return copy;
}
}
