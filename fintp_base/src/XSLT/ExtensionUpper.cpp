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
#include "ExtensionUpper.h"

#include <string>
#include <locale>
#include <sstream>
#include "XmlUtil.h"


using namespace std;
using namespace FinTP;
XALAN_CPP_NAMESPACE_USE

XObjectPtr FunctionUpper::execute( XPathExecutionContext& executionContext, XalanNode* context, const XObjectArgVectorType& args, const LocatorType* locator ) const
{
	if ( args.size() != 1 )
	{
		stringstream errorMessage;
		errorMessage << "The Upper() function takes one argument!, but received " << args.size();

#if (_XALAN_VERSION >= 11100) 
		executionContext.problem( XPathExecutionContext::eXPath, XPathExecutionContext::eError, XalanDOMString( errorMessage.str().c_str() ), locator, context); 
#else
		executionContext.error( XalanDOMString( errorMessage.str().c_str() ), context );
#endif
	}

	string s = localForm( ( const XMLCh* )( args[0]->str().data() ) );

	locale loc;

	for (size_t i=0; i<s.length(); ++i)
	    s[i] = toupper(s[i],loc);

	return executionContext.getXObjectFactory().createString( unicodeForm( s.c_str() ) );
}

#ifdef XALAN_1_9
#if defined( XALAN_NO_COVARIANT_RETURN_TYPE )
	Function*
#else
	FunctionUpper*
#endif
FunctionUpper::clone( MemoryManagerType& theManager ) const
{
    return XalanCopyConstruct(theManager, *this);
}

const XalanDOMString& FunctionUpper::getError( XalanDOMString& theResult ) const
{
	theResult.assign( "The upper function accepts only 1 argument" );
	return theResult;
}
#else
#if defined( XALAN_NO_COVARIANT_RETURN_TYPE )
	Function*
#else
	FunctionUpper*
#endif
FunctionUpper::clone() const
{
	return new FunctionUpper( *this );
}

const XalanDOMString FunctionUpper::getError() const
{
	return StaticStringToDOMString( XALAN_STATIC_UCODE_STRING( "The upper function accepts only 1 argument" ) );
}
#endif
