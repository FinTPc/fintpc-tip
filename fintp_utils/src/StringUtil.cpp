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

#include "StringUtil.h"

#include <errno.h>
#include <algorithm>
#include <locale>
#include <iostream>
#include <sstream>
#include <exception>
#include <stdexcept>
#include <fstream>

#include <boost/lexical_cast.hpp>

using namespace FinTP;

void StringUtil::Split( const string& separator )
{
	m_Separator = separator;
	m_SeparatorIndex = 0;
}

string StringUtil::NextToken()
{			
	string crtToken = "";
	
	// return empty string if we reached the end of the string
	if( m_SeparatorIndex == string::npos )
		return crtToken;
		
	string::size_type nextSeparator = m_Data.find_first_of( m_Separator, m_SeparatorIndex );
		
	// last token is special substr
	if ( nextSeparator == string::npos )
	{
		crtToken = m_Data.substr( m_SeparatorIndex );
		m_SeparatorIndex = string::npos;
	}
	else
	{
		crtToken = m_Data.substr( m_SeparatorIndex, nextSeparator - m_SeparatorIndex );
		
		if ( nextSeparator != m_Data.length() )
			m_SeparatorIndex = nextSeparator + 1;
		else
			m_SeparatorIndex = string::npos;
	}
		
	return crtToken;
}

bool StringUtil::MoreTokens() const
{
	return ( m_SeparatorIndex != string::npos );
}

string StringUtil::Trim() const
{
	return Trim( m_Data );
}

string StringUtil::Trim( const string& source, bool allWhitespaces )
{
	if ( source.size() == 0 )
		return "";

	string whitespaces = allWhitespaces?" \n\r\t":" ";
				
	string::size_type first_terminator = source.find_first_of( '\0' );
	
	string tempData = source;
	
	// limit string size to the first terminator
	if ( first_terminator != string::npos )
		tempData = source.substr( 0, first_terminator );

	string::size_type first_nonspace = tempData.find_first_not_of( whitespaces );
	string::size_type last_nonspace = tempData.find_last_not_of( whitespaces );

	// no nonspace char found
	if( first_nonspace == string::npos )
		return "";
		
	if( last_nonspace == string::npos )
		last_nonspace = tempData.length() - 1;
		
	if ( last_nonspace >= first_nonspace )
	{
		//cout << "Returning : [" << tempData.substr( first_nonspace, last_nonspace - first_nonspace + 1 ) << "]" << endl;
		return tempData.substr( first_nonspace, ( last_nonspace - first_nonspace ) + 1 );
	}
	else
	{
		return "";
	}
}

string StringUtil::RemoveBetween( const string& value, const string& first, const string& last )
{
	int firstPosition = value.find( first );
	if( firstPosition<0 )
		throw runtime_error( "Index out of bounds ( first )" );

	int lastPosition = value.find( last );
	if ( lastPosition<0 )
		throw runtime_error( "Index out of bounds ( last )" );

	string bufferMessage = value.substr( 0,firstPosition + first.length() );
	bufferMessage += value.substr( lastPosition,value.length() );

	return bufferMessage;
}
string StringUtil::AddBetween( const string& value, const string& toadd, const string& first, const string& last )
{
	int firstPosition = value.find( first );
	if ( firstPosition<0 )
		throw runtime_error( "Index out of bounds ( first )" );

	int lastPosition = value.find( last );
	if ( lastPosition<0 )
		throw runtime_error( "Index out of bounds ( last )" );

	string bufferMessage = value.substr( 0, firstPosition + first.length() );
	bufferMessage += toadd;
	bufferMessage += value.substr( lastPosition, value.length() );

	return bufferMessage;
}

string StringUtil::FindBetween( const string& value, const string& first, const string& last )
{
	string::size_type start = value.find( first );
	if( start == string::npos )
		throw runtime_error( "Index out of bounds ( first )" );
	string::size_type end = value.find( last, start );
	if ( end == string::npos )
		throw runtime_error( "Index out of bounds ( last )" );
	
	return value.substr( start + first.size(), ( end - start ) - first.size() );
}

string StringUtil::Pad( const string& value, const string& padleft, const string& padright )
{
	return padleft + value + padright;
}

string StringUtil::Replace( const string& value, const string& what, const string& with )
{
	string::size_type off = value.find( what );

	string tempString( value );
	while( off != string::npos )
	{
		tempString = tempString.replace( off, what.length(), with, 0, with.length() );
		off = tempString.find( what, off + with.length() );
	}
	//DEBUG_LOG_GLOBAL( "Replaced [" << what << "] with [" << with << "]" );
	return tempString;
}

bool StringUtil::StartsWith( const string& value, const string& with )
{
	string::size_type off = value.find( with );
	return ( off == 0 );	
}

string::size_type StringUtil::CaseInsensitiveFind( const string& first, const string& second )
{
	string::const_iterator pos = search( first.begin(), first.end(), second.begin(), second.end(), nocase_compare );

	if ( pos == first.end() )
		return string::npos;
	return ( pos - first.begin() );
}

string StringUtil::ToUpper( const string& input )
{
	string result;

	string::const_iterator inputWalker = input.begin();
	while( inputWalker != input.end() )
	{
		result.push_back( ( char )toupper( *inputWalker ) );
		inputWalker++;
	}

	return result;
}

void StringUtil::SerializeToFile( const string& filename, const string& content )
{
	ofstream destFile;
	destFile.open( filename.c_str(), ios::out );
	destFile.write( content.c_str(), content.length() );
	destFile.close();
}

string StringUtil::DeserializeFromFile( const string& filename )
{
	string returnValue = "";

	// read expected output
	ifstream sourceFile;

	sourceFile.open( filename.c_str(), ios::in );
	int errCode = errno;
	if ( ( !sourceFile ) || !sourceFile.is_open() )
	{
		stringstream errorMessage;
#ifdef CRT_SECURE
		char errBuffer[ 95 ];
		strerror_s( errBuffer, sizeof( errBuffer ), errCode );
		errorMessage << "Can't open source file [" << filename << "]. Error code : " << errCode << " [" << errBuffer << "]" << endl;
#else
		errorMessage << "Can't open source file [" << filename << "]. Error code : " << errCode << " [" << strerror( errCode ) << "]" << endl;
#endif	
		throw runtime_error( errorMessage.str() );
	}
	
	bool start = true;
	string line;
	while ( getline( sourceFile, line ) )
	{
		if ( start )
			start = false;
		else
			returnValue += string( "\n" );
		returnValue += line;
		line="";
	}
	sourceFile.close();	

	return returnValue;
}

string StringUtil::ToLower( const string& input )
{
	string result;
	
	string::const_iterator inputWalker = input.begin();
	while( inputWalker != input.end() )
	{
		result.push_back( ( char )tolower( *inputWalker ) );
		inputWalker++;
	}
	return result;
}

bool StringUtil::IsAlphaNumeric( const string& input )
{
	string::const_iterator inputWalker = input.begin();
	while( inputWalker != input.end() )
	{
		if( !isalnum( *inputWalker ) )
			return false;
		inputWalker++;
	}
	return true;
}

inline bool StringUtil::nocase_compare( char c1, char c2 )
{
    return toupper( c1 ) == toupper( c2 );
}

/*inline char StringUtil::to_upper( char c1 )
{
	return ( char )toupper( c1 );
}*/
