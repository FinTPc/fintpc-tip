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

#include <memory>

#include <xalanc/Include/PlatformDefinitions.hpp>

#include <xercesc/util/PlatformUtils.hpp>
#include <xalanc/XalanTransformer/XalanTransformer.hpp>
#include <xalanc/XPath/XObjectFactory.hpp>
#include "ExtensionEscape.h"

#include <curl/curl.h>

#include <string>
#include <sstream>
#include "XmlUtil.h"
#include "TimeUtil.h"
#include "StringUtil.h"

using namespace std;
using namespace FinTP;
XALAN_CPP_NAMESPACE_USE

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
XObjectPtr FunctionEscape::execute( XPathExecutionContext& executionContext, XalanNode* context, const XObjectArgVectorType& args, const LocatorType* locator ) const
{
	if ( args.size() > 2 )
	{
		stringstream errorMessage;
		errorMessage << "The escape() function takes at most 2 arguments! [string, bool], but received " << args.size();
#if (_XALAN_VERSION >= 11100)
		executionContext.problem( XPathExecutionContext::eXPath, XPathExecutionContext::eError, XalanDOMString( errorMessage.str().c_str() ), locator, context);
#else
		executionContext.error( XalanDOMString( errorMessage.str().c_str() ), context );
#endif
	}

	string argument = XmlUtil::XMLChtoString( args[0]->str().data() );
	bool escape = true;
	if ( args.size() == 2 )
		if ( StringUtil::ToLower( XmlUtil::XMLChtoString( args[1]->str().data() ) ) == "false" )
			escape = false;

	string result;

	std::unique_ptr<CURL, void (*)(CURL *)> curl( curl_easy_init(), curl_easy_cleanup );

	if( curl != NULL )
	{
		vector<char> error( CURL_ERROR_SIZE );
		CURLcode code = curl_easy_setopt( curl.get(), CURLOPT_ERRORBUFFER, error.data() );
		if ( code != CURLE_OK )
		{
			stringstream errorMessage;
			errorMessage << "Failed to set error buffer [" << code << "]";

			throw runtime_error( errorMessage.str() );
		}

		std::unique_ptr<char, void (*)(void *)> output( NULL, curl_free );

		if ( escape )
		{
			output.reset( curl_easy_escape( curl.get(), argument.c_str(), argument.size() ) );
			if ( output )
				result = output.get();
			else
				throw runtime_error( string( error.begin(), error.end() ) );
		}
		else
		{
			int outLength;
			output.reset( curl_easy_unescape( curl.get(), argument.c_str(), argument.size(), &outLength ) );
			if ( output )
				result = string( output.get(), outLength );
			else
				throw runtime_error( string( error.begin(), error.end() ) );
		}
	}
	else
		throw runtime_error( "Failed to create libcurl easy session" );

	return executionContext.getXObjectFactory().createString( unicodeForm( result ) );
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
	FunctionEscape*
#endif
FunctionEscape::clone( MemoryManagerType& theManager ) const
{
    return XalanCopyConstruct(theManager, *this);
}

const XalanDOMString& FunctionEscape::getError( XalanDOMString& theResult ) const
{
	theResult.assign( "The escape function accepts only 1 arg [string]" );
	return theResult;
}
#else
#if defined( XALAN_NO_COVARIANT_RETURN_TYPE )
	Function*
#else
	FunctionEscape*
#endif
FunctionEscape::clone() const
{
	return new FunctionEscape( *this );
}

const XalanDOMString FunctionEscape::getError() const
{
	return StaticStringToDOMString( XALAN_STATIC_UCODE_STRING( "The escape function accepts only 1 arg [string]" ) );
}
#endif
