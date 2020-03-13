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

#include "SequenceResponse.h"

#include "SequenceAcknowledgement.h"
#include "SequenceFault.h"

namespace wsrm
{

SequenceResponse::SequenceResponse( SequenceType::Type type, const string& identifier, const string& elementName, const string& namespaceId, const string& namespaceUri ) : 
	SequenceType( type, identifier ), WSSerializable( elementName, namespaceId, namespaceUri )
{
}

SequenceResponse::~SequenceResponse()
{
}

SequenceResponse* SequenceResponse::Deserialize( DOMNode* root, const string& messageFilename )
{
	// if no fault is present, consider it a seqack
	DOMElement *rootElem = dynamic_cast< DOMElement* >( root );
	if ( rootElem == NULL )
		throw logic_error( "Root node is not an element." );

	SequenceResponse* returnedSequence = NULL;
	try
	{
		//try to deserialize a fault
		returnedSequence = SequenceFault::Deserialize( root, messageFilename );

		// if no fault, try to deserialize a ack seq
		if ( returnedSequence == NULL )
			returnedSequence = SequenceAcknowledgement::Deserialize( root, messageFilename );
	
		// if no fault, no ack, .. who cares ?
		if ( returnedSequence == NULL )
			returnedSequence = new wsrm::SequenceAcknowledgement( messageFilename );
	}
	catch( ... )
	{
		delete returnedSequence;
		returnedSequence = NULL;

		throw;
	}
	return returnedSequence;
}
}
