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

#ifndef DATAPARAMETER_H
#define DATAPARAMETER_H

#include "DllMainUdal.h"
#include "Collections.h"

#include <stdexcept>
#include <string>
using namespace std;

namespace FinTP
{
	/**
	 * Contains an enum of all the internal datatypes used by udal 
	 * and a static helper function that returns a friendly name of the datatype
	**/
	class ExportedUdalObject DataType
	{
		public :

			/**
			 * Database parameter type.
			**/
			typedef enum
			{
			    // An enum constant representing the invalid type option
			    INVALID_TYPE,
				// An enum constant representing the char type option
			    CHAR_TYPE,
			    // An enum constant representing the date type option
			    DATE_TYPE,
				//An enum constant representing the timestamp type option
			    TIMESTAMP_TYPE,
			    //An enum constant representing the shortint type option
			    SHORTINT_TYPE,
				//An enum constant representing the longint type option
			    LONGINT_TYPE,
			    //An enum constant representing the number type option
			    NUMBER_TYPE,
				//An enum constant representing the large character type option
			    LARGE_CHAR_TYPE,
			    //An enum constant representing the binary option
			    BINARY,
				//An enum constant representing the cursortype option
			    CURSOR_TYPE,
				ARRAY

			} DATA_TYPE;

			/**
			 * returns a friendly name of the datatype.
			 * \param type	DATA_TYPE Datatype instance containing type information.
			 * \return type as a string.
			**/
			static string ToString( DATA_TYPE type );
	};
	
	/**
	 * Store information used by the database timestamp instance to write the timestamp
	**/
	class DbTimestamp
	{
		public :

			void Clear() const;

			DbTimestamp& operator=( const DbTimestamp& source );
			friend ostream& operator<< ( ostream& os, const DbTimestamp& value );
	};

	class DataParameterBase;
	template < class T > class DataParameter;

	/**
	* Store parameter information used by the database instance to execute queries (select) or nonqueries (insert, update, delete)
	**/
	class ExportedUdalObject DataParameterBase
	{
		public :			
			typedef enum
			{
				// An enum constant representing the PARAM_IN direction option
				PARAM_IN,
				// An enum constant representing the PARAM_Out direction option
				PARAM_OUT,
				// An enum constant representing the PARAM_INOUT direction option
				PARAM_INOUT
			} PARAMETER_DIRECTION;

		protected :

			string m_Name;
			PARAMETER_DIRECTION m_Direction;
			DataType::DATA_TYPE m_Type;
			unsigned int m_Dimension;
			unsigned char *m_StoragePointer;

		public:

			/**
			*  Constructor 
			* \param paramDirection	type PARAMETER_DIRECTION	The type of parameter 
			**/
			explicit inline DataParameterBase( PARAMETER_DIRECTION paramDirection = DataParameterBase::PARAM_IN ) :
				m_Name( "" ), m_Direction( paramDirection ), m_Type( DataType::INVALID_TYPE ), m_Dimension( 0 ), m_StoragePointer( NULL ) {}

			/**
			 * Constructor
			 * \param paramName	type string		The parameter name
			 * \param paramType	type DATA_TYPE	The parameter datatype
			 * \param paramDirection	type PARAMETER_DIRECTION	The parameter direction 
			**/
			inline DataParameterBase( const string &paramName, DataType::DATA_TYPE paramType, PARAMETER_DIRECTION paramDirection = DataParameterBase::PARAM_IN, const unsigned int dimension = 0 ) :
				m_Name( paramName ), m_Direction( paramDirection ), m_Type( paramType ), m_Dimension( dimension ), m_StoragePointer( NULL ) {}
			
			/**
			* Constructor
			* \param source	type DataParameterBase	Source instance containing new paramter information
			**/
			inline DataParameterBase( const DataParameterBase& source ) : m_Name( source.m_Name ), m_Direction( source.m_Direction ),
				m_Type( source.m_Type ), m_Dimension( source.m_Dimension ), m_StoragePointer( source.m_StoragePointer ) {}
			
			inline DataParameterBase& operator=( const DataParameterBase& source )
			{
				if ( this == &source )
					return *this;

				m_Name = source.m_Name;
				m_Direction = source.m_Direction;
				m_Type = source.m_Type;
				m_Dimension = source.m_Dimension;
				m_StoragePointer = source.m_StoragePointer;

				return *this;
			}

