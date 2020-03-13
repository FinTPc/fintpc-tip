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

#ifndef ORACLEDATABASEPROVIDER_H
#define ORACLEDATABASEPROVIDER_H

#include "../DatabaseProvider.h"
#include "../DataParameter.h"

#include <sstream>

#define MAX_ORACLE_DATE_DIMENSION 70
#define MAX_ORACLE_NUMBER_DIMENSION 30

namespace FinTP
{
	class OracleDatabaseFactory : public DatabaseProviderFactory
	{
		/**
		 * Store informations used by the Database instances.
		**/
		public:
			OracleDatabaseFactory() : DatabaseProviderFactory( "Oracle" ) {}

			Database* createDatabase();
			/**
			 * Create parameter
			 * \param paramType type DATA_TYPE. The parameter data type
			 * \param parameterDirection type PARAMETER_DIRECTION. The parameter direction
			**/
			DataParameterBase* createParameter( DataType::DATA_TYPE paramType, DataParameterBase::PARAMETER_DIRECTION parameterDirection );
					
			/**
			 * \note calls internalCreateColumn 
			**/
			DataColumnBase* createColumn( DataType::DATA_TYPE columnType, unsigned int dimension, int scale, const string& name );
			
			/**
			 * Create column
			 * \param columnType type DATA_TYPE. The column's datatype.
			 * \param dimension  type int.		 The column's dimension.
			 * \param scale      type int.       The column's scale.
			 * \param name       type string.    The column's name.
			**/
			static DataColumnBase* internalCreateColumn( DataType::DATA_TYPE columnType, unsigned int dimension, int scale, const string& name );

			/**
			 * Gets the Oracle SQL type as defined in Oracle Call Interface.
			 * \param type	    The internal datatype .
			 * \param dimension Only applicable to DataType::CHAR_TYPE. If larger than 4000 SQLT_LNG will be returned, else SQLT_STR
			 * \return The Oracle SQL type.
			 * \note Only used once in OracleDatabase.cpp.
			**/
			static unsigned short int getOracleSqlType( const DataType::DATA_TYPE type, const int dimension );

			/**
			 * \note Very similar to getOracleSqlType but sometimes returns different. Why?
			**/
			static unsigned short int getOracleDataType( const DataType::DATA_TYPE type );

			/**
			 * \note Returns 0. Useless function?
			**/
			static int getOracleParameterDirection( DataParameterBase::PARAMETER_DIRECTION paramDirection );

			/**
			 * Gets internal datatype
			 * \note dimension isn't used
			 * \param type	    The Oracle SQL type as defined in Oracle Call Interface.
			 * \param dimension Irelevant parameter
			 * \return The data type.
			**/
			static DataType::DATA_TYPE getDataType( const short int type, const int dimension );

			string getParamPlaceholder( const unsigned int i ) const
			{
				std::stringstream retval;
				retval << ":" << i;
				return retval.str();
			}

			string getParamPlaceholder( const string& paramname ) const
			{
				std::stringstream retval;
				retval << ":" << paramname;
				return retval.str();
			}

			string getTrimFunc( const string& value ) const {
				return string( "TRIM(" ) + value + string( ")" );
			}
	};
}

#endif // OracleDATABASEPROVIDER_H
