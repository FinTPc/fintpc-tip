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

#include "CommonExceptions.h"
#include "Trace.h"
#include <string>
#include <sstream>

using namespace std;
using namespace FinTP;

ArgumentNullException::ArgumentNullException( const string& argumentName, const EventType::EventTypeEnum eventType, NameValueCollection* additionalInfo ) :
	AppException( argumentName, eventType, additionalInfo )
{
	stringstream errorMessage;
	errorMessage << "Argument [" << argumentName << "] cannot be NULL.";
	m_Message = errorMessage.str();
}

MqException::MqException( const string& message, int reasonCode, EventType::EventTypeEnum eventType, NameValueCollection* additionalInfo ) :
	AppException( message, eventType, additionalInfo )
{
	m_ReasonCode = reasonCode;
	
	stringstream errorMessage;
	errorMessage << message << " - WMQ exception : Reason code : [" << reasonCode << "].";
	m_Message = errorMessage.str();
	
	TRACE( m_Message );
}
