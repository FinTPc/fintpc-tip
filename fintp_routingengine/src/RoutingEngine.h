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

#ifndef ROUTING_ENGINE_H
#define ROUTING_ENGINE_H

#include <omp.h> 

#include "WorkItemPool.h"
#include "AbstractWatcher.h"
#include "AppSettings.h"
#include "InstrumentedObject.h"
#include "Transactions/AbstractStatePersistence.h"
#include "Service/NTServiceBase.h"
#include "DB/DbWatcher.h"

#include "RoutingMessage.h"
#include "RoutingExceptions.h"
#include "RoutingSchema.h"
#include "RoutingDbOp.h"

#include "RoutingCOT.h"
#include "RoutingJobExecutor.h"
#include "RoutingKeyword.h"

#define DEFAULT_QUEUE "_INTERNAL_DUMMY_QUEUE"

#if defined( WIN32_SERVICE )
class ExportedTestObject RoutingEngine : public InstrumentedObject, public CNTService
#else
class ExportedTestObject RoutingEngine : public InstrumentedObject
#endif
{
#if defined( TESTDLL_EXPORT ) || defined ( TESTDLL_IMPORT )
	friend class RoutingStructuresTest;
	friend class RoutingActionsTest;
	friend class RoutingKeywordsTest;
#endif

	public:

#if defined( WIN32_SERVICE )
		virtual void Run( DWORD, LPTSTR * );
#endif
	
		static RoutingEngine* TheRoutingEngine;
		
		// .ctor, destructor
		RoutingEngine( const string& configEncriptionKey = "" );
		~RoutingEngine();
	
		// public methods
		void Start( bool startWatcher = true );
		void Stop();

		// profile options
		static void SetProfileMessageCount( const unsigned int value ) { m_ProfileMessageCount = value; }

		// callback from db watcher
		static void NotificationCallback( const AbstractWatcher::NotificationObject* notification );	
		static void OnNewJob( const AbstractWatcher::NotificationObject* notification );

		static void IdleProcess( void );
		static void PurgeHashes( void );
		static void ArchiveIdle( void );

		static WorkItemPool< RoutingJob >& getJobPool() { return TheRoutingEngine->m_JobPool; }
		static WorkItemPool< RoutingMessage >& getMessagePool() { return TheRoutingEngine->m_MessagePool; }
		static WorkItemPool< RoutingJobExecutor >& getThreadExecPool() { return TheRoutingEngine->m_ThreadExecPool; }

		AppSettings GlobalSettings;

		/*
		static string MapId( const string id, const string mappedId, const int autoUnmap );
		static string RemapId( const string id, const string mappedId );
		static string GetMappedId( const string id );
		static string GetReverseMapId( const string id );
		static bool UnmapId( const string id );*/

		static string getLiquiditiesSP() { return TheRoutingEngine->m_LiquiditiesSP; }
		static string getUpdateDateXSLT() { return TheRoutingEngine->m_UpdateDateXSLT; }
		static string getBatchXslt( const string& queue = DEFAULT_QUEUE );
		static string getBatchSchema() { return TheRoutingEngine->m_BatchSchema; } 
		static string getBatchSchemaNamespace() { return TheRoutingEngine->m_BatchSchemaNamespace; } 
		static string getBulkReactivateQueue() { return TheRoutingEngine->m_BulkReactivateQueue; }

		pthread_t getCotMonitorThreadId() { return m_CotThreadId; }
		pthread_t getRouterThreadId() { return m_RouterThreadId; }
		
		bool isRunning() const { return m_Running; }

		// read application settings at startup
		void ReadApplicationSettings();

		// performs some checks/corrective actions to get a consistent state on startup
		void RecoverLastSession() const;

		// backward compatibility - may have a negative impact on perfomance
		static bool hasIBANForLiquidities() { return TheRoutingEngine->m_LiquiditiesHasIBAN; }
		static bool hasIBANPLForLiquidities() { return TheRoutingEngine->m_LiquiditiesHasIBANPL; }
		static bool hasCorrForLiquidities() { return TheRoutingEngine->m_LiquiditiesHasCorresps; }

		// config helpers ( reads and caches config settings )
		static bool isDuplicateDetectionActive() { return TheRoutingEngine->m_DuplicateDetectionActive; }
		static bool isIdleArchivingActive() { return TheRoutingEngine->m_IdleArchivingActive; }


		static int getDuplicateDetectionTimeout() { return TheRoutingEngine->m_DuplicateDetectionTimeout; }
		static bool shouldCheckDuplicates( const string& service ) { return m_DuplicateChecks[ service ]; }

		static RoutingSchema* getRoutingSchema();

		//TODO Move directly to RoutingKeyword class
		static RoutingKeywordCollection getRoutingKeywords(){ return Keywords; }
		static RoutingKeywordMappings* getRoutingMappings(){ return &KeywordMappings; }
		static bool getRoutingIsoMessageType( const string& messageType )
		{ 
			RoutingIsoMessageTypes::const_iterator isoFinder = IsoMessageTypes.find( messageType );
			if ( isoFinder == IsoMessageTypes.end() )
				return false;
			
			return IsoMessageTypes[messageType];
		}

		/*
		static string EvaluateKeywordValue( const string& messageType, const string& value, const string& keyword, const string& field = "value" )
		{
			bool isIso = TheRoutingEngine->KeywordMappings->isIso( messageType );
			return TheRoutingEngine->Keywords->Evaluate( value, keyword, field, isIso ).first;
		}
		static RoutingKeywordCollection* getKeywords() { return TheRoutingEngine->Keywords; }

		static string GetKeywordXPath( const string& messageType, const string& keyword )
		{
			return TheRoutingEngine->KeywordMappings->getXPath( messageType, keyword );
		}

		static RoutingKeywordMappings* getKeywordMappings() { return TheRoutingEngine->KeywordMappings; }
		*/
		static bool shouldTrackMessages(){ return TheRoutingEngine->m_TrackMessages; }

	private :

		// static members
		static RoutingSchema DefaultSchema;

		static RoutingKeywordMappings KeywordMappings;
		static RoutingIsoMessageTypes IsoMessageTypes;
		static RoutingKeywordCollection Keywords;
	
		static void* ThreadPoolWatcher( void* data );
		static void* RouterRoutine( void* data );
		static void* COTMonitor( void* data );

		static unsigned int m_ProfileMessageCount;
		static unsigned int m_CotDelay;
		static int m_ParallelJobs;
		static bool m_ShouldStop;
		static RoutingCOTMarker m_ActiveCotMarker;
		static RoutingCOTMarker m_PreviousCotMarker;
		static pthread_mutex_t m_CotMarkersMutex;
		//static pthread_rwlock_t m_CotMarkersRWLock;
		
		static string m_OwnCorrelationId;	
		
		bool m_Running;
		
		static omp_lock_t parallelJobLock;

		string m_LiquiditiesSP;
		string m_UpdateDateXSLT;
		map< string, string > m_BatchXSLT;
		string m_BatchSchema;
		string m_BatchSchemaNamespace;
		string m_BulkReactivateQueue;

		pthread_t m_CotThreadId, m_RouterThreadId, m_BMThreadId;
		DbWatcher m_Watcher;

		WorkItemPool< RoutingMessage > m_MessagePool;
		WorkItemPool< RoutingJob > m_JobPool;
		WorkItemPool< RoutingJobExecutor > m_ThreadExecPool;
		
		static AbstractStatePersistence* m_IdFactory;

#if defined( WIN32 ) && defined ( _DEBUG )
		_CrtMemState m_NextMemoryState, m_OldMemoryState, m_FinishMemoryState;
		bool m_OldStateAvailable;
		static unsigned int m_Notifications;
#endif

		// backward compatibility
		// database version prior to 4.1(db2) or 2.1(oracle) doesn't have IBAN in BusinessMessages
		bool m_LiquiditiesHasIBAN; 
		bool m_LiquiditiesHasIBANPL; 
		bool m_LiquiditiesHasCorresps;
		bool m_DuplicateDetectionActive;
		bool m_IdleArchivingActive;

		int m_DuplicateDetectionTimeout;

		static map< string, bool > m_DuplicateChecks;

		static pthread_once_t SchemaKeysCreate;
		static pthread_key_t SchemaKey;

		static void CreateKeys();
		static void DeleteSchema( void* data );

		void LoadKeywords();
		void LoadMappings();
		
		bool m_TrackMessages;
};
#endif
