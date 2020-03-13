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

#include "SequenceFault.h"

#include <sstream>

namespace wsrm
{

const string SequenceFault::NOSEQFAULT = "__NOSEQFAULT";

SequenceFault::SequenceFault( const string& identifier, const string& faultCode ) : 
	SequenceResponse( SequenceType::SequenceFault, identifier, "SequenceFault", "wsrm", "http://schemas.xmlsoap.org/ws/2005/02/rm" ),
	m_ErrorCode( faultCode ), m_ErrorReason( "" )
{
}

SequenceFault::~SequenceFault()
{
}

string SequenceFault::internalSerialize() const
{
	return "";
}

SequenceFault* SequenceFault::Deserialize( DOMNode* root, const string& messageFilename )
{
	DOMElement *rootElem = dynamic_cast< DOMElement* >( root );
	if ( rootElem == NULL )
		return NULL;

	DOMNodeList* seqNodes = rootElem->getElementsByTagNameNS( unicodeFormWs( "http://schemas.xmlsoap.org/ws/2005/02/rm" ), unicodeFormWs( "SequenceFault" ) );
	if ( ( seqNodes == NULL ) || ( seqNodes->getLength() == 0 ) )
	{
		// no faults here
		return NULL;
	}

	SequenceFault* returnedResponse = NULL;
	try
	{
		returnedResponse = new SequenceFault();

		DOMElement *selfElem = dynamic_cast< DOMElement* >( seqNodes->item( 0 ) );
		if ( selfElem == NULL )
			throw logic_error( "Root node is not an element." );

		// get id from the first message
		DOMNodeList* idNodes = rootElem->getElementsByTagNameNS( unicodeFormWs( "http://schemas.xmlsoap.org/ws/2002/07/utility" ), unicodeFormWs( "Identifier" ) );
		if ( ( idNodes == NULL ) || ( idNodes->getLength() == 0 ) )
			returnedResponse->setIdentifier( messageFilename );
		else
			returnedResponse->setIdentifier( localFormWs( idNodes->item( 0 )->getTextContent() ) );

		// get fault from the first message
		DOMNodeList* faultNodes = rootElem->getElementsByTagNameNS( unicodeFormWs( "http://schemas.xmlsoap.org/ws/2005/02/rm" ), unicodeFormWs( "FaultCode" ) );
		if ( ( faultNodes != NULL ) && ( faultNodes->getLength() != 0 ) )
			returnedResponse->setErrorCode( localFormWs( faultNodes->item( 0 )->getTextContent() ) );

		// get all nacks
		for ( unsigned int i=0; i<seqNodes->getLength(); i++ )
		{
			DOMElement *seqElem = dynamic_cast< DOMElement* >( seqNodes->item( i ) );
			if ( seqElem == NULL )
				continue;

			DOMNodeList* nackNodes = seqElem->getElementsByTagNameNS( unicodeFormWs( "http://schemas.xmlsoap.org/ws/2005/02/rm" ), unicodeFormWs( "Nack" ) );
			
			// if we don't have a transaction nack in ths sequence, use it as a response code
			if ( nackNodes->getLength() == 0 )
			{
				DOMNodeList* idNodes = seqElem->getElementsByTagNameNS( unicodeFormWs( "http://schemas.xmlsoap.org/ws/2002/07/utility" ), unicodeFormWs( "Identifier" ) );
				if ( ( idNodes == NULL ) || ( idNodes->getLength() == 0 ) )
					returnedResponse->setIdentifier( messageFilename );
				else
					returnedResponse->setIdentifier( localFormWs( idNodes->item( 0 )->getTextContent() ) );

				// get fault from the first message
				DOMNodeList* faultNodes = seqElem->getElementsByTagNameNS( unicodeFormWs( "http://schemas.xmlsoap.org/ws/2005/02/rm" ), unicodeFormWs( "FaultCode" ) );
				if ( ( faultNodes != NULL ) && ( faultNodes->getLength() != 0 ) )
					returnedResponse->setErrorCode( localFormWs( faultNodes->item( 0 )->getTextContent() ) );
			}

			for ( unsigned int j=0; j<nackNodes->getLength(); j++ )
			{
				DOMElement *nackNode = dynamic_cast< DOMElement* >( nackNodes->item( j ) );
				if ( nackNode == NULL )
					continue;

				wsrm::Nack nack( localFormWs( nackNode->getTextContent() ) );
				returnedResponse->AddChild( localFormWs( nackNode->getAttribute( unicodeFormWs( "id" ) ) ), nack );
			}
		}
	}
	catch( ... )
	{
		if ( returnedResponse != NULL )
		{
			delete returnedResponse;
			returnedResponse = NULL;
		}
		throw;
	}
	return returnedResponse;
}

DOMNode* SequenceFault::internalSerialize( DOMNode* root ) const
{
	if ( root == NULL )
		throw invalid_argument( "Root node is NULL" );

	DOMDocument* doc = root->getOwnerDocument();
	if ( doc == NULL )
		throw invalid_argument( "Root node does not belong to a DOMDocument" );

	// serialize the identifier
	(&m_Identifier)->Serialize( root );

	// serialize fault code, reason
	DOMElement* faultElem = NULL;
	try
	{
		stringstream qElemName;
		qElemName << m_NamespaceId << ":FaultCode";
		faultElem = doc->createElementNS( unicodeFormWs( m_NamespaceUri ), unicodeFormWs( qElemName.str() ) );

		faultElem->setTextContent( unicodeFormWs( m_ErrorCode ) );
		root->appendChild( faultElem );
	}
	catch( ... )
	{
		if ( faultElem != NULL )
		{
			faultElem->release();
		}
		throw;
	}
		
	DOMElement* faultReasonElem = NULL;
	try
	{
		stringstream qElemName;
		qElemName << m_NamespaceId << ":FaultReason";
		faultReasonElem = doc->createElementNS( unicodeFormWs( m_NamespaceUri ), unicodeFormWs( qElemName.str() ) );

		faultReasonElem->setTextContent( unicodeFormWs( m_ErrorReason ) );
		root->appendChild( faultReasonElem );
	}
	catch( ... )
	{
		if ( faultReasonElem != NULL )
		{
			faultReasonElem->release();
		}
		throw;
	}

	// serialize all Nacks in the message
	map< string, wsrm::Nack >::const_iterator nackWalker = m_Nacks.begin();
	while( nackWalker != m_Nacks.end() )
	{
		DOMElement* nackElem = dynamic_cast< DOMElement* >( (&(nackWalker->second))->Serialize( root ) );
		if ( nackElem == NULL )
			throw logic_error( "Bad type : [SequenceFault/Nack] should be and element" );
		nackElem->setAttribute( unicodeFormWs( "id" ), unicodeFormWs( nackWalker->first ) );
			
		nackWalker++;
	}

	return root;
}

WSSerializable* SequenceFault::internalDeserialize( const DOMNode* data )
{
	return NULL;
}

bool SequenceFault::IsNack( const string reference ) const
{
	// if no seq fault, then individual nacks will be searched
	if ( m_ErrorCode == SequenceFault::NOSEQFAULT )
	{
		map< string, Nack >::const_iterator finder = m_Nacks.find( reference );
		return ( finder != m_Nacks.end() );
	}
	else
		return true;
}

bool SequenceFault::IsAck( const unsigned long sequence ) const
{
	AcknowledgementRangeSequence::const_iterator ackWalker = m_Acks.begin();
	while( ackWalker != m_Acks.end() )
	{
		// start of sequence greater than item sequence - > not in ack range
		if ( ackWalker->getLower() > sequence )
			break;
			
		// lower sequence <= sequence and upper greater -> in ack range
		if ( ackWalker->getUpper() >= sequence )
			return false;
			
		ackWalker++;
	}
	
	return false;
}
}
