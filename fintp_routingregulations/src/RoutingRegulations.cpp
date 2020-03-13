#include "RoutingRegulations.h"

#include "StringUtil.h"
#include "Trace.h"
#include "AppExceptions.h"

#include "MessageEvaluator.h"

using namespace FinTP;

// SepaRules implementation
int SepaRules::Parse( const string& options )
{
	int parsedOptions = NONE;

	StringUtil splitOptions( options );
	splitOptions.Split( "|" );

	while( splitOptions.MoreTokens() )
	{
		string crtValue = StringUtil::ToUpper( splitOptions.NextToken() );
		
		{
			stringstream errorMessage;
			errorMessage << "Unknown options used as parameter for SepaRules : [" << crtValue << "]";
			throw runtime_error( errorMessage.str() );
		}
	}
	return parsedOptions;
}

string SepaRules::ToString( int options )
{
	switch( options )
	{
		case NONE :
			return "NONE";
		default :
			break;
	}

	stringstream hybridOptions;
	//if( ( options & SEPA_SCHEMA ) == SEPA_SCHEMA )
	//	hybridOptions << "SEPA_SCHEMA,";
	//if( ( options & SEPA_BATCH_AMOUNT ) == SEPA_BATCH_AMOUNT )
	//	hybridOptions << "SEPA_BATCH_AMOUNT,";

	return hybridOptions.str();
}

// SepaOptions implementation
int SepaOptions::Parse( const string& options )
{
	int parsedOptions = NONE;

	StringUtil splitOptions( options );
	splitOptions.Split( "|" );

	while( splitOptions.MoreTokens() )
	{
		string crtValue = StringUtil::ToUpper( splitOptions.NextToken() );
		
		{
			stringstream errorMessage;
			errorMessage << "Unknown options used as parameter for SepaOptions : [" << crtValue << "]";
			throw runtime_error( errorMessage.str() );
		}
	}
	return parsedOptions;
}

string SepaOptions::ToString( int options )
{
	switch( options )
	{
		case NONE :
			return "NONE";
		default :
			break;
	}

	stringstream hybridOptions;
	//if( ( options & SEPA_SCHEMA ) == SEPA_SCHEMA )
	//	hybridOptions << "SEPA_SCHEMA,";
	//if( ( options & SEPA_BATCH_AMOUNT ) == SEPA_BATCH_AMOUNT )
	//	hybridOptions << "SEPA_BATCH_AMOUNT,";

	return hybridOptions.str();
}

// ValidationActions implementation
int ValidationActions::Parse( const string& options )
{
	int parsedOptions = NONE;

	StringUtil splitOptions( options );
	splitOptions.Split( "|" );

	while( splitOptions.MoreTokens() )
	{
		string crtValue = StringUtil::ToUpper( splitOptions.NextToken() );
		
		{
			stringstream errorMessage;
			errorMessage << "Unknown options used as parameter for ValidationActions : [" << crtValue << "]";
			throw runtime_error( errorMessage.str() );
		}
	}
	return parsedOptions;
}

string ValidationActions::ToString( int options )
{
	switch( options )
	{
		case NONE :
			return "NONE";
		default :
			break;
	}

	stringstream hybridOptions;
	//if( ( options & SEPA_SCHEMA ) == SEPA_SCHEMA )
	//	hybridOptions << "SEPA_SCHEMA,";
	//if( ( options & SEPA_BATCH_AMOUNT ) == SEPA_BATCH_AMOUNT )
	//	hybridOptions << "SEPA_BATCH_AMOUNT,";

	return hybridOptions.str();
}

//Regulations implementation
XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* Regulations::ValidateString( const string& options, const string& message )
{
	XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc = NULL;
	try
	{
		doc = XmlUtil::DeserializeFromString( message );
		if ( doc == NULL )
			throw logic_error( "Empty document received" );
	}
	catch( const AppException& ex )
	{
		DEBUG( "Document cannot be deserialized to an XML [" << ex.getMessage() << "]" );
		DEBUG( "Text payload is [" << message << "]" );
		
		if ( doc != NULL )
		{
			doc->release();
			doc = NULL;
		}
		
		throw;
	}
	catch( const std::exception& ex )
	{
		DEBUG( "Document cannot be deserialized to an XML [" << ex.what() << "]" );
		DEBUG( "Text payload is [" << message << "]" );
		
		if ( doc != NULL )
		{
			doc->release();
			doc = NULL;
		}
		
		throw;
	}
	catch( ... )
	{
		DEBUG( "Document cannot be deserialized to an XML [unknown reason]" );
		DEBUG( "Text payload is [" << message << "]" );
		
		if ( doc != NULL )
		{
			doc->release();
			doc = NULL;
		}
		
		throw;
	}
	return doc;
}

// Validate will return an "Sepa_InvalidMessage" for all errors that it can't report ( invalid xml, bad namespace.. )
MessageEvaluator* Regulations::Validate( const string& options, const string& message, const string& messageFilename )
{
	DEBUG( "Starting validation from string format" );
	XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc = NULL;
	MessageEvaluator* result = NULL;
	
	try
	{
		// Step 1 : Ensure the string is a valid XML 
		doc = ValidateString( options, message );
	}
	catch( ... )
	{
		if ( doc != NULL )
		{
			doc->release();
			doc = NULL;
		}

		if ( result != NULL )
			delete result;

		// this returns a new SepaInvalidMessage( NULL, "Payload is empty" );
		result = MessageEvaluator::getInvalidEvaluator( "XML", "Payload is empty or not a valid XML", messageFilename );
	}

	// Step 3 : Run Validation on the XML doc
	if ( !result )
		result = Validate( options, doc, messageFilename );
	
	return result;
}

// Validation will create a MessageEvaluator
// Methods invoking Validate() will have to destroy the evaluator once they are done using it
// Validate() will fill data needed to invoke getSequenceResponse() on the returned evaluator
MessageEvaluator* Regulations::Validate( const string& options, XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* message, const string& messageFilename )
{
	DEBUG( "Starting validation from XML format" );
	MessageEvaluator* evaluator = NULL;

	// Step 1 : Create the evaluator
	try
	{
		evaluator = MessageEvaluator::getEvaluator( message, messageFilename );
	}
	catch( ... )
	{
		if ( evaluator != NULL )
		{
			delete evaluator;
			evaluator = NULL;
		}
		// this returns a new SepaInvalidMessage( NULL, "Payload is empty" );
		throw runtime_error( "Could not create an evaluator for message" );
	}

	// Step 2 : Run the schema against the message -> done in xslt using external regex-match
	//if ( !evaluator->ValidateCharset() )
	//	return evaluator;

	// Step 3 : Run the schema against the message
	if ( !evaluator->ValidateToSchema() )
		return evaluator;

	// Step 4 : Run the ValidationCheck XSLT on the message
	//if ( !evaluator->ValidateToXslt() )
	//	return evaluator;
	if ( !evaluator->ValidateToXslt() )
	{
		DEBUG( "Validate to xslt failed! continuing with message specific validation" );
	}

	// Step 5 : Run evaluator specific validation
	if ( !evaluator->Validate() )
		return evaluator;

	// Step 4 : Create a response message
	return evaluator;
}

