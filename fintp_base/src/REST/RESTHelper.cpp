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

/*
#ifndef WIN32
	#include <unistd.h>
#else
	#include <windows.h>

	#define sleep( x ) Sleep( ( x )*1000 )
#endif
*/

#ifdef WIN32
	#define sleep( x ) Sleep( ( x )*1000 )
#endif
#define MAX_RETRY 3

#include <iostream>
#include <sstream>
#include <string>

#include "RESTHelper.h"
#include "Trace.h"
#include "StringUtil.h"
#include "TimeUtil.h"

using namespace std;
using namespace FinTP;

const char RESTcURLHelper::CA_CERT_FILE[] = "cacert.pem";
const char IPSHelper::m_ConfirmationBody[] = "";
const char IPSHelper::REQUEST_CHANNEL[] = "X-MONTRAN-RTP-Channel: ";
const string IPSHelper::REQUEST_VERSION = "X-MONTRAN-RTP-Version: 1";
const string IPSHelper::REQUEST_AKN_SEQUENCE = "X-MONTRAN-RTP-MessageSeq: ";

const char* IPSHelper::m_HeaderFields[HEADER_FIELDS] = { "X-MONTRAN-RTP-ReqSts","X-MONTRAN-RTP-PossibleDuplicate",
														 "X-MONTRAN-RTP-MessageSeq","X-MONTRAN-RTP-MessageType","X-MONTRAN-RTP-Version"};

#ifdef CURL_DEBUG
	#define LOG_CURL curl_easy_setopt( m_ConnsHandle, CURLOPT_VERBOSE, 1L )
	#define DEBUG_CURL( expr ) \
	{ \
		try \
		{ \
			cerr << TimeUtil::Get( "%d/%m/%Y %H:%M:%S", 19 ) << " : " << expr << endl << flush; \
		} catch( ... ){}; \
	}
#else
	#define LOG_CURL
	#define DEBUG_CURL( expr )
#endif
			
// WMqHelper implementation
RESTcURLHelper::RESTcURLHelper() : m_HttpChannel( "" ), m_RequestHeaderFields( NULL ), m_ConnsHandle( NULL ), m_HttpContentType( "" ), m_SSLEnabled( true )
{
	// constant cURL options initialization
}

RESTcURLHelper::~RESTcURLHelper()
{
	try
	{
		disconnect();
	}catch( ... ){};
}

int RESTcURLHelper::writerCallback( char *data, size_t size, size_t nmemb, void* writerBuffer )
{
	vector<unsigned char>* buffer = static_cast<vector<unsigned char>*>( writerBuffer );
	buffer->insert( buffer->end(), data, data + size * nmemb );
	return size * nmemb;
}

int RESTcURLHelper::headerCallback( char *data, size_t size, size_t nmemb, void* headerBuffer )
{
	// cURL provides each header paire ( <name>:<value> ) at once
	vector<string>* headerVector = static_cast<vector<string>*>( headerBuffer ) ;
	string buffer;
	buffer.append( data, size * nmemb );
	headerVector->push_back( buffer );
	return size * nmemb;
}

