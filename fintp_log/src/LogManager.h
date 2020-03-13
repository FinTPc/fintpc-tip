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

#ifndef LOGMANAGER_H
#define LOGMANAGER_H

#include <exception>
#include <string>
#include <iostream>
#include <vector>

#include "pthread.h"

#include <xercesc/dom/DOM.hpp>

//#include "AppSettings.h"
#include "Collections.h"
#include "WorkItemPool.h"

#include "AbstractLogPublisher.h"
#include "AppExceptions.h"

using namespace std;
XERCES_CPP_NAMESPACE_USE

namespace FinTP
{
	//EXPIMP_TEMPLATE template class ExportedLogObject std::vector< AbstractLogPublisher* >;

	class ExportedLogObject LogManager 
	{
		public :
			typedef WorkItemPool< AppException > EventsPool;

			~LogManager();// Terminate XML4C2 and delete m_AppException

			// methods
			static void Publish( const AppException& except );
			static void Publish( const string& message, const EventType::EventTypeEnum eventType = EventType::Error );
			static void Publish( const char* message, const EventType::EventTypeEnum eventType = EventType::Error );
			static void Publish( const std::exception& innerException, const EventType::EventTypeEnum eventType = EventType::Error, const string& message="" );

			// initialize m_PropSettings
			static void Initialize( const NameValueCollection& applicationSettings, bool threaded = false );		
		
			static bool isInitialized();
			static void setCorrelationId( const string& value );

			static void setSessionId( const string& value );

			static void Terminate();

			static pthread_t getPublisherThreadId();
			static void Register( AbstractLogPublisher* logPublisher, bool isDefault );
			
		private	:	

			static void CreateKeys();
			static void DeleteCorrelIds( void* data );

			LogManager();// Initialize the XML4C2 system
			bool m_Initialized;
			bool m_Threaded;

			// the one and only instance
			static LogManager Instance;

			static pthread_once_t KeysCreate;
			static pthread_key_t CorrelationKey;
			static pthread_mutex_t InstanceMutex;
			pthread_t m_PublishThreadId;

			void setThreaded( const bool threaded );
			static void* PublisherThread( void *param );
			
			string m_SessionId;
			
			// default publisher and all other publishers
			AbstractLogPublisher* m_DefaultPublisher;
			vector< AbstractLogPublisher* > m_Publishers;

			void setDefaultPublisher( AbstractLogPublisher* defaultPublisher );

			void ClearPublishers();
			
			// either inserts the event to the pool or publishers the event ( regarding m_Threaded attribute )
			void InternalPublish( const AppException& except ) throw();
			void InternalPublishProc( const AppException& except );

			static EventsPool m_EventsPool;
	};
}

#endif // LOGMANAGER_H
