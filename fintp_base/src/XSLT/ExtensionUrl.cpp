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

// Base header file.  Must be first.
#include <xalanc/Include/PlatformDefinitions.hpp>

#include <xercesc/util/PlatformUtils.hpp>
#include <xalanc/XalanTransformer/XalanTransformer.hpp>
#include <xalanc/XPath/XObjectFactory.hpp>
#include <xalanc/XercesParserLiaison/XercesDocumentWrapper.hpp>
#include "ExtensionUrl.h"

#include <string>
#include <sstream>
#include "XmlUtil.h"
#include "Base64.h"
#include "Trace.h"
#include "StringUtil.h"

using namespace std;
using namespace FinTP;
XALAN_CPP_NAMESPACE_USE

//
//  libcurl variables for error strings and returned data

char FunctionUrl::m_ErrorBuffer[ CURL_ERROR_SIZE ];
vector<unsigned char> FunctionUrl::m_Buffer;
XercesDocumentWrapper* FunctionUrl::m_XercesDocumentWrapper = NULL;

//
//  libcurl variables for connection using ssl
string FunctionUrl::SSLCertificateFileName = "";
string FunctionUrl::SSLCertificatePasswd = "";
string FunctionUrl::m_BAAuthentication = "";

//
//  libcurl write callback function
//
int FunctionUrl::writer( char *data, size_t size, size_t nmemb, unsigned char** writerDataDummy )
{
	m_Buffer.insert( m_Buffer.end(), data, data + size * nmemb );

	return size * nmemb;
}

//
//  libcurl connection initialization
//
void FunctionUrl::initCurl( CURL *&conn, const string& url, curl_slist* headerList ) const
{
	CURLcode code;
	conn = curl_easy_init();

	if ( conn == NULL )
	{
		throw runtime_error( "Failed to create CURL connection" );
	}

	code = curl_easy_setopt( conn, CURLOPT_ERRORBUFFER, m_ErrorBuffer );
	if ( code != CURLE_OK )
	{
		stringstream errorMessage;
		errorMessage << "Failed to set error buffer [" << code << "]";
		
		throw runtime_error( errorMessage.str() );
	}

	const string URL = StringUtil::Replace( url, " ", "%20" );
	code = curl_easy_setopt( conn, CURLOPT_URL, URL.c_str() );
	if ( code != CURLE_OK )
	{
		stringstream errorMessage;
		errorMessage << "Failed to set URL code [" << code << "] error [" << m_ErrorBuffer << "]" ;
		
		throw runtime_error( errorMessage.str() );
	}

	if ( SSLCertificatePasswd.length() > 0 )
	{
		
		code = curl_easy_setopt( conn, CURLOPT_KEYPASSWD, SSLCertificatePasswd.c_str() );
		if ( code != CURLE_OK )
		{
			stringstream errorMessage;
			errorMessage << "Failed to set the key password code [" << code << "] error [" << m_ErrorBuffer << "]" ;
		
			throw runtime_error( errorMessage.str() );
		}
	}

	if ( SSLCertificateFileName.length() > 0 )
	{
		code = curl_easy_setopt( conn, CURLOPT_SSLCERT, SSLCertificateFileName.c_str() );
		if ( code != CURLE_OK )
		{
			stringstream errorMessage;
			errorMessage << "Failed to set the certificate name code [" << code << "] error [" << m_ErrorBuffer << "]" ;
		
			throw runtime_error( errorMessage.str() );
		}
	
		code = curl_easy_setopt( conn, CURLOPT_SSL_VERIFYHOST, 2 );
		if ( code != CURLE_OK )
		{
			stringstream errorMessage;
			errorMessage << "Failed to set client verification code [" << code << "] error [" << m_ErrorBuffer << "]" ;
			
			throw runtime_error( errorMessage.str() );
		}
	}
	else
	{
		code = curl_easy_setopt( conn, CURLOPT_SSL_VERIFYHOST, 0 );
		if ( code != CURLE_OK )
		{
			stringstream errorMessage;
			errorMessage << "Failed to stop to verify the host code [" << code << "] error [" << m_ErrorBuffer << "]" ;
			
			throw runtime_error( errorMessage.str() );
		}
	}

	code = curl_easy_setopt( conn, CURLOPT_SSL_VERIFYPEER, 0 ); 
	if ( code != CURLE_OK )
	{
		stringstream errorMessage;
		errorMessage << "Failed to stop verifying the authenticity of the peer's certificate code [" << code << "] error [" << m_ErrorBuffer << "]" ;
	
		throw runtime_error( errorMessage.str() );
	}

	code = curl_easy_setopt( conn, CURLOPT_FOLLOWLOCATION, 1L );
	if ( code != CURLE_OK )
	{
		stringstream errorMessage;
		errorMessage << "Failed to set redirect option [" << code << "] error [" << m_ErrorBuffer << "]" ;
		
		throw runtime_error( errorMessage.str() );
	}

	code = curl_easy_setopt( conn, CURLOPT_WRITEFUNCTION, FunctionUrl::writer );
	if ( code != CURLE_OK )
	{
		stringstream errorMessage;
		errorMessage << "Failed to set writer [" << code << "] error [" << m_ErrorBuffer << "]" ;
		
		throw runtime_error( errorMessage.str() );
	}

	code = curl_easy_setopt( conn, CURLOPT_WRITEDATA, NULL );
	if ( code != CURLE_OK )
	{
		stringstream errorMessage;
		errorMessage << "Failed to set write data [" << code << "] error [" << m_ErrorBuffer << "]" ;

		throw runtime_error( errorMessage.str() );
	}

	if ( headerList != NULL )
	{
		code = curl_easy_setopt ( conn, CURLOPT_HTTPHEADER, headerList  );
		if ( code != CURLE_OK )
		{
			stringstream errorMessage;
			errorMessage << "Failed to set http header [" << code << "] error [" << m_ErrorBuffer << "]" ;

			throw runtime_error( errorMessage.str() );
		}
	}

	m_Buffer.clear();
}

