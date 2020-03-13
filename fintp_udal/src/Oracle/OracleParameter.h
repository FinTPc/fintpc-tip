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

#ifndef ORACLEPARAMETER_H
#define ORACLEPARAMETER_H

#include <cstring>
#include <string>
#include <vector>

#include "../DataParameter.h"
#include "Trace.h"

#include <oci.h>

#ifdef WIN32
#define __MSXML_LIBRARY_DEFINED__
#include <windows.h>
#define sleep(x) Sleep( (x)*1000 )
#else
#include <unistd.h>
#endif

using namespace std;

namespace FinTP
{
	/**
	 * Store parameter information used by the Oracle database instance to execute queries (select) or nonqueries (insert, update, delete)
	**/
	template < class T >
	class ExportedUdalObject OracleParameter : public DataParameter< T >
	{
		public :

			/**
			* Constructor
			* \param paramDirection type PARAMETER_DIRECTION. The parameter direction (in, out or inout)
			**/
			explicit inline OracleParameter( DataParameterBase::PARAMETER_DIRECTION paramDirection = DataParameterBase::PARAM_IN )
				: DataParameter< T >( paramDirection ), m_hBind( NULL ), m_IndicatorVariable( 0 ) {}

			/**
			* Constructor
			* \param paramName	type string.					The parameter name
			* \param paramDirection type PARAMETER_DIRECTION	The parameter direction (in, out or inout)
			**/
			explicit inline OracleParameter( const string& paramName, DataParameterBase::PARAMETER_DIRECTION paramDirection = DataParameterBase::PARAM_IN)
				: DataParameter< T >( paramName, paramDirection ), m_hBind( NULL ), m_IndicatorVariable( 0 )  {}

			/**
			 * Copy constructor.
			 * \param source The source object
			**/
			inline OracleParameter( const OracleParameter< T >& source ) {}

			/**
			 * Destructor.
			**/
			inline ~OracleParameter() {}

			/**
			 * Sets a value.
			 * \param columnValue The column value.
			**/
			inline void setValue( T columnValue )
			{
				DEBUG2( "Set value [" << columnValue << "]" );
				DataParameter< T >::m_Value = columnValue;
				if ( DataParameter< T >::m_Dimension == 0 )
					DataParameter< T >::m_Dimension = sizeof( DataParameter< T >::m_Value );
			}

			/**
			 * Gets the value.
			 * \return The value.
			**/
			inline T getValue() const
			{
				DEBUG2( "Get value [" << m_Value << "]" );
				return DataParameter< T >::m_Value;
			}

			/**
			 * Sets a dimension.
			 * \param dimension The dimension.
			**/
			inline void setDimension( const unsigned int dimension )
			{
				DataParameterBase::setDimension( dimension );
			}

			/**
			 * Gets the dimension.
			 * \return The dimension.
			**/
			inline unsigned int getDimension() const
			{
				return DataParameterBase::getDimension();
			}

			inline void push_back( const string& value ) { throw logic_error("Not supported by this data type"); }
			inline const string& getElement( size_t position ) const { throw logic_error("Not supported by this data type"); }

			/**
			 * Gets bind handle.
			 * \return Pointer to a OCIBind*. Used for calls to OCIBindByPos
			**/
			inline void** getBindHandle()
			{
				return ( void ** )&m_hBind;
			}

			void* getIndicatorValue()
			{
				return ( void * )&m_IndicatorVariable;
			}

			bool isNULLValue() const
			{
				return ( m_IndicatorVariable == -1 );
			}

		private :

			OCIBind* m_hBind;
			sb2 m_IndicatorVariable;

			//static int getOracleParameterDirection( DataParameterBase::PARAMETER_DIRECTION paramDirection );
			//static int getOracleParameterType( DataType::DATA_TYPE paramType );
	};

	class ExportedUdalObject OracleArrayParameter: public OracleParameter< vector< string > >
	{
		mutable OCIArray* m_Array;
	public:
		OracleArrayParameter(): m_Array( NULL ) {}
		inline OCIArray** getOCIArray() const { return &m_Array; }
	};

	template <>
	inline DataType::DATA_TYPE DataParameter< vector< string > >::getParamType()
	{
		return DataType::ARRAY;
	}

	template<>
	inline void OracleParameter< vector< string > >::push_back( const string& value )
	{
		m_Value.push_back( value );
	}

	template<>
	inline unsigned int OracleParameter< vector< string > >::getDimension() const
	{
		return m_Value.size();
	}

	template<>
	inline const string& OracleParameter< vector< string > >::getElement( size_t position ) const
	{
		return m_Value[position];
	}

	//override for string
	template<>
	inline unsigned int OracleParameter< string >::getDimension() const
	{
		// DB2 custom hack
		return m_Dimension + 1;
	}

	template<>
	inline void OracleParameter< string >::setDimension( const unsigned int dimension )
	{
		//DEBUG( "Should be equal ( before resize : " ) << m_StoragePointer << " , " << ( void* )m_Value.data() );
		//m_Value.resize( dimension );
		DEBUG2( "Set dimension [" << dimension << "]" );
		DataParameterBase::setDimension( dimension );

		if( m_StoragePointer != NULL )
			delete[] m_StoragePointer;

		m_StoragePointer = new unsigned char[ m_Dimension + 1 ];//( void * )m_Value.data();
		memset( m_StoragePointer, 0, m_Dimension + 1 );
		//DEBUG( "Should be equal ( after resize : " ) << m_StoragePointer << " , " << ( void* )m_Value.data() );
	}

	template<>
	inline void OracleParameter< string >::setValue( string columnValue )
	{
		//DEBUG( "SetValue : "  << columnValue );
		string::size_type paramLength = columnValue.length();
		DEBUG2( "Dimension : [" << m_Dimension << "] Address : [" << m_StoragePointer << "]" );

		if( m_StoragePointer == NULL )
			dynamic_cast< OracleParameter< string > * >( this )->setDimension( paramLength );
		DEBUG2( "Dimension : [" << m_Dimension << "] Address : [" << m_StoragePointer << "]" );

		if( m_Dimension < paramLength )
		{
			TRACE( "Parameter storage size is less than source. Data right truncation from " << paramLength << " chars to " << m_Dimension );
			memcpy( m_StoragePointer, columnValue.c_str(), m_Dimension );
			//( ( unsigned char* )m_StoragePointer )[ m_Dimension - 1 ] = 0;
		}
		else
		{
			DEBUG2( "Storage size ok." );
			memcpy( m_StoragePointer, columnValue.c_str(), paramLength );
		}
	}

	template<>
	inline string OracleParameter< string >::getValue() const
	{
		string returnValue = string( ( char* )m_StoragePointer, m_Dimension );

		//DEBUG( "pValue : "  << ( void* )m_Value.data() );
#ifdef WIN32
		DEBUG2( "Get value : [" << returnValue << "]" );
#endif

		//DEBUG( "*Size : "  << strlen( ( ( char* )m_StoragePointer ) ) );
		//DEBUG( "Should be equal : "  << m_StoragePointer << " , " << ( void* )m_Value.data() );

		return returnValue;
	}

	template <>
	inline OracleParameter< string >::~OracleParameter()
	{
		DEBUG2( "Destructor" );
		if( m_StoragePointer != NULL )
		{
			delete[] m_StoragePointer;
			m_StoragePointer = NULL;
		}
	}
}
#endif //ORACLEPARAMETER_H
