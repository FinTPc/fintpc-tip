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

#ifndef FTPMESSAGE_H
#define FTPMESSAGE_H

#include "ConnectorMain.h"

#include "XmlUtil.h"
#include "Collections.h"

namespace FinTPMessage
{
	class ExportedTestObject Metadata
	{
		private : 
			
			string m_Id;
			string m_GroupId;
			string m_Format;
			unsigned long m_Length;

		public :

			string groupId() const { return m_GroupId; }
			void setGroupId( const string& messageGroupId ) { m_GroupId = messageGroupId; }

			string id() const { return m_Id; }
			void setId( const string& messageId ) { m_Id = messageId; }

			string format() const { return m_Format; }
			void setFormat( const string& messageFormat ) { m_Format = messageFormat; }

			unsigned long length() const { return m_Length; }
			void setLength( const unsigned long messageLength ) { m_Length = messageLength; }

			Metadata();
			Metadata( const string& messageId, const string& messageGroupId = "", const unsigned long messageLength = 0 );
			Metadata( const FinTPMessage::Metadata& metadata );
	};

	class ExportedTestObject Message 
	{
		private :
			FinTPMessage::Metadata m_Metadata;
			XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* m_Dom;

			string m_Payload;
			string m_CorrelationId;
			string m_Requestor;
			string m_Responder;
			string m_Feedback;			

		public :
		
			enum ConnectorType
			{
				ACHCONNECTOR,
				DBCONNECTOR,
				FILECONNECTOR,
				MQCONNECTOR,
				AQCONNECTOR
			};		
		
			Message();
			Message( const FinTPMessage::Metadata& metadata, const string& payload = "", const string& correlationId = "", 
				const string& requestor = "", const string& responder = "", const string& feedback = "" );
			
			~Message();
			
			void releaseDom();
			
			//getting payload, requestor, ....
			void getInformation( const FinTPMessage::Message::ConnectorType connType ); 
			
			const FinTPMessage::Metadata & metadata() const { return m_Metadata; }
			XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* dom() { return m_Dom; }
			
			// needed by : all publishers
			string payload() const { return m_Payload; }

			// needed by : all publishers
			string correlationId() const { return m_CorrelationId; }

			// needed by : db publisher
			string requestor() const { return m_Requestor; }

			// needed by : db publisher
			string responder() const { return m_Responder; }
			
			// needed by : file publisher
			string feedback() const { return m_Feedback; }
			
	};

	class ExportedTestObject MessageFactory
	{
		private :

			FinTPMessage::Metadata m_Metadata;
	};
}

#endif //FTPMESSAGE_H