			/**
			* Destructor
			**/
			virtual ~DataParameterBase()
			{
				if ( m_StoragePointer != NULL )
					m_StoragePointer = NULL;
			};

			inline void setName( const string& name ) {
				m_Name = name;
			}

			inline string getName() const {
				return m_Name;
			}

			inline void setDirection( const PARAMETER_DIRECTION paramDirection ) {
				m_Direction = paramDirection;
			}
			
			inline PARAMETER_DIRECTION getDirection() const {
				return m_Direction;
			}

			inline void setType( const DataType::DATA_TYPE dataType ) {
				m_Type = dataType;
			}

			inline DataType::DATA_TYPE getType() const {
				return m_Type;
			}

			inline void* getStoragePointer() {
				return m_StoragePointer;
			}

			inline const void* getReadStoragePointer() const {
				return m_StoragePointer;
			}

			inline virtual void** getBindHandle() {
				return NULL;
			}

			virtual void* getIndicatorValue() = 0;

			inline virtual void setDimension( const unsigned int dimension ) {
				m_Dimension = dimension;
			}

			inline virtual unsigned int getDimension() const {
				return m_Dimension;
			}

			virtual bool isNULLValue() const = 0;

			inline virtual void push_back( const string& value ) { throw logic_error("Not supported by this data type"); }
			inline virtual const string& getElement( size_t position ) const { throw logic_error("Not supported by this data type"); }

			virtual void setString( string value );
			void setInt( int value );
			void setLong( long value );
			void setShort( short value );
			virtual void setDate( string value ) {
				setString( value );
			}
			//void setBlob( blob value );

			int getInt();
			long getLong();
			short getShort();
			string getString();
	};

	template < class T >
	/**
	 * Parameter class provides database query parameters as objects
	**/
	class DataParameter : public DataParameterBase
	{
		public :
			/**
			* Constructor
			* \params parameterDirection type	PARAMETER_DIRECTION	The type of the parameter (in, out , inout)
			**/
			explicit inline DataParameter( PARAMETER_DIRECTION paramDirection = DataParameterBase::PARAM_IN ) :
				DataParameterBase( paramDirection )
			{
				m_StoragePointer = ( unsigned char * )&m_Value;
				m_Type = DataParameter< T >::getParamType();
			}
			
			/**
			* Constructor
			* \param paramName type String   The parameter name
			* \param paramDirection type PARAMETER_DIRECTION The parameter direction 
			**/
			explicit inline DataParameter( const string& paramName, PARAMETER_DIRECTION paramDirection = DataParameterBase::PARAM_IN ) :
				DataParameterBase( paramName, DataParameter< T >::getParamType(), paramDirection )
			{
				m_StoragePointer = ( unsigned char * )&m_Value;
				m_Type = DataParameter< T >::getParamType();
			}

			/**
			* Constructor with parameter
			* \param source	type DataParameter
			**/
			inline DataParameter( const DataParameter< T >& source ) : DataParameterBase( source )
			{
				m_Value = source.getValue();
				m_StoragePointer = ( unsigned char * )&m_Value;
				m_Type = DataParameter< T >::getParamType();
			}

			/**
			* Stores the parameter type, value and dimension. 
			* \param source type DataParameter The source instance
			* \return A copy of this object
			***/
			inline DataParameter< T >& operator=( const DataParameter< T >& source )
			{
				if ( this == &source )
					return *this;

				DataParameterBase::operator=( source );

				m_Value = source.m_Value;
				m_StoragePointer = ( unsigned char * )&m_Value;
				m_Type = DataParameter< T >::getParamType();

				return *this;
			}

			virtual void setValue( T newValue ) = 0;
			virtual T getValue() const = 0;

			inline virtual void setDimension( const unsigned int dimension )
			{
				m_Dimension = dimension;
			}

			static DataType::DATA_TYPE getParamType();

		protected :

			T m_Value;
	};

	// custom overrides
	/*template <>
	void inline DataParameter< DbTimestamp >::setString( string value )
	{

	}*/

	template <>
	inline DataType::DATA_TYPE DataParameter< string >::getParamType()
	{
		return DataType::CHAR_TYPE;
		//VARCHAR_TYPE
	}

	template <>
	inline DataType::DATA_TYPE DataParameter< DbTimestamp >::getParamType()
	{
		return DataType::TIMESTAMP_TYPE;
	}

	template <>
	inline DataType::DATA_TYPE DataParameter< short >::getParamType()
	{
		return DataType::SHORTINT_TYPE;
	}

