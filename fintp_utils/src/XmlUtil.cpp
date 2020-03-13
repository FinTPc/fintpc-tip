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

#include <typeinfo>

#include "XmlUtil.h"
#include "Log.h"

#include <exception>
#include <stdexcept>

#ifdef WIN32
	#define __MSXML_LIBRARY_DEFINED__
#endif

#include <xercesc/sax/SAXParseException.hpp>
#if ( XERCES_VERSION_MAJOR >= 3 )
	#include <xercesc/dom/DOMLSSerializer.hpp>
#else
	#include <xercesc/dom/DOMWriter.hpp>
#endif

#include <iostream>
#include <string>
#include <sstream>
using namespace std;


using namespace FinTP;

bool XmlUtil::m_Initialized = false;
XmlUtil XmlUtil::Instance;
pthread_once_t XmlUtil::KeysCreate = PTHREAD_ONCE_INIT;
pthread_key_t XmlUtil::InstanceKey;

//pthread_mutex_t XmlUtil::InstanceSyncMutex = PTHREAD_MUTEX_INITIALIZER;

string XStr::m_SourceEncoding = "";

XmlUtil::XmlUtil() : m_ParseDocument( NULL ), m_Parser( NULL ), m_ErrorReporter( NULL ), m_UTF8Transcoder( NULL )
{
	if ( !m_Initialized )
	{
		// static instance creation
		int onceResult = pthread_once( &XmlUtil::KeysCreate, &XmlUtil::CreateKeys );
		if ( 0 != onceResult )
		{
			TRACE_LOG( "Unable to create XmlUtil keys [" << onceResult << "]" );
		}
		m_Initialized = true;

		// don't create a parser at this time
		return;
	}

	CreateParser();
	CreateUTF8Transcoder();
}

XmlUtil::~XmlUtil()
{
	try
	{
		ReleaseParser();
	}
	catch( ... )
	{
		try
		{
			TRACE_LOG( "An error occured while releasing parser" );
		}catch( ... ){}
	}

	try
	{
		ReleaseUTF8Transcoder();
	}
	catch( ... )
	{
		try
		{
			TRACE_LOG( "An error occured while releasing UTF8Transcoder" );
		}catch( ... ){}
	}
}

void XmlUtil::CreateKeys()
{
	cout << "Thread [" << pthread_self() << "] creating xml instance keys..." << endl;
	
	int keyCreateResult = pthread_key_create( &XmlUtil::InstanceKey, &XmlUtil::DeleteInstances );
	if ( 0 != keyCreateResult )
	{
		TRACE_LOG( "Unable to create thread key XmlUtil::InstanceKey [" << keyCreateResult << "]" );
	}
}

void XmlUtil::DeleteInstances( void* data )
{
	XmlUtil* instance = ( XmlUtil* )data;
	if ( instance != NULL )
		delete instance;
	
	int setSpecificResult = pthread_setspecific( XmlUtil::InstanceKey, NULL );
	if ( 0 != setSpecificResult )
	{
		TRACE_LOG( "Unable to reset thread specific XmlUtil::InstanceKey [" << setSpecificResult << "]" );
	}
}

void XmlUtil::Terminate()
{
	// deletes the instance for the main thread before the thread exists ( ...and XmlPlatformUtils is terminated by the calling program )
	DeleteInstances( pthread_getspecific( XmlUtil::InstanceKey ) );
}

XmlUtil* XmlUtil::getInstance()
{
	XmlUtil* instance = ( XmlUtil * )pthread_getspecific( XmlUtil::InstanceKey );
	if( instance == NULL ) 
	{
		instance = new XmlUtil();
		
		int setSpecificResult = pthread_setspecific( XmlUtil::InstanceKey, instance );
		if ( 0 != setSpecificResult )
		{
			TRACE_LOG( "Unable to set thread specific XmlUtil::InstanceKey [" << setSpecificResult << "]" );
		}
	}
	return instance;
}

//Contains:
//		XmlUtil						- serialize / deserialize to/from string
//		DOMWriterTreeErrorHandler	- error handler for DOMWriter
//		XercesDOMTreeErrorHandler	- error handler for XERCESDOMParser
		