/**
* Execute an XPath function object.  The function must return a valid
* XObject.
*
* @param executionContext executing context
* @param context          current context node
* @param opPos            current op position
* @param args             vector of pointers to XObject arguments
* @return                 pointer to the result XObject
*/
XObjectPtr FunctionUrl::execute( XPathExecutionContext& executionContext, XalanNode* context, const XObjectArgVectorType& args, const LocatorType* locator ) const
{
	if ( args.size() < 1 )
	{
		stringstream errorMessage;
		errorMessage << "The url-get(url, [httpHeader1, httpHeader2, ...]) function takes at least one argument: url";
#if (_XALAN_VERSION >= 11100)
		executionContext.problem( XPathExecutionContext::eXPath, XPathExecutionContext::eError, XalanDOMString( errorMessage.str().c_str() ), locator, context); 
#else
		executionContext.error( XalanDOMString( errorMessage.str().c_str() ), context );
#endif
	}

	if ( m_XercesDocumentWrapper != NULL )
	{
		delete m_XercesDocumentWrapper->getXercesDocument();
		delete m_XercesDocumentWrapper;
		m_XercesDocumentWrapper = NULL;
	}

	std::unique_ptr<curl_slist, void (*)(curl_slist *)> headerList( NULL, curl_slist_free_all );

	const string url = XmlUtil::XMLChtoString( args[0]->str().data() );

	bool encodeBase64 = true;
	if ( args.size() > 1 )
	{
		encodeBase64 = false;
		for ( size_t i = 1; i < args.size(); i++ )
		{
			const string httpHeader = XmlUtil::XMLChtoString( args[i]->str().data() );
			headerList.reset( curl_slist_append( headerList.release(), httpHeader.c_str() ) );
		}
	}

	if ( !m_BAAuthentication.empty() )
		 headerList.reset( curl_slist_append( headerList.release(), m_BAAuthentication.c_str() ) );

	curl_global_init( CURL_GLOBAL_DEFAULT );

	// Initialize CURL connection
	try
	{
		CURL *conn = NULL;
		initCurl( conn, url, headerList.get() );
		std::unique_ptr<CURL, void (*)(CURL *)> curl( conn, curl_easy_cleanup );
		conn = curl.get();

		std::unique_ptr<char, void (*)(void *)> encodedUrl( curl_easy_escape( conn, url.c_str(), url.size() ), curl_free );
		std::unique_ptr<XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument> xml;

		// Retrieve content for the URL
		CURLcode code = curl_easy_perform( conn );
		if ( code != CURLE_OK )
		{
			if ( encodeBase64 )
			{
				stringstream errorMessage;
				errorMessage << "Failed to get [" << url << "] code [" << code << "] error [" << m_ErrorBuffer << "]";
				throw runtime_error( errorMessage.str() );
			}
			else
			{
				xml = createErrorXml( encodedUrl.get(), m_ErrorBuffer );
				m_XercesDocumentWrapper = new XercesDocumentWrapper( *XMLPlatformUtils::fgMemoryManager, xml.release(), true, true, true );
				return executionContext.getXObjectFactory().createNodeSet( m_XercesDocumentWrapper );
			}
		}

		long http_code;
		code = curl_easy_getinfo (conn, CURLINFO_RESPONSE_CODE, &http_code);
		if ( code != CURLE_OK )
		{
			stringstream errorMessage;
			errorMessage << "Failed to get status code, error [" << m_ErrorBuffer << "]";
			throw runtime_error( errorMessage.str() );
		}

		char *contentType = NULL;
		code = curl_easy_getinfo( conn, CURLINFO_CONTENT_TYPE, &contentType );
		if ( code != CURLE_OK )
		{
			stringstream errorMessage;
			errorMessage << "Failed to get content type, error [" << m_ErrorBuffer << "]";
			throw runtime_error( errorMessage.str() );
		}

		string contentTypeString;
		if ( contentType )
			contentTypeString = contentType;

		if ( encodeBase64 )
		{
			if ( http_code == 200 )
			{
				DEBUG( "Content type: " << contentTypeString )
				string base64EncodedResult = Base64::encode( &m_Buffer[0], m_Buffer.size() );
				return executionContext.getXObjectFactory().createString( unicodeForm( base64EncodedResult ) );
			}
			else
			{
				stringstream errorMessage;
				errorMessage << "HTTP status code [" << http_code << "] for URL [" << url << "]";
				throw runtime_error( errorMessage.str() );
			}
		}
		else
		{
			if ( http_code == 200 )
			{
				DEBUG( "Content type: " << contentTypeString )
				if ( StringUtil::ToLower( contentTypeString ) != "application/xml" )
					TRACE( "Warning: expected content type: application/xml" )

				xml.reset( XmlUtil::DeserializeFromString( &m_Buffer[0], m_Buffer.size() ) );
				m_XercesDocumentWrapper = new XercesDocumentWrapper( *XMLPlatformUtils::fgMemoryManager, xml.release(), true, true, true );
				return executionContext.getXObjectFactory().createNodeSet( m_XercesDocumentWrapper );
			}
			else
			{
				xml = createErrorXml( encodedUrl.get(), "", StringUtil::ToString( http_code ) );
				m_XercesDocumentWrapper = new XercesDocumentWrapper( *XMLPlatformUtils::fgMemoryManager, xml.release(), true, true, true );
				return executionContext.getXObjectFactory().createNodeSet( m_XercesDocumentWrapper );
			}
		}
	}
	catch( ... )
	{
		if ( m_XercesDocumentWrapper != NULL )
		{
			delete m_XercesDocumentWrapper->getXercesDocument();
			delete m_XercesDocumentWrapper;
			m_XercesDocumentWrapper = NULL;
		}
		throw;
	}
}

