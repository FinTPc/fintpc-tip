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

#ifndef EVENTSWATCHER_H
#define EVENTSWATCHER_H

#include <string>
#include <time.h>

#include "EventsWatcherMain.h"

#include "AbstractWatcher.h"
#include "MQ/MqWatcher.h"
#include "TransportHelper.h"
#include "AppExceptions.h"
#include "InstrumentedObject.h"

using namespace std;

class Service
{
	public :
	
		typedef enum
		{
			RUNNING = 3,
			STOPPED = 1
		} SERVICE_STATE;
		
		static string ToString( const SERVICE_STATE value );

		string Report() const;
		
		// ctor
		Service() : m_State( Service::STOPPED ), m_LastHeartbeat( 0 ), m_ServiceName( "FinTPUnk00" ), m_SessionId( "" ),
			m_ServiceId( -1 ), m_HeartbeatInterval( 30 ) {};

		Service( const long serviceId, const long state, const time_t lastHeartbeat, 
			const string& serviceName="FinTPUnk00", const string& sessionId="", const long heartbeatInterval = 30 ) :
			m_State( ( SERVICE_STATE )state ), m_LastHeartbeat( lastHeartbeat ), m_ServiceName( serviceName ), 
			m_SessionId( sessionId ), m_ServiceId( serviceId ), m_HeartbeatInterval( heartbeatInterval ) {}
	
		// session id
		string getSession() const { return m_SessionId; }
		void setSessionId( const string& sessionId ) { m_SessionId = sessionId; }

		// service id
		long getServiceId() const { return m_ServiceId; }
		
		// name
		string getName() const { return m_ServiceName; }
		
		// state
		Service::SERVICE_STATE getState() const { return m_State; }
		void setState( const Service::SERVICE_STATE newState );
		
		// heartbeat
		void setLastHeartbeat( const time_t newtime ){ m_LastHeartbeat = newtime; }
		double getTimeSinceLastHeartbeat() const;
		long getHeartbeatInterval() const { return m_HeartbeatInterval; }
		
	private :
	
		SERVICE_STATE m_State;
		time_t m_LastHeartbeat;
		string m_ServiceName;
		string m_SessionId;
		long m_ServiceId;
		long m_HeartbeatInterval;
};

class ExportedTestObject EventsWatcher : public InstrumentedObject
{
#if defined( TESTDLL_EXPORT ) || defined ( TESTDLL_IMPORT )
	friend class EventsTest;
#endif

	public:

		typedef map< string, Service > ServiceMap;
		
		~EventsWatcher();

		//public methods
		void Start();
		void Stop();

		static EventsWatcher* getInstance( const string& configFile = "" );
				
		void Process( const string& messageId, const unsigned long messageLength );
		
		static void UpdateServiceState( const string& sessionId, const Service::SERVICE_STATE newState, const string& serviceName = "" );
		static void* HeartbeatMonitor( void* data );
		
		static ServiceMap* getConnectorStateMap() { return &( getInstance()->m_ConnectorState );  }
		
		static pthread_mutex_t StatusSyncMutex;

		pthread_t getPublisherThreadId() const { return m_SelfThreadId; }
		pthread_t getHBThreadId() const{ return m_MonitorThreadId; }
		
		bool isRunning() const { return true; }

	private:
		
		EventsWatcher( const string& configFile );

		static void* StartInNewThread( void* pThis );
		void internalStart( void );

		void UploadMessage( AppException &ex );

		long getServiceId( const string& sessionId ) const;

		static EventsWatcher* m_Instance;
		static bool ShouldStop;
		pthread_t m_SelfThreadId;
		
		//int m_BackoutCount;
		//int m_CurrentStage;
		
		MqWatcher m_Watcher;
		AbstractWatcher::NotificationPool m_NotificationPool;

		string m_SessionId;
		TransportHelper* m_CurrentHelper;
		TransportHelper::TRANSPORT_HELPER_TYPE m_HelperType;
		pthread_t m_MonitorThreadId;
		
		string m_WatchQueue;
		string m_WatchQueueManager;
		string m_TransportURI;

		
		// collection of connector-state pairs
		ServiceMap m_ConnectorState;
		
#if defined( WIN32 ) && defined( _DEBUG )
		_CrtMemState m_NextMemoryState, m_OldMemoryState;
		bool m_OldStateAvailable;
#endif
};

#endif //EVENTSWATCHER_H