	template <>
	inline DataType::DATA_TYPE DataParameter< long >::getParamType()
	{
		return DataType::LONGINT_TYPE;
	}

	/**
	* Constructor
	* \param parameterDirection	type PARAMETER_DIRECTION.	The direction of the parameter
	**/
	template <>
	inline DataParameter< string >::DataParameter( PARAMETER_DIRECTION paramDirection ) : DataParameterBase( paramDirection )
	{
		m_StoragePointer = NULL;
		m_Type = DataParameter< string >::getParamType();
	}

	/**
	* Constructor
	* \param paramName	type string	The parameter name
	* \param paramDirection	type PARAMETER_DIRECTION	The parameter direction
	**/
	template <>
	inline DataParameter< string >::DataParameter( const string& paramName, PARAMETER_DIRECTION paramDirection ) :
		DataParameterBase( paramName, DataParameter< string >::getParamType(), paramDirection )
	{
		m_StoragePointer = NULL;
		m_Type = DataParameter< string >::getParamType();
	}

	/**
	* Constructor
	*\param source	type DataParameter
	**/
	template <>
	inline DataParameter< string >::DataParameter( const DataParameter< string >& source ) : DataParameterBase( source )
	{
		DataParameter< string >::setDimension( source.getDimension() );

		if( m_StoragePointer != NULL )
			delete[] m_StoragePointer;

		m_StoragePointer = NULL;
		m_Value = source.m_Value;
		m_Type = DataParameter< string >::getParamType();
	}

	/**
	*\param source type DataParameter
	**/
	template <>
	inline DataParameter< string >& DataParameter< string >::operator=( const DataParameter< string >& source )
	{
		if ( this == &source )
			return *this;

		DataParameter::operator=( source );

		m_Value = source.m_Value;
		m_StoragePointer = NULL;
		m_Type = DataParameter< string >::getParamType();

		return *this;
	}

	/**
	* \note commented code block
	**/
	/*
	//blob parameter implementation related
	//blob specialization
	template<>
	class DataParameter< blob > : public DataParameterBase
	{
		protected :

			blob m_Value;

	public:

		inline DataParameter< blob >::DataParameter( PARAMETER_DIRECTION paramDirection ) : DataParameterBase( paramDirection )
		{
			m_StoragePointer = NULL;
			m_Type = DataParameter< blob >::getParamType();
		}

		inline DataParameter< blob >::DataParameter( const string& paramName, PARAMETER_DIRECTION paramDirection ) :
		DataParameterBase( paramName, DataParameter< string >::getParamType(), paramDirection )
		{
			m_StoragePointer = NULL;
			m_Type = DataParameter< blob >::getParamType();
		}

		inline DataParameter< blob >::DataParameter( const DataParameter< blob >& source ) : DataParameterBase( source )
		{
			DataParameter< blob >::setDimension( source.getDimension() );
			//!!! asta nu sterge m_StoragePointer din source (vezi DataParameterBase(source)
			if( m_StoragePointer != NULL )
				delete[] m_StoragePointer;

			m_StoragePointer = NULL;
			m_Value = source.m_Value;
			m_Type = DataParameter< string >::getParamType();
		}

		inline DataParameter< blob >& DataParameter< blob >::operator=( const DataParameter< blob >& source )
		{
			if ( this == &source )
				return *this;

			DataParameter::operator=( source );

			m_Value = source.m_Value;
			m_StoragePointer = NULL;
			m_Type = DataParameter< string >::getParamType();

			return *this;
		}

		inline DataType::DATA_TYPE DataParameter< blob >::getParamType()
		{
			return DataType::BINARY;
		}

		virtual void setValue( blob newValue ) = 0;
		virtual blob getValue() const = 0;

		inline void DataParameter< blob >::setString( string source )
		{
			blob strBlob( source.begin(), source.end());
			this->setValue( strBlob );
		}

	};
	*/

	/**
	 * Stores parameters used for database calls
	**/
	class ExportedUdalObject ParametersVector : public std::vector< DataParameterBase* >
	{
		public :
			ParametersVector();
			~ParametersVector();

			/**
			 * Dumps the name, size and value for each parameter to the log file
			**/
			void Dump() const;

		private :
			ParametersVector( const ParametersVector& source );
			ParametersVector& operator=( const ParametersVector& source );
			bool m_Copy;
	};
}

#endif // DBPARAMETER_H