#if ( XERCES_VERSION_MAJOR >= 3 )
void XmlUtil::printDOM( const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc)
{
	// Get implementation
	DOMImplementation *pImplement = DOMImplementationRegistry::getDOMImplementation( unicodeForm("LS") );
	
	// Construct the serializer
	DOMLSSerializer* theSerializer = ( ( DOMImplementationLS* ) pImplement )->createLSSerializer();

	//Do the serialization through DOMWriter::writeNode() to standard output
	StdOutFormatTarget* formatTarget = new StdOutFormatTarget();
	DOMLSOutput* theOutputDesc = pImplement->createLSOutput();
	theOutputDesc->setByteStream( formatTarget );

	theSerializer->write( doc, theOutputDesc );
	formatTarget->flush();

	// release the memory
	theOutputDesc->release();
	theSerializer->release();
	delete formatTarget;
}
#else // XERCES_VERSION_MAJOR >= 3
void XmlUtil::printDOM( const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc)
{
	// Get implementation
	DOMImplementation *pImplement = DOMImplementationRegistry::getDOMImplementation( unicodeForm("LS") );

	// Construct the serializer
	DOMWriter* theSerializer = ( ( DOMImplementationLS* ) pImplement )->createDOMWriter();
	
	//Do the serialization through DOMWriter::writeNode() to standard output
	StdOutFormatTarget* formatTarget = new StdOutFormatTarget();
	//if ( theSerializer->canSetFeature(XMLUni::fgDOMWRTFormatPrettyPrint, true) )
	//  	theSerializer->setFeature(XMLUni::fgDOMWRTFormatPrettyPrint, true);
	theSerializer->writeNode( formatTarget, *doc );
	formatTarget->flush();
	
	// release the memory
	delete formatTarget;
	theSerializer->release();
}
#endif

string XmlUtil::getNamespace( const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc )
{
	DOMElement* docElem = doc->getDocumentElement();
		
	if( docElem == NULL )
	{
		return "";
	}
	
	string ns = "";

#if ( XERCES_VERSION_MAJOR >= 3 )
	if( docElem->getNamespaceURI() != NULL )
		ns = localForm( docElem->getNamespaceURI() );
#else
	if ( docElem->getNamespaceURI() == NULL )
	{
		if ( docElem->getTypeInfo() != NULL && docElem->getTypeInfo()->getNamespace() != NULL )
			ns = localForm( docElem->getTypeInfo()->getNamespace() );
	}
	else
		ns = localForm( docElem->getNamespaceURI() );
#endif

	return ns;
}

XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* XmlUtil::DeserializeFromString( const string& buffer )
{
	return getInstance()->internalDeserialize( ( unsigned char* )buffer.data(), buffer.size() );
}

XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* XmlUtil::DeserializeFromFile( const string& filename )
{
	return getInstance()->internalDeserialize( filename );
}

XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* XmlUtil::DeserializeFromString( const unsigned char* buffer, const unsigned long bufferSize )
{
	return getInstance()->internalDeserialize( buffer, bufferSize );
}

XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* XmlUtil::internalDeserialize( const string& filename )
{
	try
	{
		DEBUG_LOG( "Parse begin" );
		
		if ( m_Parser == NULL )
			throw logic_error( "DOM parser was already destroyed. Unable to continue." );

		m_Parser->resetErrors();
		m_Parser->reset();

		m_Parser->parse( unicodeForm( filename ) );

		//TODO release document 
		m_ParseDocument = m_Parser->getDocument();
		if( m_ParseDocument == NULL )
			throw runtime_error( "Document element missing. The data may be corrupted or the file does't exist." );
			
		if( m_ParseDocument->getDocumentElement() == NULL )
			throw runtime_error( "Document element missing. The data may be corrupted or the file does't exist." );

		DEBUG_LOG( "Parse done. Document element : " << m_ParseDocument->getDocumentElement() );
	}
	catch( const XMLException& e )
    {
		TRACE_LOG( "XMLException while parsing [" << filename << "] : " << e.getMessage() );
		throw runtime_error( localForm( e.getMessage() ) );
    }
    catch( const DOMException& e )
    {
    	TRACE_LOG( "DOMException while parsing [" << filename << "] : " << e.code );
    	string message;
    	const unsigned int maxChars = 2047;
        XMLCh errText[ maxChars + 1 ];
        
        // attemt to read message text
        if ( DOMImplementation::loadDOMExceptionMsg( e.code, errText, maxChars ) )
    		message = localForm( errText );
    	else
    	{
    		// format error code
    		stringstream messageBuffer;
    		messageBuffer << "DOMException error code : [" << ( int )e.code << "]";
    		message = messageBuffer.str();
    	}
 
    	throw runtime_error( message );
    }
	catch( const std::exception& e )
	{
		TRACE_LOG( typeid( e ).name() << " exception while parsing [" << filename << "] : " << e.what() );
		throw;
	}
	catch( ... )
	{
		TRACE_LOG( "Unknown exception while parsing [" << filename << "]" );
		throw;
	}

	// tell the parser we will release the document
	m_Parser->adoptDocument();
	return m_ParseDocument;
}

XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* XmlUtil::internalDeserialize( const unsigned char* buffer, const unsigned long bufferSize )
{
	try
	{
		DEBUG_LOG( "Parse begin" );
		
		if ( m_Parser == NULL )
			throw logic_error( "DOM parser was already destroyed. Unable to continue." );

		m_Parser->resetErrors();
		m_Parser->reset();
		
		stringstream memBufIdStream;
		memBufIdStream << "BufferedInput.id," << pthread_self();
		string memBufId = memBufIdStream.str();
		
		// do not adopt buffer ( otherwise the membuf.. will release it upon dtor call )
		MemBufInputSource bufferMemSource( ( const XMLByte* )buffer, bufferSize, memBufId.c_str(), false );
//		bufferMemSource.setEncoding( unicodeForm( "UTF-8" ) );
//		
		try
		{
			m_Parser->parse( bufferMemSource );
		}
		catch( ... )
		{
			// try to release the doc
			try
			{
				m_ParseDocument = m_Parser->getDocument();
				m_Parser->adoptDocument();
				if ( m_ParseDocument != NULL )
					m_ParseDocument->release();
				m_ParseDocument = NULL;
			}
			catch( ... )
			{
				TRACE_LOG( "Failed to release dom after parse error." );
			}
			DEBUG_LOG( "Parse failed, but document released." );
			throw;
		}
			
		//TODO release document 
		m_ParseDocument = m_Parser->getDocument();
		if( m_ParseDocument == NULL )
			throw runtime_error( "Document element missing. The data may be corrupted." );
			
		if( m_ParseDocument->getDocumentElement() == NULL )
			throw runtime_error( "Document element missing. The data may be corrupted." );

		DEBUG_LOG( "Parse done. Document element : " << m_ParseDocument->getDocumentElement() );	
	}
	catch( const XMLException& e )
    {
    	TRACE_LOG( "XMLException" << e.getMessage() );
		throw;
    }
    catch( const DOMException& e )
    {
    	TRACE_LOG( "DOMException with code [" << e.code << "]" );
    	string message;
    	const unsigned int maxChars = 2047;
        XMLCh errText[ maxChars + 1 ];
        
        // attemt to read message text
        if ( DOMImplementation::loadDOMExceptionMsg( e.code, errText, maxChars ) )
    		message = localForm( errText );
    	else
    	{
    		// format error code
    		stringstream messageBuffer;
    		messageBuffer << "DOMException error code : [ " << ( int )e.code << "]";
    		message = messageBuffer.str();
    	}
 
		throw;
    }
	catch( const std::exception& e )
	{
		TRACE_LOG( "Exception " << e.what() );
		throw;
	}
	catch( ... )
	{
		TRACE_LOG( "Exception unknown" );
		throw;
	}

	// tell the parser we will release the document
	m_Parser->adoptDocument();
	return m_ParseDocument;
}

string XmlUtil::SerializeNodeToString( const XERCES_CPP_NAMESPACE_QUALIFIER DOMNode* node, int prettyPrint )
{
	if ( node == NULL )
	{
		TRACE_LOG( "NULL document being serialized" );
		return "";
	}
	return SerializeToString( node, node->getOwnerDocument()->getImplementation(), prettyPrint );
}

