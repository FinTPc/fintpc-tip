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

#ifndef DATACOLUMN_H
#define DATACOLUMN_H

#include "Collections.h"
#include "DataParameter.h"

namespace FinTP
{
	class DataCommand;

	/**
	 * Base class inherited by DataColumn template classes. Its purpose is to store database information 
	**/
	class ExportedUdalObject DataColumnBase
	{
		protected :

			string m_Name;

			/**
			 * @brief	The SQL type of the column
			 */
			DataType::DATA_TYPE	m_Type;

			/**
			 * @brief	The type of the storage used for the column. For example:
			 * 			SHORTINT_TYPE for DataColumn< short >
			 * 			CHAR_TYPE for DataColumn< string >
			 * 			LONGINT_TYPE for DataColumn< long >
			 */
			DataType::DATA_TYPE m_BaseType;
			unsigned int m_Dimension;
			int m_Scale;

			/**The storage pointer, handled to API specific routines to store data**/
			void* m_StoragePointer;
			/** The buffer indicator. The indicator buffer is used for communicating the length of data in the data buffer, or to indicate something about the data **/
			long m_BufferIndicator;

		public:

			//.ctor
			DataColumnBase() : m_Name( "" ), m_Type( DataType::INVALID_TYPE ), m_BaseType( DataType::INVALID_TYPE ), m_Dimension( 0 ),
				m_Scale( 0 ), m_StoragePointer( NULL ), m_BufferIndicator( 0 ) {};

			DataColumnBase( unsigned int dimension, int scale, const string& name = "", const DataType::DATA_TYPE dataType = DataType::INVALID_TYPE, const DataType::DATA_TYPE baseType = DataType::INVALID_TYPE ) :
				m_Name( name ), m_Type( dataType ), m_BaseType( baseType ), m_Dimension( dimension ), m_Scale( scale ),
				m_StoragePointer( NULL ), m_BufferIndicator( 0 ) {};

			DataColumnBase( const DataColumnBase& source ) : m_Name( source.m_Name ), m_Type( source.m_Type ), m_BaseType( source.m_BaseType ),
				m_Dimension( source.m_Dimension ), m_Scale( source.m_Scale ), m_StoragePointer( source.m_StoragePointer ), m_BufferIndicator( source.m_BufferIndicator ) {}
			DataColumnBase& operator=( const DataColumnBase& source )
			{
				if ( this == &source )
					return *this;

				if ( m_BaseType != source.m_BaseType )
					throw runtime_error ( "Can't assign DataColumnBase of different storage" );

				m_Name = source.m_Name;
				m_Type = source.m_Type;
				m_Dimension = source.m_Dimension;
				m_Scale = source.m_Scale;
				m_StoragePointer = source.m_StoragePointer;
				m_BufferIndicator = source.m_BufferIndicator;

				return *this;
			}

			virtual ~DataColumnBase()
			{
				if ( m_StoragePointer != NULL )
					m_StoragePointer = NULL;
			};

			void setType( const DataType::DATA_TYPE dataType ) {
				m_Type = dataType;
			}

			/**
			 * Useful for casting to the right DataColumn subtype
			 * \return The type of the column.
			**/
			DataType::DATA_TYPE getType() const {
				return m_Type;
			}

			DataType::DATA_TYPE getBaseType() const {
				return m_BaseType;
			}

			virtual void setDimension( const unsigned int newValue ) {
				m_Dimension = newValue;
			}
			unsigned int getDimension() const {
				return m_Dimension;
			}

			void setName( const string& newName ) {
				m_Name = newName;
			}
			string getName() const {
				return m_Name;
			}

			void setScale( const int newValue ) {
				m_Scale = newValue;
			}
			int getScale() const {
				return m_Scale;
			}

			void* getStoragePointer() {
				return m_StoragePointer;
			}

			virtual DataColumnBase* Clone() {
				return NULL;
			}
			virtual void Clear() {};

			const long* getBufferIndicator() {
				return &m_BufferIndicator;
			}

			virtual string getString();
			virtual int getInt();
			virtual short getShort();
			virtual long getLong();

			virtual string Dump() {
				return "base";
			}

			virtual void Sync() {};
	};

	/**
	 * Intended usage : 	
	 * \code
	 * 	DataColumn<string> myColumn;
	 *	string myValue = myColumn.getValue();
	 *	myColumn.setValue( "value" )
	 *	\endcode
	 * \param T Generic type parameter.
	**/
	template < class T >
	/**
	 * Represents a cell in a DataRow
	 * Intended usage : 	DataColumn< string > myColumn;
	 * string myValue = myColumn.getValue();
	 * myColumn.setValue( "Horia" )
	**/
	class ExportedUdalObject DataColumn : public DataColumnBase
	{
		public :

			// could translate to DataColumn< string >( 100, 10 );
			explicit DataColumn( unsigned int dimension = 0, int scale = 0, const string& name = "" );

			virtual ~DataColumn( );

			virtual void setValue( T value );
			virtual T getValue();

			virtual DataColumnBase* Clone();
			void Clear();

			static DataType::DATA_TYPE getColType();

			virtual void setDimension( const unsigned int newValue )
			{
				DataColumnBase::setDimension( newValue );
			}

			// oracle column will overrride this function to convert numeric format to int
			virtual long getLong() {
				return DataColumnBase::getLong();
			}
			virtual int getInt() {
				return DataColumnBase::getInt();
			}
			virtual short getShort() {
				return DataColumnBase::getShort();
			}

			virtual string Dump() {
				return "derived";
			}
			virtual void Sync();

		protected :

			T m_Value;

		private :

			DataColumn( const DataColumn< T >& source );
			DataColumn< T >& operator=( const DataColumn< T >& source );
	};

	template <>
	inline DataType::DATA_TYPE DataColumn< string >::getColType()
	{
		return DataType::CHAR_TYPE;
		//VARCHAR_TYPE
	}

	template <>
	inline DataType::DATA_TYPE DataColumn< short >::getColType()
	{
		return DataType::SHORTINT_TYPE;
	}

	template <>
	inline DataType::DATA_TYPE DataColumn< long >::getColType()
	{
		return DataType::LONGINT_TYPE;
	}
}

#endif // DATACOLUMN_H
