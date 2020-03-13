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

#ifdef WIN32
	#define __MSXML_LIBRARY_DEFINED__
#endif

#include "LogPublisher.h"
#include <sstream>
#include <iostream>
#include <fstream>
#include <string>

#include "Trace.h"
#include <stdexcept>
//#include "../MQ/TransportHelper.h"

using namespace std;
using namespace FinTP;
/// FileLogPublisher implementation
FileLogPublisher::FileLogPublisher( const string& filename ) : AbstractLogPublisher(), m_Filename( filename )
{		
	try
	{
		// check access rights
		ofstream fileOut;
		fileOut.open( filename.data(), ofstream::out | ofstream::app );
		if( !fileOut )
			throw runtime_error( "File open exception" );
		fileOut.close();
	}
	catch( ... )
	{
		// can't open file.. .can't do anything
		stringstream errorMessage;
		errorMessage << "Unable to open file [" << filename << "] for output. Check access rights." ;
		throw AppException( errorMessage.str() );
	}
}

FileLogPublisher::~FileLogPublisher()
{
}
		
void FileLogPublisher::Publish( const AppException& except )
{
	if ( except.getAdditionalInfo() != NULL )
		DEBUG( "Publish to File add info ex : " << except.getAdditionalInfo()->getCount() << " additional info" );
		
	string formattedException;
	
	try
	{
		formattedException = FormatException( except );
	}
	catch( const std::exception& ex )
	{
		TRACE( "Unable to format exception. " << ex.what() );
		if ( !m_Default )
			throw AppException( "Unable to format exception", ex );
		else
			return;
	}
	catch( ... )
	{
		TRACE( "Unable to format exception. [unknown error]" );
		if ( !m_Default )
			throw AppException( "Unable to format exception. [unknown error]" );
		else
			return;
	}
	
	try
	{
		//open log file
		ofstream fileOut;
		fileOut.open( m_Filename.data(), ofstream::out|ofstream::app );
		
		if ( !fileOut )
		{
			if( m_Default )
				return;
			else
				throw runtime_error( "File open exception" );
		}
			
		//append xml object to log file	
		fileOut << formattedException << endl ;
		fileOut.close();	
	}
	catch( ... )
	{
		// can't open file.. .can't do anything
		stringstream errorMessage;
		errorMessage << "Unable to open/write file [" << m_Filename << "]. Check access rights and disk space." ;
		
		if ( !m_Default )
			throw AppException( errorMessage.str() );
	}

//	DEBUG( "Publish to FILE END" );
}		

/// ScreenLogPublisher
void ScreenLogPublisher::Publish( const AppException& except ) 
{
	cerr << " *** Exception : " << except.getMessage() << endl;
	cerr << " *** Exception type : " << except.getEventType().ToString() << endl;
	if ( except.getAdditionalInfo() != NULL )
		cerr << " *** Exception additional info : " << except.getAdditionalInfo()->getCount() << endl;
	else
		cerr << " *** Exception additional info : 0" << endl;
	string formattedException;
	
	try
	{
		formattedException = FormatException( except );
	}
	catch( const std::exception& ex )
	{
		TRACE( "Unable to format exception. " << ex.what() );
		if ( !m_Default )
			throw AppException( "Unable to format exception", ex );
		else
			return;
	}
	catch( ... )
	{
		TRACE( "Unable to format exception. [unknown error]" );
		if ( !m_Default )
			throw AppException( "Unable to format exception. [unknown error]" );
		else
			return;
	}
	
	cerr << formattedException << endl;
}