string XmlUtil::SerializeToString( const XERCES_CPP_NAMESPACE_QUALIFIER DOMNode* doc )
{
	if ( doc == NULL )
	{
		TRACE_LOG( "NULL document being serialized" );
		return "";
	}
	return SerializeToString( doc->getOwnerDocument(), doc->getOwnerDocument()->getImplementation() );
}

string XmlUtil::SerializeToString( const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc )
{
	if ( doc == NULL )
	{
		TRACE_LOG( "NULL document being serialized" );
		return "";
	}
	return SerializeToString( doc, doc->getImplementation() );
}
	
#if ( XERCES_VERSION_MAJOR >= 3 )
string XmlUtil::SerializeToString( const XERCES_CPP_NAMESPACE_QUALIFIER DOMNode* doc, XERCES_CPP_NAMESPACE_QUALIFIER DOMImplementationLS* impl, int prettyPrint )
{
	if ( ( doc == NULL ) || ( impl == NULL ) )
	{
		TRACE_LOG( "NULL document being serialized" );
		return "";
	}

	string formattedDomStr;

	DOMLSSerializer* writer = NULL;

	XMLFormatTarget *formatTarget = NULL;
    DOMLSOutput *theOutputDesc = NULL;
	DOMErrorHandler *errReporter = NULL;
	
	try
	{
		// set a customized DOM Writer error handler
		errReporter = new DOMWriterTreeErrorHandler();
		if ( errReporter == NULL )
			throw runtime_error( "Unable to create error reporter" );

		// get a dom writer
		writer = impl->createLSSerializer();
		DOMConfiguration* writerConfig = writer->getDomConfig();
		if ( writer == NULL )
			throw runtime_error( "Unable to create DOM serializer" );

		writerConfig->setParameter( XMLUni::fgDOMErrorHandler, errReporter );

		theOutputDesc = impl->createLSOutput();
		if ( theOutputDesc == NULL )
			throw runtime_error( "Unable to create output target" );

		theOutputDesc->setEncoding( unicodeForm( "UTF-8" ) );

		formatTarget = new MemBufFormatTarget();
		if ( formatTarget == NULL )
			throw runtime_error( "Unable to create memory buffer target" );
		theOutputDesc->setByteStream( formatTarget );

		//Pretty print 0 - don't set, 1 - true, 2 - false
		switch( prettyPrint )
		{
			case 1 :
				if ( writerConfig->canSetParameter( XMLUni::fgDOMWRTFormatPrettyPrint, true ) )
					writerConfig->setParameter( XMLUni::fgDOMWRTFormatPrettyPrint, true );
				break;
			case 2 :
				if ( writerConfig->canSetParameter( XMLUni::fgDOMWRTFormatPrettyPrint, false ) )
					writerConfig->setParameter( XMLUni::fgDOMWRTFormatPrettyPrint, false );
				if ( writerConfig->canSetParameter( XMLUni::fgDOMWRTWhitespaceInElementContent, false ) )
					writerConfig->setParameter( XMLUni::fgDOMWRTWhitespaceInElementContent, false );
				break;
		}

		// do the serialization through DOMWriter::writeNode();
		writer->write( doc, theOutputDesc );

		MemBufFormatTarget* formatTargetMem = dynamic_cast< MemBufFormatTarget* >( formatTarget );
		if ( formatTargetMem == NULL )
			throw logic_error( "Bad type : Output format target is not MemBufFormatTarget" );

		char* str = ( char* )formatTargetMem->getRawBuffer();
		formattedDomStr = string( str );

		// release resources
        theOutputDesc->release();
        theOutputDesc = NULL;

		writer->release();

		if( errReporter != NULL )
			delete errReporter;
		errReporter = NULL;

		if( formatTarget != NULL )
			delete formatTarget;
		formatTarget = NULL;
	}
	catch( const std::exception& e )
	{
		stringstream messageBuffer;
		messageBuffer << typeid( e ).name() << " exception [" << e.what() << "]";

		// release resources
		if( theOutputDesc != NULL )
			theOutputDesc->release();

 		if( writer != NULL )
			writer->release();

		if( formatTarget != NULL )
			delete formatTarget;

		if( errReporter != NULL )
			delete errReporter;

		throw runtime_error( messageBuffer.str() );
	}
	catch( ... )
	{
		DEBUG_LOG( "Can't serialize document ( unknown exception )" );

		// release resources
		if( theOutputDesc != NULL )
			theOutputDesc->release();

 		if( writer != NULL )
			writer->release();

		if( formatTarget != NULL )
			delete formatTarget;
		
		if( errReporter != NULL )
			delete errReporter;

		// rethrow
		throw;
	}
	return formattedDomStr;
}
#else // XERCES_VERSION_MAJOR >= 3
string XmlUtil::SerializeToString( const XERCES_CPP_NAMESPACE_QUALIFIER DOMNode* doc, XERCES_CPP_NAMESPACE_QUALIFIER DOMImplementationLS* impl, int prettyPrint )
{
	if ( ( doc == NULL ) || ( impl == NULL ) )
	{
		TRACE_LOG( "NULL document being serialized" );
		return "";
	}

	string formattedDomStr;

	DOMWriter* writer = NULL;

	XMLFormatTarget *formatTarget = NULL;
	DOMErrorHandler *errReporter = NULL;

	try
	{
		// set a customized DOM Writer error handler
		errReporter = new DOMWriterTreeErrorHandler();
		if ( errReporter == NULL )
			throw runtime_error( "Unable to create error reporter" );

		// get a dom writer
		writer = impl->createDOMWriter();
		if ( writer == NULL )
			throw runtime_error( "Unable to create DOM writer" );

		writer->setErrorHandler( errReporter );
		writer->setEncoding( unicodeForm( "UTF-8" ) );
				
		formatTarget = new MemBufFormatTarget();
		if ( formatTarget == NULL )
			throw runtime_error( "Unable to create memory buffer target" );
		
		//Pretty print 0 - don't set, 1 - true, 2 - false
		switch( prettyPrint )
		{
			case 1 :
				if ( writer->canSetFeature( XMLUni::fgDOMWRTFormatPrettyPrint, true ) )
					writer->setFeature( XMLUni::fgDOMWRTFormatPrettyPrint, true ); 
				break;
			case 2 :
				if ( writer->canSetFeature( XMLUni::fgDOMWRTFormatPrettyPrint, false ) )
					writer->setFeature( XMLUni::fgDOMWRTFormatPrettyPrint, false );
				if ( writer->canSetFeature( XMLUni::fgDOMWRTWhitespaceInElementContent, false ) )
					writer->setFeature( XMLUni::fgDOMWRTWhitespaceInElementContent, false );
				break;
		}
		
		// do the serialization through DOMWriter::writeNode();
		writer->writeNode( formatTarget, *doc );
		formatTarget->flush();
		
		MemBufFormatTarget* formatTargetMem = dynamic_cast< MemBufFormatTarget* >( formatTarget );
		if ( formatTargetMem == NULL )
			throw logic_error( "Bad type : Output format target is not MemBufFormatTarget" );

		char* str = ( char* )formatTargetMem->getRawBuffer();
		formattedDomStr = string( str );

		// release resources
		if( errReporter != NULL )
			delete errReporter;
		errReporter = NULL;

		if( formatTarget != NULL )
			delete formatTarget;
		formatTarget = NULL;
		
		writer->release();
	}
	catch( const std::exception& e )
	{
		stringstream messageBuffer;
		messageBuffer << typeid( e ).name() << " exception [" << e.what() << "]";
		
		// release resources
		if( formatTarget != NULL )
			delete formatTarget;
		
		if( errReporter != NULL )
			delete errReporter;
	
 		if( writer != NULL )
			writer->release(); 
 		
		throw runtime_error( messageBuffer.str() );
	}
	catch( ... )
	{
		DEBUG_LOG( "Can't serialize document ( unknown exception )" );
		
		// release resources
		if( formatTarget != NULL )
			delete formatTarget;
		
		if( errReporter != NULL )
			delete errReporter;
	
 		if( writer != NULL )
			writer->release(); 
			
		// rethrow
		throw;
	}
	return formattedDomStr;
}
#endif

