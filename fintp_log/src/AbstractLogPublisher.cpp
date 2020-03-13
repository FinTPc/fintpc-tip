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

#include "AbstractLogPublisher.h"

#include <xercesc/framework/StdOutFormatTarget.hpp>
#include <xercesc/framework/LocalFileFormatTarget.hpp>
#include <xercesc/framework/MemBufFormatTarget.hpp>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/dom/DOM.hpp>

#include <string>
#include <exception>
#include <stdexcept>
#include "XmlUtil.h"
#include "StringUtil.h"
#include "Trace.h"

using namespace std;
XERCES_CPP_NAMESPACE_USE
using namespace FinTP;

AbstractLogPublisher::AbstractLogPublisher() : m_Default( false ), m_EventFilter( EventType::Error + EventType::Info + EventType::Warning + EventType::Management )
{
	DEBUG2( ".ctor" );
}

AbstractLogPublisher::~AbstractLogPublisher()
{
	DEBUG2( ".dtor" );
}

string AbstractLogPublisher::FormatException( const AppException& except )
{
	//return SerializeToXmlStr( except );
	XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* exceptionDoc = NULL;
	string exceptionStr = "";
	
	try
	{
		// serialize exception
		exceptionDoc = SerializeToXml( except );
		exceptionStr = XmlUtil::SerializeToString( exceptionDoc );
				
		try
		{
			if ( exceptionDoc != NULL )
			{
				exceptionDoc->release();
				exceptionDoc = NULL;
			}
		}
		catch( ... )
		{
			TRACE( "Error deleting document." );
		}
	}
	catch( const std::exception& ex )
	{
		try
		{
			if( exceptionDoc != NULL )
				exceptionDoc->release();
		}
		catch( ... ){}
		
    	TRACE_GLOBAL( "Can't format exception : " << ex.what() );
		throw;
	}
	catch( const XMLException& e )
    {
    	try
		{
			if( exceptionDoc != NULL )
				exceptionDoc->release();
		}
		catch( ... ){}
		
    	TRACE_GLOBAL( "Can't format exception : " << e.getMessage() );
		throw runtime_error( localForm( e.getMessage() ) );
    }
	catch( ... )
	{
		try
		{
			if( exceptionDoc != NULL )
				exceptionDoc->release( );
		}
		catch( ... ){}

		TRACE_GLOBAL( "Can't format exception : unknown exception" );
		throw;
	}
	return exceptionStr;
}

AppException AbstractLogPublisher::DeserializeFromXml( const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument *doc )
{
	if ( doc == NULL )
		throw runtime_error( "Deserialization error : Empty document" );
		
	DOMElement* theRootElement = doc->getDocumentElement();
	if ( theRootElement == NULL )
		throw runtime_error( "Deserialization error : Empty document" );
	
	if ( ( string )( localForm( theRootElement->getNodeName() ) ) != "Status" )
		throw runtime_error( "Deserialization error : Expecting [Status] as root element of the message." );
	
	AppException ex;
	for ( DOMNode* appNode = theRootElement->getFirstChild(); appNode != 0; appNode=appNode->getNextSibling() )
	{
		string keyName = "";
		string keyValue = "";
		
		// skip non element nodes
		if ( appNode->getNodeType() != DOMNode::ELEMENT_NODE )
			continue;
				
		keyName = localForm( appNode->getNodeName() );

		//AdditionalInfo		
		if ( keyName == "AdditionalInfo" )
		{
			DEBUG2( "AdditionalInfo" );
			string keyNameAddInfo, keyValueAddInfo;
			for ( DOMNode* appNodeAddInfo = appNode->getFirstChild(); appNodeAddInfo != 0; appNodeAddInfo=appNodeAddInfo->getNextSibling() )
			{
				if ( appNodeAddInfo->getNodeType() != DOMNode::ELEMENT_NODE )
					continue;
			
				keyNameAddInfo = localForm( appNodeAddInfo->getNodeName() );
				DEBUG2( "Key : " << keyNameAddInfo );
				
				if ( appNodeAddInfo->getFirstChild() != NULL )
				{
					if ( appNodeAddInfo->getFirstChild()->getNodeType() != DOMNode::TEXT_NODE )
					{
						DEBUG2( "Node not text : " << appNodeAddInfo->getFirstChild()->getNodeType() );
						continue;
					}
					DOMText* appAddInfoText = static_cast< DOMText* >( appNodeAddInfo->getFirstChild() );
					keyValueAddInfo = localForm( appAddInfoText->getData() );
				}
				else
					keyValueAddInfo = "NULL";
				
				DEBUG_GLOBAL( "Deserialized additionalInfo : " << keyNameAddInfo << ", " << 
					( ( keyValueAddInfo.length() > 100 ) ? "-- large data --" : keyValueAddInfo ) );
				
				ex.addAdditionalInfo( keyNameAddInfo, keyValueAddInfo ); 
			}
			continue;
		}
		
		try
		{
			DOMText* appText = static_cast< DOMText* >( appNode->getFirstChild() );			
			if ( ( appText == NULL ) || ( appNode->getFirstChild()->getNodeType() != DOMNode::TEXT_NODE ) )
				continue;
				
			keyValue = localForm( appText->getData() );	
		}
		catch( ... )
		{
			continue;
		}
		
		//InnerException
		if ( keyName == "InnerException" )
		{
			DEBUG2( "InnerException :"  << keyValue );
			ex.setException( keyValue );
			continue;
		}
		
		if ( keyName == "MachineName" )
		{
			DEBUG2( "MachineName :"  << keyValue );
			ex.setMachineName( keyValue );
			continue;
		}
		if ( keyName == "CorrelationId" )
		{
			DEBUG2( "CorrelationId :"  << keyValue );
			ex.setCorrelationId( keyValue );
			continue;
		}
		if ( keyName == "CreatedDateTime" )
		{
			DEBUG2( "CDT :"  << keyValue );
			ex.setCreatedDateTime( keyValue );
			continue;
		}
		if ( keyName == "Pid" )
		{
			DEBUG2( "Pid :"  << keyValue );
			ex.setPid( keyValue );
			continue;
		}
		if ( keyName == "EventType" )
		{
			DEBUG2( "EventType :"  << keyValue );
			ex.setEventType( EventType::Parse( keyValue ) );
			continue;
		}
		if ( keyName == "Class" )
		{
			DEBUG2( "Class :"  << keyValue );
			ex.setClassType( ClassType::Parse( keyValue ) );
			continue;
		}
		if ( keyName == "Message" )
		{
			DEBUG2( "Message :"  << keyValue );
			ex.setMessage( keyValue );
			continue;
		}
	}
	DEBUG_GLOBAL( "Deserialization done" );
	return ex;
}

