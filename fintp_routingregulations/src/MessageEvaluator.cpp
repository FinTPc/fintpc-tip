#if ( defined( WIN32 ) && ( _MSC_VER <= 1400 ) )
	#ifdef _HAS_ITERATOR_DEBUGGING
		#undef _HAS_ITERATOR_DEBUGGING
	#endif
	#define _HAS_ITERATOR_DEBUGGING 0
#endif
#ifdef WIN32
	#define __MSXML_LIBRARY_DEFINED__
	#include "windows.h"
#else
	#include <unistd.h>
#endif

#include <xalanc/PlatformSupport/XSLException.hpp>
#include <xalanc/XPath/NodeRefList.hpp>
#include <xalanc/XalanDOM/XalanDOMString.hpp>
#include <xalanc/XercesParserLiaison/XercesDocumentWrapper.hpp>

#include "XmlUtil.h"
#include "StringUtil.h"
#include "Collaboration.h"
#include "Trace.h"
#include "TimeUtil.h"

#include "XSD/XSDFilter.h"
#include "XSD/XSDValidationException.h"

#include "XSLT/XSLTFilter.h"

#include "WSRM/SequenceAcknowledgement.h"
#include "WSRM/SequenceFault.h"

#include "PayloadEvaluators/SepaCreditTransfer.h"
#include "PayloadEvaluators/SepaReject.h"
#include "PayloadEvaluators/SepaReturn.h"
#include "PayloadEvaluators/SepaInvalidMessage.h"
#include "PayloadEvaluators/SepaCustomerDirectDebit.h"
#include "PayloadEvaluators/SepaPaymentReversal.h"
#include "PayloadEvaluators/SagStsReport.h"
#include "MessageEvaluator.h"

MessageEvaluator::MessageEvaluator( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* document, MessageEvaluator::EvaluatorType evaluatorType, const string& messageFilename ) :
	m_MessageFilename( messageFilename )
{
	m_Document = document;
	m_Valid = false;
	
	m_DocumentWrapper = NULL;
	m_XalanDocument = NULL;
	m_EvaluatorType = evaluatorType;
}

MessageEvaluator::MessageEvaluator( const MessageEvaluator& source )
{
	DEBUG2( "Copy ctor" );
	m_Document = source.getDocument();
	m_Valid = ( m_Document != NULL );
	m_MessageFilename = source.m_MessageFilename;
	
	m_DocumentWrapper = NULL;
	m_XalanDocument = NULL;
	m_EvaluatorType = source.getEvaluatorType();
}

MessageEvaluator& MessageEvaluator::operator=( const MessageEvaluator& source )
{
	if ( this == &source )
		return *this;

	DEBUG2( "op=" );
	if ( m_DocumentWrapper != NULL )
	{
		//m_DocumentWrapper->destroyWrapper();
		DEBUG( "Destroying wrapper" );
		
		delete m_DocumentWrapper;
	}
	
	m_Document = source.getDocument();
	m_Valid = ( m_Document != NULL );
	m_MessageFilename = source.m_MessageFilename;
	m_Namespace = source.m_Namespace;
	m_XPathFields = source.m_XPathFields;
	m_Fields = source.m_Fields;
		
	m_DocumentWrapper = NULL;
	m_XalanDocument = NULL;
	m_EvaluatorType = source.getEvaluatorType();
	return *this;
}

//void MessageEvaluator::UpdateMessage( RoutingMessage* message )
//{
//}
MessageEvaluator* MessageEvaluator::getInvalidEvaluator( const string& errorCode, const string& errorMessage, const string& messageFilename )
{
	MessageEvaluator* returnedEvaluator = NULL;
	returnedEvaluator = new SepaInvalidMessage( NULL, errorCode, errorMessage, messageFilename );
	return returnedEvaluator;
}

