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

#include <sstream>
#include <errno.h>

#include "XmlUtil.h"
#include "Trace.h"
#include "Base64.h"
#include "BatchFlatfileStorage.h"

#include <boost/filesystem.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/regex.hpp>
#include <boost/scoped_ptr.hpp>

using namespace FinTP;

BatchFlatfileStorage::BatchFlatfileStorage() : BatchStorageBase(), m_CrtIndex( 0 ), m_CrtSequence( 0 ), m_ChunkSize( 100000 ), m_BufferLength( 0 ), m_Buffer( NULL ), m_LastMatchEnd( 0 ), m_ProcessedHeader( false )
{
}

BatchFlatfileStorage::~BatchFlatfileStorage()
{
	try
	{
		if( m_CrtStorage.is_open() )
		{
			m_CrtStorage.close();
		}
	} 
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while closing batch storage" );
		} catch ( ... ){}
	}

	if ( m_Buffer != NULL )
		delete[] m_Buffer;
}

void BatchFlatfileStorage::setTemplate( const string& templateFilename )
{
	m_Parser = TemplateParserFactory::getParser( templateFilename );
}	

void BatchFlatfileStorage::enqueue( BatchResolution& resolution )
{
	DEBUG( "Enqueue" );
	m_CrtStorage << resolution.getItem().getPayload() << flush;
}

BatchItem BatchFlatfileStorage::dequeueBuffer()
{
	bool found = false;

	while ( found == false )
	{
		if ( m_NextItem.getSequence() == BatchItem::INVALID_SEQUENCE )
			throw runtime_error( "Attempt to read past the end of the batch file" );

		BatchItem item( m_NextItem );

		if ( m_Parser.getTemplateFile().empty() )
		{
			if ( m_MemCrtStorage.empty() )
			{
				item.setSequence( BatchItem::LAST_IN_SEQUENCE );
				item.setLast();
				m_NextItem.setSequence( BatchItem::INVALID_SEQUENCE );

				return item;
			}

			if ( m_ChunkSize <= m_MemCrtStorage.size() )
			{
				m_NextItem.setSequence( m_CrtSequence++ );
				m_NextItem.setMessageId( Collaboration::GenerateGuid() );

				m_NextItem.setPayload( string( m_MemCrtStorage.begin(), m_MemCrtStorage.begin() + m_ChunkSize ) );
				m_MemCrtStorage.erase( m_MemCrtStorage.begin(), m_MemCrtStorage.begin() + m_ChunkSize );

				return item;
			}
			else
				throw runtime_error( "Batch file contains no usable data" );
		}
		else
		{
			DOMImplementation* implementation = DOMImplementationRegistry::getDOMImplementation( unicodeForm( "Core" ) );
			boost::scoped_ptr<XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument> outputData ( implementation->createDocument(
				0,						// root element namespace URI.
				unicodeForm( "root" ),	// root element name
				0 ));					// document type object (DTD).

			if ( m_MemCrtStorage.empty() || m_CrtIndex == m_MemCrtStorage.size() )
			{
				item.setSequence( BatchItem::LAST_IN_SEQUENCE );
				item.setLast();
				m_NextItem.setSequence( BatchItem::INVALID_SEQUENCE );
				return item;
			}

			DOMElement* rootElem = outputData->createElement( unicodeForm( "Message" ) );
			DEBUG( "Root element created." );
				
			outputData->getDocumentElement()->appendChild( rootElem );
			DEBUG( "Root node appended." );

			bool matched = false;

			ManagedBuffer inputBuffer( m_MemCrtStorage.data() + m_CrtIndex , ManagedBuffer::Ref, m_MemCrtStorage.size() - m_CrtIndex );
			matched = m_Parser.Matches( inputBuffer, rootElem );

			DEBUG( "Match " << ( matched ? "successful" : "failed" ) );

			if ( !matched )
				throw runtime_error ( "Batch file contains no usable data" );
			else
			{
				m_NextItem.setSequence( m_CrtSequence++ );
				m_NextItem.setMessageId( Collaboration::GenerateGuid() );

				m_CrtIndex += m_Parser.getMatchEnd();

				DEBUG( "End point for match is : " << m_Parser.getMatchEnd() );

				// from root( Message ) -> child ( template format ) -> child ( selection friendly name ) -> attribute ( data )
				if( rootElem == NULL )
					throw logic_error( "Root empty" );
				if( rootElem->getFirstChild() == NULL )
					throw logic_error( "Template child empty" );
				if( rootElem->getFirstChild()->getFirstChild() == NULL )
				{
					TRACE( XmlUtil::SerializeToString( rootElem ) );
					throw logic_error( "Template child2 empty" );
				}

				DOMNamedNodeMap *attributes = rootElem->getFirstChild()->getFirstChild()->getAttributes();
				if ( attributes != NULL )
				{
					DEBUG( "Number of attributes for selection : " << attributes->getLength() );

					string value;
					bool ignore = false;
					for ( unsigned int i=0; i<attributes->getLength(); i++ )
					{
						DOMAttr *attribute = dynamic_cast< DOMAttr * >( attributes->item( i ) );
						if ( attribute == NULL )
							continue;

						string attributeName = localForm( attribute->getName() );
						string attributeValue = localForm( attribute->getValue() );
					
						string attributeValueToLog = ( attributeValue.length() > 100 ) ? attributeValue.substr( 0, 100 ) + string( "..." ) : attributeValue;
						DEBUG( "Attribute [" << attributeName << "] = [" << attributeValueToLog << "]" );

						if ( attributeName  == "ignore" && attributeValue.empty() == false )
							ignore = true;

						if ( attributeName == "value" && attributeValue.empty() == false )
							value = attributeValue;
					}

					if ( ignore )
					{
						if ( value.empty() )
							found = false;
						else
							throw runtime_error ( "Can't ignore non empty value" );
					}

					if ( value.empty() == false )
					{
						found = true;
						m_NextItem.setPayload( value );
					}
				}
			}

			if ( found )
				return item;
		}
	}
	throw runtime_error ( "Batch file contains no usable data" );
}

