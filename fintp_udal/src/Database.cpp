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

#include "Database.h"

#include "Trace.h"
#include "StringUtil.h"
#include "XmlUtil.h"

#include <xercesc/dom/DOM.hpp>
XERCES_CPP_NAMESPACE_USE
#include <sstream>
#include <string>
#include <iostream>
using namespace std;

#ifdef WIN32
#define __MSXML_LIBRARY_DEFINED__
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace FinTP;

string Database::DateFormat = "DD.MM.YYYY";
string Database::TimestampFormat = "DD.MM.YYYY HH:MI";

Database::Database() : m_LastErrorCode( "" ), m_LastNumberofAffectedRows( 0 )
{
}

Database::~Database()
{
	try
	{
		m_StatementCache.clear();
	}
	catch( ... ) {}
}

XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* Database::ConvertToXML( const DataSet* theDataSet, const bool doTrimm )
{	
	if ( theDataSet == NULL )
		throw runtime_error( "Unable to convert empty dataset to XML" );

	XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc = NULL;
	try
	{
		// create the document 
		DOMImplementation* impl = DOMImplementationRegistry::getDOMImplementation( unicodeForm( "LS" ) );

		//assign the root element
		doc = impl->createDocument( 0, unicodeForm( "TableName" ), 0 );
		
		DOMElement* rootElem = doc->getDocumentElement();
		
		DOMElement* secondElem = doc->createElement( unicodeForm( "Record" ) );
		rootElem->appendChild( secondElem );
		
		//cout << "No of records " << theDataSet->size() << endl;
		//for every row in the DataSet
		for( unsigned int i=0; i<theDataSet->size(); i++ )
		{
			map< std::string, DataColumnBase* >::iterator myIterator = ( *theDataSet )[ i ]->begin();
			for ( ; myIterator != ( *theDataSet )[ i ]->end(); myIterator++ )
			{
				string columnName = myIterator->first;
				DataColumnBase* crtColumn = myIterator->second;
				
				stringstream elementText;
				switch ( crtColumn->getBaseType() )
				{
					case DataType::SHORTINT_TYPE:
						{
							DataColumn< short >* crtTypedColumn = dynamic_cast< DataColumn< short >* >( crtColumn );
							if ( crtTypedColumn == NULL )
								elementText << "Bad_cast_to_DataColumn_short";
							else
								elementText << crtTypedColumn->getValue();	
							break;
						}

					case DataType::CHAR_TYPE:
						{
							DataColumn< string >* crtTypedColumn = dynamic_cast< DataColumn< string >* >( crtColumn );
							if ( crtTypedColumn == NULL )
								elementText << "Bad_cast_to_DataColumn_string";
							else
							{
//								DEBUG( "Serializing CHAR column [" << columnName << "] of base type [" << crtColumn->getBaseType() << "]" );
								elementText << crtTypedColumn->getValue();	
							}
							break;
						}

					case DataType::LONGINT_TYPE:
						{
							DataColumn< long >* crtTypedColumn = dynamic_cast< DataColumn< long >* >( crtColumn );
							if ( crtTypedColumn == NULL )
							{
								DataColumn< short >* crtTypedColumn2 = dynamic_cast< DataColumn< short >* >( crtColumn );
								if ( crtTypedColumn2 == NULL )
									elementText << "Bad_cast_to_DataColumn_long_or_short";
								else
									elementText << crtTypedColumn2->getValue();	
							}
							else
								elementText << crtTypedColumn->getValue();	
							break;
						}

					default:
						DEBUG( "Warning : skipped a column from conversion to XML [ not a supported type ]" );
						break;
				}		
				
				DOMElement* prodElem = doc->createElement( unicodeForm( columnName ) );
				secondElem->appendChild( prodElem );

				string elementTextValue = elementText.str();
				if( doTrimm )
					elementTextValue = StringUtil::Trim( elementTextValue );
				DOMText* prodDataVal = doc->createTextNode( unicodeForm( elementTextValue ) );			
				prodElem->appendChild( prodDataVal );

				// add an attribute datatype : dt:dt="bin.base64"
				// see http://www.xml.com/pub/a/98/07/binary/binary.html
				if ( crtColumn->getType() == DataType::BINARY )
				{
					//DOMAttr	*prodAttr = doc->createAttributeNS( unicodeForm( "urn:schemas-microsoft-com:datatypes" ), unicodeForm( "dt:dt" ) );
					DOMAttr *prodAttr = doc->createAttribute( unicodeForm( "dt" ) );
					prodAttr->setValue( unicodeForm( "bin.base64" ) );

					prodElem->setAttributeNodeNS( prodAttr );			
				}
			}
		}		
		return doc;
	}
	catch( const XMLException& e )
  	{
  		if( doc != NULL )
  			doc->release();
  		TRACE_GLOBAL( "XMLException " << e.getMessage() );
  		
  		throw runtime_error( localForm( e.getMessage() ) );
  	}
  	catch( const DOMException& e )
  	{
		if( doc != NULL )
			doc->release();
			
  		TRACE_GLOBAL( "DOMException " << e.code );
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
		TRACE_GLOBAL( messageBuffer.str() );

    	//throw runtime_error( "Parse error" );
    	throw runtime_error( messageBuffer.str() );
  	}
	catch( ... )
	{
		if( doc != NULL )
  			doc->release();
		TRACE_GLOBAL( "Unhandled exception" );
		
		throw;
	}    
}

