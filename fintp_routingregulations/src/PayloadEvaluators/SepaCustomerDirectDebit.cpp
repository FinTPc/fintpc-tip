#if ( defined( WIN32 ) && ( _MSC_VER <= 1400 ) )
	#ifdef _HAS_ITERATOR_DEBUGGING
		#undef _HAS_ITERATOR_DEBUGGING
	#endif
	#define _HAS_ITERATOR_DEBUGGING 0
#endif

#include <sstream>

#include "WSRM/SequenceAcknowledgement.h"
#include "WSRM/SequenceFault.h"

#include "SepaCustomerDirectDebit.h"

#undef S_NORMAL

#define BOOST_REGEX_MATCH_EXTRA
#include <boost/regex.hpp>

#include "../CurrencyAmount.h"

SepaCustomerDirectDebit::SepaCustomerDirectDebit( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* document, const string& messageFilename ) :
	MessageEvaluator( document, MessageEvaluator::Sepa_CreditTransfer, messageFilename )
{
	m_SequenceResponse = NULL;
}

SepaCustomerDirectDebit::~SepaCustomerDirectDebit()
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

bool SepaCustomerDirectDebit::Validate()
{
	try
	{
		DOMElement *rootElem = m_Document->getDocumentElement();
		if ( rootElem == NULL )
			return false;

		DOMNodeList* msgElemNodes = rootElem->getElementsByTagName( unicodeFormWs( "pacs.003.001.01" ) );
		if ( ( msgElemNodes == NULL ) || ( msgElemNodes->getLength() == 0 ) )
			return false;
		
		DOMElement *msgElemNode = dynamic_cast< DOMElement* >( msgElemNodes->item( 0 ) );
		if ( msgElemNode == NULL )
			throw logic_error( "Expected [pacs.003.001.01] to be an element." );

		// get the header amt
		DOMNodeList* grpHdrNodes = msgElemNode->getElementsByTagName( unicodeFormWs( "GrpHdr" ) );
		if ( ( grpHdrNodes == NULL ) || ( grpHdrNodes->getLength() == 0 ) )
			return false;
		
		DOMElement *grpHdrNode = dynamic_cast< DOMElement* >( grpHdrNodes->item( 0 ) );
		if ( grpHdrNode == NULL )
			throw logic_error( "Expected [pacs.003.001.01/GrpHdr] to be an element." );

		DOMNodeList* hdrAmtNodes = grpHdrNode->getElementsByTagName( unicodeFormWs( "TtlIntrBkSttlmAmt" ) );
		if ( ( hdrAmtNodes == NULL ) || ( hdrAmtNodes->getLength() == 0 ) )
			return false;
		
		DOMElement *hdrAmtNode = dynamic_cast< DOMElement* >( hdrAmtNodes->item( 0 ) );
		if ( hdrAmtNode == NULL )
			throw logic_error( "Expected [pacs.003.001.01/GrpHdr/TtlIntrBkSttlmAmt] to be an element." );

		currency_amount hdrAmt( localForm( hdrAmtNode->getTextContent() ) );

		// get the trns amt
		DOMNodeList* txInfNodes = msgElemNode->getElementsByTagName( unicodeFormWs( "CdtTrfTxInf" ) );
		if ( ( txInfNodes == NULL ) || ( txInfNodes->getLength() == 0 ) )
			return false;
		
		currency_amount trnAmt( 0, 0 );
		for ( unsigned int i=0; i<txInfNodes->getLength(); i++ )
		{
			DOMElement *txInfNode = dynamic_cast< DOMElement* >( txInfNodes->item( i ) );
			if ( txInfNode == NULL )
				throw logic_error( "Expected [pacs.003.001.01/CdtTrfTxInf] to be an element." );

			DOMNodeList* txAmtNodes = txInfNode->getElementsByTagName( unicodeFormWs( "IntrBkSttlmAmt" ) );
			if ( ( txAmtNodes == NULL ) || ( txAmtNodes->getLength() == 0 ) )
				return false;
			
			DOMElement *txAmtNode = dynamic_cast< DOMElement* >( txAmtNodes->item( 0 ) );
			if ( txAmtNode == NULL )
				throw logic_error( "Expected [pacs.003.001.01/CdtTrfTxInf/IntrBkSttlmAmt] to be an element." );

			trnAmt = trnAmt + currency_amount( localForm( txAmtNode->getTextContent() ) );

			// validate rmtinfo
			DOMNodeList* rmtInfNodes = txInfNode->getElementsByTagName( unicodeFormWs( "RmtInf" ) );
			if ( ( rmtInfNodes == NULL ) || ( rmtInfNodes->getLength() == 0 ) )
				continue;
			
			DOMElement *rmtInfNode = dynamic_cast< DOMElement* >( rmtInfNodes->item( 0 ) );
			if ( rmtInfNode == NULL )
				continue;

			DOMNodeList* strdNodes = rmtInfNode->getElementsByTagName( unicodeFormWs( "Strd" ) );
			if ( ( strdNodes == NULL ) || ( strdNodes->getLength() == 0 ) )
				continue;
			
			string strdRmtInfoRaw = XmlUtil::SerializeNodeToString( strdNodes->item( 0 ) );
			// remove element tag
			strdRmtInfoRaw = strdRmtInfoRaw.substr( strdRmtInfoRaw.find_first_of( "<", 1 ) );
			strdRmtInfoRaw = strdRmtInfoRaw.substr( 0, strdRmtInfoRaw.find_last_of( ">", strdRmtInfoRaw.length() - 2 ) + 1 );

			boost::regex ex( "(>[\\s\\t\\r\\n]*<)" );
			string strdRmtInfo = boost::regex_replace( strdRmtInfoRaw, ex, "><", boost::match_default | boost::format_all );

			if ( strdRmtInfo.length() > 141 )
			{
				m_Valid = false;

				if ( m_SequenceResponse == NULL )
					m_SequenceResponse = new wsrm::SequenceFault( m_MessageFilename );
				wsrm::SequenceFault* response = dynamic_cast< wsrm::SequenceFault* >( m_SequenceResponse );
				if ( response == NULL )
				{
					// this must be an ack seq
					delete m_SequenceResponse;
					m_SequenceResponse = new wsrm::SequenceFault( m_MessageFilename );
					response = dynamic_cast< wsrm::SequenceFault* >( m_SequenceResponse );
				}
				if ( response == NULL )
					throw logic_error( "Sequence response is not a fault sequence" );

				wsrm::Nack nack( "T30_T_Structured_Remittance_Information_Length_Out_of_Range" );

				DOMNodeList* pmtIdNodes = txInfNode->getElementsByTagName( unicodeFormWs( "PmtId" ) );
				if ( ( pmtIdNodes == NULL ) || ( pmtIdNodes->getLength() == 0 ) )
					return false;
			
				DOMElement *pmtIdNode = dynamic_cast< DOMElement* >( pmtIdNodes->item( 0 ) );
				if ( pmtIdNode == NULL )
					throw logic_error( "Expected [pacs.003.001.01/CdtTrfTxInf/PmtId] to be an element." );

				DOMNodeList* txIdNodes = pmtIdNode->getElementsByTagName( unicodeFormWs( "TxId" ) );
				if ( ( txIdNodes == NULL ) || ( txIdNodes->getLength() == 0 ) )
					return false;
			
				DOMElement *txIdNode = dynamic_cast< DOMElement* >( txIdNodes->item( 0 ) );
				if ( txIdNode == NULL )
					throw logic_error( "Expected [pacs.003.001.01/CdtTrfTxInf/PmtId/TxId] to be an element." );

				response->AddChild( localFormWs( txIdNode->getTextContent() ), nack );
			}
		}

		if( hdrAmt != trnAmt )
		{
			stringstream errorMessage;
			errorMessage << "Header amount [" << hdrAmt << "] != sum( transaction amount ) [" << trnAmt << "]";
			m_Valid = false;
			if ( m_SequenceResponse == NULL )
				m_SequenceResponse = new wsrm::SequenceFault( m_MessageFilename );
			wsrm::SequenceFault* response = dynamic_cast< wsrm::SequenceFault* >( m_SequenceResponse );
			if ( response == NULL )
			{
				// this must be an ack seq
				delete m_SequenceResponse;
				m_SequenceResponse = new wsrm::SequenceFault( m_MessageFilename );
				response = dynamic_cast< wsrm::SequenceFault* >( m_SequenceResponse );
			}
			if ( response == NULL )
				throw logic_error( "Sequence response is not a fault sequence" );
			response->setErrorCode( "H4_T_Bad_Total_Amount" );
			response->setErrorReason( errorMessage.str() );
		}
	}
	catch ( ... )
	{
		if ( m_SequenceResponse != NULL )
		{
			delete m_SequenceResponse;
			m_SequenceResponse = NULL;
		}
		throw;
	}

	return m_Valid;
}