BatchItem BatchFlatfileStorage::dequeue()
{
	if ( m_CrtStorageId.empty() )
		return dequeueBuffer();
	
	if( !m_CrtStorage.is_open() )
	{
		stringstream errorMessage;
		errorMessage << "Batch file [" << m_CrtStorageId << "] was not opened";
		
		TRACE( errorMessage.str() );
		throw runtime_error( errorMessage.str() );
	}

	bool found = false;
	while ( found == false )
	{
		DEBUG( "Dequeue" );

		if ( m_NextItem.getSequence() == BatchItem::INVALID_SEQUENCE )
		{
			TRACE( "Attempt to read past the end of the batch file" );
			throw runtime_error( "Attempt to read past the end of the batch file" );
		}
	
		// alloc buffer and perform a first read
		DEBUG( "Buffer is " << ( ( m_Buffer == NULL ) ? "empty" : "available " ) );
	
		if ( m_Buffer == NULL )
		{
			DEBUG( "Allocating buffer [" << m_ChunkSize << "] bytes ..." );
			m_Buffer = new unsigned char[ m_ChunkSize + 1 ];
			m_CrtIndex = 0;
			memset( m_Buffer, 0, m_ChunkSize + 1 );
		
			m_CrtStorage.read( ( char * )m_Buffer, m_ChunkSize );
			m_BufferLength = m_CrtStorage.gcount();

			string bufferEx = "";
			if ( m_BufferLength > 100 )
				bufferEx = string( ( char * )m_Buffer, 100 ) + string( "..." );
			else
				bufferEx = string( ( char * )m_Buffer );
			DEBUG( "Position in file is [" << m_BufferLength << "]. Buffer is [" << bufferEx << "]" );
		}

		if ( !m_ProcessedHeader && !m_HeaderRegex.empty() )
		{
			boost::regex re( m_HeaderRegex );
			boost::cmatch matches;
			char* buff = reinterpret_cast<char*>( m_Buffer );
			if ( boost::regex_search( buff, matches, re ) )
			{
				if ( matches[0].first != buff )
					throw runtime_error( "Matched header does not start at the beginning of the file" );
				auto headerEnd = matches[0].length();
				const string foundHeader = string( buff, buff + headerEnd );
				DEBUG( "Found header: " << foundHeader )
				memset( m_Buffer, 0, m_ChunkSize + 1 );
				m_CrtStorage.clear();
				m_CrtStorage.seekg( headerEnd, m_CrtStorage.beg );
				m_CrtStorage.read( buff, m_ChunkSize );
				m_BufferLength = m_CrtStorage.gcount();
			}
			m_ProcessedHeader = true;
		}

		BatchItem item( m_NextItem );

		XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* outputData = NULL;
		DOMImplementation* implementation = DOMImplementationRegistry::getDOMImplementation( unicodeForm( "Core" ) );
		outputData = implementation->createDocument(
			0,						// root element namespace URI.
			unicodeForm( "root" ),	// root element name
			0 );					// document type object (DTD).

		try
		{
			DOMElement* rootElem = outputData->createElement( unicodeForm( "Message" ) );
			DEBUG( "Root element created." );
				
			outputData->getDocumentElement()->appendChild( rootElem );
			DEBUG( "Root node appended." );

			bool matched = false;
		
			// if there is no parser ( no template ) just base64 what we read and consider a match
			if ( m_Parser.getTemplateFile().length() == 0 )
			{
				matched = ( m_BufferLength > 0 );

				/*// quick workaround xmling
				string base64Value = "<bin>" + Base64::encode( m_Buffer, m_BufferLength ) + "</bin>";  

				//recreate the template structure as if there was one
				XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc = rootElem->getOwnerDocument();
				DOMElement* element = doc->createElement( unicodeForm( "dummy" ) );
				rootElem->appendChild( element );

				DOMElement* secelement = doc->createElement( unicodeForm( "dummy" ) );
				element->appendChild( secelement );

				secelement->setAttribute( unicodeForm( "base64" ), unicodeForm( base64Value ) );*/
			}
			else
			{
				DEBUG( "Attempting match of batch template at index [" << m_CrtIndex << "] ..." );
				try
				{
					ManagedBuffer inputBuffer( m_Buffer + m_CrtIndex, ManagedBuffer::Ref, m_BufferLength - m_CrtIndex );
					matched = m_Parser.Matches( inputBuffer, rootElem );
				}
				catch( ... )
				{
					TRACE( "Match failed" );
					throw;
				}
			}
			DEBUG( "Match " << ( matched ? "successful" : "failed" ) );

			if ( !matched )
			{
				DEBUG( "Template doesn't match .." );
				// if end of file, no more reads
				if ( m_CrtStorage.eof() )
				{
					DEBUG( "EOF" );
					// no items read
					if ( m_CrtSequence == 0 )
					{
						DEBUG( "Current sequence is 0" );
						stringstream errorMessage;
						errorMessage << "Batch file [" << m_CrtStorageId << "] contains no usable data";
						TRACE( errorMessage.str() );
						throw runtime_error( errorMessage.str() );
					}
					DEBUG( "Current sequence is [" << m_CrtSequence << "] at index [" << m_CrtIndex << "]. Buffer size [" << m_BufferLength << "]" );

					if ( m_CrtIndex == m_BufferLength )
					{
						item.setSequence( BatchItem::LAST_IN_SEQUENCE );
						item.setLast();
						m_NextItem.setSequence( BatchItem::INVALID_SEQUENCE );

						found = true;
					}
					else
					{
						stringstream errorMessage;
						errorMessage << "Batch file [" << m_CrtStorageId << "] contains no usable data";
						TRACE( errorMessage.str() );
						throw runtime_error( errorMessage.str() );
					}

				}
				// is not end of file, but template doesn't match
				// reason : wrong format or Chunksize is not multiple of message size
				// solution:
				// 		repositoning in the file back to the end of the last match and delete the buffer and dequeue again
				//		if match is OK the file is parsed with next chunk
				//		if it was a problem with the format the first match failed, crtIndex will be 0 and will throw an exception
				//
				else
				{
					if ( m_CrtIndex == 0 )
					{
						DEBUG( "Current position in chunk is "  );
						stringstream errorMessage;
						errorMessage << "Batch file [" << m_CrtStorageId << "] contains no usable data";
						TRACE( errorMessage.str() );
						throw runtime_error( errorMessage.str() );	
					}
					else
					{
						//repositioning in the file to the end of the last match 
						DEBUG( "Current position in chunk is " << m_CrtIndex );
						const streamoff offset = (streampos)m_CrtIndex - (streampos)m_ChunkSize;
						m_CrtStorage.seekg( offset, ios::cur );
						// reset the chunk
						if ( m_Buffer != NULL )
						{
							delete[] m_Buffer;
							m_Buffer = NULL;
						}
						//try with next chunk or reset the chunk and verify if it is bad formatted storage
						found = false;
					
					}
				}
				// realloc buffer
			}
			else // template matched
			{
				m_NextItem.setSequence( m_CrtSequence++ );
				m_NextItem.setBatchId( m_CrtStorageId );
				m_NextItem.setMessageId( Collaboration::GenerateGuid() );

				if ( m_Parser.getTemplateFile().length() == 0 )
				{
					// quick workaround xmling
					string base64Value = Base64::encode( m_Buffer, m_BufferLength );
					rootElem->setAttribute( unicodeForm( "base64" ), unicodeForm( base64Value ) );

					m_NextItem.setPayload( XmlUtil::SerializeToString( outputData ) );

					// reset the chunk
					if ( m_Buffer != NULL )
					{
						delete[] m_Buffer;
						m_Buffer = NULL;
					}

					found = true;
				}
				else
				{
					m_CrtIndex += m_Parser.getMatchEnd();
					if ( m_CrtIndex == m_ChunkSize )
					{
						//we may have matched an incomplete message, it's best to revert to our last match and try again
						const streamoff offset = (streampos)m_LastMatchEnd - (streampos)m_CrtIndex;
						m_CrtStorage.seekg( offset, ios::cur );
						delete[] m_Buffer;
						m_Buffer = NULL;
						continue;
					}
					m_LastMatchEnd = m_CrtIndex;
			
					DEBUG( "End point for match is : " << m_CrtIndex );
			
					// from root( Message ) -> child ( template format ) -> child ( selection friendly name ) -> attribute ( data )
					if( rootElem == NULL )
						throw logic_error( "Root empty" );
					if( rootElem->getFirstChild() == NULL )
						throw logic_error( "Template child empty" );
					if( rootElem->getFirstChild()->getFirstChild() == NULL )
					{
						TRACE( XmlUtil::SerializeToString( rootElem ) );
						throw logic_error( "Template child2 empty" );
					}

					DOMNamedNodeMap *attributes = rootElem->getFirstChild()->getFirstChild()->getAttributes();
					if ( attributes != NULL )
					{
						DEBUG( "Number of attributes for selection : " << attributes->getLength() );

						string value;
						bool ignore = false;
						for ( unsigned int i=0; i<attributes->getLength(); i++ )
						{
							DOMAttr *attribute = dynamic_cast< DOMAttr * >( attributes->item( i ) );
							if ( attribute == NULL )
								continue;

							string attributeName = localForm( attribute->getName() );
							string attributeValue = localForm( attribute->getValue() );

							if ( attributeName == "value" &&  attributeValue.empty() == false )
								value = attributeValue;

							if ( attributeName == "ignore" && attributeValue.empty() == false )
								ignore = true;

							string attributeValueToLog = ( attributeValue.length() > 100 ) ? attributeValue.substr( 0, 100 ) + string( "..." ) : attributeValue;
							DEBUG( "Attribute [" << attributeName << "] = [" << attributeValueToLog << "]" );
						}

						if ( ignore )
						{
							if ( value.empty() )
								found = false;
							else
								throw runtime_error ( "Can't ignore non empty value" );
						}

						if ( value.empty() == false )
						{
							found = true;
							m_NextItem.setPayload( value );
						}
					}
				}
			}
			if ( outputData != NULL )
			{
				outputData->release();
				outputData = NULL;
			}
		}
		catch( ... )
		{
			if ( outputData != NULL )
			{
				outputData->release();
				outputData = NULL;
			}
			throw;
		}
		if ( found )
			return item;
	}
	throw runtime_error ( "Batch file contains no usable data" );
}

