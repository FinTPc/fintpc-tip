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

#pragma once

#ifdef WIN32
	#define __MSXML_LIBRARY_DEFINED__
#endif

#include <xercesc/util/XMLString.hpp>
XERCES_CPP_NAMESPACE_USE

class XStrWs
{
	public :
		//  Constructors and Destructor
		XStrWs( const char* toTranscode )
		{
			// Call the private transcoding method
			fUnicodeForm = XMLString::transcode( toTranscode );
		}
	    
		XStrWs( const std::string& toTranscode )
		{
    		// Call the private transcoding method
			fUnicodeForm = XMLString::transcode( toTranscode.c_str() );
		}
		
		~XStrWs()
		{
			try
			{
				XMLString::release( &fUnicodeForm );
			}catch(...){};
		}
	
		//  Getter methods
		const XMLCh* uniForm() const
		{
			return fUnicodeForm;
		}
	    
	private :
		//  Private data members
		//  fUnicodeForm - This is the Unicode XMLCh format of the string.
		XMLCh* fUnicodeForm;
		XStrWs() : fUnicodeForm( NULL ) {};
};

class StrXWs
{
	public :
		// -----------------------------------------------------------------------
		//  Constructors and Destructor
		// -----------------------------------------------------------------------
		StrXWs( const XMLCh* toTranscode )
		{
			// Call the private transcoding method
			fLocalForm = XMLString::transcode( toTranscode );
		}
	
		~StrXWs()
		{
			try
			{
				XMLString::release( &fLocalForm );
			}catch( ... ){};
		}
	
	
		// -----------------------------------------------------------------------
		//  Getter methods
		// -----------------------------------------------------------------------
		const char* locForm() const
		{
			return fLocalForm;
		}
	
	private :
		// -----------------------------------------------------------------------
		//  Private data members
		//
		//  fLocalForm
		//      This is the local code page form of the string.
		// -----------------------------------------------------------------------
		char* fLocalForm;
		StrXWs() : fLocalForm( NULL ) {};
};

#define unicodeFormWs( str ) ( XStrWs( str ).uniForm() )
#define localFormWs( str ) ( StrXWs( str ).locForm() )