void RESTcURLHelper::connect( const string& httpChannel, const string& certFile, const string& certPhrase, bool force )
{
	if( m_ConnsHandle != NULL )
	{
		if( force )
		{
			DEBUG( "Force reconnecting..." );
			disconnect();
		}
		else
		{
			DEBUG( "Already connected!. Skip connection perform" );
			return;
		}
	}

	DEBUG( "Connecting to remote host ..." );
	stringstream errorMessage;
	m_HttpChannel = httpChannel;

	if( m_HttpChannel.empty() )
		throw runtime_error( "Cannot connect. Http channel not set" );

	m_ConnsHandle = curl_easy_init();
	if ( m_ConnsHandle == NULL )
		throw runtime_error( "Failed to create CURL connection" );

	//code = curl_easy_setopt( m_ConnsHandle, CURLOPT_FOLLOWLOCATION, 1L );
	//curl_easy_setopt( m_ConnsHandle, CURLOPT_USERAGENT, "libcurl-agent/1.0" ); // required by some servers
	//std::unique_ptr<char, void (*)(void *)> encodedUrl( curl_easy_escape( m_ConnsHandle, m_HttpChannel.c_str(), m_HttpChannel.size() ), curl_free );

	CURLcode code;
	LOG_CURL;
	//curl_easy_setopt( m_ConnsHandle, CURLOPT_HEADER, 1L );

	code = curl_easy_setopt( m_ConnsHandle, CURLOPT_ERRORBUFFER, m_ErrorBuffer );
	if( code != CURLE_OK )
	{
		errorMessage << "Failed to set error buffer [" << code << "] error [" << m_ErrorBuffer << "]" ;
		throw runtime_error( errorMessage.str() );
	}

	if( m_SSLEnabled )
	{
		code = curl_easy_setopt( m_ConnsHandle, CURLOPT_SSLVERSION, 6 );// CURL_SSLVERSION_TLSv1_2=6
		if( code != CURLE_OK )
		{
			errorMessage << "Failed to set required TLS 1.2 version [" << code << "] error [" << m_ErrorBuffer << "]" ;
			throw runtime_error( errorMessage.str() );
		}
		// set Cerificates
		//code = curl_easy_setopt( m_ConnsHandle, CURLOPT_CAINFO, CA_CERT_FILE );
		code = curl_easy_setopt( m_ConnsHandle, CURLOPT_SSL_VERIFYPEER, 0 );
		code = curl_easy_setopt( m_ConnsHandle, CURLOPT_SSL_VERIFYHOST, 0 );
		if ( code != CURLE_OK )
			TRACE( "Failed to prevent certificate verification [" << code << "] error [" << m_ErrorBuffer << "]" );
		
		if( !certFile.empty() )
		{
			//TODO: change this
			if( certFile.find_last_of( ".pem" ) == string::npos )
			{
				DEBUG( "Client certificate is PEM default format" );
			}
			else
			{
				DEBUG( "Fall back to the P12 supported format" );
				code = curl_easy_setopt( m_ConnsHandle, CURLOPT_SSLCERTTYPE, "P12" );
				if ( code != CURLE_OK )
				{
					errorMessage << "Failed to set the certificate format [" << code << "] error [" << m_ErrorBuffer << "]" ;
					throw runtime_error( errorMessage.str() );
				}
			}
			code = curl_easy_setopt( m_ConnsHandle, CURLOPT_SSLCERT, certFile.c_str() );
			if ( code != CURLE_OK )
			{
				errorMessage << "Failed to set the certificate name [" << code << "] error [" << m_ErrorBuffer << "]" ;
				throw runtime_error( errorMessage.str() );
			}
			if( !certPhrase.empty() )
			{
				code = curl_easy_setopt( m_ConnsHandle, CURLOPT_KEYPASSWD, certPhrase.c_str() );
				if ( code != CURLE_OK )
				{
					errorMessage << "Failed to set the certificate phrase  [" << code << "] error [" << m_ErrorBuffer << "]" ;
					throw runtime_error( errorMessage.str() );
				}
			}
		}
	}
	else
	{
		code = curl_easy_setopt( m_ConnsHandle, CURLOPT_SSL_VERIFYHOST, 0 );
		if ( code != CURLE_OK )
			TRACE( "Failed to prevent server host verification [" << code << "] error [" << m_ErrorBuffer << "]" );
		code = curl_easy_setopt( m_ConnsHandle, CURLOPT_SSL_VERIFYPEER, 0 );
		if ( code != CURLE_OK )
			TRACE( "Failed to prevent peer's certificate verification  [" << code << "] error [" << m_ErrorBuffer << "]" );
	}

	/*
	//TODO: some logic for reusing connection ???
	if ( force )
	{
		DEBUG( "Forced connect" );
		disconnect();
		//TODO: loop trying to conect
	}
	else
		DEBUG( "Reconnect not forced" );
		//TODO: Some logic to check connection status
	*/
}

void RESTcURLHelper::closeConnection()
{
	//TODO: soft connection resources reset
	disconnect();
}

void RESTcURLHelper::disconnect()
{
	//TODO: some checks and closes to be sure disconect can performe. Hard resource free

	try
	{
		if( m_RequestHeaderFields != NULL )
		{
			curl_slist_free_all( m_RequestHeaderFields );
			m_RequestHeaderFields = NULL;
		}
		if( m_ConnsHandle != NULL )
		{
			curl_easy_cleanup( m_ConnsHandle );
			m_ConnsHandle = NULL;
		}

	}
	catch( ... )
	{
		TRACE( "Unhandled exeption while clean up Http Connections Handler" );
	}
	DEBUG( "Close all Http connections" );
}

