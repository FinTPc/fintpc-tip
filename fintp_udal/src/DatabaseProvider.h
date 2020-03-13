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

#ifndef DATABASEPROVIDER_H
#define DATABASEPROVIDER_H

#include "ConnectionString.h"
#include "Database.h"
#include "DataColumn.h"

#include <map>
using namespace std;

/**
 * .
**/
namespace FinTP
{
	/**
	 * Abstract Factory class whose implementations are responsible to provide
	 * all runtime provider specific objects needed to support database related operations
	**/
	class ExportedUdalObject DatabaseProviderFactory
	{
		protected :

			string m_Name;

			explicit DatabaseProviderFactory( const string& providername ) : m_Name( providername ) {}

		public :

			virtual ~DatabaseProviderFactory() {};

			/**
			 * Creates the provider specific database.
			 * \return The new database.
			**/
			virtual Database* createDatabase() = 0;

			/**
			 * Creates a databse provider specific parameter.
			 * \param paramType		 Type of the parameter.
			 * \param paramDirection The parameter direction.
			 * \return The new parameter.
			**/
			virtual DataParameterBase* createParameter( DataType::DATA_TYPE paramType, DataParameterBase::PARAMETER_DIRECTION paramDirection ) = 0;
			DataParameterBase* createParameter( DataType::DATA_TYPE paramType ) {
				return createParameter( paramType, DataParameterBase::PARAM_IN );
			}

			/**
			 * Creates a databse provider specific column.
			 * \param columnType Type of the column.
			 * \param dimension  The dimension.
			 * \param scale		 The scale.
			 * \param name		 The name.
			 * \return The new column.
			**/
			virtual DataColumnBase* createColumn( DataType::DATA_TYPE columnType, unsigned int dimension, int scale, const string& name ) = 0;
			DataColumnBase* createColumn( DataType::DATA_TYPE columnType, unsigned int dimension, int scale ) {
				return createColumn( columnType, dimension, scale, "" );
			}

			/**
			 * Getter method of the provider name.
			 * \return The provider name string representation.
			**/
			string name() const {
				return m_Name;
			}

			/**
			 * Returns a string representing a sql statement parameter placeholder
			 * \param i The bind parameter index.
			 * \return The parameter placeholder.
			**/
			virtual string getParamPlaceholder( const unsigned int i ) const = 0;

			/**
			 * Returns a string representing a sql statement parameter placeholder
			 * \param paramname The bind parameter name.
			 * \return The parameter placeholder.
			**/
			virtual string getParamPlaceholder( const string& paramname ) const = 0;

			/**
			 * Returns sql statement call to a provider specific trim function.
			 * \param value The string used in the trim function call.
			 * \return The trim function call.
			**/
			virtual string getTrimFunc( const string& value ) const = 0;
	};

	/**
	 * Provider class used to create database specific factory.
	 **/
	class ExportedUdalObject DatabaseProvider
	{
		public:

			typedef enum
			{
			    None = 0,
			    DB2 = 1,
			    Oracle = 2,
			    Fox = 3,
			    Informix = 4,
			    SqlServer = 5,
			    Postgres = 6
			} PROVIDER_TYPE;

			/**
			 * Creates specific factory.
			 * \param provider The provider.
			 * \return the factory or throw runtime_error if fails
			**/
			static DatabaseProviderFactory* GetFactory( const PROVIDER_TYPE& provider );

			/**
			 * Overload to factory creation.
			 * \param provider The provider.
			 * \return null if it fails, else the factory.
			**/
			static DatabaseProviderFactory* GetFactory( const string& provider );

			/**
			 * Convert the given provider name to PROVIDER_TYPE representation
			 * \param providerName Name of the provider as string.
			 * \return PROVIDER_TYPE or throw invalid_argument
			**/
			static PROVIDER_TYPE Parse( const string& providerName );

			/**
			 * Convert PROVIDER_TYPE value into a string representation.
			 * \param providerType Type of the provider.
			 * \return providerType as a string.
			**/
			static string ToString( PROVIDER_TYPE providerType );
	};
}

#endif // DATABASEPROVIDER_H
