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

#ifndef COMMONEXCEPTIONS_H
#define COMMONEXCEPTIONS_H

#include "AppExceptions.h"

namespace FinTP
{
	class ExportedLogObject ArgumentNullException : public AppException
	{
		public:
			explicit ArgumentNullException( const string& argumentName, const EventType::EventTypeEnum eventType = EventType::Error, NameValueCollection* additionalInfo = NULL );		
			~ArgumentNullException() throw() {};
	};

	class ExportedLogObject MqException : public AppException
	{
		private :
			int m_ReasonCode;
			
		public:
			MqException( const string& message, int reasonCode, EventType::EventTypeEnum eventType = EventType::Error, NameValueCollection* additionalInfo = NULL );
			~MqException() throw() {};
			
			int getReasonCode() const { return m_ReasonCode; }
	};
}

#endif // COMMONEXCEPTIONS_H