MessageEvaluator* MessageEvaluator::getEvaluator( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* document, const string& messageFilename )
{			
	MessageEvaluator* returnedEvaluator = NULL;

	if ( document == NULL )
	{
		return getInvalidEvaluator( "XML", "Payload is empty", messageFilename );
	}
	try
	{
		DEBUG_GLOBAL( "Evaluating namespace ..." );   	
		string ns = XmlUtil::getNamespace( document );
		DEBUG_GLOBAL( "Namespace is : [" << ns << "]" );

		if ( ns == "urn:iso:std:iso:20022:tech:xsd:pacs.008.001.01" )
		{
			DEBUG_GLOBAL( "The message is in FI to FI Customer Credit Transfer xml format" );
			returnedEvaluator = new SepaCreditTransfer( document, messageFilename );
		}
		else if ( ns == "urn:iso:std:iso:20022:tech:xsd:pacs.004.001.01" )
		{
			DEBUG_GLOBAL( "The message is in Return-Payment Return xml format" );
			returnedEvaluator = new SepaReturn( document, messageFilename );
		}
		else if ( ns == "urn:iso:std:iso:20022:tech:xsd:pacs.002.001.02" )
		{
			DEBUG_GLOBAL( "The message is in Reject-Payment Status Report xml format" );
			returnedEvaluator = new SepaReject( document, messageFilename );
		}
		else if ( ns == "urn:iso:std:iso:20022:tech:xsd:pacs.003.001.01" )
		{
			DEBUG_GLOBAL( "The message is in Customer Direct Debit xml format" );
			returnedEvaluator = new SepaCustomerDirectDebit( document, messageFilename );
		}
		else if ( ns == "urn:iso:std:iso:20022:tech:xsd:pacs.007.001.01" )
		{
			DEBUG_GLOBAL( "The message is in Payment Reversal xml format" );
			returnedEvaluator = new SepaPaymentReversal( document, messageFilename );
		}
		else if ( ns == "urn:swift:sag:xsd:fta.report.1.0" )
		{
			DEBUG_GLOBAL( "The message is in SAG FTA Report xml format" );
			returnedEvaluator = new SagStsReport( document, messageFilename );
		}
		if ( returnedEvaluator != NULL )
		{
			returnedEvaluator->setNamespace( ns );
			return returnedEvaluator;
		}

		DEBUG( "No known evaluator matches this payload [" << ns << "]" );
		return getInvalidEvaluator( "XSD", "Invalid namespace for message", messageFilename );
	}
	catch( const exception &ex )
	{
		if ( returnedEvaluator != NULL )
		{
			delete returnedEvaluator;
			returnedEvaluator = NULL;
		}

		TRACE_GLOBAL( "Namespace evaluation failed : [" << ex.what() << "]" );
	}
	catch( ... )
	{
		if ( returnedEvaluator != NULL )
		{
			delete returnedEvaluator;
			returnedEvaluator = NULL;
		}

		TRACE_GLOBAL( "Namespace evaluation failed. [unknown exception]" );
	}

	return getInvalidEvaluator( "XML", "Payload is empty or not a valid XML", messageFilename );
}

MessageEvaluator::EvaluatorType MessageEvaluator::Parse( const string& evaluatorType )
{
	if ( evaluatorType == "Sepa_CreditTransfer" )
		return MessageEvaluator::Sepa_CreditTransfer;

	if ( evaluatorType == "Sepa_Return" )
		return MessageEvaluator::Sepa_Return;

	if ( evaluatorType == "Sepa_Reject" )
		return MessageEvaluator::Sepa_Reject;

	if ( evaluatorType == "Sag_StsReport" )
		return MessageEvaluator::Sag_StsReport;

	if ( evaluatorType == "Sepa_InvalidMessage" )
		return MessageEvaluator::Sepa_InvalidMessage;

	stringstream errorMessage;
	errorMessage << "Invalid evaluator type [" << evaluatorType << "]";
	throw invalid_argument( errorMessage.str() );
}

string MessageEvaluator::getField( const int field )
{
	if ( !m_Valid )
		throw logic_error( "The message evaluator is invalid. Unable to evaluate field" );
		
	if ( m_Document == NULL )
		throw logic_error( "Document is NULL and cannot be evaluated" );
		
	try 
	{
		XALAN_USING_XALAN( XercesDocumentWrapper );
		XALAN_USING_XALAN( XalanDocument );
		
		if ( m_Fields.find( field ) == m_Fields.end() )
		{
			// map xerces dom to xalan document
			if ( m_XalanDocument == NULL )
			{
#ifdef XALAN_1_9
				XALAN_USING_XERCES( XMLPlatformUtils )
				m_DocumentWrapper = new XercesDocumentWrapper( *XMLPlatformUtils::fgMemoryManager, m_Document, true, true, true );
#else
				m_DocumentWrapper = new XercesDocumentWrapper( m_Document, true, true, true );
#endif
				m_XalanDocument = ( XalanDocument* )m_DocumentWrapper;
				
				if( m_XalanDocument == NULL )
					throw runtime_error( "Unable to parse payload for evaluation." );
			}
			m_Fields.insert( pair< int, string >( field, internalGetField( field ) ) );
		}
		string result = m_Fields[ field ];		
		string fieldName = internalGetFieldName( field );
		DEBUG( "Field [" << fieldName << "] = [" << result << "]" );
		return result;
	}
	catch( const exception &ex )
	{
		TRACE( "Error evaluating field [" << field << "] : " << ex.what() );
		return "";
	}
	catch( ... )
	{
		TRACE( "Error evaluating field [" << field << "] : Unknown reason" );
		return "";
	}
}

