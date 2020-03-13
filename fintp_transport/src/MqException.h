#ifndef MQEXCEPTION_H
#define MQEXCEPTION_H

#include "DllMainTransport.h"
#include "AppExceptions.h"

namespace FinTP
{
	class ExportedTransportObject MqException : public AppException
	{
		private :
			int m_ReasonCode;
			
		public:
			MqException( const string& message, int reasonCode, EventType::EventTypeEnum eventType = EventType::Error, NameValueCollection* additionalInfo = NULL );
			~MqException() throw() {};
			
			int getReasonCode() const { return m_ReasonCode; }
	};		
}

#endif