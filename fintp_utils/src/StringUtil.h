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

#ifndef STRINGUTIL_H
#define STRINGUTIL_H

#include "Collections.h"
#include <string>
#include <limits>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

using namespace std;

namespace FinTP
{
	class ExportedUtilsObject StringUtil
	{
		private :
		
			string m_Separator;
			string m_Data;
			
			string::size_type m_SeparatorIndex;
			
			inline static bool nocase_compare( char c1, char c2 );
			//inline static char to_upper( char c1 );

		public:

			explicit StringUtil( const string& data = "" ) : m_Separator( " " ),  m_Data( data ), m_SeparatorIndex( 0 ) {};
			
			void Split( const string& separator );
			string NextToken();
			bool MoreTokens() const;
			
			
			string Trim() const;
			static string Trim( const string& source, bool allWhitespaces = false );
			
			// conversion functions
			static inline long ParseLong( const string& source ) { return strtol( source.c_str(), NULL, 10 ); }
			static inline unsigned long ParseULong( const string& source ) 
			{
				unsigned long x = strtoul( source.c_str(), NULL, 10 );
				if ( source[ 0 ] == '-' )
					throw boost::bad_lexical_cast( typeid( source ), typeid( x ) );
				return x;
			}

			static inline int ParseInt( const string& source ) { return atoi( source.c_str() ); }
			static inline unsigned int ParseUInt( const string& source ) 
			{
				unsigned int x = ( unsigned int )strtoul( source.c_str(), NULL, 10 );
				if ( source[ 0 ] == '-' )
					throw boost::bad_lexical_cast( typeid( source ), typeid( x ) );
				return x;
			}

			static inline short ParseShort( const string& source ) 
			{
				int x = atoi( source.c_str() );
				if ( ( x < SHRT_MIN ) || ( x > SHRT_MAX ) )
					throw boost::bad_lexical_cast( typeid( source ), typeid( x ) );
				return ( short )x;
			}
			static inline float ParseFloat( const string& source ) { return ( float )atof( source.c_str() ); }
			static inline double ParseDouble( const string& source ){ return strtod( source.c_str(), NULL ); }

			template < class T >
			static inline string ToString( const T value ) { return boost::lexical_cast< string >( value ); }
			
			static string FindBetween( const string& value, const string& first, const string& last );
			static string RemoveBetween( const string& value, const string& first, const string& last );
			static string AddBetween( const string& value, const string& toadd, const string& first, const string& last );
			static string Pad( const string& value, const string& padleft, const string& padright );
			static string Replace( const string& value, const string& what, const string& with );
			static bool StartsWith( const string& value, const string& with );
			
			static string::size_type CaseInsensitiveFind( const string& first, const string& second );
			static string ToUpper( const string& input );
			static string ToLower( const string& input );
			
			static bool IsAlphaNumeric( const string& input );

			static string DeserializeFromFile( const string& filename );
			static void SerializeToFile( const string& filename, const string& content );
	};
}

#endif // STRINGUTIL_H