string MessageEvaluator::getCustomXPath( const string& xpath, const bool noCache )
{
	if ( !m_Valid )
		throw logic_error( "The message evaluator is invalid. Unable to evaluate custom xpath." );
	
	if ( m_Document == NULL )
		throw logic_error( "Document is NULL and cannot be evaluated" );
	
	try
	{
		XALAN_USING_XALAN( XercesDocumentWrapper );
		//XALAN_USING_XALAN( XalanDocument );
		
		string result = "";
		if ( !noCache )
			result = m_XPathFields[ xpath ];
			
		if ( result.length() == 0 )
		{
			// map xerces dom to xalan document
			if ( m_XalanDocument == NULL )
			{
#ifdef XALAN_1_9
				XALAN_USING_XERCES( XMLPlatformUtils )
				m_DocumentWrapper = new XercesDocumentWrapper( *XMLPlatformUtils::fgMemoryManager, m_Document, true, true, true );
#else
				m_DocumentWrapper = new XercesDocumentWrapper( m_Document, true, true, true );
#endif
				m_XalanDocument = ( XALAN_CPP_NAMESPACE_QUALIFIER XalanDocument* )m_DocumentWrapper;
				
				if( m_XalanDocument == NULL )
					throw runtime_error( "Unable to parse payload for evaluation." );
			}
			if ( noCache )
			{
				return XPathHelper::SerializeToString( XPathHelper::Evaluate( xpath, m_XalanDocument, m_Namespace ) );
			}
				
			m_XPathFields[ xpath ] = XPathHelper::SerializeToString( XPathHelper::Evaluate( xpath, m_XalanDocument, m_Namespace ) );
			result = m_XPathFields[ xpath ];
		}
		DEBUG( "Custom XPath evaluation of [" << xpath << "] = [" << result << "]" );
		return result;
	}
	catch( ... )
	{
		TRACE( "Error evaluating custom xpath [" << xpath << "]" );
		return "";
	}
}

bool MessageEvaluator::ValidateCharset()
{
	/*DOMNode* crtNode = m_Document->getDocumentElement();
	string messageData = localFormWs( crtNode->getTextContent() );

	string charsetRegex = "^[[:latin:]]*$";
	boost::regex boostCharsetRegex( charsetRegex );
	boost::smatch charsetMatch;
	
	bool match = false;
	try
	{
		match = boost::regex_search( messageData, charsetMatch, boostCharsetRegex, boost::match_extra );
	}	
	catch( const boost::bad_pattern& ex )
	{
		stringstream errorMessage;
		errorMessage << "Unable to evaluate charset using regex [" << charsetRegex << "] : Boost regex exception : " << ex.what() ;
		
		TRACE( errorMessage.str() );
		throw runtime_error( errorMessage.str() );
	}
	
	if( !match )
	{
		stringstream errorMessage;
		errorMessage << "Charset evaluation failed";
		
		m_SequenceResponse = new wsrm::SequenceFault( m_MessageFilename );
		wsrm::SequenceFault* response = dynamic_cast< wsrm::SequenceFault* >( m_SequenceResponse );
		if ( response == NULL )
			throw logic_error( "Sequence response is not a fault sequence" );
		response->setErrorCode( "G4_T_Invalid_Charset" );
		response->setErrorReason( errorMessage.str() );
	}*/
	return true;
}

bool MessageEvaluator::ValidateToSchema()
{
	string xsdSchemaName = this->getSchema();
	if ( xsdSchemaName.length() > 0 )
	{
		XSDFilter xsdFilter;
		NameValueCollection headers;
		headers.Add( XSDFilter::XSDFILE, xsdSchemaName );
		headers.Add( XSDFilter::XSDNAMESPACE, this->getNamespace() );

		try
		{	
			xsdFilter.ProcessMessage( m_Document, headers, true );
			m_Valid = true;
		}
		catch( const XSDValidationException& ex )
		{
			stringstream errorMessage;
			errorMessage << ex.getMessage() << " - " << ex.getAdditionalInfo( "Invalid_reason" );

			m_Valid = false;

			m_SequenceResponse = new wsrm::SequenceFault( m_MessageFilename );
			wsrm::SequenceFault* response = dynamic_cast< wsrm::SequenceFault* >( m_SequenceResponse );
			if ( response == NULL )
				throw logic_error( "Sequence response is not a fault sequence" );
			response->setErrorCode( "XSD" );
			response->setErrorReason( errorMessage.str() );
		}
		catch( ... )
		{
			TRACE( "An unknown error occured during validation with schema" );
			m_Valid = false;

			throw;
		}
	}
	return m_Valid;
}

