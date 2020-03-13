#if ( defined( WIN32 ) && ( _MSC_VER <= 1400 ) )
	#ifdef _HAS_ITERATOR_DEBUGGING
		#undef _HAS_ITERATOR_DEBUGGING
	#endif
	#define _HAS_ITERATOR_DEBUGGING 0
#endif

#include <sstream>

#include "Collaboration.h"

#include "WSRM/SequenceAcknowledgement.h"
#include "WSRM/SequenceFault.h"

#include "SepaInvalidMessage.h"

SepaInvalidMessage::SepaInvalidMessage( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* document, const string& errorCode, const string& reason, const string& messageFilename ) :
	MessageEvaluator( document, MessageEvaluator::Sepa_InvalidMessage, messageFilename )
{
	m_Valid = false;

	string batchId = Collaboration::GenerateGuid();

	m_SequenceResponse = new wsrm::SequenceFault( m_MessageFilename );	
	wsrm::SequenceFault* response = dynamic_cast< wsrm::SequenceFault* >( m_SequenceResponse );
	if ( response == NULL )
		throw logic_error( "Sequence response is not a fault sequence" );
	response->setErrorCode( errorCode );
	response->setErrorReason( reason );
}
	
SepaInvalidMessage::~SepaInvalidMessage()
{
	try
	{
		if ( m_SequenceResponse != NULL )
			delete m_SequenceResponse;
		m_SequenceResponse = NULL;
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while deleting sequence response" );
		} catch( ... ) {}
	}
}