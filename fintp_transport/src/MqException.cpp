#include "MqException.h"
#include "Trace.h"
#include <string>
#include <sstream>

using namespace std;
namespace  FinTP {

MqException::MqException( const string& message, int reasonCode, EventType::EventTypeEnum eventType, NameValueCollection* additionalInfo ) :
	AppException( message, eventType, additionalInfo )
{
	m_ReasonCode = reasonCode;
	
	stringstream errorMessage;
	errorMessage << message << " - WMQ exception : Reason code : [" << reasonCode << "].";
	m_Message = errorMessage.str();
	
	TRACE( m_Message );
}

}