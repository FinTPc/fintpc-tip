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

#ifndef ORACLECOLUMN_H
#define ORACLECOLUMN_H

#include "StringUtil.h"
#include "../DataColumn.h"
#include "OracleParameter.h"

#include <string>
#include <vector>
#include <map>
using namespace std;

#define MAX_ORACLE_COLUMN_NAME 50

namespace FinTP
{
	template <class T>

	/**
	 * OracleColumn template class Redefine the OracleColumnBase taking in consideration different
	 * data types. 
	**/
	class OracleColumn : public DataColumn< T >
	{
		/**
		 * Constructor.
		 * \param dimension Column dimension.
		 * \param scale	    Column scale.
		 * \param name	    Column name.
		**/
		public :

			OracleColumn( unsigned int dimension, int scale, const string& name = "" );
			~OracleColumn();

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

			/**
			 * Makes a deep copy of this object.
			 * \return A copy of this object.
			**/
			DataColumnBase* Clone();

		private :

			OracleColumn();
	};

	template <>
	inline long OracleColumn< string >::getLong()
	{
		return StringUtil::ParseLong( StringUtil::Trim( m_Value ) );
	}

	template <>
	inline int OracleColumn< string >::getInt()
	{
		return StringUtil::ParseInt( StringUtil::Trim( m_Value ) );
	}

	template <>
	inline short OracleColumn< string >::getShort()
	{
		return StringUtil::ParseShort( StringUtil::Trim( m_Value ) );
	}
	//typedef std::pair< std::string, OracleColumnBase *> OracleResultColumn;
}

#endif // ORACLECOLUMN_H