//serializes an exception together with all additional information to XML
string AbstractLogPublisher::SerializeToXmlStr( const AppException& except )
{
	stringstream result;
	DEBUG_GLOBAL( "Serialization begin" );

	try
	{
		result << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?><Status><MachineName>" << 
			except.getMachineName() << "</MachineName><CorrelationId>" << 
			except.getCorrelationId() << "</CorrelationId><CreatedDateTime>" << 
			except.getCreatedDateTime() << "</CreatedDateTime><Pid>" << 
			except.getPid() << "</Pid><EventType>" << 
			except.getEventType().ToString() << "</EventType><Class>"<<
			except.getClassType().ToString() << "</Class><Message>" << 
			except.getMessage() << "</Message><InnerException>" << 
			except.getException() << "</InnerException><AdditionalInfo>";
	}
	catch( ... )
	{
		TRACE( "Error serializing exception fields" );
		throw;
	}
	try
	{
		if ( except.getAdditionalInfo() != NULL )
		{
			DEBUG_GLOBAL( "Serializing " << except.getAdditionalInfo()->getCount() << " additional info in exception" );
			// add info passed as param to this function
			const vector< DictionaryEntry >& addInfoCollection = except.getAdditionalInfo()->getData();
			for( vector< DictionaryEntry >::const_iterator addinfodata = addInfoCollection.begin(); addinfodata != addInfoCollection.end(); addinfodata++ )
			{
				DEBUG_GLOBAL2( "\tSerializing entry [" << addinfodata->first << "] = [" << addinfodata->second << "]" );								
	
				string infoElemName = StringUtil::Replace( addinfodata->first, " ", "_" );
				string infoElemText = StringUtil::Replace( addinfodata->second, " ", "_" );
			
				result << "<" << infoElemName << ">" << infoElemText << "</" << infoElemName << ">";
			}
			DEBUG_GLOBAL2( "Serialization of additional info in exception done." );
		}
	}
	catch( ... )
	{
		TRACE( "Error serializing additional info" );			
		throw;
	}
	result << "</AdditionalInfo></Status>";

	return result.str();
}

XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* AbstractLogPublisher::SerializeToXml( const AppException& except )	
{
	DEBUG_GLOBAL( "Serialization begin" );
	
	XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc = NULL;
    DOMElement* rootElem = NULL;
	try
	{
	    DOMImplementation* impl = DOMImplementationRegistry::getDOMImplementation( unicodeForm( "Core" ) );
	    if( impl == NULL )
	    	throw runtime_error( "Unable to obtain DOM implementation" );
	    	
	    doc = impl->createDocument( 0, unicodeForm( "Status" ), 0 );     
	    if ( doc == NULL )
	    	throw runtime_error( "Unable to create DOM document" );            
	    	
		// ( root element namespace URI, root element name, document type object (DTD) )
		rootElem = doc->getDocumentElement();
				
		//"MachineName"
		string elementName = "MachineName";
		string elementText = except.getMachineName();
		DEBUG2( "\tSerializing entry [" << elementName << "] = [" << elementText << "]" );
					
		DOMElement* prodElem = doc->createElement( unicodeForm( elementName ) );
		rootElem->appendChild( prodElem );

		DOMText* prodDataVal = doc->createTextNode( unicodeForm( elementText ) );
		prodElem->appendChild( prodDataVal );
		
		//"CorrelationId"
		elementName = "CorrelationId";
		elementText = except.getCorrelationId();
		DEBUG2( "\tSerializing entry [" << elementName << "] = [" << elementText << "]" );
					
		prodElem = doc->createElement( unicodeForm( elementName ) );
		rootElem->appendChild( prodElem );

		prodDataVal = doc->createTextNode( unicodeForm( elementText ) );
		prodElem->appendChild( prodDataVal );
		
		//"CreatedDateTime"
		elementName = "CreatedDateTime";
		elementText = except.getCreatedDateTime();
		DEBUG2( "\tSerializing entry [" << elementName << "] = [" << elementText << "]" );
					
		prodElem = doc->createElement( unicodeForm( elementName ) );
		rootElem->appendChild( prodElem );

		prodDataVal = doc->createTextNode( unicodeForm( elementText ) );
		prodElem->appendChild( prodDataVal );
		
		//"Pid"
		elementName = "Pid";
		elementText = except.getPid();
		DEBUG2( "\tSerializing entry [" << elementName << "] = [" << elementText << "]" );
					
		prodElem = doc->createElement( unicodeForm( elementName ) );
		rootElem->appendChild( prodElem );

		prodDataVal = doc->createTextNode( unicodeForm( elementText ) );
		prodElem->appendChild( prodDataVal );
		
		//"EventType"
		elementName = "EventType";
		elementText = except.getEventType().ToString();
		DEBUG2( "\tSerializing entry [" << elementName << "] = [" << elementText << "]" );
					
		prodElem = doc->createElement( unicodeForm( elementName ) );
		rootElem->appendChild( prodElem );

		prodDataVal = doc->createTextNode( unicodeForm( elementText ) );
		prodElem->appendChild( prodDataVal );
		
		//"ClassType"
		elementName = "Class";
		elementText = except.getClassType().ToString();
		DEBUG2( "\tSerializing entry [" << elementName << "] = [" << elementText << "]" );
					
		prodElem = doc->createElement( unicodeForm( elementName ) );
		rootElem->appendChild( prodElem );

		prodDataVal = doc->createTextNode( unicodeForm( elementText ) );
		prodElem->appendChild( prodDataVal );

		//"Message"
		elementName = "Message";
		elementText = except.getMessage();
		DEBUG2( "\tSerializing entry [" << elementName << "] = [" << elementText << "]" );
					
		prodElem = doc->createElement( unicodeForm( elementName ) );
		rootElem->appendChild( prodElem );

		prodDataVal = doc->createTextNode( unicodeForm( elementText ) );
		prodElem->appendChild( prodDataVal );
		
		//"InnerException"
		elementName = "InnerException";
		elementText = except.getException();
		DEBUG2( "\tSerializing entry [" << elementName << "] = [" << elementText << "]" );
					
		prodElem = doc->createElement( unicodeForm( elementName ) );
		rootElem->appendChild( prodElem );

		prodDataVal = doc->createTextNode( unicodeForm( elementText ) );
		prodElem->appendChild( prodDataVal );
		//}
	}
	catch( ... )
	{
		TRACE( "Error serializing exception fields" );
		if ( doc != NULL )
			doc->release();
		doc = NULL;
			
		throw;
	}
	
	DOMElement* prodElem = NULL;
	try
	{
		prodElem = doc->createElement( unicodeForm( "AdditionalInfo" ) );
		rootElem->appendChild( prodElem );
		
		DEBUG_GLOBAL2( "----- serializing add info in exception" );
		if ( except.getAdditionalInfo() != NULL )
		{
			DEBUG_GLOBAL( "Serializing " << except.getAdditionalInfo()->getCount() << " additional info in exception" );
			// add info passed as param to this function
			const vector< DictionaryEntry >& addInfoCollection = except.getAdditionalInfo()->getData();
			for( vector< DictionaryEntry >::const_iterator addinfodata = addInfoCollection.begin(); addinfodata != addInfoCollection.end(); addinfodata++ )
			{
				DEBUG_GLOBAL2( "\tSerializing entry [" << addinfodata->first << "] = [" << addinfodata->second << "]" );								
	
				string infoElemName = StringUtil::Replace( addinfodata->first, " ", "_" );
				string infoElemText = StringUtil::Replace( addinfodata->second, " ", "_" );
			
				DEBUG_GLOBAL2( "Creating elem" );
				DOMElement* infoElem = doc->createElement( unicodeForm( infoElemName ) );
				prodElem->appendChild( infoElem );
	
				DEBUG_GLOBAL2( "Appending text" );
				DOMText* addInfoVal = doc->createTextNode( unicodeForm( infoElemText ) );
				infoElem->appendChild( addInfoVal );
			}
			DEBUG_GLOBAL2( "Serialization of additional info in exception done." );
		}
	}
	catch( ... )
	{
		TRACE( "Error serializing additional info" );
		if ( doc != NULL )
			doc->release();
		doc = NULL;
			
		throw;
	}
	
    return doc;
}
