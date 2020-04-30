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

#include "Base64.h"
#include "SSL/HMAC.h"
#include "SSL/X509Parser.h"
#include "SSL/RSASign.h"
#include "SSL/ECDSASign.h"
#include "StringUtil.h"
#include "Trace.h"

#include <boost/regex.hpp>
#include <boost/smart_ptr/scoped_ptr.hpp>
#include <xalanc/XercesParserLiaison/XercesDocumentWrapper.hpp>
#include <xsec/canon/XSECC14n20010315.hpp>
#include <xsec/framework/XSECException.hpp>

#include "SwiftFormatFilter.h"
#include "XPathHelper.h"

namespace FinTP
{

const string SwiftFormatFilter::SERVICE_KEY = "SERVICE";
const string SwiftFormatFilter::LAU_KEY = "LAUKEY";
const string SwiftFormatFilter::PAYLOAD_DIGEST = "DIGEST";

const string SwiftFormatFilter::SAA_FILEACT = "SAA_FILEACT";
const string SwiftFormatFilter::SAA_FILEACTMX = "SAA_FILEACT_MX";
const string SwiftFormatFilter::SAA_FILEACTIP = "SAA_FILEACT_IP";
const string SwiftFormatFilter::SAA_FIN = "SAA_FIN";
const string SwiftFormatFilter::SAA_FINPCC = "SAA_FIN_PCC";
const string SwiftFormatFilter::SAE = "SAE";

const static string SIGN_SECTION_BEGIN = "{MDG:";
const static string SIGN_SECTION_END = "}}";
const static string MESSAGE_SECTION_END = "{S:";

/*
XML Mediator Format
The application and Alliance Lite2 exchange PDUs that are sequences of bytes with the following structure:
* Prefix ( 1 byte )
* Length ( 6 bytes )
* Signature ( 24 bytes )
* DataPDU: XML structure containing the information relevant for processing ( message or report ) encoded in UTF-8 format.
*/

#define PREFIX_SIZE		1
#define LENGTH_SIZE		6
#define SIGNATURE_SIZE	24
#define HEADER_SIZE		31


bool SwiftFormatFilter::isMethodSupported( FilterMethod method, bool asClient )
{
	switch ( method )
	{
	case SwiftFormatFilter::BufferToBuffer :
		return true;
	default:
		return false;
	}
}

/*
The application and Alliance Lite2 exchange PDUs that are sequences of bytes with the following structure:
* Prefix (1 byte)
* Length (6 bytes)
* Signature (24 bytes)
* DataPDU: XML structure containing the information relevant for processing (message or report) encoded in UTF-8 format.DataPDU: XML structure containing the information relevant for processing (message or report) encoded in UTF-8 format.
*/

#define PREFIX_SIZE		1
#define LENGTH_SIZE		6
#define SIGNATURE_SIZE	24
#define HEADER_SIZE		31

bool SwiftFormatFilter::isStrictFormat( const ManagedBuffer& managedBuffer )
{
	unsigned char* rawBuffer = managedBuffer.buffer();
	size_t size = managedBuffer.size();
	return ( rawBuffer[0] == 0x1F && size > HEADER_SIZE && rawBuffer[HEADER_SIZE] == '<' && rawBuffer[size-1] == '>' );
}

AbstractFilter::FilterResult SwiftFormatFilter::ProcessMessage( AbstractFilter::buffer_type inputData, AbstractFilter::buffer_type outputData, NameValueCollection& transportHeaders, bool asClient )
{
	ManagedBuffer *inputBuffer = inputData.get(), *outputBuffer = outputData.get();
	string service = transportHeaders[SwiftFormatFilter::SERVICE_KEY];
	if ( inputBuffer && outputBuffer )
	{
		string key = transportHeaders[SwiftFormatFilter::LAU_KEY];

		DEBUG( "Applying SwiftFormatFilter filter with service: " << service );
		//TODO: Another design for this if loaded implementation
		if( service == SwiftFormatFilter::SAA_FIN )
		{
			bool authenticated = true;
			string errorMessage;
			string SIGN_SECTION_BEGIN = "{MDG:";
			string SIGN_SECTION_END = "}}";
			string MESSAGE_SECTION_END = "{S:";
			if ( asClient )
			{
				string inputString = inputBuffer->str();
				size_t signSectionPos = inputString.find( SIGN_SECTION_BEGIN );
				size_t messageSectionEndPos = inputString.find( MESSAGE_SECTION_END );
				if( signSectionPos != string::npos )//signed message
				{
					if ( key.empty() )
					{
						errorMessage = "Signed message processed, but no key provided in .config file";
						authenticated = false;
						TRACE( errorMessage );

					}
					inputString = inputString.substr( 0, messageSectionEndPos );
					if( HMAC::HMAC_Sha256Gen( inputString, key, HMAC::HEXSTRING_UPPERCASE ) != ( inputBuffer->str() ).substr( signSectionPos + 5, 64 ) )
					{
						errorMessage = "Message not authenticated";
						authenticated = false;
						TRACE( errorMessage );
					}
				}
				else
				{
					if( !key.empty() )
					{
						errorMessage = "Signed message expected (check MQSeriesToApp.LAUCertificateFile key in .config file ). Message not authenticated";
						authenticated = false;
						TRACE( errorMessage );
					}
				}

				if ( !authenticated )
				{
					errorMessage = "Message not authenticated";
					throw runtime_error( errorMessage );
				}
				outputBuffer->copyFrom( inputString );
			}
			else
			{
				const string& inputString = inputBuffer->str();
				stringstream signedOutput;
				signedOutput << inputString << MESSAGE_SECTION_END << SIGN_SECTION_BEGIN << HMAC::HMAC_Sha256Gen( inputString, key, HMAC::HEXSTRING_UPPERCASE ) << SIGN_SECTION_END;
				outputBuffer->copyFrom( signedOutput.str() );
			}
		}
		else if( service == SwiftFormatFilter::SAA_FILEACT )
		{
			if ( asClient ) //delete 31 byte prefix
			{
				unsigned char* rawBuffer = inputBuffer->buffer();
				size_t size = inputBuffer->size();

				if ( isStrictFormat( *inputBuffer ) )
				{
					bool authenticated = true;

					if ( key.empty() )
					{
						for ( size_t i = 0; i < SIGNATURE_SIZE; ++i )
							if ( rawBuffer[i + PREFIX_SIZE + LENGTH_SIZE ] != 0 )
							{
								authenticated = false;
								break;
							}
					}
					else
					{
						//Signature (24 bytes): HMAC SHA-256 digest truncated to 128 bits Base64 encoded
						vector<unsigned char> digest = HMAC::HMAC_Sha256Gen( rawBuffer + HEADER_SIZE, size - HEADER_SIZE, reinterpret_cast<const unsigned char*>( key.c_str() ), key.size() );
						string base64Signature = Base64::encode( &digest[0], 16 ); // 16 bytes = 128 bits if CHAR_BIT = 8

						if ( memcmp( base64Signature.c_str(), rawBuffer + PREFIX_SIZE + LENGTH_SIZE, SIGNATURE_SIZE ) != 0 )
							authenticated = false;
					}

					if ( !authenticated )
						throw runtime_error( "Message not authenticated" );

					string messageSizeString( reinterpret_cast<char*>( rawBuffer ), PREFIX_SIZE, LENGTH_SIZE );
					size_t messageSize = StringUtil::ParseInt( messageSizeString );
					if ( messageSize == size - PREFIX_SIZE - LENGTH_SIZE )
					{
						size -= HEADER_SIZE;
						outputBuffer->allocate( size );
						memcpy( outputBuffer->buffer(), &rawBuffer[HEADER_SIZE], size );
					}
					else
						throw runtime_error( "Incorrect message size specified in message" );
				}
				else
					outputBuffer->copyFrom( inputBuffer );
			}
			else //add 31 byte prefix
			{
				size_t putMessageSize = inputBuffer->size() + HEADER_SIZE;
				outputBuffer->allocate( putMessageSize );
				unsigned char* putBuffer = outputBuffer->buffer();

				//Prefix (1 byte): the character '0x1f'.
				putBuffer[0] = 0x1f;

				//Length (6 bytes): length (in bytes) of the Signature and DataPDU fields: this length is base-10
				//encoded as 6 ASCII characters, left padded with 0s if needed.
				string sizePrefix = StringUtil::ToString( inputBuffer->size() + SIGNATURE_SIZE );
				size_t sizePrefixLength = sizePrefix.length();
				string leftZeroes;
				for ( int i = 0; i < LENGTH_SIZE - sizePrefixLength; ++i )
					leftZeroes += "0";
				sizePrefix = leftZeroes + sizePrefix;
				memcpy( &putBuffer[1], sizePrefix.c_str(), LENGTH_SIZE );

				if ( key.empty() )
					//Signature (24 bytes): null bytes
					memset( &putBuffer[PREFIX_SIZE + LENGTH_SIZE], 0, SIGNATURE_SIZE );
				else
				{
					//Signature (24 bytes): HMAC SHA-256 digest truncated to 128 bits Base64 encoded
					vector<unsigned char> digest = HMAC::HMAC_Sha256Gen( inputBuffer->buffer(), inputBuffer->size(), reinterpret_cast<const unsigned char*>( key.c_str() ), key.size() );
					string base64Signature = Base64::encode( &digest[0], 16 ); // 16 bytes = 128 bits if CHAR_BIT = 8
					memcpy( &putBuffer[PREFIX_SIZE + LENGTH_SIZE], base64Signature.c_str(), SIGNATURE_SIZE );
				}

				//DataPDU:
				memcpy( &putBuffer[HEADER_SIZE], inputBuffer->buffer(), inputBuffer->size() );
			}
		}
		return SwiftFormatFilter::Completed;
	}
	TRACE( "SwiftFormatFilter not applied!. Service: " <<  service )
	return SwiftFormatFilter::Completed;
}

AbstractFilter::FilterResult SwiftFormatFilter::ProcessMessage( AbstractFilter::buffer_type inputData, unsigned char** outputData, NameValueCollection& transportHeaders, bool asClient )
{
	throw FilterInvalidMethod( AbstractFilter::BufferToBuffer );
}

AbstractFilter::FilterResult SwiftFormatFilter::ProcessMessage( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* inputOutputData, NameValueCollection& transportHeaders, bool asClient )
{
	throw FilterInvalidMethod( AbstractFilter::XmlToXml );
}

AbstractFilter::FilterResult SwiftFormatFilter::ProcessMessage( const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* inputData, AbstractFilter::buffer_type outputData, NameValueCollection& transportHeaders, bool asClient )
{
	throw FilterInvalidMethod( AbstractFilter::XmlToBuffer );
}

AbstractFilter::FilterResult SwiftFormatFilter::ProcessMessage( const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* inputData, unsigned char** outputData, NameValueCollection& transportHeaders, bool asClient )
{
	throw FilterInvalidMethod( AbstractFilter::XmlToBuffer );
}

AbstractFilter::FilterResult SwiftFormatFilter::ProcessMessage( AbstractFilter::buffer_type inputData, XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* outputData, NameValueCollection& transportHeaders, bool asClient )
{
	throw FilterInvalidMethod( AbstractFilter::BufferToXml );
}

AbstractFilter::FilterResult SwiftFormatFilter::ProcessMessage( unsigned char* inputData, XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* outputData, NameValueCollection& transportHeaders, bool asClient )
{
	throw FilterInvalidMethod( AbstractFilter::BufferToXml );
}


UnicodeString SwiftFormatFilter::EscapeNonSwiftChars( UnicodeString& in, const string& replacement )
{
	static const UnicodeString allowedCharacters = UnicodeString::fromUTF8( "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ \t\r\n0123456789/-?:().,'+\"<>={}" );
	 

	UnicodeString UnicodeReplacement = UnicodeString::fromUTF8( replacement );
	const UChar *text = in.getTerminatedBuffer();
	UCharCharacterIterator iter( text, in.length() );

	UnicodeString result;

	UChar32 c;
while ( iter.hasNext() )
	{
c = iter.next32PostInc();
		if ( allowedCharacters.indexOf( c ) < 0 )
			result += UnicodeReplacement;
		else
			result += c;
	}

	return result;
}

void SwiftFormatFilter::EscapeNonSwiftChars( DOMElement* currentItemRoot, const string& replacement )
{
	static const XMLCh* const ALL = TranscodeFromStr( reinterpret_cast<const XMLByte*>( "*" ), sizeof( "*" ), "utf-8" ).adopt();
	const auto elems = currentItemRoot->getElementsByTagName( ALL );
	for ( int i = 0; i < elems->getLength(); i++ )
	{
		DOMNode* item = elems->item( i );
		if ( item->getFirstChild() )
		{
			const XMLCh* text = item->getFirstChild()->getNodeValue();
			UnicodeString textContent( text );
			if ( textContent.length() == 0 )
				continue;
			item->getFirstChild()->setNodeValue( reinterpret_cast<const XMLCh*> ( EscapeNonSwiftChars( textContent, replacement ).getTerminatedBuffer() ) );
		}
	}
}

string SwiftFormatFilter::EscapeNonSwiftChars( const string& in, const string& replacement )
{
	string result;
	UnicodeString inUnicode = UnicodeString::fromUTF8( in );
	EscapeNonSwiftChars( inUnicode, replacement ).toUTF8String( result );
	return result;
}

auto_ptr<SwiftMediator> SwiftFormatFilter::GetSwiftMediator( const string& service )
{
	if( service == SwiftFormatFilter::SAA_FIN )
		return auto_ptr<SwiftMediator>( new FINMediator() );
	else if( service == SwiftFormatFilter::SAA_FINPCC )
		return auto_ptr<SwiftMediator>( new PCCMediator() );
	else if( service == SwiftFormatFilter::SAA_FILEACT )
		return auto_ptr<SwiftMediator>( new StrictFormatMediator() );
	else if( service == SwiftFormatFilter::SAA_FILEACTMX )
		return auto_ptr<SwiftMediator>( new MXMediator() );
	else if ( service == SwiftFormatFilter::SAA_FILEACTIP )
		return auto_ptr<SwiftMediator>( new IPMediator() );
	else
	{
		DEBUG( "Swift message format not provided. Use default FIN format" );
		return auto_ptr<SwiftMediator>( new FINMediator() );
	}
}

// FIN Mediator implementation
void FINMediator::FetchPreparation( ManagedBuffer* inputBuffer, ManagedBuffer* outputBuffer, const string& key, const string& digest )
{
	bool authenticated = true;
	string inputString = inputBuffer->str();
	std::string errorMessage;
	if( isSuportedFormat( inputString ) )
	{
		size_t signSectionPos = inputString.find( SIGN_SECTION_BEGIN );
		size_t messageSectionEndPos = inputString.find( MESSAGE_SECTION_END );
		if( signSectionPos != string::npos )//signed message
		{
			if ( key.empty() )
			{
				errorMessage = "Signed message processed, but no key provided in .config file";
				authenticated = false;
				TRACE( errorMessage );
			}
			else
			{
				DEBUG( "Filter configured to check security markers" );
				string hashInput = inputString.substr( 0, messageSectionEndPos );
				if( HMAC::HMAC_Sha256Gen( hashInput, key, HMAC::HEXSTRING_UPPERCASE ) != inputString.substr( signSectionPos + 5, 64 ) )
					authenticated = false;
			}
		}
		else
		{
			if( !key.empty() )
			{
				errorMessage = "Signed message expected ( check MQSeriesToApp.LAUCertificateFile key in .config file )";
				authenticated = false;
				TRACE( errorMessage );
			}
			else
				DEBUG( "Unsigned FIN format message processed" );
		}

		if ( !authenticated )
			throw runtime_error( string( "Message not authenticated. " ).append( errorMessage ) );
	}
	outputBuffer->copyFrom( inputString );
}

void FINMediator::PublishPreparation( ManagedBuffer* inputBuffer, ManagedBuffer* outputBuffer, const string& key, const string& digest )
{
	if( !key.empty() )
	{
		DEBUG( "Signing Swift mesage..." );
		stringstream signedOutput;

		string data = inputBuffer->str();
		const static boost::regex re( "(.*?)\\{S:.*?\\}" );
		boost::smatch results;
		if ( boost::regex_match( data, results, re ) && results.size() == 2 )
		{
			signedOutput << SIGN_SECTION_BEGIN << HMAC::HMAC_Sha256Gen( results[1], key, HMAC::HEXSTRING_UPPERCASE ) << "}";
			data.insert( data.size() - 1, signedOutput.str() );
			outputBuffer->copyFrom( data );
		}
		else
		{
			signedOutput << data << MESSAGE_SECTION_END << SIGN_SECTION_BEGIN << HMAC::HMAC_Sha256Gen( data, key, HMAC::HEXSTRING_UPPERCASE ) << SIGN_SECTION_END;
			outputBuffer->copyFrom( signedOutput.str() );
		}
	}
	else
	{
		DEBUG( "Swift message not signed. No key provided in .config file" );
		outputBuffer->copyFrom( inputBuffer );
	}
		
}

bool FINMediator::isSuportedFormat( const string& inputMessage )
{
	const static boost::regex re( "^\\{\\d{1,3}:" );
	boost::match_results<std::string::const_iterator> results;
	if( boost::regex_search( inputMessage, results, re ) )
		return true;
	else
		return false;
}

// PCC Mediator implementation
void PCCMediator::FetchPreparation( ManagedBuffer* inputBuffer, ManagedBuffer* outputBuffer, const string& key, const string& payloadDigest )
{
	bool authenticated = true;
	string inputString = inputBuffer->str();
	const static boost::regex re( "\\x01(.*?)\\{S:.*?\\{MDG:([^\\}]*)\\}.*?\\x03" );
	boost::match_results<std::string::const_iterator> results;
	string message, hash;
	string errorMessage;
	if ( boost::regex_match( inputString, results, re ) && results.size() == 3 )//signed message
	{
		message = results[1];
		hash = results[2];

		if ( key.empty() )
		{
			errorMessage = "Signed message processed, but no key provided in .config file";
			authenticated = false;
			TRACE( errorMessage );
		}
		else
		{
			DEBUG( "Filter configured to check security markers" );
			if( HMAC::HMAC_Sha256Gen( message, key, HMAC::HEXSTRING_UPPERCASE ) != hash )
				authenticated = false;
		}
	}
	else
	{
		if( !key.empty() )
		{
			errorMessage = "Invalid message format. Signed DOS-PCC message expected!" ;
			authenticated = false;
			TRACE( errorMessage );
		}
		else
		{
			const static boost::regex unsignedRegex( "\\x01(.*?)\\x03" );
			boost::match_results<std::string::const_iterator> results;
			if( boost::regex_match( inputString, results, unsignedRegex ) )
				message = results[1];
			
			DEBUG( "Unsigned FIN DOS-PCC message processed" );
		}
	}

	if ( !authenticated )
		throw runtime_error( string( "Message not authenticated. " ).append( errorMessage ) );

	outputBuffer->copyFrom( message );
}

void PCCMediator::PublishPreparation( ManagedBuffer* inputBuffer, ManagedBuffer* outputBuffer, const string& key, const string& payloadDigesty )
{
	string inputString = inputBuffer->str();
	if( !key.empty() )
	{
		const static boost::regex re( "\\x01(.*?)\\{S:.*?\\x03" );
		boost::match_results<std::string::const_iterator> results;
		if( boost::regex_match( inputString, results, re ) && results.size() == 2 )
		{
			string message = results[1];
			string mdgField = "{MDG:";
			mdgField += HMAC::HMAC_Sha256Gen( message, key, HMAC::HEXSTRING_UPPERCASE );
			mdgField += "}";
			inputString.insert( inputString.size() - 2, mdgField );
		}
		else
		{
			size_t messageEnd = inputString.find_last_of( '\x3' );
			if ( messageEnd != string::npos )
				inputString = string( inputString.c_str() + 1, inputString.c_str() + messageEnd );
			else
				throw runtime_error( "Incorrect message format" );
			string mdgField = SIGN_SECTION_BEGIN + HMAC::HMAC_Sha256Gen( inputString, key, HMAC::HEXSTRING_UPPERCASE ) + SIGN_SECTION_END;
			inputString.insert( 0, "\x1" );
			inputString.append( MESSAGE_SECTION_END ).append( mdgField ).append( "\x3" );
		}
	}
	else
	{
		DEBUG( "Swift messagenot signed. No key provided in .config file" );
		inputString.insert( 0, "\x1" );
		inputString.append( "\x3" );
	}
	inputString = addWhitespaces( inputString );
	outputBuffer->copyFrom( inputString );
}

string PCCMediator::addWhitespaces( const string& inputString )
{
	int addWhitespaces = inputString.size() % 512;
	if ( addWhitespaces )
	{
		string newString = inputString;
		addWhitespaces = 512 - addWhitespaces;
		newString += string( addWhitespaces , ' ' );
		return newString;
	}
	else return inputString;
}

// StrictFormat Mediator implementation
void StrictFormatMediator::FetchPreparation( ManagedBuffer* inputBuffer, ManagedBuffer* outputBuffer, const string& key, const string& digest )
{
	unsigned char* rawBuffer = inputBuffer->buffer();
	size_t size = inputBuffer->size();
	bool authenticated = true;
	bool isSigned = false;
	string errorMessage;
	if ( isStrictFormat( *inputBuffer ) )
	{
		// find signature
		for ( size_t i = 0; i < SIGNATURE_SIZE; ++i )
			if ( rawBuffer[i + PREFIX_SIZE + LENGTH_SIZE ] != 0 ) 
			{
				isSigned = true;
				break;
			}

			if( key.empty() )
			{
				if( isSigned )
				{
					errorMessage = "Signed message processed, but no key provided in .config file";
					authenticated = false;
					TRACE( errorMessage );
				}
				else
					DEBUG( "Unsigned prefixed XML format message processed" );
			}
			else
			{
				if( !isSigned )
				{
					errorMessage = "Invalid message format. Signed XML message expected!" ;
					authenticated = false;
					TRACE( errorMessage );
				}
				else
				{
					DEBUG( "Filter configured to check security markers" );
					//Signature ( 24 bytes ): HMAC SHA-256 digest truncated to 128 bits Base64 encoded
					vector<unsigned char> digest = HMAC::HMAC_Sha256Gen( rawBuffer + HEADER_SIZE, size - HEADER_SIZE, reinterpret_cast<const unsigned char*>( key.c_str() ), key.size() );
					string base64Signature = Base64::encode( &digest[0], 16 ); // 16 bytes = 128 bits if CHAR_BIT = 8

					if ( memcmp( base64Signature.c_str(), rawBuffer + PREFIX_SIZE + LENGTH_SIZE, SIGNATURE_SIZE ) != 0 )
						authenticated = false;
				}
			}

			if ( !authenticated )
				throw runtime_error( string( "Message not authenticated. " ).append( errorMessage ) );

			// trim payload
			string messageSizeString( reinterpret_cast<char*>( rawBuffer ), PREFIX_SIZE, LENGTH_SIZE );
			size_t messageSize = StringUtil::ParseInt( messageSizeString );
			if ( messageSize == size - PREFIX_SIZE - LENGTH_SIZE )
			{
				size -= HEADER_SIZE;
				outputBuffer->allocate( size );
				memcpy( outputBuffer->buffer(), &rawBuffer[HEADER_SIZE], size );
			}
			else
				throw runtime_error( "Incorrect message size specified in message" );
	}
	else
	{
		DEBUG( "XML format message processed ( no prefix, no signature )" );
		outputBuffer->copyFrom( inputBuffer );
	}

}

void StrictFormatMediator::PublishPreparation( ManagedBuffer* inputBuffer, ManagedBuffer* outputBuffer, const string& key, const string& digest )
{
	size_t putMessageSize = inputBuffer->size() + HEADER_SIZE;
	outputBuffer->allocate( putMessageSize );
	unsigned char* putBuffer = outputBuffer->buffer();

	//Prefix ( 1 byte ): the character '0x1f'.
	putBuffer[0] = 0x1f;

	//Length ( 6 bytes ): length ( in bytes ) of the Signature and DataPDU fields: this length is base-10
	//encoded as 6 ASCII characters, left padded with 0s if needed.
	string sizePrefix = StringUtil::ToString( inputBuffer->size() + SIGNATURE_SIZE );
	size_t sizePrefixLength = sizePrefix.length();
	string leftZeroes;
	for ( int i = 0; i < LENGTH_SIZE - sizePrefixLength; ++i )
		leftZeroes += "0";
	sizePrefix = leftZeroes + sizePrefix;
	memcpy( &putBuffer[1], sizePrefix.c_str(), LENGTH_SIZE );

	if ( key.empty() )
	{
		DEBUG( "Swift messagenot signed. No key provided in .config file" );
		//Signature ( 24 bytes ): null bytes
		memset( &putBuffer[PREFIX_SIZE + LENGTH_SIZE], 0, SIGNATURE_SIZE );
	}
	else
	{
		//Signature ( 24 bytes ): HMAC SHA-256 digest truncated to 128 bits Base64 encoded
		vector<unsigned char> digest = HMAC::HMAC_Sha256Gen( inputBuffer->buffer(), inputBuffer->size(), reinterpret_cast<const unsigned char*>( key.c_str() ), key.size() );
		string base64Signature = Base64::encode( &digest[0], 16 ); // 16 bytes = 128 bits if CHAR_BIT = 8
		memcpy( &putBuffer[PREFIX_SIZE + LENGTH_SIZE], base64Signature.c_str(), SIGNATURE_SIZE );
	}

	//DataPDU:
	memcpy( &putBuffer[HEADER_SIZE], inputBuffer->buffer(), inputBuffer->size() );
}

bool StrictFormatMediator::isStrictFormat( const ManagedBuffer& managedBuffer )
{
	unsigned char* rawBuffer = managedBuffer.buffer();
	size_t size = managedBuffer.size();
	return ( rawBuffer[0] == 0x1F && size > HEADER_SIZE && rawBuffer[HEADER_SIZE] == '<' && rawBuffer[size-1] == '>' );
}

//MX Mediator implementation
void MXMediator::FetchPreparation( ManagedBuffer* inputBuffer, ManagedBuffer* outputBuffer, const string& key, const string& payloadDigest )
{
	bool authenticated = true;
	string errorMessage;
	boost::scoped_ptr<XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument> dataPDU( XmlUtil::DeserializeFromString( inputBuffer->str() ) );
	if ( dataPDU == nullptr )
		throw runtime_error( "Expect DataPDU to be a node" );

	if( !isSuportedFormat( payloadDigest ) )
	{
		TRACE( "Security markers not checked. Payload digest expected!" );
		outputBuffer->copyFrom( inputBuffer );
	}
	else if( !key.empty() )
	{
		DEBUG( "Filter configured to check security markers. Check payload digest with [" << payloadDigest << "]." );
		string ns = XmlUtil::getNamespace( dataPDU.get() );
		XALAN_CPP_NAMESPACE::XercesDocumentWrapper wrapper( *XMLPlatformUtils::fgMemoryManager, dataPDU.get(), true, true, true );
		string digest = XPathHelper::SerializeToString( XPathHelper::Evaluate( "/x:DataPDU/x:Header/x:Message/x:SecurityInfo/x:SWIFTNetSecurityInfo/x:FileDigestValue/text()", &wrapper, ns ) );
		if( digest.empty() )
		{
			authenticated = false;
			errorMessage = "Paylaod digest value expected in node /DataPDU.../FileDigestValue";
			TRACE( errorMessage );
		}
		else if( digest != payloadDigest )
		{
			authenticated = false;
			errorMessage = "Payload is corrupted.";
			TRACE( errorMessage );
		}

		try
		{
			authenticated = CheckLAU( dataPDU.get(), key );
		}
		catch( runtime_error& re )
		{
			errorMessage = re.what();
			TRACE( errorMessage );
			authenticated = false;
		}
	}
	else
	{
		DOMNodeList* list = dataPDU->getDocumentElement()->getElementsByTagNameNS( unicodeForm( "urn:swift:saa:xsd:saa.2.0" ), unicodeForm( "LAU" ) );
		if ( list->getLength() != 0 )
		{
			errorMessage = "Signed message processed, but no key provided in .config file";
			authenticated = false;
			TRACE( errorMessage );
		}
	}

	if ( !authenticated )
		throw runtime_error( string( "Message not authenticated. " ).append( errorMessage ) );


	//PDU file processed as inputBuffer. It required no further process.
	//outputBuffer->copyFrom( outputBuffer );
}

void MXMediator::PublishPreparation( ManagedBuffer *inputBuffer, ManagedBuffer* outputBuffer, const string& key, const string& payloadDigest )
{
	if( !key.empty() ) //add payload Digest and LAU signature
	{
		DEBUG( "Filter configured to add signature markers");
		auto_ptr<XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument> dataPDU( XmlUtil::DeserializeFromString( inputBuffer->str() ) );
		auto root = dataPDU->getDocumentElement();
		string ns = XmlUtil::getNamespace( dataPDU.get() );
		{
			const auto_ptr<DOMXPathNSResolver> prefixResolver( dataPDU->createNSResolver( root ) );
			const XStr namespaceURI( "urn:swift:saa:xsd:saa.2.0" );
			const XMLCh* const prefix = prefixResolver->lookupPrefix( namespaceURI.uniForm() );
			if ( XMLString::stringLen( prefix ) == 0 )
				prefixResolver->addNamespaceBinding( unicodeForm( "Saa" ), namespaceURI.uniForm() );

		    const auto_ptr<DOMXPathResult> xpathResult( dataPDU->evaluate( unicodeForm( "/Saa:DataPDU/Saa:Header/Saa:Message/Saa:SecurityInfo/Saa:SWIFTNetSecurityInfo" ), root, prefixResolver.get(), DOMXPathResult::ORDERED_NODE_SNAPSHOT_TYPE, NULL ) );
			if ( xpathResult->getSnapshotLength() > 0 && xpathResult->snapshotItem( 0 ) )
			{
				DOMElement& SWIFTNetSecurityInfoNode = dynamic_cast<DOMElement&>( *xpathResult->getNodeValue() );
				auto_ptr<DOMElement> FileDigestAlgorithmNode( dataPDU->createElementNS( unicodeForm( ns ), unicodeForm( "FileDigestAlgorithm" ) ) );
				FileDigestAlgorithmNode->setTextContent( unicodeForm( "SHA-256" ) );
				FileDigestAlgorithmNode->setPrefix( prefix );
				SWIFTNetSecurityInfoNode.appendChild( FileDigestAlgorithmNode.get() );
				FileDigestAlgorithmNode.release();

				auto_ptr<DOMElement> FileDigestValueNode( dataPDU->createElementNS( unicodeForm( ns ), unicodeForm( "FileDigestValue" ) ) );
				FileDigestValueNode->setTextContent( unicodeForm( payloadDigest ) );
				FileDigestValueNode->setPrefix( prefix );
				SWIFTNetSecurityInfoNode.appendChild( FileDigestValueNode.get() );
				FileDigestValueNode.release();
			}
			else
				TRACE( "Could not find SWIFTNetSecurityInfo" )
		}

		string canonicalDataPDU = Canonicalize( dataPDU.get() );
		string dataPDUDigest = HMAC::Sha256( canonicalDataPDU, HMAC::BASE64 );

		string signedInfo = "<ds:SignedInfo xmlns:ds=\"http://www.w3.org/2000/09/xmldsig#\">\n"
			"<ds:CanonicalizationMethod Algorithm=\"http://www.w3.org/2001/10/xml-exc-c14n#\"/>\n"
			"<ds:SignatureMethod Algorithm=\"http://www.w3.org/2001/04/xmldsig-more#hmac-sha256\"/>\n"
			"<ds:Reference URI=\"\">\n"
			"<ds:Transforms>\n"
			"<ds:Transform Algorithm=\"http://www.w3.org/2000/09/xmldsig#enveloped-signature\"/>\n"
			"<ds:Transform Algorithm=\"http://www.w3.org/2001/10/xml-exc-c14n#\"/>\n"
			"</ds:Transforms>\n"
			"<ds:DigestMethod Algorithm=\"http://www.w3.org/2001/04/xmlenc#sha256\"/>\n"
			"<ds:DigestValue>";
		signedInfo += dataPDUDigest;
		signedInfo += "</ds:DigestValue>\n"
			"</ds:Reference>\n"
			"</ds:SignedInfo>";

		string signatureValue;
		{
			auto_ptr<XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument> signedInfoXml( XmlUtil::DeserializeFromString( signedInfo ) );
			signatureValue = HMAC::HMAC_Sha256Gen( Canonicalize( signedInfoXml.get() ), key, HMAC::BASE64 );
		}

		//append LAU
		{
			stringstream tagLAU;
			tagLAU << "<ds:Signature xmlns:ds=\"http://www.w3.org/2000/09/xmldsig#\">\n" << signedInfo << "\n<ds:SignatureValue>" << signatureValue << "</ds:SignatureValue>\n</ds:Signature>";
			auto_ptr<XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument> signature( XmlUtil::DeserializeFromString( tagLAU.str() ) );

			auto LAUNode = dataPDU->createElementNS( unicodeForm( ns ), unicodeForm( "LAU" ) );
			LAUNode->appendChild( dataPDU->importNode( signature->getDocumentElement(), true ) );
			root->appendChild( LAUNode );
		}
		outputBuffer->copyFrom( XmlUtil::SerializeToString( dataPDU.get() ) );
	}
	else
	{
		DEBUG( "Swift message not signed. No key provided in .config file" );
		outputBuffer->copyFrom( inputBuffer );
	}
}

string MXMediator::Canonicalize( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc ) const
{
	string result;
	try
	{
		auto_ptr<XSECC14n20010315> canon( new XSECC14n20010315( doc ) );
		canon->setExclusive();

		char buffer[512];
		xsecsize_t res = canon->outputBuffer( ( unsigned char * ) buffer, 128 );

		while ( res != 0 ) 
		{
			buffer[res] = '\0';
			result += buffer;
			res = canon->outputBuffer( ( unsigned char * ) buffer, 128 );
		}
	}
	catch( ... )
	{
		throw runtime_error( "Unhandled error while canonicalize" );
	}

	return result;
}

bool MXMediator::CheckLAU( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* dataPDU, const string& key ) const
{
	//get LAU
	if ( dataPDU == nullptr )
		throw runtime_error( "Expect DataPDU to be a node" );
	DOMNodeList* list = dataPDU->getDocumentElement()->getElementsByTagNameNS( unicodeForm( "urn:swift:saa:xsd:saa.2.0" ), unicodeForm( "LAU" ) );
	if ( list->getLength() != 1 )
		throw runtime_error( "Could not find LAU node" );
	DOMNode* LAUNode = list->item( 0 );

	//getSignature
	if ( LAUNode == nullptr )
		throw runtime_error( "Expect LAU to be a node" );
	const DOMElement& lauElement = dynamic_cast<const DOMElement&>( *LAUNode );
	list = lauElement.getElementsByTagNameNS( unicodeForm( "http://www.w3.org/2000/09/xmldsig#" ), unicodeForm( "Signature" ) );
	if ( list->getLength() != 1 )
		throw runtime_error( "Could not find Signature node" );
	DOMNode* signatureNode = list->item( 0 );

	DOMNode* signedInfo = NULL;
	string signatureValue;
	for ( DOMNode* childNode = signatureNode->getFirstChild(); childNode != NULL; childNode = childNode->getNextSibling() )
	{
		if ( childNode->getNodeType() == DOMNode::ELEMENT_NODE )
		{
			string name = XmlUtil::XMLChtoString( childNode->getNodeName() );
			if ( name == "ds:SignatureValue" )
				signatureValue = XmlUtil::XMLChtoString( childNode->getTextContent() );
			if ( name == "ds:SignedInfo" )
				signedInfo = childNode;
		}
	}
	if ( signedInfo == nullptr )
		throw runtime_error( "Expect SignedInfo to be a node" );

	//get digest
	string digestValue;
	if ( signedInfo == nullptr )
		throw runtime_error( "Expect SignedInfo to be a node" );
	const DOMElement& signatureElement = dynamic_cast<const DOMElement&>( *signatureNode );
	list = signatureElement.getElementsByTagNameNS( unicodeForm( "http://www.w3.org/2000/09/xmldsig#" ), unicodeForm( "Reference" ) );
	if ( list->getLength() != 1 )
		throw runtime_error( "Could not find Reference node" );

	for ( DOMNode* childNode = list->item( 0 )->getFirstChild(); childNode != NULL; childNode = childNode->getNextSibling() )
	{
		if ( childNode->getNodeType() == DOMNode::ELEMENT_NODE )
		{
			string name = XmlUtil::XMLChtoString( childNode->getNodeName() );
			if ( name == "ds:DigestValue" )
				digestValue = XmlUtil::XMLChtoString( childNode->getTextContent() );
		}
	}
	if( digestValue.empty() )
		throw runtime_error( "Could not find DigestValue" );

	//check LAU and Signaturevalue
	dataPDU->getDocumentElement()->removeChild( LAUNode );
	if ( digestValue != HMAC::Sha256( Canonicalize( dataPDU ), HMAC::BASE64 ) )
		throw runtime_error( "DataPDU might be corrupted." );

	const string toCanonicalize = XmlUtil::SerializeNodeToString( signedInfo );
	auto_ptr<XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument> doc( XmlUtil::DeserializeFromString( toCanonicalize ) );
	if ( signatureValue != HMAC::HMAC_Sha256Gen( Canonicalize( doc.get() ), key, HMAC::BASE64 ) )
		throw runtime_error( "SignatureValue doesn't match" );

	return true;
}

bool MXMediator::isSuportedFormat( const string& payloadDigest )
{
	if( !payloadDigest.empty() )
		return true;
	else
		return false;
}
// IP Mediator implementation
void IPMediator::FetchPreparation( ManagedBuffer* inputBuffer, ManagedBuffer* outputBuffer, const string& key, const string& digest )
{ 
	auto_ptr<XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument> messageData( XmlUtil::DeserializeFromString( inputBuffer->str() ) );
	string message;
	bool authenticated = false;

	//Calculate HASH Imput (SHA256) and verify digest value
	try 
	{
		message = StringUtil::RemoveBetween( inputBuffer->str(), "<Sgntr>", "</Sgntr>" );
	}
	catch ( runtime_error& error )
	{
		throw runtime_error( "<Sgntr> tag in xml message not found" );
	}

	try 
	{
		message = message.substr(message.find("<hdr:Message"),message.length());
	}
	catch ( exception& error )
	{
		throw runtime_error( "<hdr:Message> tag in xml message not found" );
	}
	
	string digestValue;

	try 
	{
		digestValue = StringUtil::FindBetween( inputBuffer->str(), "<DigestValue>", "</DigestValue>" );
	}
	catch ( runtime_error& error )
	{
		throw runtime_error( "<DigestValue> tag in xml message not found" );
	}
	if ( digestValue == HMAC::Sha256( message, HMAC::BASE64 ) )
		authenticated = CheckSignature( messageData.get(), key);
	else
	{
		TRACE( "DigestValue doesn't match" )
		throw runtime_error( "DigestValue doesn't match" );

	}

	if(!authenticated)
		throw runtime_error("Message not authenticated. ");

	//Message processed as inputBuffer. It required no further process.
	//outputBuffer->copyFrom( outputBuffer );
}

void IPMediator::PublishPreparation( ManagedBuffer *inputBuffer, ManagedBuffer* outputBuffer, const string& keyFile, const string& digest )
{
	DEBUG( "Parse sign certificate" );
	//Parse sign certificate
	X509Parser x509Parser( keyFile.c_str() );
	string keyType = x509Parser.getPublicKeyType();
	string signatureAlgorithm;
	string toCalculateDigestMessage, message;
	AlgorithmType signatureAlgorithmInt = UKN_ALGORITHM;

	if( keyType == "ECC" )
	{
		signatureAlgorithm = "http://www.w3.org/2001/04/xmldsig-more#ecdsa-sha256";
		signatureAlgorithmInt = ECC_ALGORITHM;
	}
	else if( keyType == "RSA" )
	{
		signatureAlgorithmInt = RSA_ALGORITHM;
		signatureAlgorithm = "http://www.w3.org/2001/04/xmldsig-more#rsa-sha256";
	}
	else
		throw runtime_error( "Certificate sign algorithm not reconized. Algorithm type: [" + keyType + "]" );
	
	DEBUG( "Prepare message for sign ..." );
	
	XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* messageData = NULL;														  
	try
	{
		//Overwrite <Sgntr> to hold digest value
		message = StringUtil::Replace( inputBuffer->str(), "</hdr:AppHdr>", "<Sgntr></Sgntr></hdr:AppHdr>" );
		//Remove xml header to calculate digest value	
		toCalculateDigestMessage = message.substr( message.find( "<hdr:Message" ), message.length() );

		//Create <SignedInfoTag>
		string signedInfo = 
			"<SignedInfo>"
			"<CanonicalizationMethod Algorithm=\"http://www.w3.org/TR/2001/REC-xml-c14n-20010315\"/>"
			"<SignatureMethod Algorithm=\""+ signatureAlgorithm +"\"/>"
			"<Reference URI=\"\">"
			"<Transforms>"
			"<Transform Algorithm=\"http://www.w3.org/2000/09/xmldsig#enveloped-signature\"/>"
			"<Transform Algorithm=\"http://www.w3.org/2006/12/xml-c14n11\"/>"
			"</Transforms>"
			"<DigestMethod Algorithm=\"http://www.w3.org/2001/04/xmlenc#sha256\"/>"
			"<DigestValue>"+ HMAC::Sha256( toCalculateDigestMessage, HMAC::BASE64 ) +"</DigestValue>"
			"</Reference>"
			"</SignedInfo>";

		//Create certficate info tags
		stringstream keyInfo;
		keyInfo << "<KeyInfo><X509Data>";

		keyInfo << "<X509SubjectName>" << x509Parser.getSubject() << "</X509SubjectName>";
		keyInfo << "<X509IssuerSerial>";
		keyInfo << "<X509IssuerName>" << x509Parser.getIssuer() << "</X509IssuerName>";
		keyInfo << "<X509SerialNumber>" << x509Parser.getSerialNumber() << "</X509SerialNumber>";
		keyInfo << "</X509IssuerSerial>";
		keyInfo << "</X509Data></KeyInfo>";

		//Add signature info to message
		string signature = "<Signature xmlns=\"http://www.w3.org/2000/09/xmldsig#\">" + signedInfo ;
		signature += "<SignatureValue></SignatureValue>";
		signature += keyInfo.str();
		signature += "</Signature>";

		message = StringUtil::AddBetween( message, signature, "<Sgntr>", "</Sgntr>" );

		messageData = XmlUtil::DeserializeFromString( message );
		if ( messageData == nullptr )
			throw runtime_error( "Expect message to be an xml" );
		DOMNodeList* list = messageData->getDocumentElement()->getElementsByTagName( unicodeForm( "hdr:AppHdr" ) );

		if( list == nullptr || list->getLength() != 1 )
			throw runtime_error( "Expected message to have an <hdr:AppHdr> element" );
		DOMNode* mesageNode = list->item( 0 );
		//getSignature
		if ( mesageNode == nullptr )
			throw runtime_error( "Expect <hdr:AppHdr> to be a node" );
		const DOMElement* lauElement = dynamic_cast<const DOMElement*>( mesageNode );
		if( lauElement == nullptr )
			throw runtime_error( "Expect <hdr:AppHdr> to be an message element" );
		list = lauElement->getElementsByTagNameNS( unicodeForm( "http://www.w3.org/2000/09/xmldsig#" ), unicodeForm( "Signature" ) );
		if( list == nullptr || list->getLength() != 1 )
			throw runtime_error( "Expected message to have an <Signature> element" );
		DOMNode* signatureNode = list->item( 0 );
		if( signatureNode == nullptr )
			throw runtime_error( "Expect <Signature> to be an message element" );
		DOMNode* signInfo = NULL;

		for ( DOMNode* childNode = signatureNode->getFirstChild(); childNode != NULL; childNode = childNode->getNextSibling() )
		{
			if ( childNode->getNodeType() == DOMNode::ELEMENT_NODE )
			{
				string name = XmlUtil::XMLChtoString( childNode->getNodeName() );
				if ( name == "SignedInfo" )
				{
					DEBUG( "Expected <SignedInfo> element found" );
					signInfo = childNode;
				}
			}
		}
		if ( signInfo == nullptr )
			throw runtime_error( "Expected <SignedInfo> child node, no" );

		//Canonicalize added tags
		const string toCanonicalize = XmlUtil::SerializeNodeToString( signInfo );
		auto_ptr<XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument> doc( XmlUtil::DeserializeFromString( toCanonicalize ) );
		string cann = Canonicalize( doc.get() );

		//Somenthing strange , but needed!!
		cann = StringUtil::AddBetween( cann, " xmlns:hdr=\"urn:montran:message.01\"", "<SignedInfo xmlns=\"http://www.w3.org/2000/09/xmldsig#\"", "><CanonicalizationMethod" );

		DEBUG( "Generate signature on <SignedInfo> elemment. Element content size is [" << cann.size() << "]." );
		//Signing message
		switch ( signatureAlgorithmInt )
		{
		case ( RSA_ALGORITHM ) :
			message = StringUtil::AddBetween( message, RSASign::SignMessage( keyFile.c_str(), cann ), "<SignatureValue>", "</SignatureValue>" );
			break;
		case ( ECC_ALGORITHM ) :
			message = StringUtil::AddBetween( message, ECDSASign::SignMessage( keyFile.c_str(), cann ), "<SignatureValue>", "</SignatureValue>" );
			break;
		default:
			throw runtime_error( "Algorith for sign message not reconized" );
			break;
		}

		if( messageData != NULL )
		{
			messageData->release();
			messageData = NULL ;
		}
	}
	catch( runtime_error& re )
	{
		if( messageData != NULL )
		{
			messageData->release();
			messageData = NULL ;
		}

		TRACE( "Error while performe sign preparation" );
		throw re;
	}
	catch( ... )
	{
		if( messageData != NULL )
		{
			messageData->release();
			messageData = NULL ;
		}
		TRACE( "Unhandled error while performe sign preparation" );
		throw;
	}

	outputBuffer->copyFrom( message );
}

string IPMediator::Canonicalize( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc ) const
{
	string result;
	try
	{
		auto_ptr<XSECC14n20010315> canon( new XSECC14n20010315( doc ) );
		canon->setExclusive();

		char buffer[512];
		xsecsize_t res = canon->outputBuffer( ( unsigned char * )buffer, 128 );

		while ( res != 0 )
		{
			buffer[res] = '\0';
			result += buffer;
			res = canon->outputBuffer( ( unsigned char * )buffer, 128 );
		}
	}
	catch( XSECException &se )
	{
		string errorMessage = XmlUtil::XMLChtoString( se.getMsg() );
		TRACE( errorMessage );
		throw runtime_error( errorMessage );
		
	}
	catch( ... )
	{
		stringstream errorMessage;
		errorMessage << "Unhandled error while canonicalize";
		TRACE( errorMessage.str() );
		throw runtime_error( errorMessage.str() );
	}

	return result;
}

bool IPMediator::CheckSignature( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* messageData, const string& pemKeyFile ) const
{
	//get messageData
	if ( messageData == nullptr )
		throw runtime_error( "Expect Message to be a node" );
	DOMNodeList* list = messageData->getDocumentElement()->getElementsByTagName( unicodeForm( "hdr:AppHdr" ) );
	if ( list->getLength() != 1 )
		throw runtime_error( "Could not find hdr:AppHdr node" );
	DOMNode* mesageNode = list->item( 0 );

	//get Signature
	if ( mesageNode == nullptr )
		throw runtime_error( "Expect hdr:AppHdr to be a node" );
	const DOMElement& lauElement = dynamic_cast<const DOMElement&>( *mesageNode );
	list = lauElement.getElementsByTagNameNS( unicodeForm( "http://www.w3.org/2000/09/xmldsig#" ), unicodeForm( "Signature" ) );
	if ( list->getLength() != 1 )
		throw runtime_error( "Could not find Signature node" );
	DOMNode* signatureNode = list->item( 0 );

	DOMNode* signedInfo = NULL;
	string signatureValue;

	//parse Signature
	for( DOMNode* childNode = signatureNode->getFirstChild(); childNode != NULL; childNode = childNode->getNextSibling() )
	{
		if ( childNode->getNodeType() == DOMNode::ELEMENT_NODE )
		{
			string name = XmlUtil::XMLChtoString( childNode->getNodeName() );
			if ( name == "SignatureValue" )
				signatureValue = XmlUtil::XMLChtoString( childNode->getTextContent() );
			if ( name == "SignedInfo" )
				signedInfo = childNode;
		}
	}
	if ( signedInfo == nullptr )
		throw runtime_error( "Expect SignedInfo to be a node" );

	//verify Signature

	bool authenticated = false;

	const string toCanonicalize = XmlUtil::SerializeNodeToString( signedInfo );
	auto_ptr<XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument> doc( XmlUtil::DeserializeFromString( toCanonicalize ) );
	string cann = Canonicalize( doc.get() );

	cann = StringUtil::AddBetween( cann, " xmlns:hdr=\"urn:montran:message.01\"", "<SignedInfo xmlns=\"http://www.w3.org/2000/09/xmldsig#\"", "><CanonicalizationMethod" );
	
	X509Parser x509Certificate( pemKeyFile.c_str() );
	string keyType = x509Certificate.getPublicKeyType();

	if( keyType == "RSA" )
		authenticated = RSASign::VerifySignature( pemKeyFile.c_str(), cann, signatureValue );
	else if( keyType == "ECC" )
		authenticated = ECDSASign::VerifySignature( pemKeyFile.c_str(), cann, signatureValue );
	else
		throw runtime_error( "[" + keyType + "]Algorithm for verify signature not reconized" );

	return authenticated;
}

bool IPMediator::isSuportedFormat( const string& payloadDigest )
{
	if ( !payloadDigest.empty() )
		return true;
	else
		return false;
}
}