bool MessageEvaluator::ValidateToXslt()
{
	XSLTFilter xsltFilter;

	NameValueCollection trHeaders;
	trHeaders.Add( XSLTFilter::XSLTFILE, getValidationXslt() );
	trHeaders.Add( XSLTFilter::XSLTUSEEXT, "true" );

	XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument *doc = NULL;
		
	try
	{
		WorkItem< ManagedBuffer > workItem( new ManagedBuffer() );
		xsltFilter.ProcessMessage( m_Document, workItem, trHeaders, true );
		string actualOutput = workItem.get()->str();

		doc = XmlUtil::DeserializeFromString( actualOutput );
		if ( doc == NULL ) 
			throw logic_error( "Document could not be deserialized from input data" );

		DOMElement* root = doc->getDocumentElement();
		if ( root == NULL ) 
			throw logic_error( "Document could not be deserialized from input data [missing root]" );

		m_SequenceResponse = wsrm::SequenceResponse::Deserialize( root, m_MessageFilename );
		wsrm::SequenceFault* response = dynamic_cast< wsrm::SequenceFault* >( m_SequenceResponse );
		if ( response != NULL )
			m_Valid = false;
	}
	catch( ... )
	{
		TRACE( "An unknown error occured during validation with XSLT" );
		m_Valid = false;

		if ( doc != NULL )
			doc->release();
		throw;
	}
	
	if ( doc != NULL )
	{
		doc->release();
		doc = NULL;
	}

	return m_Valid;
}
	
string MessageEvaluator::SerializeResponse()
{
	if ( m_Document == NULL )
	{
		// the doc could not be parsed, invalid xml, etc...
		try
		{
			// create the document 
			DOMImplementation* impl = DOMImplementationRegistry::getDOMImplementation( unicodeForm( "LS" ) );

			//assign the root element
			m_Document = impl->createDocument( 0, unicodeForm( "Document" ), 0 );
			m_Document->getDocumentElement()->setAttribute( unicodeFormWs( "xmlns" ), unicodeFormWs( "urn:iso:std:iso:20022:tech:xsd:pacs.002.001.02" ) );
			m_SequenceResponse->Serialize( m_Document->getDocumentElement() );
		}
		catch( ... )
		{
			if( m_Document != NULL )
				m_Document->release();
			throw;
		}
	}
	else
		m_SequenceResponse->Serialize( m_Document->getDocumentElement() );

	TRACE( "Sequence response before transformation [" << XmlUtil::SerializeToString( m_Document ) << "]" );
	XSLTFilter xsltFilter;

	NameValueCollection trHeaders;
	trHeaders.Add( XSLTFilter::XSLTFILE, getReplyXslt() );
	trHeaders.Add( XSLTFilter::XSLTUSEEXT, "true" );
	trHeaders.Add( "XSLTPARAMMSGID", StringUtil::Pad( Collaboration::GenerateGuid(), "\'", "\'" ) );
	trHeaders.Add( "XSLTPARAMDATETIME", StringUtil::Pad( TimeUtil::Get( "%Y-%m-%dT%H:%M:%S", 19 ), "\'", "\'" ) );
	trHeaders.Add( "XSLTPARAMFILENAME", StringUtil::Pad( m_MessageFilename, "\'", "\'" ) );

	xsltFilter.ProcessMessage( m_Document, trHeaders, true );

	return XmlUtil::SerializeToString( m_Document );
}

MessageEvaluator::~MessageEvaluator()
{
	try
	{
		if ( m_Document != NULL )
		{
			//m_DocumentWrapper->destroyWrapper();
			DEBUG( "Destroying doc" );

			m_Document->release();
			m_Document = NULL;
		}
		if ( m_DocumentWrapper != NULL )
		{
			//m_DocumentWrapper->destroyWrapper();
			DEBUG( "Destroying wrapper" );

			delete m_DocumentWrapper;
			m_DocumentWrapper = NULL;
		}
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while destroying m_DocumentWrapper" );
		}catch( ... ){};
	}

	try
	{
		m_Fields.clear();
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while clearing fields" );
		}catch( ... ){};
	}
}