void XmlUtil::CreateParser()
{
	try
	{
		// create the parser
		m_Parser = new XercesDOMParser();
		if ( m_Parser == NULL )
			throw runtime_error( "Could not create Xerces parser" );

		// create error reporter
		m_ErrorReporter = new XercesDOMTreeErrorHandler();		
		if ( m_ErrorReporter == NULL )
			throw runtime_error( "Could not create error reporter" );
		
   		m_Parser->setErrorHandler( m_ErrorReporter );
		m_Parser->setDoNamespaces( true );

	}
	catch( ... )
	{
		ReleaseParser();
		throw runtime_error( "Unable to create the parser [unspecified error]" );
	}
}

void XmlUtil::ReleaseParser()
{
	try
	{
		if ( m_ErrorReporter != NULL )
		{
			if ( m_Parser != NULL )
				m_Parser->setErrorHandler( NULL );
			
			delete m_ErrorReporter;
			m_ErrorReporter = NULL;
		}
		
		if ( m_Parser != NULL )
		{
			//TODO check why this fails
			delete m_Parser;
			m_Parser = NULL;
		}
	}
	catch( ... )
	{
		// TODO check why this happens
		TRACE_LOG( "Parser released, but some errors have occured." );
		m_Parser = NULL;
	}
}

// Error handler
bool DOMWriterTreeErrorHandler::handleError( const DOMError &domError )
{
    // Display whatever error message passed from the serializer
   	DEBUG_LOG( "ERROR handler" );
   	
    const char *msg = localForm( domError.getMessage() );
    
    if ( domError.getSeverity() == DOMError::DOM_SEVERITY_WARNING )
    {
       	TRACE_LOG( "Warning message : " << msg );
    }   
    else if ( domError.getSeverity() == DOMError::DOM_SEVERITY_ERROR )
    {
       string message( "Severe error : " );
       ( void )message.append( msg );
       throw runtime_error( message );
    }      
    else
    {
       string message( "Fatal error : " );
       ( void )message.append( msg );
       throw runtime_error( message );
    }
    		
    // Instructs the serializer to continue serialization if possible.
    return true;
}

