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

#ifndef DATASET_H
#define DATASET_H

#include "DataRow.h"
#include <map>
#include <vector>

using namespace std;

namespace FinTP
{	
	/**
	 * Store result set information retrieved from the database
	**/
	class ExportedUdalObject DataSet : public std::vector< DataRow* >
	{
		public:
			DataSet();
			~DataSet();

			/**
			 * Gets cell value by name
			 * \param row		 The row index.
			 * \param columnName Name of the column.
			 * \return null if it fails, else the cell value.
			**/
			DataColumnBase* getCellValue( const unsigned int row, const string& columnName );

			/**
			 * Gets cell value by index
			 * \param row		  The row index.
			 * \param columnIndex Zero-based index of the column.
			 * \return null if it fails, else the cell value.
			**/
			DataColumnBase* getCellValue( const unsigned int row, const unsigned int columnIndex );

	private :
			/**
			 * Shallow copy. Instance own a reference to source dataset
			 * \param source	type DataSet.	Source instance containing dataset information
			 * \return A copy of this object.
			**/
			DataSet( const DataSet& source );
			DataSet& operator=( const DataSet& source );

			/**True if instance is a copy.**/
			bool m_Copy;
	};
}

#endif // DATASET_H