void BatchFlatfileStorage::close( const string& storageId )
{
	if ( m_CrtStorage.is_open() )
	{
		m_CrtStorage.close();
		m_CrtStorage.clear();
	}

	//clear vector capacity
	vector<unsigned char> vec;
	m_MemCrtStorage.swap( vec );


	m_NextItem.setSequence( BatchItem::FIRST_IN_SEQUENCE );
}

void BatchFlatfileStorage::open( const string& FileContents )
{
	if ( FileContents.empty() )
		throw runtime_error( "Empty file" );

	m_MemCrtStorage.assign( FileContents.begin(), FileContents.end() );
	m_Buffer = &m_MemCrtStorage[0];
	m_BufferLength = m_MemCrtStorage.size();

	m_CrtIndex = 0;
	m_LastMatchEnd = 0;
	m_CrtSequence = BatchItem::FIRST_IN_SEQUENCE;
	m_ProcessedHeader = false;

	dequeueBuffer();
}

// open storage
void BatchFlatfileStorage::open( const string& storageId, ios_base::openmode openMode )
{
	// if the crt. storage is open ...
	if( m_CrtStorage.is_open() )
	{
		// if it's not the same storage, close the previous 
		if ( m_CrtStorageId != storageId )
			m_CrtStorage.close();
		else
			return;
	}
	m_ProcessedHeader = false;
	m_CrtIndex = 0;
	m_CrtSequence = 0;
	m_CrtStorageId = storageId;
	if ( m_Buffer != NULL )
	{
		delete[] m_Buffer;
		m_Buffer = NULL;
	}

	// open the requested storage
	m_CrtStorage.open( storageId.c_str(), openMode );
	//if ( ( !m_CrtStorage ) || !m_CrtStorage.is_open() )
	if ( !m_CrtStorage.is_open() )
	{
		int errCode = errno;
		stringstream errorMessage;
#ifdef CRT_SECURE
		char errBuffer[ 95 ];
		strerror_s( errBuffer, sizeof( errBuffer ), errCode );
 		errorMessage << "Can't open source file [" << storageId << "]. Error code : " << errCode << " [" << errBuffer << "]";
#else
		errorMessage << "Can't open source file [" << storageId << "]. Error code : " << errCode << " [" << strerror( errCode ) << "]";
#endif				
		TRACE( errorMessage.str() );
		throw runtime_error( errorMessage.str() );
	}

	string extension = boost::filesystem::path(storageId).extension().string();

	if ( extension == ".gz" )
	{
		boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
		in.push(boost::iostreams::gzip_decompressor());
		in.push(m_CrtStorage);

		boost::iostreams::copy(in, back_inserter(m_MemCrtStorage));

		m_CrtStorageId = "";
	}

	// read ahead next item
	dequeue();
}