void XercesDOMTreeErrorHandler::warning( const SAXParseException& toCatch )
{
 	DEBUG_LOG( "ERROR handler - warn" );
    m_SawErrors = false;
          
   	stringstream messageBuffer;
	
	messageBuffer << "File/buffer : ";
	
	if( toCatch.getSystemId() != NULL )
		messageBuffer << localForm( toCatch.getSystemId() ) << " ";
	if( toCatch.getPublicId() != NULL )
   		messageBuffer << localForm( toCatch.getPublicId() ) << " ";
   		
	messageBuffer << " line " << ( int )toCatch.getLineNumber()	<<
		", column " << ( int )toCatch.getColumnNumber()	<< " : " << localForm( toCatch.getMessage() );
	m_LastError = messageBuffer.str();
    
    DEBUG_LOG( "ERROR handler returning warning" );
	TRACE_LOG( m_LastError );
}

void XercesDOMTreeErrorHandler::error( const SAXParseException& toCatch )
{   //Errors from schema validation
	
	DEBUG_LOG( "ERROR handler - severe" );
    m_SawErrors = true;
          
   	stringstream messageBuffer;
	
	messageBuffer << "File/buffer : ";
	
	if( toCatch.getSystemId() != NULL )
		messageBuffer << localForm( toCatch.getSystemId() ) << " ";
	if( toCatch.getPublicId() != NULL )
   		messageBuffer << localForm( toCatch.getPublicId() ) << " ";
   		
	messageBuffer << " line " << ( int )toCatch.getLineNumber()	<<
		", column " << ( int )toCatch.getColumnNumber()	<< " : " << localForm( toCatch.getMessage() );
	m_LastError = messageBuffer.str();
    
    DEBUG_LOG( "ERROR handler returning exception" );
    throw runtime_error( m_LastError );
}

void XercesDOMTreeErrorHandler::fatalError( const SAXParseException& toCatch )
{		//Errors from incorrect XML
	DEBUG_LOG( "ERROR handler - fatal" );
	 	
	m_SawErrors = true;
    
	stringstream messageBuffer;
	
	messageBuffer << "File/buffer : ";
	
	if( toCatch.getSystemId() != NULL )
		messageBuffer << localForm( toCatch.getSystemId() ) << " ";
	if( toCatch.getPublicId() != NULL )
   		messageBuffer << localForm( toCatch.getPublicId() ) << " ";
   		
	messageBuffer << " line " << ( int )toCatch.getLineNumber()	<<
		", column " << ( int )toCatch.getColumnNumber()	<< " : " << localForm( toCatch.getMessage() );
	m_LastError = messageBuffer.str();
    
	DEBUG_LOG( "ERROR handler returning exception" );
	throw runtime_error( m_LastError );
}

