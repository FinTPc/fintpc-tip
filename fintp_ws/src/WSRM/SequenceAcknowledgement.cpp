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

#include "SequenceAcknowledgement.h"

namespace wsrm
{

SequenceAcknowledgement::SequenceAcknowledgement( const string& identifier ) : 
	SequenceResponse( SequenceType::SequenceAcknowledgement, identifier, "SequenceAcknowledgement", "wsrm", "http://schemas.xmlsoap.org/ws/2005/02/rm" )
{
}

SequenceAcknowledgement::~SequenceAcknowledgement()
{
}

string SequenceAcknowledgement::internalSerialize() const
{
	return "";
}

WSSerializable* SequenceAcknowledgement::internalDeserialize( const DOMNode* data )
{
	return NULL;
}

SequenceAcknowledgement* SequenceAcknowledgement::Deserialize( DOMNode* root, const string& messageFilename )
{
	return new SequenceAcknowledgement();
}

DOMNode* SequenceAcknowledgement::internalSerialize( DOMNode* root ) const
{
	if ( root == NULL )
		throw invalid_argument( "Root node is NULL" );

	DOMDocument* doc = root->getOwnerDocument();
	if ( doc == NULL )
		throw invalid_argument( "Root node does not belong to a DOMDocument" );

	// serialize the identifier
	(&m_Identifier)->Serialize( root );

	// serialize all Acks in the message
	AcknowledgementRangeSequence::const_iterator ackWalker = m_Acks.begin();
	while( ackWalker != m_Acks.end() )
	{
		ackWalker++;
	}

	return root;
}

bool SequenceAcknowledgement::IsNack( const string reference ) const
{
	return false;
}

bool SequenceAcknowledgement::IsAck( const unsigned long sequence ) const
{
	AcknowledgementRangeSequence::const_iterator ackWalker = m_Acks.begin();
	while( ackWalker != m_Acks.end() )
	{
		// start of sequence greater than item sequence - > not in ack range
		if ( ackWalker->getLower() > sequence )
			break;

		// lower sequence <= sequence and upper greater -> in ack range
		if ( ackWalker->getUpper() >= sequence )
			return true;

		ackWalker++;
	}
	return false;
}
bool SequenceAcknowledgement::IsAck( const string reference ) const
{
		map< string, Ack >::const_iterator finder = m_DistinctAcks.find( reference );
		return ( finder != m_DistinctAcks.end() );
}
bool SequenceAcknowledgement::HasAcks() const
{
	if( m_DistinctAcks.size() > 0 )
		return true;
	else
		return false;
}
}
