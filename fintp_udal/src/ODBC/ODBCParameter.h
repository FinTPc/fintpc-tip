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

#ifndef ODBCPARAMETER_H
#define ODBCPARAMETER_H

#include "../DataParameter.h"
#include "Trace.h"

#include <string>
#include <vector>
#include <cstring>

#include <sql.h>

using namespace std;

namespace FinTP
{
	/**
	 * Store parameter information used by the database instance to execute queries (select) or nonqueries (insert, update, delete)
	**/
	template < class T >
	class ExportedUdalObject ODBCParameter : public DataParameter< T >
	{
		private:
			SQLLEN m_StrLen_or_IndPtr;
		public :

			// create DB2 parameter, implicit IN parameter
			inline ODBCParameter( DataParameterBase::PARAMETER_DIRECTION paramDirection = DataParameterBase::PARAM_IN )
				: DataParameter< T >( paramDirection ), m_StrLen_or_IndPtr( NULL ) {}

			/**
			* Constructor
			* \param paramName	type string.					The parameter name
			* \param paramDirection type PARAMETER_DIRECTION	The parameter direction (in, out or inout)
			**/
			inline ODBCParameter( const string& paramName, DataParameterBase::PARAMETER_DIRECTION paramDirection = DataParameterBase::PARAM_IN)
				: DataParameter< T >( paramName, paramDirection ) {}

			inline ODBCParameter( const ODBCParameter< T >& source ) {};

			inline ~ODBCParameter() {};

			void* getIndicatorValue() { return reinterpret_cast<void *>(&m_StrLen_or_IndPtr); }

			inline void setValue( T columnValue )
			{
				DataParameter< T >::m_Value = columnValue;
				if ( DataParameter< T >::m_Dimension == 0 )
					DataParameter< T >::m_Dimension = sizeof( DataParameter< T >::m_Value );
			}

			inline void push_back( const string& value ) { throw logic_error("Not supported by this data type"); }
			inline const string& getElement( size_t position ) const { throw logic_error("Not supported by this data type"); }

			inline T getValue() const
			{
				DEBUG2( "Get value [" << DataParameter< T >::m_Dimensionm_Value << "]" );
				return DataParameter< T >::m_Value;
			}

			inline void setDimension( const unsigned int dimension )
			{
				DataParameterBase::setDimension( dimension );
			}

			inline unsigned int getDimension() const
			{
				return DataParameterBase::getDimension();
			}

			bool isNULLValue() const
			{
				return ( m_StrLen_or_IndPtr == SQL_NULL_DATA );
			}
	};


	template<>
	inline void ODBCParameter< vector< string > >::push_back( const string& value )
	{
		m_Value.push_back( value );
	}

	template <>
	inline DataType::DATA_TYPE DataParameter< vector< string > >::getParamType()
	{
		return DataType::ARRAY;
	}

	template <>
	inline unsigned int ODBCParameter< vector< string > >::getDimension() const
	{
		return m_Value.size();
	}

	template <>
	inline const string& ODBCParameter< vector< string > >::getElement(  size_t position ) const
	{
		return m_Value[position];
	}

	template <>
	inline void  ODBCParameter< vector< string > >::setValue( vector< string > columnValue )
	{
		m_Value = columnValue;
		m_Dimension= m_Value.size();
	}

	//override for string
	template<>
	inline unsigned int ODBCParameter< string >::getDimension() const
	{
		// ODBC custom hack
		return m_Dimension + 1;
	}

	template<>
	inline void ODBCParameter< string >::setDimension( const unsigned int dimension )
	{
		DataParameterBase::setDimension( dimension );

		delete[] m_StoragePointer;
		m_StoragePointer = NULL;

		m_StoragePointer = new unsigned char[ m_Dimension + 1 ];
		memset( m_StoragePointer, 0, m_Dimension + 1 );
	}

	template<>
	inline void ODBCParameter< string >::setValue( string columnValue )
	{
		ODBCParameter< string >::setDimension( columnValue.length() );

		memcpy( m_StoragePointer, columnValue.c_str(), m_Dimension );

		switch ( m_Type )
		{
			case DataType::CHAR_TYPE:
			case DataType::LARGE_CHAR_TYPE:
			case DataType::DATE_TYPE:
				m_StrLen_or_IndPtr = SQL_NTS;
				break;
			case DataType::BINARY :
				m_StrLen_or_IndPtr = m_Dimension;
				break;
		}
	}

	template<>
	inline string ODBCParameter< string >::getValue() const
	{
		return string( reinterpret_cast<char*>(m_StoragePointer), m_Dimension );
	}

	template <>
	inline ODBCParameter< string >::~ODBCParameter()
	{
		delete[] m_StoragePointer;
	}
}

#endif // ODBCPARAMETER_H