void XercesDOMTreeErrorHandler::resetErrors()
{
	m_SawErrors = false;
}

void XmlUtil::CreateUTF8Transcoder()
{
	XMLTransService::Codes resultCode ;
	const unsigned int maxBlockSize = 16*1024;
	m_UTF8Transcoder =  XMLPlatformUtils::fgTransService->makeNewTranscoderFor("UTF-8", resultCode, maxBlockSize );
	if ( resultCode != XMLTransService::Ok || m_UTF8Transcoder == NULL  )
	{
		ReleaseUTF8Transcoder();
		throw runtime_error( "Could not create Xerces UTF8Transcoder" );
	}
}
void XmlUtil::ReleaseUTF8Transcoder()
{
	if ( m_UTF8Transcoder != NULL )
	{
		delete m_UTF8Transcoder;
		m_UTF8Transcoder = NULL;
	}
}

string XmlUtil::XMLChtoString( const XMLCh* inputXml  )
{
	XMLTranscoder* UTF8Transcoder = getInstance()->m_UTF8Transcoder;
	if ( UTF8Transcoder == NULL )
		throw runtime_error( "UTF-8 transcoder was already destroyed. Unable to continue." );

	unsigned int utf16size = 0;

	if ( inputXml  == NULL || ( utf16size = XMLString::stringLen( inputXml  ) ) == 0  )
		return "";

	unsigned int utf8AllocatedSize = 2*utf16size;
	XMLByte* resultUtf8  = NULL;
	XMLByte* largerBuffer = NULL;
	
	try
	{
		resultUtf8  = static_cast<XMLByte*>(XMLPlatformUtils::fgMemoryManager->allocate( utf8AllocatedSize ));

		unsigned int utf8Size = 0;
		unsigned int charsTranscoded = 0;

#if ( XERCES_VERSION_MAJOR >= 3 )
		XMLSize_t charsEaten = 0;
#else
		unsigned int charsEaten = 0;
#endif

		while (true)
		{
			utf8Size += UTF8Transcoder->transcodeTo( inputXml  + charsTranscoded, utf16size - charsTranscoded, resultUtf8 + utf8Size, utf8AllocatedSize - utf8Size, charsEaten, XMLTranscoder::UnRep_Throw );
			charsTranscoded += charsEaten;
			if ( charsTranscoded == utf16size )
				break;
			utf8AllocatedSize *= 2;
			largerBuffer = static_cast<XMLByte*>( XMLPlatformUtils::fgMemoryManager->allocate( utf8AllocatedSize ) );
			memcpy( largerBuffer, resultUtf8, utf8Size ) ;	
			XMLPlatformUtils::fgMemoryManager->deallocate( resultUtf8 );
			resultUtf8 = largerBuffer;
			largerBuffer = NULL;
		}

		if ( ( utf8Size + 1 ) > utf8AllocatedSize )
		{
			utf8AllocatedSize = utf8Size + 1;
			largerBuffer = static_cast<XMLByte*>( XMLPlatformUtils::fgMemoryManager->allocate( utf8AllocatedSize ) );
			memcpy(largerBuffer, resultUtf8, utf8Size);	
			XMLPlatformUtils::fgMemoryManager->deallocate( resultUtf8 );
			resultUtf8 = largerBuffer;
			largerBuffer = NULL;
		}

		resultUtf8 [utf8Size] = '\0';
		string result = reinterpret_cast<char*>( resultUtf8  );
		XMLPlatformUtils::fgMemoryManager->deallocate( resultUtf8  );
		resultUtf8  = NULL;
		return result;
	}
	catch ( ... )
	{
		XMLPlatformUtils::fgMemoryManager->deallocate( resultUtf8  );
		resultUtf8  = NULL;
		XMLPlatformUtils::fgMemoryManager->deallocate( largerBuffer );
		TRACE_LOG( "Error transcoding XMLCh* to UTF-8" );
		throw;
	}
}

