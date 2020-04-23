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

#ifndef ROUTINGDBOP_H
#define ROUTINGDBOP_H

#include "RoutingEngineMain.h"
#include "DatabaseProvider.h"
#include "Database.h"
#include "RoutingStructures.h"
#include "RoutingAggregationManager.h"


#include "ODBC\Postgres\PostgresDatabase.h"
#include "ODBC\Postgres\PostgresDatabaseProvider.h"


class ExportedTestObject RoutingDbOp
{
#if defined( TESTDLL_EXPORT ) || defined ( TESTDLL_IMPORT )
	friend class DbTestsUtil;
#endif

	friend class RoutingEngine;
	friend class RoutingSchema;

	private :
	
		// default .ctor is private ( no instance required )
		RoutingDbOp();

		static void Initialize();

		static pthread_once_t DatabaseKeysCreate;
		static pthread_key_t DataKey;
		static pthread_key_t ConfigKey;

		static void CreateKeys();
		static void DeleteData( void* data );
		static void DeleteConfig( void* data );

		static DatabaseProviderFactory *m_DatabaseProvider;
		
		static Database* getData();
		static Database* getConfig();
		static DatabaseProviderFactory* getProvider() { return m_DatabaseProvider; }

		static void createProvider( const NameValueCollection& configSection );
		
		static map< long, RoutingQueue >* m_QueueIdCache;
		static map< string, string >* m_ReplyQueueCache;
		static map< string, string >* m_DuplicateQueueCache;
		static map< string, string >* m_DuplicateReplyQueueCache;
		static map< string, string >* m_DelayedReplyQueueCache;
		
		static pthread_mutex_t m_SyncRoot;

		static ConnectionString m_ConfigConnectionString;
		static ConnectionString m_DataConnectionString;

		// fill queues cache	
		static void GetQueues();
		
	public:
		enum BatchItemsType
		{
			Batch_Items,
			DD_Ref_Reply,
			Sepa_Sts_Reply
		};
		// destructor
		~RoutingDbOp();
		static void Terminate();
		static void TerminateSelf();
						
		// static methods
		static void SetConfigDataSection( const NameValueCollection& dataSection );
		static void SetConfigCfgSection( const NameValueCollection& cfgSection );

		static bool isConnected();

		// configs
		static string GetActiveRoutingSchemaName();	
		static DataSet* GetActiveRoutingSchemas();	
		static DataSet* ReadRoutingRules( const string& schemaName );
		static DataSet* ReadRoutingRule( const long ruleId );
		
		// cot
		static DataSet* GetCOTMarkers();

		// archiving
		static unsigned long Archive();
						
		// routing jobs
		static DataSet* GetRoutingJob( const string& jobId );
		static DataSet* ReadJobParams( const string& jobId );
		static void CommitJob( const string& jobId, const bool isolate = true );
		static void AbortJob( const string& jobId );
		static void RollbackJob( const string& jobId );
		static void DeferJob( const string& jobId, const long deferedQueue, const string& routingPoint, const string& function, const int userId );
		static void ResumeJobs( const long deferedQueue );
		static void InsertJob( const string& jobId, const string& routingPoint, const string& function, const int userId );
		//static void AddRoutingJob( const string routingPoint, const string routingKey, const string routingFunc );
		
		// batching
		static BatchManagerBase::BATCH_STATUS BatchJob( const string& jobId, const string& sequence,
			const string& batchId, const string& correlId, const string& feedback, const string& xformItem, const string& amountBDP, const string& amountADP );
		static BatchManagerBase::BATCH_STATUS GetBatchStatus( const string& batchId, const string& batchUID, string& comBatchId, const int userId, 
			const unsigned long batchCount, const string& batchAmount, const long serviceId, const string& routingPoint );
		static void UpdateBatchCode( const string& batchId, const string& code );

		static void TerminateBatch( const string& batchId, const string& batchType, const BatchManagerBase::BATCH_STATUS status, const string& reason );
		static void TerminateRapidBatch( const string& batchId, const int userId, const string& tableName, const string& responder );
		//static DataSet* GetBatchJobs( const string batchId );
		static unsigned long GetBatchCount( const string& batchId ){ return 0; }
		static DataSet* GetBatchMessages( const string& batchId, const bool isReply, const string& issuer, int ddbtSettledReply = 0 );
		static DataSet* GetBatchMessages( const string& batchId, const string& tableName, const string& spName = "GetMessagesInAssembly" );
		static DataSet* GetBatchPart( const int userId, const string& receiver, const int serviceId, const string& queue );
		static void RemoveBatchMessages( const string& batchId, const string& tableName, const string& spName = "DeleteMessagesInAssembly" );
		static void UpdateOriginalBatchMessages( const string& batchid, const string& code = RoutingMessageEvaluator::FEEDBACKFTP_REFUSE );
		static string GetBatchType( const string& batchId, const string& tableName = "BATCHJOBS", const string& sender = "" );
		static void InsertIncomingBatch( const string& batchId, const string& messageId, const string& messageNamespace );
		static string GetOriginalRef( const string& reference, const string& batchId );
		
