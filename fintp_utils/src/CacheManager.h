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

#ifndef CACHEMANAGER_H
#define CACHEMANAGER_H

#include <string>
#include <map>
#include <sstream>
#include "WorkItemPool.h"

using namespace std;

// Scenario : Add to cache
// // assume member CacheManager< int, DataParameterBase > m_ParamCache of DataStatement class
// // DataStatement statement;
// // in statement.AddParameter( const unsigned int index, DataParameterBase param )
// 
// m_ParamCache.Add( index, param );

// Scenario : Get from cache
//
// DataParamBase param;
// param.setDimension( statement.ParamCache[ index ].getDimension() );

// Scenario : Get from cache, or add and return
// // in statement.AddParameter( const unsigned int index, DataParameterBase param )
//
// if ( m_ParamCache.Contains( index ) )
//		...
// else
//		...

namespace FinTP
{
	template< class K, class V >
	class CacheManager
	{
		private :
			map< K, V > m_Cache;

		public:

			typedef typename map< K, V >::const_iterator const_iterator;
			const_iterator begin() const { return m_Cache.begin(); }
			const_iterator end() const { return m_Cache.end(); }

			CacheManager()
			{
				//INIT_COUNTER( CACHE_MISSES );
				//INIT_COUNTER( CACHE_HITS );
			}

			virtual ~CacheManager()
			{
				/*try
				{
					DESTROY_COUNTER( CACHE_MISSES );
				}catch( ... ){};
				try
				{
					DESTROY_COUNTER( CACHE_HITS );
				}catch( ... ){};*/
			}

			void Add( const K& key, V value )
			{
				( void )m_Cache.insert( pair< K, V >( key, value ) );
			}
			
			map< K, V >& data() 
			{
				return m_Cache;
			}

			void clear()
			{
				m_Cache.clear();
			}

			unsigned int size() const
			{
				return m_Cache.size();
			}

			bool Contains( const K& key )
			{
				typename map< K, V >::const_iterator cacheFinder = m_Cache.find( key );
				typename map< K, V >::const_iterator cacheEnd = m_Cache.end();
				bool retValue = ( cacheFinder != cacheEnd );
				/*if ( retValue )
					INCREMENT_COUNTER( CACHE_HITS );
				else
					INCREMENT_COUNTER( CACHE_MISSES );*/

				return retValue;
			}

			const V& operator[]( const K& key ) const
			{
				typename map< K, V >::const_iterator cacheFinder = m_Cache.find( key );
				typename map< K, V >::const_iterator cacheEnd = m_Cache.end();
				if( cacheFinder != cacheEnd )
					return cacheFinder->second;
				
				stringstream errorMessage;
				errorMessage << "Value not found in cache [" << key << "]";
				throw logic_error( errorMessage.str() );
			}
	};
}

#endif //CACHEMANAGER_H
