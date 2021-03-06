#if ( defined( WIN32 ) && ( _MSC_VER <= 1400 ) )
	#ifdef _HAS_ITERATOR_DEBUGGING
		#undef _HAS_ITERATOR_DEBUGGING
	#endif
	#define _HAS_ITERATOR_DEBUGGING 0
#endif

#include <sstream>

#include "WSRM/SequenceAcknowledgement.h"

#include "SepaReject.h"

SepaReject::SepaReject( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* document, const string& messageFilename ) :
	MessageEvaluator( document, MessageEvaluator::Sepa_Reject, messageFilename )
{
	m_SequenceResponse = NULL;
}

SepaReject::~SepaReject()
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