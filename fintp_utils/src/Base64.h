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

/**	\file Base64.h
	\brief Base64 encode/decode functions
*/
#ifndef BASE64_H
#define BASE64_H

#include "Collections.h"
#include <string>

using namespace std;

namespace FinTP
{
	/**
	\class Base64
	\brief Implement decode and encode function for base64
	\author Horia Beschea
	*/
	class ExportedUtilsObject Base64
	{
		private :
			/// \brief The alphabet to use for encode/decode operations
			static string m_Alphabet;
			/// \brief Character to fill the output when the length isn't 4 multiplier
			static char m_FillChar;
			
		public:
			/** 
			\brief Encodes the given string data
			\param[in] data Input data to encode
			\return Encoded string in base64 format
			*/
			static string encode( const string& data );
			
			/**
			\brief Encodes the first <size> bytes of the given buffer
			\param[in] data The string to be encoded
			\param[in] size Number of characters to be encoded
			\return The encoded string
			*/
			static string encode( const unsigned char* data, const unsigned int size );
				    	    
			/**
			\brief Decodes given data
			\param[in] data Data to be decoded in base64 format
			\return The decoded string
			*/
			static string decode( const string& data );
		    
			/**
			\brief Decodes given data with the specified size
			\param[in] data The string to be decoded
			\param[in] size Number of characters to be decoded
			\return The decoded data
			*/
			static string decode( const unsigned char* data, const unsigned int size );
		    
			/**
			\brief Returns the length of the encoded data
			\param[in] data String to be encoded and return it's length
			\return The length of the encoded data
			*/
			static unsigned int encodedLength( const string& data );
			/**
			\brief Returns the length of the encoded data
			\param[in] data String to be encoded and return it's length
			\param[in] size Number of characters to encode from input string
			\return The length of the encoded data
			*/
			static unsigned int encodedLength( const unsigned char* data, const unsigned int size );
		    
			/**
			\brief Returns the length of the decoded data given as a param
			\param[in] data String to be decoded and return it's length
			\return The length of the decoded data
			*/
			static unsigned int decodedLength( const string& data );
			/**
			\brief Returns the length of the decoded data given as a param
			\param[in] data String to be decoded and return it's length
			'param[in] size Number of characters to decode from input string
			\return The length of the decoded data
			*/
			static unsigned int decodedLength( const unsigned char* data, const unsigned int size );
		    
			/**
			\brief Return \a TRUE if input text is in base64 format, \a FALSE otherwise 
			\param[in] text Input string to be tested
			\return \a TRUE if input text is in base64 format, \a FALSE otherwise
			*/
			static bool isBase64( const string& text );
	};
}

#endif //BASE64_H
