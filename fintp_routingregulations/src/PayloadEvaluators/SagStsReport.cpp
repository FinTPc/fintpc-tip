#if ( defined( WIN32 ) && ( _MSC_VER <= 1400 ) )
	#ifdef _HAS_ITERATOR_DEBUGGING
		#undef _HAS_ITERATOR_DEBUGGING
	#endif
	#define _HAS_ITERATOR_DEBUGGING 0
#endif

#include <sstream>

#include "WSRM/SequenceAcknowledgement.h"
#include "WSRM/SequenceFault.h"

#include "SagStsReport.h"

#undef S_NORMAL

#define BOOST_REGEX_MATCH_EXTRA
#include <boost/regex.hpp>

#include "../CurrencyAmount.h"

SagStsReport::SagStsReport( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* document, const string& messageFilename ) :
	MessageEvaluator( document, MessageEvaluator::Sag_StsReport, messageFilename )
{
	m_SequenceResponse = NULL;
}

SagStsReport::~SagStsReport()
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