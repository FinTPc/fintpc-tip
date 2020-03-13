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

#ifndef SEQUENCEFAULT_H
#define SEQUENCEFAULT_H

#include "../WSSerializable.h"
#include "SequenceResponse.h"

namespace wsrm
{
	class ExportedWsObject SequenceFault : public SequenceResponse
	{
		public :

			// failed batches with all messages rejected will have seqfault != NOSEQFAULT
			// failed batches with distinct messages rejected will have seqfault == NOSEQFAULT
			static const string NOSEQFAULT;

			SequenceFault( const string& identifier = "", const string& errorCode = SequenceFault::NOSEQFAULT );
			~SequenceFault();
						
			string getErrorCode() const { return m_ErrorCode; }
			void setErrorCode( const string& errorCode ) { m_ErrorCode = errorCode; }
			string getErrorReason() const { return m_ErrorReason; }
			void setErrorReason( const string& errorReason ) { m_ErrorReason = errorReason; }
			
			bool IsNack( const string reference ) const;
			bool IsAck( const unsigned long sequence ) const;

			static SequenceFault* Deserialize( DOMNode* root, const string& messageFilename );
			
		protected : 
		
			string internalSerialize() const;
			DOMNode* internalSerialize( DOMNode* root ) const;

			WSSerializable* internalDeserialize( const DOMNode* data );
			
		private : 
			
			string m_ErrorCode, m_ErrorReason;
	};
}

#endif // SEQUENCEFAULT_H
