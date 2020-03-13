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

#include "SequenceDistinctAck.h"

#include <sstream>

namespace wsrm
{

SequenceDistinctAck::SequenceDistinctAck( const string& identifier, const string& amount ) : 
	SequenceResponse( SequenceType::SequenceDistinctAck, identifier, "SequenceDistinctAck", "wsrm", "http://wwww.bisnet.ro/ws/2005/02/rm" ),
	m_Amount( amount )
{
}

SequenceDistinctAck::~SequenceDistinctAck()
{
}

string SequenceDistinctAck::internalSerialize() const
{
	return "";
}

SequenceDistinctAck* SequenceDistinctAck::Deserialize( DOMNode* root, const string& messageFilename )
{
	return NULL;
}

DOMNode* SequenceDistinctAck::internalSerialize( DOMNode* root ) const
{
	return NULL;
}

WSSerializable* SequenceDistinctAck::internalDeserialize( const DOMNode* data )
{
	return NULL;
}

bool SequenceDistinctAck::IsNack( const string reference ) const
{
	return false;
}

bool SequenceDistinctAck::IsAck( const string reference ) const
{
		map< string, Ack >::const_iterator finder = m_DistinctAcks.find( reference );
		return ( finder != m_DistinctAcks.end() );
}
}
