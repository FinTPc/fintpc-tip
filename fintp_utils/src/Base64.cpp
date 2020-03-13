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

#include "Base64.h"
#include <string>
#include <exception>
#include <stdexcept>

#include "Log.h"

using namespace std;
using namespace FinTP;

char Base64::m_FillChar = '=';

string Base64::m_Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

bool Base64::isBase64( const string& text )
{
	if ( text.length() % 4 != 0 )
		return false;
		
	try
	{
		Base64::decode( text );
		return true;
	}
	catch( ... )
	{
		return false;
	}
}

string Base64::encode( const unsigned char* data, const unsigned int size )
{
	unsigned char c;
	
	//aloc output, allow some slack
	string returnValue = "";

	unsigned int indexInOutput = 0;

	// Do not uncomment this.. it may break the output by sending unescaped characters
	//DEBUG_LOG( "Encode unsigned char * : "  << data );
	DEBUG_LOG( "Encode unsigned char * size : " << size );
    for ( unsigned int i = 0; i < size; ++i )
    {
		c = ( data[ i ] >> 2 ) & 0x3f; 	// shift 2right and mask resulted bits
        
		returnValue.push_back( m_Alphabet[ c ] ); 		//lookup char and append
		indexInOutput++;

		c = ( data[i] << 4 ) & 0x3f;
		if ( ++i < size )
			c |= ( data[ i ] >> 4 ) & 0x0f;

		returnValue.push_back( m_Alphabet[ c ] ); 		//lookup char and append
		indexInOutput++;

		if ( i < size )
		{
			c = ( data[ i ] << 2 ) & 0x3f;
			if ( ++i < size )
				c |= ( data[ i ] >> 6 ) & 0x03;

			returnValue.push_back( m_Alphabet[ c ] ); 		//lookup char and append
			indexInOutput++;
		}
		else
		{
			++i;
			returnValue.push_back( Base64::m_FillChar ); 	//append filler
			indexInOutput++;
		}

		if ( i < size )
		{
			c = data[ i ] & 0x3f;
            returnValue.push_back( m_Alphabet[ c ] ); 		//lookup char and append
			indexInOutput++;
        }
        else
        {
            returnValue.push_back( Base64::m_FillChar ); 	//append filler
			indexInOutput++;
        }
    }
	
	DEBUG_LOG( "Encoded" );
    return returnValue;
}

string Base64::decode( const unsigned char* data, const unsigned int size )
{
	unsigned char c, c1;
	string::size_type idx;
	string returnValue = "";

	for( unsigned int i=0; i < size; ++i )
	{
		//DEBUG_LOG( i );
   		
		idx = m_Alphabet.find( data[ i ] );		//lookup char
		if( idx == string::npos )
			throw invalid_argument( "Input data passed to Base64::decode is not a valid base64 text" );
		c = ( unsigned char )idx;
        
		if ( ( i+1 ) >= size )
			throw invalid_argument( "Input data passed to Base64::decode is not a valid base64 text" );
        	
		idx = m_Alphabet.find( data[ ++i ] );	//lookup next char
		if( idx == string::npos )
			throw invalid_argument( "Input data passed to Base64::decode is not a valid base64 text" );
		c1 = ( unsigned char )idx;
		
		c = ( c << 2 ) | ( ( c1 >> 4 ) & 0x3 ); 	//recompose char
		returnValue.push_back( c );
		if ( ++i < size )
		{
			c = data[ i ];
			if ( m_FillChar == c )
				break;

			idx = m_Alphabet.find( c );
			if( idx == string::npos )
				throw invalid_argument( "Input data passed to Base64::decode is not a valid base64 text" );
			c = ( unsigned char )idx;

			c1 = ( ( c1 << 4 ) & 0xf0 ) | ( ( c >> 2 ) & 0xf );
			returnValue.push_back( c1 );
		}

		if ( ++i < size )
		{
			c1 = data[ i ];
			if ( m_FillChar == c1 )
				break;

			idx = m_Alphabet.find( c1 );
			if( idx == string::npos )
				throw invalid_argument( "Input data passed to Base64::decode is not a valid base64 text" );
			c1 = ( unsigned char )idx;
			
			c = ( ( c << 6 ) & 0xc0 ) | c1;
			returnValue.push_back( c );
        }
    }

	return( returnValue );
}

string Base64::decode( const string& data )
{
	DEBUG_LOG( "Encoded string length : " << data.length() );
	//DEBUG_LOG( "Encoded string : ["  << data << "]" );

	string returnString = "";
	returnString = decode( ( unsigned char* )data.c_str(), data.length() );
	
    DEBUG_LOG( "Decoded string : [" << returnString << "]" );
    return returnString;
}

string Base64::encode( const string& data )
{
	DEBUG_LOG( "String to encode initial length : " << data.length() );
	return encode( ( unsigned char* )data.c_str(), data.length() );
}

unsigned int Base64::encodedLength( const string& data )
{
	string temp = encode( data );
	return temp.length();
}

unsigned int Base64::encodedLength( const unsigned char* data, const unsigned int size )
{
	string temp = encode( data, size );
	return temp.length();
}

unsigned int Base64::decodedLength( const string& data )
{
	string temp = decode( data );
	return temp.length();
}

unsigned int Base64::decodedLength( const unsigned char* data, const unsigned int size )
{
	string temp = decode( data, size );
	return temp.length();
}