int RESTcURLHelper::internalPerform()
{
	int retryCount = 0;
	stringstream errorMessage;
	CURLcode code;
	long httpCode = 0;
	bool retry = false;
	if( m_HttpContentType.empty() )
		setContentType();
	do
	{
		DEBUG( TimeUtil::Get( "%d/%m/%Y %H:%M:%S", 19 ) << ". New HTTP request perform ..." );
		code = curl_easy_perform( m_ConnsHandle );
		httpCode = code;
		retryCount++;
        retry = false;
		if( code != CURLE_OK )
		{
			//TODO: Put http action and uri in TRACE
			TRACE( "Failed to get data. Error code [" << code << "]. Error details [" << m_ErrorBuffer << "]. Retry " << MAX_RETRY - retryCount << "more times" );
			retry = true;
		}
		else
		{
			curl_easy_getinfo ( m_ConnsHandle, CURLINFO_TOTAL_TIME, &m_LastRequestTime );
			code = curl_easy_getinfo ( m_ConnsHandle, CURLINFO_RESPONSE_CODE, &httpCode );
			if( code != CURLE_OK || httpCode != 200 )
			{
				TRACE( "Fail http response code [" << httpCode << "] error [" << m_ErrorBuffer << "]. Retry " << MAX_RETRY - retryCount << " more times" );
				if( httpCode == 500 )
				{
					CURLcode recCode = curl_easy_setopt( m_ConnsHandle, CURLOPT_FRESH_CONNECT, 1 );
					if( recCode == CURLE_OK )
					{
						DEBUG( "Enforce new connection ..." );
					}
					else
						DEBUG( "Fail to reinforce new connection with code [" << recCode << "]" );
				}
				retry = true;
			}
		}

		DEBUG_CURL( "Http request performed: [" << m_HttpChannel << "]. Response code: [" << httpCode << "]." );

		if( retry )
			sleep( 1 );

	}while( retry && retryCount < MAX_RETRY );

	if( retryCount >= MAX_RETRY )
	{
		errorMessage << "Failed to get response data: cURL failed code [" << code << "], HTTP response code [" << httpCode << "], error [" << m_ErrorBuffer << "]" ;
		throw runtime_error( errorMessage.str() );
	}
	
	return 0;
}

int RESTcURLHelper::getOne( const string& resourcePath,  vector<unsigned char>& outputBuffer )
{
	DEBUG( "Getting one message" );
	stringstream errorMessage;

	CURLcode code;

	//set Method
	code = curl_easy_setopt( m_ConnsHandle, CURLOPT_HTTPGET, 1L );
	if( code != CURLE_OK )
	{
		errorMessage << "Failed to set GET request method [" << code << "] error [" << m_ErrorBuffer << "]" ;
		throw runtime_error( errorMessage.str() );
	}
	string url;
	url.append( m_HttpChannel ).append( resourcePath );
	code = curl_easy_setopt( m_ConnsHandle, CURLOPT_URL, url.c_str() );
	if( code != CURLE_OK )
	{
		errorMessage << "Failed to set URL request [" << code << "] error [" << m_ErrorBuffer << "]" ;
		throw runtime_error( errorMessage.str() );
	}

	// set Response Callback
	code = curl_easy_setopt( m_ConnsHandle, CURLOPT_WRITEFUNCTION, RESTcURLHelper::writerCallback );
	if ( code != CURLE_OK )
	{
		errorMessage << "Failed to set writer [" << code << "] error [" << m_ErrorBuffer << "]" ;
		throw runtime_error( errorMessage.str() );
	}

	//set Data Callback : local variable for thread-safe construct !!!
	code = curl_easy_setopt( m_ConnsHandle, CURLOPT_WRITEDATA, &outputBuffer );
	if ( code != CURLE_OK )
	{
		errorMessage << "Failed to set write data [" << code << "] error [" << m_ErrorBuffer << "]" ;
		throw runtime_error( errorMessage.str() );
	}

	// set Header Callback
	code = curl_easy_setopt( m_ConnsHandle, CURLOPT_HEADERFUNCTION, RESTcURLHelper::headerCallback );
	if( code != CURLE_OK )
	{
		errorMessage << "Failed to set write header [" << code << "] error [" << m_ErrorBuffer << "]" ;
		throw runtime_error( errorMessage.str() );
	}
	code = curl_easy_setopt( m_ConnsHandle, CURLOPT_HEADERDATA, &m_HeaderBuffer );
	if( code != CURLE_OK )
	{
		errorMessage << "Failed to set header data [" << code << "] error [" << m_ErrorBuffer << "]" ;
		throw runtime_error( errorMessage.str() );
	}
	m_HeaderBuffer.clear(); //response buffer
	m_HeaderFields.clear();
	
	int result = internalPerform();
	
	DEBUG( "Get one message - done" );
	return result;
}

