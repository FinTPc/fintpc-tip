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

#include "CSVFilter.h"
#include "StringUtil.h"
#include "XmlUtil.h"
#include "Trace.h"

#include <xercesc/util/PlatformUtils.hpp>

using namespace std;
XERCES_CPP_NAMESPACE_USE
using namespace FinTP;

CSVFilter::CSVFilter( ) : AbstractFilter( FilterType::CSV )
{
}

CSVFilter::~CSVFilter()
{
}

AbstractFilter::FilterResult CSVFilter::ProcessMessage(FinTP::AbstractFilter::buffer_type inputData, XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument *outputData, NameValueCollection &transportHeaders, bool asClient)
{
	ValidateProperties();
	DEBUG( "About to aply CSV filter..." );

	if( asClient )
	{
		//fetcher 
		if( outputData == NULL )
		{
			TRACE( "Input document is empty" );
			throw runtime_error( "Input document is empty" );
		}

		try
		{
			DOMElement* firstElem = outputData->createElement( unicodeForm( "Message" ) );
			outputData->getDocumentElement()->appendChild( firstElem );
			DEBUG( "First payload element created: [Message]!" );

			ManagedBuffer* inputBuffer = inputData.get();
			char* data = NULL;
			if ( inputBuffer )
				data = reinterpret_cast<char*>( inputBuffer->buffer() );
			else
				throw runtime_error ( "Input data is empty" ); 

			int fieldNr = 0;
			bool inQuotes = false;
			string field;
			bool newField = true;

			for ( int i = 0; i < inputBuffer->size(); i++ )
			{
				if ( data[i] == '\"' )
				{
					if ( !inQuotes && !newField )
					{
						field += '\"';
						continue;
					}
					else
					{
						if ( i+1 < inputBuffer->size() )
							if ( data[ i+1 ] == '\"' && inQuotes )
							{
								field += '\"';
								i++;
								continue;
							}
						inQuotes = !inQuotes;
						newField = false;
						continue;
					}
				}

				if ( ( data[i] == '\n' && !inQuotes ) || ( data[i] == ',' && !inQuotes ) )
				{
					stringstream fieldName;
					fieldName << "Field" << fieldNr;
					DOMElement* prodElem = outputData->createElement( unicodeForm( fieldName.str() ) );
					firstElem->appendChild( prodElem );


					DOMText* prodDataVal = outputData->createTextNode( unicodeForm( StringUtil::Trim( field ) ) );
					prodElem->appendChild( prodDataVal );

					fieldNr++;
					field = "";
					newField = true;
					continue;
				}

				field += data[i];
				newField = false;
			}

			stringstream fieldName;
			fieldName << "Field" << fieldNr;
			DOMElement* prodElem = outputData->createElement( unicodeForm( fieldName.str() ) );
			firstElem->appendChild( prodElem );

			DOMText* prodDataVal = outputData->createTextNode( unicodeForm( StringUtil::Trim( field ) ) );
			prodElem->appendChild( prodDataVal );

			DEBUG( "XML conversion done. Output format is [" << XmlUtil::SerializeToString( outputData ) << "]." );

		}
		catch( const XMLException& e )
		{
			stringstream errorMessage;
			
			errorMessage << "XmlException during CSV filter apply. Type : " << ( int )e.getErrorType() << 
				" Code : [" << e.getCode() << "] Message = " << localForm( e.getMessage() ) <<
				" Line : " << e.getSrcLine();

			throw runtime_error( errorMessage.str() );
		}
		catch( const std::exception& e )
		{
			TRACE( "Error parsing / appending original node : "  << e.what() );

			throw;
		}
		catch( ... )
		{
			TRACE( "Error parsing / appending original node : Unknown exception" );

			throw;
		}
	}
	else
	{
		//publisher
	}
	return AbstractFilter::Completed;
}

AbstractFilter::FilterResult CSVFilter::ProcessMessage(FinTP::AbstractFilter::buffer_type inputData, FinTP::AbstractFilter::buffer_type outputData, NameValueCollection &transportHeaders, bool asClient)
{
	ValidateProperties();
	DEBUG( "About to aply CSV filter..." );

	if( asClient )
	{
		XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument *output = NULL;
		try
		{
			DOMImplementation* impl = DOMImplementationRegistry::getDOMImplementation( unicodeForm( "LS" ) );
			output = impl->createDocument( 0, unicodeForm( "root" ), 0 );
			DEBUG( "Root element.created!" );
			ProcessMessage( inputData, output, transportHeaders, asClient );

			string serializedDoc = XmlUtil::SerializeToString( output );
			outputData.get()->copyFrom( serializedDoc );

			DEBUG( "Transformed done. Output format is [" << serializedDoc << "]." );
			if ( output != NULL )
			{
				output->release();
				output = NULL;
			}
		}
		catch( ... )
		{
			if ( output != NULL )
			{
				output->release();
				output = NULL;
			}
		}
	}
	else
	{
	}

	return AbstractFilter::Completed;
}

void CSVFilter::ValidateProperties()
{
}

bool CSVFilter::isMethodSupported( AbstractFilter::FilterMethod method, bool asClient )
{	
	switch( method )
	{
		case AbstractFilter::BufferToXml :
			return true;
		case AbstractFilter::BufferToBuffer :
			return true;
		default:
			return false;
	}
}
