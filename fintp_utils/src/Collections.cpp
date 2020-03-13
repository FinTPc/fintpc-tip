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

#include <iostream>
#include <sstream>

#include "Collections.h"
#include "XmlUtil.h"
#include "StringUtil.h"
#include "Log.h"

using namespace FinTP;

/// dump the contents of this collection

NameValueCollection::NameValueCollection() : m_Data() 
{
}

NameValueCollection::NameValueCollection( const NameValueCollection& source )
{
	m_Data.clear();
	m_Data = source.m_Data;
}

NameValueCollection& NameValueCollection::operator=( const NameValueCollection& source )
{
	if ( this == &source )
		return *this;

	m_Data.clear();
	m_Data = source.m_Data;
	return *this;
}

NameValueCollection::~NameValueCollection() 
{
	try
	{
		m_Data.clear();
	}
	catch( ... )
	{
		try
		{
			TRACE_LOG( "An error occured while clearing data" );
		} catch( ... ){}
	}
}

const string NameValueCollection::operator[]( const string& name ) const
{
	vector< DictionaryEntry >::const_iterator dataWalker = m_Data.begin();
	while( dataWalker != m_Data.end() )
	{
		if ( dataWalker->first == name )
			return dataWalker->second;
		dataWalker++;
	}
	return ""; //Diff from map
}

DictionaryEntry& NameValueCollection::operator[]( const unsigned int index )
{
	return m_Data[ index ];
}

const DictionaryEntry& NameValueCollection::operator[]( const unsigned int index ) const
{
	return m_Data[ index ];
}

void NameValueCollection::ChangeValue( const string &name, const string &value )
{
	vector< DictionaryEntry >::iterator dataWalker = m_Data.begin();
	while( dataWalker != m_Data.end() )
	{
		if ( dataWalker->first == name )
		{
			dataWalker->second = value;
			return;
		}
		dataWalker++;
	}
	DictionaryEntry entry = make_pair( name, value );
	m_Data.push_back( entry );
}

void NameValueCollection::Add( const char* nameCh, const char* valueCh )
{
	string name = string( nameCh );
	string value = string( valueCh );
	
	DictionaryEntry entry = make_pair( name, value );
	m_Data.push_back( entry );
}

void NameValueCollection::Add( const char* nameCh, const string& value )
{
	string name = string( nameCh );
	
	DictionaryEntry entry = make_pair( name, value );
	m_Data.push_back( entry );
}
		
void NameValueCollection::Add( const string& name, const char* valueCh )
{
	string value = string( valueCh );
			
	DictionaryEntry entry = make_pair( name, value );
	m_Data.push_back( entry );
}

void NameValueCollection::Add( const string& name, const string& value )
{
	DictionaryEntry entry = make_pair( name, value );
	m_Data.push_back( entry );
}

void NameValueCollection::Remove( const string& name )
{
	vector< DictionaryEntry >::iterator dataWalker = m_Data.begin();
	while( dataWalker != m_Data.end() )
	{
		if ( dataWalker->first == name )
		{
			( void )m_Data.erase( dataWalker );
			break;
		}
		dataWalker++;
	}
}

void NameValueCollection::Clear()
{
	m_Data.clear();
}

bool NameValueCollection::ContainsKey( const string& name ) const
{
	vector< DictionaryEntry >::const_iterator dataWalker = m_Data.begin();
	while( dataWalker != m_Data.end() )
	{
		if ( dataWalker->first == name )
			return true;
		dataWalker++;
	}
	return false;
}

//vector< DictionaryEntry > getData() const { return m_Data; }

void NameValueCollection::Dump() const
{
	unsigned int dataSize = m_Data.size();
	
	DEBUG_LOG( dataSize << " entries allocated." );
	for( unsigned int i=0; i<dataSize; i++ )
	{
		DEBUG_LOG( "\tEntry[ " << i << " ] = ( " << m_Data[ i ].first << ", " << m_Data[ i ].second << " )" );
	}
}

string NameValueCollection::ToString() const
{
	stringstream dump;
	for( unsigned int i=0; i<m_Data.size(); i++ )
	{
		dump << "[" << m_Data[ i ].first << "] = [" << m_Data[ i ].second << "]; ";
	}
	return dump.str();
}

string NameValueCollection::ConvertToXml( const string& rootName ) const
{
	XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc = NULL;
	string returnValue = "";
	try
	{
		// create the document 
		DOMImplementation* impl = DOMImplementationRegistry::getDOMImplementation( unicodeForm( "LS" ) );

		//assign the root element
		doc = impl->createDocument( 0, unicodeForm( rootName ), 0 );
		
		DOMElement* rootElem = doc->getDocumentElement();

		for( unsigned int i=0; i<m_Data.size(); i++ )
		{
			DOMElement* prodElem = doc->createElement( unicodeForm( m_Data[ i ].first ) );
			rootElem->appendChild( prodElem );

			DOMText* prodDataVal = doc->createTextNode( unicodeForm( StringUtil::Trim( m_Data[ i ].second ) ) );
			prodElem->appendChild( prodDataVal );
		}

		returnValue = XmlUtil::SerializeToString( doc );

		if( doc != NULL )
		{
			doc->release();	
			doc = NULL;
		}
	}
	catch( const XMLException& e )
  	{
  		if( doc != NULL )
  			doc->release();
  		TRACE_LOG( "XMLException " << e.getMessage() );
  		
  		throw runtime_error( localForm( e.getMessage() ) );
  	}
  	catch( const DOMException& e )
  	{
		if( doc != NULL )
			doc->release();
			
  		TRACE_LOG( "DOMException " << e.code );
  		string message;
  		
  		const unsigned int maxChars = 2047;
		
		XMLCh errText[ maxChars + 1 ];
      
		// attemt to read message text
    	if ( DOMImplementation::loadDOMExceptionMsg( e.code, errText, maxChars ) )
    	{
			message = localForm( errText );
		}
  		else
  		{
  			// format error code
  			stringstream messageBuffer;
  			messageBuffer << "DOMException error code : [ " << ( int )e.code << "]";
    		message = messageBuffer.str();
    	}		
 		throw runtime_error( message );
 	}
	catch( const std::exception& e )
	{
		if( doc != NULL )
  			doc->release();
  			
		stringstream messageBuffer;
	  	messageBuffer << typeid( e ).name() << " exception [" << e.what() << "]";
		TRACE_LOG( messageBuffer.str() );

    	//throw runtime_error( "Parse error" );
    	throw runtime_error( messageBuffer.str() );
  	}
	catch( ... )
	{
		if( doc != NULL )
  			doc->release();
		TRACE_LOG( "Unhandled exception" );
		
		throw;
	}    
	return returnValue;
}
