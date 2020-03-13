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

#ifndef DATAROW_H
#define DATAROW_H

#include <string>
#include <map>
#include "DataColumn.h"

using namespace std;

namespace  FinTP
{
	/**
	 * Store row information queried from database. Represents a collection of DataColumnBase
	 **/
	class DataRow : public std::map< std::string, DataColumnBase* >
	{
		public:
			/**
			 * Default constructor.
			**/
			DataRow();

			/**
			 * Copy constructor.
			 * \param source. Source of the new set of column.
			**/
			DataRow( const DataRow& source );

			/**
			 * Destructor.
			**/
			~DataRow();

			/**
			 * Empty every column.
			**/
			void Clear();

			/**
			 * Dumps columns informations to log file.
			**/
			void DumpHeader() const;

			/**
			 * Synchronises columns in this object.
			**/
			void Sync();

		private :

			/**
			 * Assignment operator.
			 * \param source Source type DataRow . The data row.
			 * \return A shallow copy of this object.
			**/
			DataRow& operator=( const DataRow& source );
	};

	/**
	 * Defines an alias used when inserting a new cell in a DataRow
	**/
	typedef pair< string, DataColumnBase* > makeColumn;
}

#endif // DATAROW_H