void Database::DisplayDataSet( const DataSet* theDataSet )
{
	DEBUG_GLOBAL( "********************** Display dataset **********************" );
	DEBUG_GLOBAL( "Dataset rows number : " << theDataSet->size() );
	DEBUG_GLOBAL( "DataSet columns number : " << ( *theDataSet )[ 0 ]->size() );

	for( unsigned int i=0; i<theDataSet->size(); i++ )
	{
		//For every column in the row
		map< std::string, DataColumnBase* >::iterator myIterator = ( *theDataSet )[ i ]->begin();

		for ( ; myIterator != ( *theDataSet )[ i ]->end(); myIterator++ )
		{
			//Display the column information
			DataColumnBase *crtColumn = myIterator->second;

			DEBUG_GLOBAL2( "Column [" << myIterator->first << " ] : Dimension : " <<
			               crtColumn->getDimension() << "; Scale : " << crtColumn->getScale() );

			switch ( crtColumn->getBaseType() )
			{
				case DataType::SHORTINT_TYPE :
					DEBUG( "Type SHORT; Value : "  << ( ( DataColumn< short >* )crtColumn )->getValue() );
					break;

				case DataType::CHAR_TYPE :
					DEBUG( "Type CHAR; Value : " << ( ( DataColumn< string >* )crtColumn )->getValue() );
					if (crtColumn->getStoragePointer() != NULL)
						DEBUG("*Value : " << *( char * )( crtColumn->getStoragePointer() ) );
					break;

				case DataType::LONGINT_TYPE:
					DEBUG( "Type INT; Value : "  << ( ( DataColumn< long >* )crtColumn )->getValue() );
					break;

				default :
					DEBUG( "Type " << crtColumn->getType() << "; Skipped..." );
					break;
			}
		}
	}
	DEBUG_GLOBAL( "********************** End of display dataset **********************" );
}

DBWarningException::DBWarningException( const string& argumentName, const EventType::EventTypeEnum eventType, const NameValueCollection* additionalInfo ) :
	AppException( argumentName, eventType, additionalInfo )
{
	stringstream errorMessage;
	errorMessage << "Database operation warning [" << argumentName << "] ";
	m_Message = errorMessage.str();
}

DBErrorException::DBErrorException( const string& argumentName, const EventType::EventTypeEnum eventType, const NameValueCollection* additionalInfo )
	:AppException( argumentName, eventType, additionalInfo )
{
	stringstream errorMessage;
	errorMessage << "Database operation error [" << argumentName << "]";
	m_Message = errorMessage.str();
}

DBNoUpdatesException::DBNoUpdatesException( const string& argumentName, const EventType::EventTypeEnum eventType, const NameValueCollection* additionalInfo ) :
	AppException( argumentName, eventType, additionalInfo )
{
	stringstream errorMessage;
	errorMessage << "Database operation warning [" << argumentName << "] ";
	m_Message = errorMessage.str();
}

DBConnectionLostException::DBConnectionLostException(  const string& argumentName, const EventType::EventTypeEnum eventType, const NameValueCollection* additionalInfo )
	:AppException( argumentName, eventType, additionalInfo )
{
	stringstream errorMessage;
	errorMessage << "Lost connection to database [" << argumentName << "]";
	m_Message = errorMessage.str();
}

string DBErrorException::code() const
{
	return m_Code;
}

void DBErrorException::setCode( const string& excode )
{
	m_Code = excode;
}
