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

#ifndef SEQUENCERESPONSE_H
#define SEQUENCERESPONSE_H

#include <map>

#include "SequenceType.h"
#include "AcknowledgementRange.h"
#include "Nack.h"
#include "Ack.h"

namespace wsrm
{
	//EXPIMP_TEMPLATE template class ExportedWsObject std::vector< wsrm::AcknowledgementRange >;
	//EXPIMP_TEMPLATE template class ExportedWsObject std::map< string, wsrm::Nack >;

	class ExportedWsObject SequenceResponse : public SequenceType, public WSSerializable
	{
		protected :
		
			SequenceResponse( SequenceType::Type type = SequenceType::Sequence, const string& identifier = "", const string& elementName = "name", const string& namespaceId = "ns", const string& namespaceUri = "uri" );
		
		public:
				
			virtual ~SequenceResponse();
			
			void AddChild( const wsrm::AcknowledgementRange& ackRange ){ m_Acks.push_back( ackRange ); }
			void AddChild( const string& reference, const wsrm::Nack& nack ){ (void)m_Nacks.insert( pair< string, wsrm::Nack >( reference, nack ) ); }
			void AddChild( const string& reference, const wsrm::Ack& ack ){ (void)m_DistinctAcks.insert( pair< string, wsrm::Ack >( reference, ack ) ); }
			const AcknowledgementRangeSequence& getAcknowledgementRanges() const { return m_Acks; }
			const map< string, wsrm::Ack >& getAcks() const { return m_DistinctAcks; }
			const map< string, wsrm::Nack >& getNacks() const { return m_Nacks; }
			const wsrm::Nack& getNack( const string& reference ) { return m_Nacks[ reference ]; }
			const wsrm::Ack& getAck( const string& reference ) { return m_DistinctAcks[ reference ]; }
			
			virtual bool IsAck( const unsigned long sequence ) const = 0;
			virtual bool IsAck( const string reference ) const { return false; };
			virtual bool IsNack( const string reference ) const = 0;
			unsigned int seqSize() const
			{
				return m_Nacks.size() + m_DistinctAcks.size() ;
			};

			static SequenceResponse* Deserialize( DOMNode* root, const string& messageFilename );
			
		protected :
		
			virtual string internalSerialize() const = 0;
			virtual DOMNode* internalSerialize( DOMNode* root ) const = 0;

			virtual WSSerializable* internalDeserialize( const DOMNode* root ) = 0;

		protected :
		
			wsrm::AcknowledgementRangeSequence m_Acks;
			map< string, wsrm::Nack > m_Nacks;
			map< string, wsrm::Ack > m_DistinctAcks;
	};
}


#endif // SEQUENCERESPONSE_H