int RESTcURLHelper::putOne( const string& resourcePath, ManagedBuffer* inputBuffer, ManagedBuffer* outputBuffer )
{
	DEBUG( "Putting one message" );
	
	//TODO: put specific settings
	CURLcode code;
	code = curl_easy_setopt( m_ConnsHandle, CURLOPT_URL, resourcePath.c_str() );
	if( code != CURLE_OK )
	{
		stringstream errorMessage;
		errorMessage << "Failed to set URL code [" << code << "] error [" << m_ErrorBuffer << "]" ;
		
		throw runtime_error( errorMessage.str() );
	}

	m_HeaderBuffer.clear(); //previous response buffer
	m_HeaderFields.clear(); // previous header fields
	resetHeader();
	int result = internalPerform();
	DEBUG( "One message put" );
	return result;
}

int RESTcURLHelper::postOne( const string& resourcePath, ManagedBuffer* inputBuffer, vector<unsigned char>& responseBuffer, const bool headerReset )
{
	// set Response Callback
	CURLcode code;
	stringstream errorMessage;
	code = curl_easy_setopt( m_ConnsHandle, CURLOPT_WRITEFUNCTION, RESTcURLHelper::writerCallback );
	if ( code != CURLE_OK )
	{
		errorMessage << "Failed to set response calback writer [" << code << "] error [" << m_ErrorBuffer << "]" ;
		throw runtime_error( errorMessage.str() );
	}

	//set Data Callback : local variable for thread-safe construct !!!
	code = curl_easy_setopt( m_ConnsHandle, CURLOPT_WRITEDATA, &responseBuffer );
	if ( code != CURLE_OK )
	{
		errorMessage << "Failed to set response write data [" << code << "] error [" << m_ErrorBuffer << "]" ;
		throw runtime_error( errorMessage.str() );
	}

	// set Header Callback
	code = curl_easy_setopt( m_ConnsHandle, CURLOPT_HEADERFUNCTION, RESTcURLHelper::headerCallback );
	if( code != CURLE_OK )
	{
		errorMessage << "Failed to set response header callback [" << code << "] error [" << m_ErrorBuffer << "]" ;
		throw runtime_error( errorMessage.str() );
	}
	code = curl_easy_setopt( m_ConnsHandle, CURLOPT_HEADERDATA, &m_HeaderBuffer );
	if( code != CURLE_OK )
	{
		errorMessage << "Failed to set response header callback data [" << code << "] error [" << m_ErrorBuffer << "]" ;
		throw runtime_error( errorMessage.str() );
	}
	m_HeaderBuffer.clear(); //response buffer
	m_HeaderFields.clear();

	return postOne( resourcePath, inputBuffer, headerReset );
}

int RESTcURLHelper::postOne( const string& resourcePath, ManagedBuffer* inputBuffer, const bool headerReset )
{
	DEBUG( "Posting one message" );
	
	//TODO: post settings, 1/just generic ones; 2/check on those already set
	CURLcode code;
	stringstream errorMessage;

	//Set request http method
	code = curl_easy_setopt( m_ConnsHandle, CURLOPT_HTTPPOST, 1L );
	if( code != CURLE_OK )
	{
		errorMessage << "Failed to set POST request method [" << code << "] error [" << m_ErrorBuffer << "]" ;
		throw runtime_error( errorMessage.str() );
	}
	string url;
	url.append( m_HttpChannel ).append( resourcePath );
	code = curl_easy_setopt( m_ConnsHandle, CURLOPT_URL, url.c_str() );
	if( code != CURLE_OK )
	{
		errorMessage << "Failed to set URL request [" << code << "] error [" << m_ErrorBuffer << "]" ;
		throw runtime_error( errorMessage.str() );
	}
	
	//Reset request header fields
	if( headerReset )
		resetHeader();
	code = curl_easy_setopt( m_ConnsHandle, CURLOPT_HTTPHEADER, m_RequestHeaderFields );
	if( code != CURLE_OK )
	{
		errorMessage << "Failed to set IPS mandatory header fields [" << code << "] error [" << m_ErrorBuffer << "]" ;
		throw runtime_error( errorMessage.str() );
	}

	// Set formal body
	if( inputBuffer != NULL && inputBuffer->size() )
	{
		char* buffer = ( char* )inputBuffer->buffer();
		curl_easy_setopt( m_ConnsHandle, CURLOPT_POSTFIELDS, buffer );
		curl_easy_setopt( m_ConnsHandle, CURLOPT_POSTFIELDSIZE_LARGE, ( curl_off_t )inputBuffer->size() );
	}

	code = curl_easy_setopt( m_ConnsHandle, CURLOPT_TIMEOUT, 20L );
	if ( code != CURLE_OK )
	{
		errorMessage << "Failed to set timeout [" << code << "] error [" << m_ErrorBuffer << "]";
		throw runtime_error( errorMessage.str() );
	}

	int result = internalPerform();
	
	DEBUG( "One message post" );
	return result;
}

