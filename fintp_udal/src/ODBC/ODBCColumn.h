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

#ifndef ODBCCOLUMN_H
#define ODBCCOLUMN_H

#include "StringUtil.h"
#include "../DataColumn.h"
#include "ODBCParameter.h"

#include <string>
#include <vector>
#include <map>
using namespace std;

#define MAX_ODBC_COLUMN_NAME 50

namespace FinTP
{
	template <class T>
	/**
	 * ODBCColumn template class Redifine the ODBCColumnBase taking in considerration different
	 * data types. Store ODBC column information 
	**/
	class ODBCColumn : public DataColumn< T >
	{
		public :

			/**
			* Constructor
			* \param dimension type int	The column dimension
			* \param scale type int	The column scale
			* \param name type string	The column name
			**/ 
			ODBCColumn( unsigned int dimension, int scale, const string name = "" );
			~ODBCColumn();

			void setValue( T value );
			T getValue();

			int getInt()
			{
				return DataColumn< T >::getInt();
			}

			long getLong()
			{
				return DataColumn< T >::getLong();
			}

			short getShort()
			{
				return DataColumn< T >::getShort();
			}
			//copy column
			DataColumnBase* Clone();
			void Sync();
	};

	template <>
	inline long ODBCColumn< string >::getLong()
	{
		return StringUtil::ParseLong( m_Value );
	}

	template <>
	inline int ODBCColumn< string >::getInt()
	{
		return StringUtil::ParseInt( m_Value );
	}

	template <>
	inline short ODBCColumn< string >::getShort()
	{
		return StringUtil::ParseShort( m_Value );
	}
	//typedef std::pair< std::string, Db2ColumnBase *> ODBCResultColumn;
}

#endif // ODBCCOLUMN_H
