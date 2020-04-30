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

#ifndef WIN32
#include <unistd.h>
#endif
#include <string.h>
#include <errno.h>
#include <sstream>
#include <string>
#include <exception>
#include <stdexcept>

#include "Log.h"
#include "Collaboration.h"
#include "FileUtils.h"

using namespace std;
using namespace FinTP;

size_t FileUtils::readCallbackMemory( void *data, size_t size, size_t nmemb, void *readBuffer )
{
	uploadBuffer* writeBuffer = ( struct uploadBuffer* )readBuffer;
	size_t bufferSize = size*nmemb;

	if( writeBuffer->m_SizeLeft ) 
	{
		size_t copyLength = writeBuffer->m_SizeLeft;
		if( copyLength > bufferSize )
			copyLength = bufferSize;
		memcpy( data, writeBuffer->m_ReadPtr, copyLength );

		writeBuffer->m_ReadPtr += copyLength;
		writeBuffer->m_SizeLeft -= copyLength;
		return copyLength;
	}
	return 0;
}

void FileUtils::RenameFile( const string& sourceFilename, string& destinationFilename )
{
#ifndef WIN32
	if ( access( destinationFilename.c_str(), F_OK ) == 0 )
	{
		destinationFilename.append( Collaboration::GenerateGuid() );
		DEBUG_LOG( "Attempting to rename output file because destination already exists. New name is [" << destinationFilename << "]" );
	}
#endif
	// rename source ( may fail ! )
	if( rename( sourceFilename.c_str(), destinationFilename.c_str() ) != 0 )
	{
		int errCode = errno;
		stringstream errorMessage;
		
#ifdef CRT_SECURE
		char errBuffer[ 95 ];
		strerror_s( errBuffer, sizeof( errBuffer ), errCode );
		errorMessage << "Rename [" << sourceFilename << "] -> [" << destinationFilename << "] failed with code : " << errCode << " " << errBuffer;
#else
		errorMessage << "Rename [" << sourceFilename << "] -> [" << destinationFilename << "] failed with code : " << errCode << " " << strerror( errCode );
#endif	
		TRACE_LOG( errorMessage.str() );
#ifdef WIN32
		if( errCode == EEXIST )
		{
			destinationFilename.append( Collaboration::GenerateGuid() );
			DEBUG_LOG( "Attempting to rename output file because destination already exists. New name is [" << destinationFilename << "]" );
			
			if( rename( sourceFilename.c_str(), destinationFilename.c_str() ) != 0 )
			{
				errorMessage.str("");
				errorMessage.clear();
				errCode = errno;
#ifdef CRT_SECURE
				char errBuffer2[ 95 ];
				strerror_s( errBuffer2, sizeof( errBuffer2 ), errCode );
				errorMessage << "Second attempt to rename [" << sourceFilename << "] -> [" << destinationFilename << "] failed with code : " << errCode << " " << errBuffer2;
#else
				errorMessage << "Second attempt to rename [" << sourceFilename << "] -> [" << destinationFilename << "] failed with code : " << errCode << " " << strerror( errCode );
#endif	
				throw runtime_error( errorMessage.str() );
			}
		}
		else
#endif
		throw runtime_error( errorMessage.str() );
	}

}
void FileUtils::SecureUploadFile( const string& uri, const string& buffer )
{
	CURL * connsHandle;
	CURLcode code;


	char errorBuffer[CURL_ERROR_SIZE];
	errorBuffer[0] = 0;

	connsHandle = curl_easy_init();
	if ( connsHandle == NULL )
	{
		curl_easy_cleanup( connsHandle );
		throw runtime_error( "Failed to create CURL connection" );
	}

	curl_easy_setopt( connsHandle, CURLOPT_VERBOSE, 1L );

	code = curl_easy_setopt( connsHandle, CURLOPT_ERRORBUFFER, errorBuffer );
	if ( code != CURLE_OK )
	{
		stringstream errorMessage;
		errorMessage << "Failed to set error buffer [" << code << "] error [" << errorBuffer << "]";

		curl_easy_cleanup( connsHandle );
		throw runtime_error( errorMessage.str() );
	}
	code = curl_easy_setopt( connsHandle, CURLOPT_SSH_AUTH_TYPES, CURLSSH_AUTH_PASSWORD );
	if ( code != CURLE_OK )
	{
		stringstream errorMessage;
		errorMessage << "Failed to set SSH Password Authentification [" << code << "] error [" << errorBuffer << "]";

		curl_easy_cleanup(connsHandle);
		throw runtime_error(errorMessage.str());
	}
	code = curl_easy_setopt( connsHandle, CURLOPT_URL, uri.c_str() );
	if ( code != CURLE_OK )
	{
		stringstream errorMessage;
		errorMessage << "Failed to set URL code [" << code << "] error [" << errorBuffer << "]";

		curl_easy_cleanup( connsHandle );
		throw runtime_error( errorMessage.str() );
	}
	code = curl_easy_setopt( connsHandle, CURLOPT_PORT, 22 );
	if ( code != CURLE_OK )
	{
		stringstream errorMessage;
		errorMessage << "Failed to set PORT code [" << code << "] error [" << errorBuffer << "]";

		curl_easy_cleanup( connsHandle );
		throw runtime_error( errorMessage.str() );
	}
	
	uploadBuffer writeBuffer;
	writeBuffer.m_ReadPtr = buffer.c_str();
	writeBuffer.m_SizeLeft = strlen( buffer.c_str() );

	curl_easy_setopt( connsHandle, CURLOPT_READFUNCTION, readCallbackMemory) ;
	curl_easy_setopt( connsHandle, CURLOPT_READDATA, &writeBuffer );
	
	curl_easy_setopt( connsHandle, CURLOPT_UPLOAD, 1L );
	code = curl_easy_perform( connsHandle );
	if (code != CURLE_OK)
		DEBUG_LOG( "Failed to upload data. Error code [" << code << "]. Error details [" << errorBuffer << "]" );

	curl_easy_cleanup( connsHandle );
}