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

#ifndef COLLECTIONS_H
#define COLLECTIONS_H

#include "DllMainUtils.h"

#include <map>
#include <vector>
#include <string>

using namespace std;

namespace FinTP
{
	typedef pair< std::string, std::string > DictionaryEntry;

	//EXPIMP_TEMPLATE template class ExportedUtilsObject std::vector< DictionaryEntry >;

	///<summary>
	/// Holds a collection of key-value pairs
	/// Keys and values can only be strings
	///</summary>
	class ExportedUtilsObject NameValueCollection
	{
		private :
			vector< DictionaryEntry > m_Data;
			//map<std::string, std::string, noorder<std::string> > m_Data;
			
		public :
			
			// .ctor & destructor
			NameValueCollection();		
			NameValueCollection( const NameValueCollection& source );
			NameValueCollection& operator=( const NameValueCollection& source );
			
			~NameValueCollection();
			
			// operator overrides
			const string operator[]( const string& name ) const;
			DictionaryEntry& operator[]( const unsigned int index );
			const DictionaryEntry& operator[]( const unsigned int index ) const;
			void ChangeValue( const string& name, const string& value );
			
			// public methods
			void Add( const string& name, const string& value );
			void Add( const string& name, const char* valueCh );
			void Add( const char* nameCh, const string& value );
			void Add( const char* nameCh, const char* valueCh );
							
			void Remove( const string& name );
			void Clear();
			
			bool ContainsKey( const string& name ) const;
			
			const vector< DictionaryEntry >& getData() const { return m_Data; }
			
			void Dump() const;
			string ToString() const;

			//vector< DictionaryEntry > getData() const { return m_Data; }
			
			// accessors		
			inline unsigned int getCount() const { return m_Data.size(); }
			inline unsigned int getCapacity() const { return m_Data.max_size(); }

			string ConvertToXml( const string& rootName ) const;
	};
}

#endif //COLLECTIONS_H
