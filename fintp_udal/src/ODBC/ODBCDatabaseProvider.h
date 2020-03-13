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

#ifndef ODBCDATABASEPROVIDER_H
#define ODBCDATABASEPROVIDER_H

#include "../DatabaseProvider.h"

namespace FinTP
{
	class ExportedUdalObject ODBCDatabaseFactory : public DatabaseProviderFactory
	{
		/**
		 * Base class for database provider specific objects creations. Factory specific implementations are responsable to provide
		 * every runtime specific object needed to perform database related operations
	    **/
		public:

			ODBCDatabaseFactory( const string& provider ) : DatabaseProviderFactory( provider ) {}
			~ODBCDatabaseFactory() {};

			virtual Database* createDatabase() = 0;

			/**
			 * Create parameter
			 * \param paramType type DATA_TYPE. The parameter data type
			 * \param parameterDirection type PARAMETER_DIRECTION. The parameter direction
			**/
			DataParameterBase* createParameter( DataType::DATA_TYPE paramType, DataParameterBase::PARAMETER_DIRECTION parameterDirection = DataParameterBase::PARAM_IN );
			/**
			 * \note calls internalCreateColumn 
			**/
			DataColumnBase* createColumn( DataType::DATA_TYPE columnType, unsigned int dimension, int scale, const string& name = "" );
			/**
			 * Create column
			 * \param columnType type DATA_TYPE. The column's datatype.
			 * \param dimension  type int.		 The column's dimension.
			 * \param scale      type int.       The column's scale.
			 * \param name       type string.    The column's name.
			**/
			static DataColumnBase* internalCreateColumn( DataType::DATA_TYPE columnType, unsigned int dimension, int scale, const string& name = "" );

			static short int getODBCSqlType( const DataType::DATA_TYPE type, const int dimension );
			static short int getODBCDataType( const DataType::DATA_TYPE type );
			static int getODBCParameterDirection( DataParameterBase::PARAMETER_DIRECTION paramDirection );
			static DataType::DATA_TYPE getDataType( const short int type, const int dimension );

			string getParamPlaceholder( const unsigned int i ) const {
				return "?";
			}
			string getParamPlaceholder( const string& paramname ) const {
				return "?";
			}
			string getTrimFunc( const string& value ) const {
				return string( "LTRIM(RTRIM(" ) + value + string( "))" );
			}

			//void UploadUsingDad( const string& dadFileName, const string& xmlData, Database* currentDatabase, bool usingParams = false );
	};
}

#endif // ODBCDATABASEPROVIDER_H