string RESTcURLHelper::getHeaderField( const string& fieldName )
{
	//TODO: parse usefull technical header fields with getInfo
	if( m_HeaderFields.empty() )
	{
		string firstToken, secondToken;
		for( unsigned int i = 0; i < m_HeaderBuffer.size(); i++ )
		{
			const string bufferElement = string( m_HeaderBuffer[i] );
			StringUtil unparsedField( bufferElement );
			unparsedField.Split( ":" );
			if ( unparsedField.MoreTokens() ) {
				firstToken = StringUtil::Trim( unparsedField.NextToken(), true );
				secondToken = StringUtil::Trim( unparsedField.NextToken(), true );
				m_HeaderFields.insert( pair<string, string>( firstToken, secondToken ) );
			}
			else
				TRACE( "Unexpected header format ( <field>:<value> ):[" << bufferElement << "]." )

		}	
	}

	string fieldValue;
	map<string,string>::const_iterator it = m_HeaderFields.find( fieldName );
	if( it != m_HeaderFields.end() )
		fieldValue = it->second;
	
	return fieldValue;
}

void RESTcURLHelper::resetHeader()
{
	CURLcode code;
	if( m_RequestHeaderFields != NULL )
	{
		curl_slist_free_all( m_RequestHeaderFields );
		m_RequestHeaderFields = NULL ;
	}

}

void RESTcURLHelper::setContentType( const RESTcURLHelper::HttpContentType contentType )
{
	string fieldPrefix = "Content-Type: ";
	switch( contentType )
	{
		case XML :
		{
			m_HttpContentType = fieldPrefix.append( "application/xml" );
			break;
		}
		case SOAP :
		{
			m_HttpContentType = fieldPrefix.append( "application/soap" );
			break;
		}
		case TXT :
		{
			m_HttpContentType = fieldPrefix.append( "text/xml" );
			break;
		}
		case JSON:
		{
			m_HttpContentType = fieldPrefix.append("application/json");
			break;
		}
		default :
			m_HttpContentType = fieldPrefix.append( "text/plain" );

	}
}

void RESTcURLHelper::setRequestHeaderItem( const string& headerItem, const bool force )
{
	struct curl_slist* tempList = NULL;
	int isContentType = headerItem.find( "Content-Type:" ) == 0 ? true : false;

	if( !isContentType || ( isContentType && force ) )
	{
		tempList = curl_slist_append( m_RequestHeaderFields, headerItem.c_str() );
		if( tempList != NULL )
		{
			m_RequestHeaderFields = tempList;
			if( isContentType )
				setContentType( parseContentType( StringUtil::Trim( headerItem.substr( 13 ) ) ) );
		}
		else 
		{
			stringstream errorMessage;
			errorMessage << "Can't set requiered HTTP header field: [" << headerItem << "]";
			TRACE( errorMessage.str() );
		}
	}
	else
		DEBUG( "Use force parameter to override 'Content-type field' as [" << headerItem << "]" );
}

RESTcURLHelper::HttpContentType RESTcURLHelper::parseContentType( const string contentType ) 
{
	string httpContentType = StringUtil::ToLower( contentType  );

	if( httpContentType == "application/xml" )
		return RESTcURLHelper::XML;
	else if( httpContentType == "application/soap" )
		return RESTcURLHelper::SOAP;
	else if( httpContentType == "text/xml" )
		return RESTcURLHelper::TXT;
	else if( httpContentType == "application/json" )
		return RESTcURLHelper::JSON;
	else 
	{
		DEBUG( "Unmanaged Content-type is set for: [" << httpContentType << "]" );
		return RESTcURLHelper::UNMGT;
	}
		
}

bool RESTcURLHelper::commit()
{
	//TODO: try release some resources
	DEBUG( "Commit" );
	return ( true );
}

bool RESTcURLHelper::abort()
{
	//TODO: if resets client side resources. Try to guess server/client status
	DEBUG( "Abort" );
	return( true );
}