		// message functions
		static DataSet* GetRoutingMessage( const string& tableName, const string& messageId );
		/*static void MoveRoutingMessage( string sourceTable, string destTable, string messageId );*/
		static void MoveRoutingMessage( const string& sourceTable, const string& destTable, const string& messageId,
			const string& payload, const string& batchId, const string& correlationId, const string& sessionId,
			const string& requestorService, const string& responderService, const string& requestType, 
			const unsigned long priority, const short holdstatus, const long sequence, const string& feedback );
		/*Only replies are truly deleted, other messages are only marked as deleted*/
		static void DeleteRoutingMessage( const string& tableName, const string& messageId, bool isReply = false );
		static void InsertRoutingMessage( const string& tableName, const string& messageId, const string& payload, const string& batchId, 
			const string& correlationId, const string& sessionId, const string& requestorService, const string& responderService,
			const string& requestType,	const unsigned long priority, const short holdstatus, const long sequence, const string& feedback );
		static void UpdateRoutingMessage( const string& tableName, const string& messageId, const string& payload, const string& batchId, 
			const string& correlationId, const string& sessionId, const string& requestorService, const string& responderService,
			const string& requestType,	unsigned long priority, short holdstatus, long sequence, const string& feedback );
		static string GetOriginalPayload( const string& correlationId );
		static string GetOriginalMessageId( const string& correlationId );
				
		//reports support
		static void InsertBusinessMessage( const string& messageType, const string& entity, const string& senderApp, const string& receiverApp, const string& messageId, const string& correlationId, const vector<string>& keywords, const vector<string>& keywordValues );
		/*
		static void InsertBusinessMessage( const string& messageId, const string& correlationId, int crtQueue, const string& messageType, 
			const string& senderBIC, const string& receiverBIC, const string& currDate, const string& currType, const string& currAmmount,
			const string& senderApp, const string& receiverApp, const string& trn, const string& relref, const string& mur, 
			const string& iban, const string& ibanpl, const string& senderCorr, const string& receiverCorr, const int userid,
			const string& edToEdId, const string& orgInstrId, const string& orgTxId, const string& addMsgInf );
		*/
		static void EnrichMessageData( const vector<string>& tableNames, const vector<string>& tableValues, const vector<string>& enrichNames, const vector<string>& enrichValues );
		static void UpdateBusinessMessageResponder( const string& messageId, const string& receiverApp );
		static void UpdateBusinessMessageUserId( const string& correlationId, const int userId );
		static void UpdateBusinessMessageValueDate( const string& correlationId, const string& newDate );
		static void UpdateBusinessMessageAck( const string& id, bool batch );
		static void UpdateLiquidities( const string& correlationId );
		static void UpdateBMAssembleResponder( const string& batchId, const string& receiverApp );

		//aggregation
		static bool AggregationRequestOp( const string& aggregationTable, const RoutingAggregationCode& request, bool& trim );
		static void InsertAggregationRequest( const string& aggregationTable, const RoutingAggregationCode& request );
		static bool UpdateAggregationRequest( const string& aggregationTable, const RoutingAggregationCode& request, bool trim = false );		
		static bool GetAggregationFields( const string& aggregationTable, RoutingAggregationCode& request, bool trim = false );
				
		static string GetReplyQueue( const string& senderApp );
		static string GetDuplicateQueue( const string& senderApp );
		static string GetDuplicateReplyQueue( const string& senderApp );
		static string GetDelayedReplyQueue( const string& senderApp );
		
		//keywords
		static DataSet* GetKeywordMappings();
		static DataSet* GetKeywords();

		// services - duplicate detection
		static map< string, bool > GetDuplicateServices();
		static int GetDuplicates( const string& service, const string& messageId );
		static void PurgeHashes( const int hours );
		static unsigned int GetNextSequence( const long serviceId );

		//queue functions
		static string GetQueueName( const long queueId );
		static long GetQueueId( const string& queueName );
		static RoutingQueue& GetQueue( const long queueId );
		static RoutingQueue& GetQueue( const string& queueName );
		static void ChangeQueueHoldStatus( const string& queueName, const bool holdStatus );
		
		static map< long, RoutingQueue >* GetQueueCache()
		{
			if ( m_QueueIdCache == NULL )
				throw runtime_error( "Unable to obtain queue definitions from the database [queue cache empty]" );
			return m_QueueIdCache;
		}

		static map< string, string >* GetReplyQueueCache()
		{
			if ( m_ReplyQueueCache == NULL )
				throw runtime_error( "Unable to obtain reply queue definitions from the database [reply queue cache empty]" );
			return m_ReplyQueueCache;
		}

		static map< string, string >* GetDuplicateQueueCache()
		{
			if ( m_DuplicateQueueCache == NULL )
				throw runtime_error( "Unable to obtain duplicate queue definitions from the database [duplicate queue cache empty]" );
			return m_DuplicateQueueCache;
		}

		static map< string, string >* GetDuplicateReplyQueueCache()
		{
			if ( m_DuplicateReplyQueueCache == NULL )
				throw runtime_error( "Unable to obtain duplicate replies queue definitions, from the database [duplicate reply queue cache empty]" );
			return m_DuplicateQueueCache;
		}
		
		static map< string, string >* GetDelayedReplyQueueCache()
		{
			if ( m_DelayedReplyQueueCache == NULL )
				throw runtime_error( "Unable to obtain delayed replies queue definitions, from the database [delayed reply queue cache empty]" );
			return m_DelayedReplyQueueCache;
		}

		// call runtime procedures
		// SP signature : procName( messageId in varchar2, result out number )
		// returns true if result>0, false otherwise
		static bool CallBoolProcedure( const string& procName, const string& messageId, const string& messageTable );
		static DataSet* GetDelayedMessages( const string& tableName, const string& tokenId );
		static void UpdateDelayedId( const string& tableName, const string& messageId, const string& delayedId );
		static DataSet* GetEnrichData( const string& tableName, const string& filterId );
		
		static void insertMatchMessage( const string& spName, const string& location, const string& messageId, const string& correlId, const string& reference, 
			const string& hash, const string& messageType, const string& hashId, vector<string>& internalFields );
		static const string CheckMatchHash( const string hash, const string location );
};
#endif