std::unique_ptr<XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument> FunctionUrl::createErrorXml( const string& url, const string& errorMessage, const string& errorStatus ) const
{
	DOMImplementation* impl = DOMImplementationRegistry::getDOMImplementation( unicodeForm( "LS" ) );

	std::unique_ptr<XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument> xml;
	xml.reset( impl->createDocument( 0, unicodeForm( "urlEntity" ), 0 ) ) ;

	DOMElement* rootElem = xml->getDocumentElement();
	DOMElement* urlElem = xml->createElement( unicodeForm( "url" ) );
	rootElem->appendChild( urlElem );
	DOMText* urlValue = xml->createTextNode( unicodeForm( url ) );
	urlElem->appendChild( urlValue );

	if ( !errorMessage.empty() )
	{
		DOMElement* error = xml->createElement( unicodeForm( "error" ) );
		rootElem->appendChild( error );
		DOMText* errorValue = xml->createTextNode( unicodeForm( errorMessage ) );
		error->appendChild( errorValue );
	}

	if ( !errorStatus.empty() )
	{
		DOMElement* status = xml->createElement( unicodeForm( "status" ) );
		rootElem->appendChild( status );
		DOMText* statusValue = xml->createTextNode( unicodeForm( errorStatus ) );
		status->appendChild( statusValue );
	}

	return xml;
}

/**
* Implement clone() so Xalan can copy the function into
* its own function table.
*
* @return pointer to the new object
*/
// For compilers that do not support covariant return types,
// clone() must be declared to return the base type.
#ifdef XALAN_1_9
#if defined( XALAN_NO_COVARIANT_RETURN_TYPE )
	Function*
#else
	FunctionUrl*
#endif
FunctionUrl::clone( MemoryManagerType& theManager ) const
{
    return XalanCopyConstruct(theManager, *this);
}

const XalanDOMString& FunctionUrl::getError( XalanDOMString& theResult ) const
{
	theResult.assign( "The url function accepts only 1 argument [ url ]" );
	return theResult;
}
#else
#if defined( XALAN_NO_COVARIANT_RETURN_TYPE )
	Function*
#else
	FunctionUrl*
#endif
FunctionUrl::clone() const
{
	return new FunctionUrl( *this );
}

const XalanDOMString FunctionUrl::getError() const
{
	return StaticStringToDOMString( XALAN_STATIC_UCODE_STRING( "The url function accepts only 1 argument [ url ]" ) );
}
#endif