//IPSHelper Implementation
void IPSHelper::resetHeader()
{
	CURLcode code;
	if( m_RequestHeaderFields != NULL )
	{
		curl_slist_free_all( m_RequestHeaderFields );
		m_RequestHeaderFields = NULL ;
	}

	string channelId = REQUEST_CHANNEL;
	m_RequestHeaderFields = curl_slist_append( m_RequestHeaderFields, channelId.append( m_ClientId ).c_str() );
	m_RequestHeaderFields = curl_slist_append( m_RequestHeaderFields, REQUEST_VERSION.c_str() );
	code = curl_easy_setopt( m_ConnsHandle, CURLOPT_HTTPHEADER, m_RequestHeaderFields );
	setRequestHeaderItem( "Content-Type: application/xml", true );
	if( code != CURLE_OK )
	{
		stringstream errorMessage;
		errorMessage << "Failed to set IPS mandatory header fields [" << code << "] error [" << m_ErrorBuffer << "]" ;
		throw runtime_error( errorMessage.str() );
	}
}

int IPSHelper::getOne( const string& resourcePath, vector<unsigned char>& outputBuffer )
{
	resetHeader();
	return RESTcURLHelper::getOne( resourcePath, outputBuffer );
}

int IPSHelper::postOneConfirmation( const string& resourcePath, const string& messageId )
{
	DEBUG( "Posting one confirmation" );


	CURLcode code;
	stringstream errorMessage;

	//Set request http method
	code = curl_easy_setopt( m_ConnsHandle, CURLOPT_HTTPPOST, 1L );
	if( code != CURLE_OK )
	{
		errorMessage << "Failed to set POST request method [" << code << "] error [" << m_ErrorBuffer << "]" ;
		throw runtime_error( errorMessage.str() );
	}
	string url;
	url.append( m_HttpChannel ).append( resourcePath );
	code = curl_easy_setopt( m_ConnsHandle, CURLOPT_URL, url.c_str() );
	if( code != CURLE_OK )
	{
		errorMessage << "Failed to set URL request [" << code << "] error [" << m_ErrorBuffer << "]" ;
		throw runtime_error( errorMessage.str() );
	}

	//Reset request header fields
	resetHeader();
	string sequenceId = REQUEST_AKN_SEQUENCE;
	curl_slist_append( m_RequestHeaderFields, sequenceId.append( messageId ).c_str() );
	code = curl_easy_setopt( m_ConnsHandle, CURLOPT_HTTPHEADER, m_RequestHeaderFields );
	if( code != CURLE_OK )
	{
		errorMessage << "Failed to set IPS mandatory header fields [" << code << "] error [" << m_ErrorBuffer << "]" ;
		throw runtime_error( errorMessage.str() );
	}

	// Set formal body
	curl_easy_setopt( m_ConnsHandle, CURLOPT_POSTFIELDS, m_ConfirmationBody );
	curl_easy_setopt( m_ConnsHandle, CURLOPT_POSTFIELDSIZE, ( long )strlen( m_ConfirmationBody ) );

	int result = internalPerform();

	DEBUG( "One confirmation post" );
	return result;
}

/*
bool IPSHelper::requiresSecurityCheck()
{
	string messageType = getLastMessageType();
	return ( messageType.find( "recon" ) == string::npos );
}

bool IPSHelper::requiresReply()
{
	string messageType = getLastMessageType();
	return ( messageType.find( "pacs.008" ) != string::npos );
}

bool IPSHelper::requiresConfirmation()
{
	string messageType = getLastMessageType();
	return ( messageType.find( "pacs.008" ) == string::npos );
}


int IPSHelper::postOneReply ( const string& resourcePath, ManagedBuffer* inputBuffer, ManagedBuffer* outputBuffer )
{
	DEBUG( "Posting one reply" );

	//TODO: Specific reply settings
	CURLcode code;
	code = curl_easy_setopt( m_ConnsHandle, CURLOPT_URL, resourcePath.c_str() );
	if( code != CURLE_OK )
	{
		stringstream errorMessage;
		errorMessage << "Failed to set URL code [" << code << "] error [" << m_ErrorBuffer << "]" ;
		
		throw runtime_error( errorMessage.str() );
	}

	//WorkItem< ManagedBuffer > managedBuffer( new ManagedBuffer( buffer, ManagedBuffer::Ref, bufferSize ) );

	//putOne( managedBuffer );

	DEBUG( "One reply post" );
}
*/

