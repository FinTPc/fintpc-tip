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

#ifdef WIN32
	/*#ifdef _DEBUG
		//#define _CRTDBG_MAP_ALLOC
		#include <stdlib.h>
		#include <crtdbg.h>
	#endif*/
#endif

#include "Trace.h"
#include "AppSettings.h"
#include "StringUtil.h"
#include "TimeUtil.h"

#include "RoutingDbOp.h"
#include "RoutingExceptions.h"
#include "RoutingEngine.h"

pthread_once_t RoutingDbOp::DatabaseKeysCreate = PTHREAD_ONCE_INIT;
pthread_key_t RoutingDbOp::DataKey;
pthread_key_t RoutingDbOp::ConfigKey;

DatabaseProviderFactory* RoutingDbOp::m_DatabaseProvider = NULL;

map< long, RoutingQueue >* RoutingDbOp::m_QueueIdCache = NULL;
map< string, string >* RoutingDbOp::m_ReplyQueueCache = NULL;
map< string, string >* RoutingDbOp::m_DuplicateQueueCache = NULL;
map< string, string >* RoutingDbOp::m_DuplicateReplyQueueCache = NULL;
map< string, string >* RoutingDbOp::m_DelayedReplyQueueCache = NULL;

ConnectionString RoutingDbOp::m_ConfigConnectionString;
ConnectionString RoutingDbOp::m_DataConnectionString;

pthread_mutex_t RoutingDbOp::m_SyncRoot = PTHREAD_MUTEX_INITIALIZER;

RoutingDbOp::RoutingDbOp()
{
}

RoutingDbOp::~RoutingDbOp()
{
}

void RoutingDbOp::Initialize()
{
	int onceResult = pthread_once( &RoutingDbOp::DatabaseKeysCreate, &RoutingDbOp::CreateKeys );
	if ( 0 != onceResult )
	{
		TRACE( "One time key creation for DbOps failed [" << onceResult << "]" );
	}

	GetQueues();
}

void RoutingDbOp::CreateKeys()
{
	cout << "Thread [" << pthread_self() << "] creating DBOps data/config keys..." << endl;

	int keyCreateResult = pthread_key_create( &RoutingDbOp::DataKey, &RoutingDbOp::DeleteData );
	if ( 0 != keyCreateResult )
	{
		TRACE( "An error occured while creating DbOps data key [" << keyCreateResult << "]" );
	}
	keyCreateResult = pthread_key_create( &RoutingDbOp::ConfigKey, &RoutingDbOp::DeleteConfig );
	if ( 0 != keyCreateResult )
	{
		TRACE( "An error occured while creating DbOps config key [" << keyCreateResult << "]" );
	}
}

void RoutingDbOp::DeleteData( void* data )
{
	pthread_t selfId = pthread_self();	
	//TRACE_NOLOG( "Deleting data connection for thread [" << selfId << "]" );
	Database* database = ( Database* )data;
	if ( database != NULL )
	{
		try
		{
			database->Disconnect();
		}
		catch( ... )
		{
			TRACE_NOLOG( "An error occured while disconnecting from the data database" );
		}
		delete database;
		database = NULL;
	}
	int setSpecificResult = pthread_setspecific( RoutingDbOp::DataKey, NULL );
	if ( 0 != setSpecificResult )
	{
		TRACE_NOLOG( "Set thread specific DataKey failed [" << setSpecificResult << "]" );
	}
}

void RoutingDbOp::DeleteConfig( void* data )
{
	//TRACE_NOLOG( "Deleting config connection for thread [" << pthread_self() << "]" );
	Database* database = ( Database* )data;
	if ( database != NULL )
	{
		try
		{
			database->Disconnect();
		}
		catch( ... )
		{
			TRACE_NOLOG( "An error occured while disconnecting from the config database" );
		}
		delete database;
		database = NULL;
	}
	int setSpecificResult = pthread_setspecific( RoutingDbOp::ConfigKey, NULL );
	if ( 0 != setSpecificResult )
	{
		TRACE_NOLOG( "Set thread specific ConfigKey failed [" << setSpecificResult << "]" );
	}
}

void RoutingDbOp::createProvider( const NameValueCollection& configSection )
{
	if ( configSection.ContainsKey( "provider" ) )
	{
		string provider = configSection[ "provider" ];

		// create a provider if necessary
		if ( m_DatabaseProvider == NULL )
		{
			// lock and attempt to create the provider
			int mutexLockResult = pthread_mutex_lock( &m_SyncRoot );
			if ( 0 != mutexLockResult )
			{
				stringstream errorMessage;
				errorMessage << "Unable to lock Sync mutex [" << mutexLockResult << "]";
				TRACE( errorMessage.str() );

				throw runtime_error( errorMessage.str() );
			}

			try
			{			
				// other thread may have already created the provider
				if ( m_DatabaseProvider == NULL )
					m_DatabaseProvider = DatabaseProvider::GetFactory( provider );
			}
			catch( ... )
			{
				TRACE( "Unable to create database provider [" << provider << "]" );

				int mutexUnlockResult = pthread_mutex_unlock( &m_SyncRoot );
				if ( 0 != mutexUnlockResult )
				{
					TRACE( "Unable to unlock Sync mutex [" << mutexUnlockResult << "]" );
				}
				throw;
			}

			int mutexUnlockResult = pthread_mutex_unlock( &m_SyncRoot );
			if ( 0 != mutexUnlockResult )
			{
				TRACE( "Unable to unlock Sync mutex [" << mutexUnlockResult << "]" );
			}
		}
	}
}

void RoutingDbOp::SetConfigDataSection( const NameValueCollection& dataSection )
{
	createProvider( dataSection );

	m_DataConnectionString.setDatabaseName( dataSection[ "database" ] );
	m_DataConnectionString.setUserName( dataSection[ "user" ] );
	m_DataConnectionString.setUserPassword( dataSection[ "password" ] );
}				

void RoutingDbOp::SetConfigCfgSection( const NameValueCollection& cfgSection )
{
	createProvider( cfgSection );

	m_ConfigConnectionString.setDatabaseName( cfgSection[ "database" ] );
	m_ConfigConnectionString.setUserName( cfgSection[ "user" ] );
	m_ConfigConnectionString.setUserPassword( cfgSection[ "password" ] );
}

void RoutingDbOp::TerminateSelf()
{
}

void RoutingDbOp::Terminate()
{
	if ( RoutingDbOp::m_DatabaseProvider != NULL )
	{
		delete RoutingDbOp::m_DatabaseProvider;
		RoutingDbOp::m_DatabaseProvider = NULL;
	}
		
	if ( m_QueueIdCache != NULL )
	{
		delete m_QueueIdCache;
		m_QueueIdCache = NULL;
	}
		
	if ( m_ReplyQueueCache != NULL )
	{
		delete m_ReplyQueueCache;
		m_ReplyQueueCache = NULL;
	}

	if ( m_DuplicateQueueCache != NULL )
	{
		delete m_DuplicateQueueCache;
		m_DuplicateQueueCache = NULL;
	}
}

bool RoutingDbOp::isConnected()
{
	Database* database = ( Database * )pthread_getspecific( RoutingDbOp::DataKey );
	if ( database == NULL )
		return false;

	return database->IsConnected();
}

Database* RoutingDbOp::getData()
{
	Database* crtDatabase = ( Database* )pthread_getspecific( RoutingDbOp::DataKey );
	if ( crtDatabase == NULL )
	{
		// create a provider if necessary
		if ( m_DatabaseProvider == NULL ) 
		{
			if ( RoutingEngine::TheRoutingEngine != NULL )
				createProvider( RoutingEngine::TheRoutingEngine->GlobalSettings.getSection( "DataConnectionString" ) );
			else	
				throw logic_error( "Unable to find data provider definition. You should setup the config for db." );
		}

		if ( m_DatabaseProvider == NULL ) 
			throw runtime_error( "Unable to create database provider" );

		crtDatabase = m_DatabaseProvider->createDatabase();
		if ( crtDatabase == NULL )
			throw runtime_error( "Unable to create database definition" );

		int setSpecificResult = pthread_setspecific( RoutingDbOp::DataKey, crtDatabase );
		if ( 0 != setSpecificResult )
		{
			TRACE( "Set thread specific DataKey failed [" << setSpecificResult << "]" );
		}
	}

	// don't reconnect if already connected
	pthread_t selfId = pthread_self();	
	if ( crtDatabase->IsConnected() )
	{
		DEBUG_GLOBAL( "[" << selfId << "] already connected to Data database" );
		return crtDatabase;
	}
	
	// lock and attempt to create the provider
	int mutexLockResult = pthread_mutex_lock( &m_SyncRoot );
	if ( 0 != mutexLockResult )
	{
		stringstream errorMessage;
		errorMessage << "Unable to lock Sync mutex [" << mutexLockResult << "]";
		TRACE( errorMessage.str() );

		throw runtime_error( errorMessage.str() );
	}

	try
	{			
		if ( !m_DataConnectionString.isValid() )
		{
			if ( RoutingEngine::TheRoutingEngine == NULL )
				throw logic_error( "Unable to find connection string definition for data. You should setup the config for db." );

			m_DataConnectionString.setDatabaseName( RoutingEngine::TheRoutingEngine->GlobalSettings.getSectionAttribute( "DataConnectionString", "database" ) );
			m_DataConnectionString.setUserName( RoutingEngine::TheRoutingEngine->GlobalSettings.getSectionAttribute( "DataConnectionString", "user" ) );
			m_DataConnectionString.setUserPassword( RoutingEngine::TheRoutingEngine->GlobalSettings.getSectionAttribute( "DataConnectionString", "password" ) );
		}

		crtDatabase->Connect( m_DataConnectionString );
	}
	catch( ... )
	{
		TRACE( "An error occured while connecting to the data database" );
		int mutexUnlockResult = pthread_mutex_unlock( &m_SyncRoot );
		if ( 0 != mutexUnlockResult )
		{
			TRACE( "Unable to unlock Sync mutex [" << mutexUnlockResult << "]" );
		}
		throw;
	}

	int mutexUnlockResult = pthread_mutex_unlock( &m_SyncRoot );
	if ( 0 != mutexUnlockResult )
	{
		TRACE( "Unable to unlock Sync mutex [" << mutexUnlockResult << "]" );
	}
	
	return crtDatabase;
}

Database* RoutingDbOp::getConfig()
{
	Database* crtDatabase = ( Database * )pthread_getspecific( RoutingDbOp::ConfigKey );
	if ( crtDatabase == NULL )
	{
		// create a provider if necessary
		if ( m_DatabaseProvider == NULL )
		{
			if ( RoutingEngine::TheRoutingEngine != NULL )
				createProvider( RoutingEngine::TheRoutingEngine->GlobalSettings.getSection( "ConfigConnectionString" ) );
			else
				throw logic_error( "Unable to find data provider definition. You should setup the config for db." );
		}

		if ( m_DatabaseProvider == NULL ) 
			throw runtime_error( "Unable to create database provider" );

		crtDatabase = m_DatabaseProvider->createDatabase();
		if ( crtDatabase == NULL )
			throw runtime_error( "Unable to create database definition" );

		int setSpecificResult = pthread_setspecific( RoutingDbOp::ConfigKey, crtDatabase );
		if ( 0 != setSpecificResult )
		{
			TRACE( "Set thread specific ConfigKey failed [" << setSpecificResult << "]" );
		}
	}

	// don't reconnect if already connected
	pthread_t selfId = pthread_self();	
	if ( crtDatabase->IsConnected() )
	{
		DEBUG_GLOBAL( "[" << selfId << "] already connected to Config database" );
		return crtDatabase;
	}
	
	// lock and attempt to create the provider
	int mutexLockResult = pthread_mutex_lock( &m_SyncRoot );
	if ( 0 != mutexLockResult )
	{
		stringstream errorMessage;
		errorMessage << "Unable to lock Sync mutex [" << mutexLockResult << "]";
		TRACE( errorMessage.str() );

		throw runtime_error( errorMessage.str() );
	}

	try
	{			
		if ( !m_ConfigConnectionString.isValid() )
		{
			if ( RoutingEngine::TheRoutingEngine == NULL )
				throw logic_error( "Unable to find connection string definition for config. You should setup the config for db." );

			m_ConfigConnectionString.setDatabaseName( RoutingEngine::TheRoutingEngine->GlobalSettings.getSectionAttribute( "ConfigConnectionString", "database" ) );
			m_ConfigConnectionString.setUserName( RoutingEngine::TheRoutingEngine->GlobalSettings.getSectionAttribute( "ConfigConnectionString", "user" ) );
			m_ConfigConnectionString.setUserPassword( RoutingEngine::TheRoutingEngine->GlobalSettings.getSectionAttribute( "ConfigConnectionString", "password" ) );
		}

		crtDatabase->Connect( m_ConfigConnectionString );
	}
	catch( ... )
	{
		TRACE( "An error occured while connecting to the config database" );
		int mutexUnlockResult = pthread_mutex_unlock( &m_SyncRoot );
		if ( 0 != mutexUnlockResult )
		{
			TRACE( "Unable to unlock Sync mutex [" << mutexUnlockResult << "]" );
		}
		throw;
	}

	int mutexUnlockResult = pthread_mutex_unlock( &m_SyncRoot );
	if ( 0 != mutexUnlockResult )
	{
		TRACE( "Unable to unlock Sync mutex [" << mutexUnlockResult << "]" );
	}
	
	return crtDatabase;
}

// Message functions

void RoutingDbOp::InsertRoutingMessage( const string& tableName, const string& messageId, const string& payload, const string& batchId, 
	const string& correlationId, const string& sessionId, const string& requestorService, const string& responderService,
	const string& requestType, const unsigned long priority, const short holdstatus, const long sequence, const string& feedback )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Inserting message ["  << messageId << "] into queue [" << tableName << "]" );	
	
	ParametersVector params;
	DataParameterBase *messageIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	messageIdParam->setDimension( messageId.length() );
	messageIdParam->setString( messageId );
	params.push_back( messageIdParam );
	
	DataParameterBase *payloadParam = m_DatabaseProvider->createParameter( DataType::LARGE_CHAR_TYPE );
	payloadParam->setDimension( payload.length() );
	payloadParam->setString( payload );
	params.push_back( payloadParam );

	DataParameterBase *batchIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	batchIdParam->setDimension( batchId.length() );
	batchIdParam->setString( batchId );
	params.push_back( batchIdParam );
	
	DataParameterBase *correlIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	correlIdParam->setDimension( correlationId.length() );
	correlIdParam->setString( correlationId );
	params.push_back( correlIdParam );
	
	DataParameterBase *sessionIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	sessionIdParam->setDimension( sessionId.length() );
	sessionIdParam->setString( sessionId );
	params.push_back( sessionIdParam );
	
	DataParameterBase *requestorParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	requestorParam->setDimension( requestorService.length() );
	requestorParam->setString( requestorService );
	params.push_back( requestorParam );
	
	DataParameterBase *responderIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	responderIdParam->setDimension( responderService.length() );
	responderIdParam->setString( responderService );
	params.push_back( responderIdParam );
	
	DataParameterBase *reqTypeParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	reqTypeParam->setDimension( requestType.length() );
	reqTypeParam->setString( requestType );
	params.push_back( reqTypeParam );
	
	DataParameterBase *priorityParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	priorityParam->setLong( priority );
	priorityParam->setName( "Priority" );
	params.push_back( priorityParam );
	
	DataParameterBase *holdstatusParam = m_DatabaseProvider->createParameter( DataType::SHORTINT_TYPE );
	holdstatusParam->setShort( holdstatus );
	params.push_back( holdstatusParam );
	
	DataParameterBase *sequenceParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	sequenceParam->setLong( sequence );
	params.push_back( sequenceParam );

	DataParameterBase *feedbackParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	feedbackParam->setDimension( feedback.length() );
	feedbackParam->setString( feedback );
	params.push_back( feedbackParam );
		
	DataParameterBase *queueNameParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	queueNameParam->setDimension( tableName.length() );
	queueNameParam->setString( tableName );
	params.push_back( queueNameParam );
		
	data->ExecuteNonQueryCached( DataCommand::SP, "insertmessageinqueue", params );
}

void RoutingDbOp::UpdateRoutingMessage( const string& tableName, const string& messageId, const string& payload, const string& batchId, 
	const string& correlationId, const string& sessionId, const string& requestorService, const string& responderService,
	const string& requestType,	unsigned long priority, short holdstatus, long sequence, const string& feedback )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Updating message [" << messageId << "] in queue [" << tableName << "]" );	
	
	ParametersVector params;
	DataParameterBase *messageIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	messageIdParam->setDimension( messageId.length() );
	messageIdParam->setString( messageId );
	params.push_back( messageIdParam );
	
	DataParameterBase *payloadParam = m_DatabaseProvider->createParameter( DataType::LARGE_CHAR_TYPE );
	payloadParam->setDimension( payload.length() );
	payloadParam->setString( payload );
	params.push_back( payloadParam );

	DataParameterBase *batchIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	batchIdParam->setDimension( batchId.length() );
	batchIdParam->setString( batchId );
	params.push_back( batchIdParam );
	
	DataParameterBase *correlIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	correlIdParam->setDimension( correlationId.length() );
	correlIdParam->setString( correlationId );
	params.push_back( correlIdParam );
	
	DataParameterBase *sessionIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	sessionIdParam->setDimension( sessionId.length() );
	sessionIdParam->setString( sessionId );
	params.push_back( sessionIdParam );
	
	DataParameterBase *requestorParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	requestorParam->setDimension( requestorService.length() );
	requestorParam->setString( requestorService );
	params.push_back( requestorParam );
	
	DataParameterBase *responderIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	responderIdParam->setDimension( responderService.length() );
	responderIdParam->setString( responderService );
	params.push_back( responderIdParam );
	
	DataParameterBase *reqTypeParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	reqTypeParam->setDimension( requestType.length() );
	reqTypeParam->setString( requestType );
	params.push_back( reqTypeParam );
	
	DataParameterBase *priorityParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	priorityParam->setLong( priority );
	params.push_back( priorityParam );
	
	DataParameterBase *holdstatusParam = m_DatabaseProvider->createParameter( DataType::SHORTINT_TYPE );
	holdstatusParam->setShort( holdstatus );
	params.push_back( holdstatusParam );
	
	DataParameterBase *sequenceParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	sequenceParam->setLong( sequence );
	params.push_back( sequenceParam );

	DataParameterBase *feedbackParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	feedbackParam->setDimension( feedback.length() );
	feedbackParam->setString( feedback );
	params.push_back( feedbackParam );
	
	data->ExecuteNonQueryCached( DataCommand::SP, "updatemessageinqueue", params );
}

#ifdef SCROLL_CURSOR
void RoutingDbOp::DeleteRoutingMessage( const string tableName, const string messageId )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Deleting message [" << messageId << "] from queue [" << tableName << "]" );	
	
	ParametersVector params;
	
	if ( data->CursorHeld() )
	{
		// build sp name
		stringstream statement;
		statement << "DELETE FROM " << tableName;
		
		data->ExecuteNonQuery( DataCommand::INLINE, statement.str(), params, true );
		data->ReleaseCursor();
		data->EndTransaction( TransactionType::COMMIT );
	}
	else
	{
		DataParameterBase *messageIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
		messageIdParam->setDimension( messageId.length() );
		messageIdParam->setString( messageId );
		
		params.push_back( messageIdParam );
		
		// build sp name
		stringstream spName;
		spName << "Delete" << tableName;
		
		data->ExecuteNonQueryCached( DataCommand::SP, spName.str(), params );
	}
}

DataSet* RoutingDbOp::GetRoutingMessage( string tableName, string messageId )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Reading message [" << messageId << "] from queue [" << tableName << "]" );	
	
	ParametersVector params;
	stringstream statement;
	/*statement << "SELECT Guid, Payload, BatchId, CorrelationId, SessionId, RequestorService, ResponderService, RequestType, " <<
   		"Priority, HoldStatus, Sequence, Feedback FROM " << tableName << " WHERE " << tableName << ".Guid = \'" << 
		messageId << "\' FOR UPDATE";// FETCH FIRST 1 ROWS ONLY OPTIMIZE FOR 1 ROWS"; */
	
	statement << "SELECT * FROM EntryQueue WHERE " << " EntryQueue.Guid = \'" << 
		messageId << "\' AND EntryQueue.QueueName = \'" << tableName << "\' FOR UPDATE";// FETCH FIRST 1 ROWS ONLY OPTIMIZE FOR 1 ROWS"; */

	data->BeginTransaction();
	DataSet* result = data->ExecuteQueryCached( DataCommand::INLINE, statement.str(), params, true );
	if ( ( result == NULL ) || ( result->size() <= 0 ) )
	{
		//check if we got a messsage send a nice error message if no result is available
		AppException aex( "No message matching the specified messageid was found" );
		aex.setSeverity( EventSeverity::Fatal );
		aex.addAdditionalInfo( "MessageId", messageId );
		aex.addAdditionalInfo( "Queue", tableName );
		
		TRACE_GLOBAL( aex.getMessage() );
		
		if( result != NULL )
		{
			delete result;
			result = NULL;
		}
			
		throw aex;
	}
	
	return result;
}
#else
void RoutingDbOp::DeleteRoutingMessage( const string& tableName, const string& messageId, bool isReply )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Deleting message [" << messageId << "] from queue [" << tableName << "]" );	
	
	ParametersVector params;

	DataParameterBase *messageIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	messageIdParam->setDimension( messageId.length() );
	messageIdParam->setString( messageId );
	params.push_back( messageIdParam );

	DataParameterBase *queueNameParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	queueNameParam->setDimension( tableName.length() );
	queueNameParam->setString( tableName );
	params.push_back( queueNameParam );

	DataParameterBase *isMessageReply = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	isMessageReply->setLong( isReply );
	params.push_back( isMessageReply );
		
	data->ExecuteNonQueryCached( DataCommand::SP, "DeleteMessageFromQueue", params );
}

DataSet* RoutingDbOp::GetRoutingMessage( const string& tableName, const string& messageId )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Reading message [" << messageId << "] from queue [" << tableName << "]" );	
	
	ParametersVector params;
	DataParameterBase *messageIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	messageIdParam->setDimension( messageId.length() );
	messageIdParam->setString( messageId );
	params.push_back( messageIdParam );
	
	DataParameterBase *queueNameParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	queueNameParam->setDimension( tableName.length() );
	queueNameParam->setString( tableName );
	params.push_back( queueNameParam );
	
	DataSet* result = NULL;
	try
	{
		result = data->ExecuteQueryCached( DataCommand::SP, "ReadMessageInQueue", params );
		
		if ( ( result == NULL ) || ( result->size() == 0 ) )
		{
			//check if we got a messsage send a nice error message if no result is available
			RoutingExceptionMessageNotFound aex( "matching the specified messageid" );
			aex.setSeverity( EventSeverity::Fatal );
			aex.addAdditionalInfo( "MessageId", messageId );
			aex.addAdditionalInfo( "Queue", tableName );
			
			TRACE_GLOBAL( aex.getMessage() );

			throw aex;
		}
	}
	catch( ... )
	{
		if( result != NULL )
		{
			delete result;
			result = NULL;
		}
		throw;
	}
	
	return result;
}

DataSet* RoutingDbOp::GetEnrichData( const string& tableName, const string& filterId )
{
	Database* config = getConfig();

	DEBUG_GLOBAL( "Reading enrich data [" << filterId << "] from list [" << tableName << "]" );	
	
	ParametersVector params;
	DataParameterBase *filterIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	filterIdParam->setDimension( filterId.length() );
	filterIdParam->setString( filterId );
	params.push_back( filterIdParam );

	DataParameterBase *tableNameParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	tableNameParam->setDimension( tableName.length() );
	tableNameParam->setString( tableName );
	params.push_back( tableNameParam );
	
	DataSet* result = NULL;
	try
	{
		result = config->ExecuteQuery( DataCommand::SP, "GetMsgEnrichData", params );
		
		if ( ( result == NULL ) || ( result->size() == 0 ) )
		{
			AppException aex( "Enriched data not found." );
			aex.setSeverity( EventSeverity::Info );
			aex.addAdditionalInfo( "Searched Id", filterId );
			aex.addAdditionalInfo( "List", tableName );
			
			TRACE_GLOBAL( aex.getMessage() );

			throw aex;
		}
	}
	catch( ... )
	{
		if( result != NULL )
		{
			delete result;
			result = NULL;
		}
		throw;
	}
	
	return result;
}

#endif

void RoutingDbOp::MoveRoutingMessage( const string& sourceTable, const string& destTable, const string& messageId,
	const string& payload, const string& batchId, const string& correlationId, const string& sessionId, 
	const string& requestorService, const string& responderService, const string& requestType, const unsigned long priority, 
	const short holdstatus, const long sequence, const string& feedback )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Move changed routing  message ["  << messageId << "] into queue [" << destTable << "]" );	
	
	ParametersVector params;
	DataParameterBase *messageIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	messageIdParam->setDimension( messageId.length() );
	messageIdParam->setString( messageId );
	params.push_back( messageIdParam );
	
	DataParameterBase *payloadParam = m_DatabaseProvider->createParameter( DataType::LARGE_CHAR_TYPE );
	payloadParam->setDimension( payload.length() );
	payloadParam->setString( payload );
	params.push_back( payloadParam );

	DataParameterBase *batchIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	batchIdParam->setDimension( batchId.length() );
	batchIdParam->setString( batchId );
	params.push_back( batchIdParam );
	
	DataParameterBase *correlIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	correlIdParam->setDimension( correlationId.length() );
	correlIdParam->setString( correlationId );
	params.push_back( correlIdParam );
	
	DataParameterBase *sessionIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	sessionIdParam->setDimension( sessionId.length() );
	sessionIdParam->setString( sessionId );
	params.push_back( sessionIdParam );
	
	DataParameterBase *requestorParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	requestorParam->setDimension( requestorService.length() );
	requestorParam->setString( requestorService );
	params.push_back( requestorParam );
	
	DataParameterBase *responderIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	responderIdParam->setDimension( responderService.length() );
	responderIdParam->setString( responderService );
	params.push_back( responderIdParam );
	
	DataParameterBase *reqTypeParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	reqTypeParam->setDimension( requestType.length() );
	reqTypeParam->setString( requestType );
	params.push_back( reqTypeParam );
	
	DataParameterBase *priorityParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	priorityParam->setLong( priority );
	priorityParam->setName( "Priority" );
	params.push_back( priorityParam );
	
	DataParameterBase *holdstatusParam = m_DatabaseProvider->createParameter( DataType::SHORTINT_TYPE );
	holdstatusParam->setShort( holdstatus );
	params.push_back( holdstatusParam );
	
	DataParameterBase *sequenceParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	sequenceParam->setLong( sequence );
	params.push_back( sequenceParam );

	DataParameterBase *feedbackParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	feedbackParam->setDimension( feedback.length() );
	feedbackParam->setString( feedback );
	params.push_back( feedbackParam );
		
	DataParameterBase *queueNameParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	queueNameParam->setDimension( destTable.length() );
	queueNameParam->setString( destTable );
	params.push_back( queueNameParam );
		
	data->ExecuteNonQueryCached( DataCommand::SP, "UpdateMessageToNewQueue", params );
	
}

/*void RoutingDbOp::MoveRoutingMessage( string sourceTable, string destTable, string messageId )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Moving message ["  << messageId << "] from queue [" << sourceTable << "] to queue [" << destTable << "]" );	
	
	ParametersVector params;
	
	DataParameterBase *srcTableParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	srcTableParam->setDimension( sourceTable.length() );
	srcTableParam->setString( sourceTable );
	
	params.push_back( srcTableParam );
	
	DataParameterBase *destTableParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	destTableParam->setDimension( destTable.length() );
	destTableParam->setString( destTable );
	
	params.push_back( destTableParam );
	
	DataParameterBase *messageIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	messageIdParam->setDimension( messageId.length() );
	messageIdParam->setString( messageId );
	
	params.push_back( messageIdParam );

	// call move	
	data->ExecuteNonQuery( DataCommand::SP, "REunittest.movemessage", params );
}*/

string RoutingDbOp::GetOriginalMessageId( const string& correlationId )
{
	Database* data = getData();
	
	ParametersVector params;
	
	DataParameterBase *correlIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	correlIdParam->setDimension( correlationId.length() );
	correlIdParam->setString( correlationId );
	params.push_back( correlIdParam );
	
	DataParameterBase *msgIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE, DataParameterBase::PARAM_OUT );
	msgIdParam->setDimension( 26 );
	params.push_back( msgIdParam );

	DEBUG_GLOBAL( "Reading original message id for [" << correlationId << "]" );	
		
	data->ExecuteNonQueryCached( DataCommand::SP, "GetOriginalMessageId", params );
	string originalMsgId = StringUtil::Trim( msgIdParam->getString() );

	return originalMsgId;
}

string RoutingDbOp::GetOriginalPayload( const string& messageId )
{
	Database* data = getData();
	
	ParametersVector params;
	
	DataParameterBase *messageIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	messageIdParam->setDimension( messageId.length() );
	messageIdParam->setString( messageId );
	params.push_back( messageIdParam );
	
	DEBUG_GLOBAL( "Reading original payload for [" << messageId << "]" );	

	DataSet* result = NULL;
	string originalPayload = "";
	
	try
	{
		result = data->ExecuteQueryCached( DataCommand::SP, "GetOriginalPayload", params );
		
		if ( ( result == NULL ) || ( result->size() == 0 ) )
		{
			throw runtime_error( "Unable to retrieve original payload from the History queue." );
		}
		originalPayload = StringUtil::Trim( result->getCellValue( 0, 0 )->getString() );
		
		if( result != NULL )
		{
			delete result;
			result = NULL;
		}
	}
	catch( ... )
	{
		if( result != NULL )
		{
			delete result;
			result = NULL;
		}
		throw;
	}
	
	return originalPayload;
}

unsigned long RoutingDbOp::Archive()
{
	DEBUG( "Archiving messages..." );
	
	Database* data = getData();

	ParametersVector params;
	DataParameterBase *archivedParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE, DataParameterBase::PARAM_OUT );
	archivedParam->setLong( 0 );
	params.push_back( archivedParam );

	data->BeginTransaction();
	try
	{
		data->ExecuteNonQueryCached( DataCommand::SP, "ArchiveIdle", params );
	}
	catch( ... )
	{
		data->EndTransaction( TransactionType::ROLLBACK );
		throw;
	}

	data->EndTransaction( TransactionType::COMMIT );

	return archivedParam->getLong();
}

void RoutingDbOp::UpdateBusinessMessageAck( const string& id, bool batch )
{
	string batchId = "", correlationId = "";
	Database* data = getData();

	ParametersVector params;
	DataParameterBase *messageIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	messageIdParam->setDimension( id.length() );
	messageIdParam->setString( id );
	params.push_back( messageIdParam );

	if ( batch )
	{
		batchId = id;
		DEBUG_GLOBAL( "Updating business message ack for batch [" << batchId << "]" );
		data->ExecuteNonQueryCached( DataCommand::SP, "UpdateRMAckBatch", params );
	}
	else
	{
		correlationId = id;
		DEBUG_GLOBAL( "Updating business message ack for correlation id [" << correlationId << "]" );
		data->ExecuteNonQueryCached( DataCommand::SP, "UpdateRMAck", params );
	}
}

void RoutingDbOp::UpdateBusinessMessageValueDate( const string& correlationId, const string& newDate )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Updating business message value date [" << correlationId << "] with [" << newDate << "]" );	
	
	ParametersVector params;
	DataParameterBase *messageIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	messageIdParam->setDimension( correlationId.length() );
	messageIdParam->setString( correlationId );
	messageIdParam->setName( "CorrelationId" );
	params.push_back( messageIdParam );
	
	DataParameterBase *dateParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	dateParam->setDimension( newDate.length() );
	dateParam->setString( newDate );
	dateParam->setName( "ValueDate" );
	params.push_back( dateParam );
	
	data->ExecuteNonQueryCached( DataCommand::SP, "UpdateRMValueDate", params );
}

void RoutingDbOp::UpdateBusinessMessageResponder( const string& messageId, const string& receiverApp )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Updating business message responder [" << messageId << "] with [" << receiverApp << "]" );	
	
	ParametersVector params;
	DataParameterBase *messageIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	messageIdParam->setDimension( messageId.length() );
	messageIdParam->setString( messageId );
	messageIdParam->setName( "Message_id" );
	params.push_back( messageIdParam );
	
	DataParameterBase *receiverAppParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	receiverAppParam->setDimension( receiverApp.length() );
	receiverAppParam->setString( receiverApp );
	receiverAppParam->setName( "Receiver_app" );
	params.push_back( receiverAppParam );
	
	data->ExecuteNonQueryCached( DataCommand::SP, "UpdateRMResponder", params );
}

void RoutingDbOp::UpdateBMAssembleResponder( const string& batchId, const string& receiverApp )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Updating business message responder [" << batchId << "] with [" << receiverApp << "]" );

	ParametersVector params;
	DataParameterBase *batchIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	batchIdParam->setDimension( batchId.length() );
	batchIdParam->setString( batchId );
	batchIdParam->setName( "Batch_id" );
	params.push_back( batchIdParam );

	DataParameterBase *receiverAppParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	receiverAppParam->setDimension( receiverApp.length() );
	receiverAppParam->setString( receiverApp );
	receiverAppParam->setName( "Receiver_app" );
	params.push_back( receiverAppParam );

	data->ExecuteNonQueryCached( DataCommand::SP, "UpdateRMAssembleResponder", params );
}

void RoutingDbOp::UpdateBusinessMessageUserId( const string& correlationId, const int userId )
{
	Database* data = getData();

	if ( userId > 0 )
	//if ( ( userId.length() <= 0 ) || ( userId == "-1" ) )
	{
		DEBUG_GLOBAL( "Business message userid for [" << correlationId << "] will not be updated [empty]" );
		return;
	}
	else
	{
		DEBUG_GLOBAL( "Updating business message userid for [" << correlationId << "] with [" << userId << "]" );	
	}
	
	ParametersVector params;
	DataParameterBase *messageIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	messageIdParam->setDimension( correlationId.length() );
	messageIdParam->setString( correlationId );
	params.push_back( messageIdParam );
	
	DataParameterBase *userIdParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	userIdParam->setInt( userId );
	params.push_back( userIdParam );
	
	data->ExecuteNonQueryCached( DataCommand::SP, "UpdateRMUserId", params );
}

void RoutingDbOp::InsertBusinessMessage( const string& messageType, const string& entity, const string& senderApp, const string& receiverApp, const string& messageId, const string& correlationId, const vector<string>& keywords, const vector<string>& keywordValues )
{
	Database* data = getData();

	ParametersVector params;

	DataParameterBase *messageTypeParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	messageTypeParam->setDimension( messageType.length() );
	messageTypeParam->setString( messageType );
	messageTypeParam->setName( "Message type" );
	params.push_back( messageTypeParam );

	DataParameterBase *senderAppParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	senderAppParam->setDimension( senderApp.length() );
	senderAppParam->setString( senderApp );
	senderAppParam->setName( "Sender application" );
	params.push_back( senderAppParam );

	DataParameterBase *receiverAppParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	receiverAppParam->setDimension( receiverApp.length() );
	receiverAppParam->setString( receiverApp );
	receiverAppParam->setName( "Receiver application" );
	params.push_back( receiverAppParam );

	DataParameterBase *messageIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	messageIdParam->setDimension( messageId.length() );
	messageIdParam->setString( messageId );
	messageIdParam->setName( "Message id" );
	params.push_back( messageIdParam );

	DataParameterBase *correlIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	correlIdParam->setDimension( correlationId.length() );
	correlIdParam->setString( correlationId );
	correlIdParam->setName( "Correlation id" );
	params.push_back( correlIdParam );

	DataParameterBase *keywordsParam = m_DatabaseProvider->createParameter( DataType::ARRAY );
	keywordsParam->setName( "Keywords_and_Values" );
	for ( size_t i=0; i < keywords.size(); i++ )
		keywordsParam->push_back( keywords[i] );

	for ( size_t i=0; i < keywordValues.size() ; i++ )
		keywordsParam->push_back( keywordValues[i] );
		
	params.push_back( keywordsParam );

	DataParameterBase *entityParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	entityParam->setDimension( entity.length() );
	entityParam->setString( entity );
	entityParam->setName( "Entity" );
	params.push_back( entityParam );

	data->ExecuteNonQuery( DataCommand::SP, "InsertMessage", params );
}

void RoutingDbOp::EnrichMessageData( const vector<string>& tableNames, const vector<string>& tableValues, const vector<string>& enrichNames, const vector<string>& enrichValues )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Update business message [" << tableValues[4] << "]" );

	ParametersVector params;

	for( int i = 0; i < tableValues.size() ; i++ )
	{
		if( i == tableValues.size() - 1 ) 
		{
			DataParameterBase *enrichParam = m_DatabaseProvider->createParameter( DataType::ARRAY );
			enrichParam->setName( "Keywords_and_Values" );
			for( size_t j = 0; j < enrichNames.size(); j++ )
				enrichParam->push_back( enrichNames[j] );

			for( size_t j = 0; j < enrichValues.size(); j++ )
				enrichParam->push_back( enrichValues[j] );

			params.push_back( enrichParam );
		}

		DataParameterBase* param = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
		param->setDimension( tableValues[i].length() );
		param->setString( tableValues[i] );
		param->setName( tableNames[i] );
		params.push_back( param );
	}

	data->ExecuteNonQueryCached( DataCommand::SP, "ENRICHMESSAGEDATA", params );

}

void RoutingDbOp::UpdateLiquidities( const string& correlationId )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Updating liquidities [" << correlationId << "]" );	
	
	ParametersVector params;
	
	DataParameterBase *correlIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	correlIdParam->setDimension( correlationId.length() );
	correlIdParam->setString( correlationId );
	correlIdParam->setName( "Correlation id" );
	params.push_back( correlIdParam );

	// build sp name
	data->ExecuteNonQueryCached( DataCommand::SP, RoutingEngine::getLiquiditiesSP(), params );
}

// aggregation
bool RoutingDbOp::GetAggregationFields( const string& aggregationTable, RoutingAggregationCode& request, bool trim )
{
	Database* data = getData();
	
	// TODO escape input
	
	// build the statement 
	stringstream statement;
	
	// statement like "select F1 from FEEDBACKAGG where WMQID='tahsdhcn3487'
	statement << "SELECT ";
	
	bool moreFields = false;
	
	RoutingAggregationFieldArray aggregationCode = request.getFields();
	RoutingAggregationFieldArray::iterator fieldIterator = aggregationCode.begin();
	
	while( fieldIterator != aggregationCode.end() )
	{
		if ( moreFields )
			statement << ", ";
		else
			moreFields = true;
			
		// add the token to select outputs
		statement << fieldIterator->first;		
		fieldIterator++;
	}
	
	string requestCorrelId = request.getCorrelId();
	ParametersVector params;
	DataParameterBase *reqIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	reqIdParam->setDimension( requestCorrelId.length() );
	reqIdParam->setString( requestCorrelId );

	params.push_back( reqIdParam );

	statement << " from " << aggregationTable << " where ";
	if ( trim )
		statement << m_DatabaseProvider->getTrimFunc( request.getCorrelToken() );
	else
		statement << request.getCorrelToken();
	statement << "=" << m_DatabaseProvider->getParamPlaceholder( 1 );
	
	DEBUG_GLOBAL( "Retrieve aggregation fields statement : [" << statement.str() << "]" );
	
	DataSet* result = NULL;
	
	try
	{	
		result = data->ExecuteQueryCached( DataCommand::INLINE, statement.str(), params, false, 1 );
		if( ( result != NULL ) && ( result->size() > 0 ) )
		{
			int index = 0;
			
			fieldIterator = aggregationCode.begin();
			while( fieldIterator != aggregationCode.end() )
			{
				string resultField = StringUtil::Trim( result->getCellValue( 0, index )->getString() );
				request.setAggregationField( fieldIterator->first, resultField );
				
				//DEBUG_GLOBAL( "Got [" << fieldIterator->first << "] = [" << resultField << "]" );
				fieldIterator++;
				index++;
			}
			
			if ( result != NULL )
			{
				delete result;
				result = NULL;
			}
		}
		else
		{
			if ( result != NULL )
			{
				delete result;
				result = NULL;
			}

			// try again trimming the value
			if ( !trim )
			{
				TRACE( "Performance warning : Attempting to use TRIM on field because a previous statement returned no data" );
				return GetAggregationFields( aggregationTable, request, true );
			}
			else
			{
				stringstream errorMessage;
				errorMessage << "Unable to retrieve field(s) from [" << aggregationTable << "] matching condition [" <<
					request.getCorrelToken() << "] = [" << request.getCorrelId() << "]";
				throw runtime_error( errorMessage.str() );
			}
		}
	}
	catch( ... )
	{
		if ( result != NULL )
		{
			delete result;
			result = NULL;
		}
			
		throw;
	}
	return trim;
}

bool RoutingDbOp::AggregationRequestOp( const string& aggregationTable, const RoutingAggregationCode& request, bool& trim )
{
	Database* data = getData();
	
	// TODO validate input
	// check if we need to update or insert the request
	unsigned int paramCount = 0;

	stringstream checkStatement;
	string requestCorrelId = request.getCorrelId();

	// placeholder for soft parsing ( usable with db2 only ? )
	checkStatement << "SELECT COUNT(" << request.getCorrelToken() << ") FROM " << aggregationTable << " WHERE ( ";
	if( trim )
	{
		checkStatement << m_DatabaseProvider->getTrimFunc( request.getCorrelToken() );
	}
	else
	{
		checkStatement << request.getCorrelToken();
	}
	checkStatement << " = " << m_DatabaseProvider->getParamPlaceholder( ++paramCount ) << " )";

	ParametersVector params;
	
	DataParameterBase *reqIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	reqIdParam->setDimension( requestCorrelId.length() );
	reqIdParam->setString( requestCorrelId );
	params.push_back( reqIdParam );

	// get each condition in the request
	RoutingAggregationConditionArray aggregationCondition = request.getConditions();
	
	RoutingAggregationConditionArray::const_iterator conditionIterator = aggregationCondition.begin();
	while( conditionIterator != aggregationCondition.end() )
	{
		// no value condition ( ex : blabla is null )
		if ( conditionIterator->second.length() == 0 ) 
			checkStatement << " AND (" << conditionIterator->first << ")";
		else
		{
			string paramValue = conditionIterator->second;

			checkStatement << " AND (" << conditionIterator->first << "=" << m_DatabaseProvider->getParamPlaceholder( ++paramCount ) << ") ";
					
			reqIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
			reqIdParam->setDimension( paramValue.length() );
			reqIdParam->setString( paramValue );
			params.push_back( reqIdParam );
		}
		conditionIterator++;
	}
	
	//checkStatement << " AND ( TFDCODE is NULL )";
		
	//DataParameterBase *retParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE, DataParameterBase::PARAM_OUT );
	//retParam->setDimension( 30 );
	//retParam->setString( "" );
	//params.push_back( retParam );

	// this will cause hard parsing of the statement
	/*checkStatement << "SELECT COUNT(" << request.getCorrelToken() << ") FROM " << aggregationTable << " WHERE ( " <<
		request.getCorrelToken() << " = \'" << requestCorrelId << "\' )";*/
		
	DEBUG_GLOBAL( "Check for aggregation request statement : [" << checkStatement.str() << "] with [" <<
		request.getCorrelToken() << "] = [" << requestCorrelId << "]" );
	// with param [" << requestCorrelId << "]" );
	
	DataSet* countData = NULL;
	bool recordExists;
	
	try
	{
		countData = data->ExecuteQueryCached( DataCommand::INLINE, checkStatement.str(), params );
		//countData = data->ExecuteQuery( DataCommand::INLINE, checkStatement.str() ); 
		//countData = data->ExecuteQuery( DataCommand::SP, "GETFEED", params );
		long returnValue = countData->getCellValue( 0, 0 )->getLong(); 

		//data->ExecuteNonQuery( DataCommand::INLINE, checkStatement.str(), params );
		//long returnValue = retParam->getLong();

		recordExists = ( returnValue != 0 );
		DEBUG( "Aggregation value existence is [" << returnValue << "]" );

		if ( countData != NULL )
		{
			delete countData;
			countData = NULL;
		}

		// attempt trim if result is "no records"
		if ( !recordExists && !trim )
		{
			TRACE( "Performance warning : Attempting to use TRIM on field because a previous statement returned no data" );
			trim = true;
			recordExists = RoutingDbOp::AggregationRequestOp( aggregationTable, request, trim );
		}
	}
	catch( ... )
	{
		if ( countData != NULL )
		{
			delete countData;
			countData = NULL;
		}
		throw;
	}
		
	return recordExists;
}

void RoutingDbOp::InsertAggregationRequest( const string& aggregationTable, const RoutingAggregationCode& request )
{
	Database* data = getData();
	
	// build the statement 
	stringstream statement;
	
	// statement like "insert into FEEDBACKAGG( WMQID, F1, F2 ) values( 'tahsdhcn3487', 'F1', 'F2' )"
	// insert into FEEDBACKAGG( WMQID, F1, F2 )
	statement << "INSERT INTO " << aggregationTable << "( " << request.getCorrelToken() ;
	
	// TODO validate input
	stringstream valuesString;
	unsigned int paramCount = 0;
	valuesString << " ) VALUES ( " << m_DatabaseProvider->getParamPlaceholder( ++paramCount );
	
	// get each field in the request
	RoutingAggregationFieldArray aggregationCode = request.getFields();
	
	ParametersVector params;
	
	// add agg as 1st param
	string paramValue = request.getCorrelId();
	
	DataParameterBase *reqIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	reqIdParam->setDimension( paramValue.length() );
	reqIdParam->setString( paramValue );
	
	params.push_back( reqIdParam );
	
	RoutingAggregationFieldArray::const_iterator requestIterator = aggregationCode.begin();
	while( requestIterator != aggregationCode.end() )
	{
		statement << ", " << requestIterator->first;
		
		paramValue = requestIterator->second;

		if ( requestIterator->first == "PAYLOAD" )
			reqIdParam = m_DatabaseProvider->createParameter( DataType::LARGE_CHAR_TYPE );
		else
			reqIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );

		reqIdParam->setDimension( paramValue.length() );

		reqIdParam->setString( paramValue );
		
		params.push_back( reqIdParam );
	
		valuesString << ", " << m_DatabaseProvider->getParamPlaceholder( ++paramCount );
		requestIterator++;
	}

	// build the final statement
	stringstream finalStatement;
	finalStatement << statement.str() << valuesString.str() << " )";
	
	DEBUG_GLOBAL( "Insert aggregation request statement : [" << finalStatement.str() << "]" );
	data->ExecuteNonQueryCached( DataCommand::INLINE, finalStatement.str(), params );
}

bool RoutingDbOp::UpdateAggregationRequest( const string& aggregationTable, const RoutingAggregationCode& request, bool trim )
{
	Database* data = getData();
	
	// build the statement 
	stringstream statement;
	unsigned int paramCount = 0;
	
	// statement like "update FEEDBACKAGG set F1='V1', F2='V2' where WMQID='tahsdhcn3487'"
	statement << "UPDATE " << aggregationTable << " SET ";
	
	ParametersVector params;
	DataParameterBase *reqIdParam;
	
	// get each field in the request
	RoutingAggregationFieldArray aggregationCode = request.getFields();
	
	string paramValue = "";
	
	RoutingAggregationFieldArray::const_iterator requestIterator = aggregationCode.begin();
	while( requestIterator != aggregationCode.end() )
	{
		paramValue = requestIterator->second;

		statement << requestIterator->first << "=" << m_DatabaseProvider->getParamPlaceholder( ++paramCount ) << ",";
				
		reqIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
		reqIdParam->setDimension( paramValue.length() );
		reqIdParam->setString( paramValue );
		params.push_back( reqIdParam );

		requestIterator++;
	}
	
	// build the final statement
	stringstream finalStatement;
	
	// add agg as last param
	paramValue = request.getCorrelId(); 

	string statementstr = statement.str();
	finalStatement << statementstr.substr( 0, statementstr.length() - 1 ) << " WHERE ( ";
	if( trim )
	{
		finalStatement << m_DatabaseProvider->getTrimFunc( request.getCorrelToken() );
	}
	else
	{
		finalStatement << request.getCorrelToken();
	}
	finalStatement << " = " << m_DatabaseProvider->getParamPlaceholder( ++paramCount ) << " )";

	//finalStatement << " AND ( TFDCODE is NULL )";
			
	reqIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );

	reqIdParam->setDimension( paramValue.length() ); 
	reqIdParam->setString( paramValue ); 
	params.push_back( reqIdParam );

	// get each condition in the request
	RoutingAggregationConditionArray aggregationCondition = request.getConditions();
	
	RoutingAggregationConditionArray::const_iterator conditionIterator = aggregationCondition.begin();
	while( conditionIterator != aggregationCondition.end() )
	{
		// no value condition ( ex : blabla is null )
		if ( conditionIterator->second.length() == 0 ) 
			finalStatement << " AND (" << conditionIterator->first << ")";
		else
		{
			string innerParamValue = conditionIterator->second;

			finalStatement << " AND (" << conditionIterator->first << "=" << m_DatabaseProvider->getParamPlaceholder( ++paramCount ) << ") ";
					
			reqIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
			reqIdParam->setDimension( innerParamValue.length() );
			reqIdParam->setString( innerParamValue );
			params.push_back( reqIdParam );
		}
		conditionIterator++;
	}
	
	DEBUG_GLOBAL( "Update aggregation fields statement : [" << finalStatement.str() << "]" );

	try
	{
		data->ExecuteNonQueryCached( DataCommand::INLINE, finalStatement.str(), params );
	}
	catch( const DBWarningException& ex )
	{
		// update failed ( no rows to update ), try again with trim
		if ( !trim )
		{
			TRACE( "Performance warning : Attempting to use TRIM on field because a previous statement returned no data [" << ex.getMessage() << "]" );
			return RoutingDbOp::UpdateAggregationRequest( aggregationTable, request, true );
		}
		else
			throw;
	}
	
	if( data->getLastNumberofAffectedRows() == 0 )
	{
		// update succeded but no rows updated
		// statement executed, but no data was retrieved, or another warning
		DBNoUpdatesException warningEx( "Aggregation performed but no records were updated" );
		warningEx.addAdditionalInfo( "location", "OracleDatabase::ExecuteNonQuery" );

		TRACE( "Aggregation performed but no records were updated" );

		throw warningEx;
	}

	//params.Dump();
	/*ParametersVector params2;

	DataParameterBase *reqIdParam2 = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE ); 

	reqIdParam2->setDimension( paramValue.length() ); 
	reqIdParam2->setString( paramValue ); 
	params2.push_back( reqIdParam2 );

	data->ExecuteNonQuery( DataCommand::INLINE, "insert into fbcheck( whclause ) values( :1 )", params2 );*/
	return trim;
}

void RoutingDbOp::UpdateOriginalBatchMessages( const string& batchid, const string& code )
{
	//RoutingAggregationFieldArray fields = request.getFields();
	//string origBatchId = fields[ RoutingMessageEvaluator::AGGREGATIONTOKEN_BATCHID ];
	
	Database* data = getData();
	
	ParametersVector params;
	DataParameterBase *batchIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	batchIdParam->setDimension( batchid.length() );
	batchIdParam->setString( batchid );
	batchIdParam->setName( "BatchId" );
	params.push_back( batchIdParam );
	
	DataParameterBase *tfdCodeParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	tfdCodeParam->setDimension( code.length() );
	tfdCodeParam->setString( code );
	tfdCodeParam->setName( "TFDCode" );
	params.push_back( tfdCodeParam );

	data->ExecuteNonQuery( DataCommand::SP, "updaterefusalfbcode", params );	
}

void RoutingDbOp::RemoveBatchMessages( const string& batchId, const string& tableName, const string& spName )
{
	DEBUG( "Deleting all messages in batch [" << batchId << "]" );
	Database* data = getData();
	
	ParametersVector params;
	DataParameterBase *batchIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	batchIdParam->setDimension( batchId.length() );
	batchIdParam->setString( batchId );
	
	params.push_back( batchIdParam );

	DataParameterBase *tableParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	tableParam->setDimension( tableName.length() );
	tableParam->setString( tableName );

	params.push_back( tableParam );
	
	data->ExecuteNonQueryCached( DataCommand::SP, spName, params );
}

DataSet* RoutingDbOp::GetBatchMessages( const string& batchId, const string& tableName, const string& spName )
{
	DEBUG( "Getting all messages in batch [" << batchId << "]" );
	Database* data = getData();
	
	ParametersVector params;
	DataParameterBase *batchIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	batchIdParam->setDimension( batchId.length() );
	batchIdParam->setString( batchId );
	
	params.push_back( batchIdParam );

	DataParameterBase *tableParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	tableParam->setDimension( tableName.length() );
	tableParam->setString( tableName );
	
	params.push_back( tableParam );
	
	DataSet* result = NULL;

	try
	{
		result = data->ExecuteQueryCached( DataCommand::SP, spName, params );
	}
	catch( ... )
	{
		if ( result != NULL )
		{
			delete result;
			result = NULL;
		}

		throw;
	}	
	return result;
}

DataSet* RoutingDbOp::GetBatchPart( const int userId, const string& receiver, const int serviceId, const string& queue )
{
	DEBUG( "Getting first 1000 messages in batch for [" << receiver << "]" );
	
	Database* data = getData();
	
	ParametersVector params;
	DataParameterBase *userIdParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	userIdParam->setInt( userId );
	params.push_back( userIdParam );

	DataParameterBase *receiverParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	receiverParam->setDimension( receiver.length() );
	receiverParam->setString( receiver );
	params.push_back( receiverParam );

	DataParameterBase *serviceIdParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	serviceIdParam->setInt( serviceId );
	params.push_back( serviceIdParam );
	
	DataParameterBase *queueParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	queueParam->setDimension( queue.length() );
	queueParam->setString( queue );
	params.push_back( queueParam );


	DataSet* result = NULL;

	try
	{
#if defined( TESTDLL_EXPORT ) || defined ( TESTDLL_IMPORT )
		result = data->ExecuteQueryCached( DataCommand::SP, "REunittest.rbatchtest", params );
#else
		result = data->ExecuteQueryCached( DataCommand::SP, "RBatch", params );
#endif
	}
	catch( ... )
	{
		TRACE( "Execute query [first 1000 messages in batch] failed" );
		if ( result != NULL )
		{
			delete result;
			result = NULL;
		}

		throw;
	}
	return result;
}

DataSet* RoutingDbOp::GetBatchMessages( const string& batchId, const bool isReply, const string& issuer, int batchItemsType )
{	
	if( isReply )
	{
		DEBUG( "Getting all original messages in batch [" << batchId << "]" );
	}
	else
	{
		DEBUG( "Getting all messages in batch [" << batchId << "]" );
	}
	
	Database* data = getData();
	
	ParametersVector params;
	DataParameterBase *batchIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	batchIdParam->setDimension( batchId.length() );
	batchIdParam->setString( batchId );
	
	params.push_back( batchIdParam );

	if( isReply )
	{
		DataParameterBase *batchIssuerParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
		batchIssuerParam->setDimension( issuer.length() );
		batchIssuerParam->setString( issuer );
		
		params.push_back( batchIssuerParam );
	}
	
	DataSet* result = NULL;

	try
	{
		if( isReply )
		{
			switch ( batchItemsType )
			{
				case DD_Ref_Reply :
					result = data->ExecuteQueryCached( DataCommand::SP, "GetMessagesInBatchRFD", params );
					break;
				case Batch_Items :
					result = data->ExecuteQueryCached( DataCommand::SP, "GetMessagesInBatch", params );
					break;
				case Sepa_Sts_Reply :
					result = data->ExecuteQueryCached( DataCommand::SP, "GetMessagesInBatchSEN", params );
			}
		}
		else
			result = data->ExecuteQuery( DataCommand::SP, "GetBatchJobs", params );
	}
	catch( ... )
	{
		TRACE( "Execute query [get all batch items] failed" );
		if ( result != NULL )
		{
			delete result;
			result = NULL;
		}

		throw;
	}
	return result;
}

// Routing jobs functions
DataSet* RoutingDbOp::GetRoutingJob( const string& jobId )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Getting the first routing job ..." );
	DataSet* result = data->ExecuteQueryCached( DataCommand::SP, "GetFirstNewJob" );
	DEBUG_GLOBAL( "Routing job read" );
	
	return result;
}

DataSet* RoutingDbOp::ReadJobParams( const string& jobId )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Getting params for job : "  << jobId );	
	
	ParametersVector params;
	DataParameterBase *jobidParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	jobidParam->setDimension( jobId.length() );
	jobidParam->setString( jobId );
	
	params.push_back( jobidParam );
	
	DataSet* result = data->ExecuteQueryCached( DataCommand::SP, "GetParamsForJob", params );
	
	DEBUG_GLOBAL( "[" << result->size() << "] params for job [" << jobId << "]" );
	
	return result;
}

void RoutingDbOp::CommitJob( const string& jobId, const bool isolate )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Commiting job : "  << jobId );	
		
	ParametersVector params;
	DataParameterBase *jobidParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	jobidParam->setDimension( 30 );
	jobidParam->setString( jobId );
	
	params.push_back( jobidParam );
	
	if ( isolate )
		data->BeginTransaction();
	data->ExecuteNonQueryCached( DataCommand::SP, "CommitJob", params );
	if ( isolate )
		data->EndTransaction( TransactionType::COMMIT );
}

void RoutingDbOp::AbortJob( const string& jobId )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Aborting job : " << jobId );	
	
	ParametersVector params;
	DataParameterBase *jobidParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	jobidParam->setDimension( 30 );
	jobidParam->setString( jobId );
	
	params.push_back( jobidParam );
	
	data->BeginTransaction();
	data->ExecuteNonQuery( DataCommand::SP, "AbortJob", params );
	data->EndTransaction( TransactionType::COMMIT );
}

void RoutingDbOp::RollbackJob( const string& jobId )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Commiting job : " << jobId );	
	
	ParametersVector params;
	DataParameterBase *jobidParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	jobidParam->setDimension( jobId.length() );
	jobidParam->setString( jobId );
	
	params.push_back( jobidParam );
	
	data->BeginTransaction();
	data->ExecuteNonQuery( DataCommand::SP, "RollbackJob", params );
	data->EndTransaction( TransactionType::COMMIT );
}

void RoutingDbOp::InsertJob( const string& jobId, const string& routingPoint, const string& function, const int userId )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Inserting job : " << jobId );	

	ParametersVector params;
	DataParameterBase *jobidParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	jobidParam->setDimension( jobId.length() );
	jobidParam->setString( jobId );
	params.push_back( jobidParam );
	
	DataParameterBase *rpParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	rpParam->setDimension( routingPoint.length() );
	rpParam->setString( routingPoint );
	params.push_back( rpParam );
	
	DataParameterBase *funcParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	funcParam->setDimension( function.length() );
	funcParam->setString( function );
	params.push_back( funcParam );

	DataParameterBase *useridParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	useridParam->setInt( userId );
	params.push_back( useridParam );
	
	data->ExecuteNonQuery( DataCommand::SP, "qPIRoutingHelper", params );
}

void RoutingDbOp::DeferJob( const string& jobId, const long deferedQueue, const string& routingPoint, const string& function, const int userId )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Deferring job : " << jobId );	

	ParametersVector params;
	DataParameterBase *jobidParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	jobidParam->setDimension( jobId.length() );
	jobidParam->setString( jobId );
	params.push_back( jobidParam );
	
	DataParameterBase *crtQueueParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	crtQueueParam->setLong( deferedQueue );
	params.push_back( crtQueueParam );
	
	DataParameterBase *rpParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	rpParam->setDimension( routingPoint.length() );
	rpParam->setString( routingPoint );
	params.push_back( rpParam );
	
	DataParameterBase *funcParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	funcParam->setDimension( function.length() );
	funcParam->setString( function );
	params.push_back( funcParam );

	DataParameterBase *useridParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	useridParam->setInt( userId );
	params.push_back( useridParam );
	
	data->ExecuteNonQueryCached( DataCommand::SP, "DeferJob", params );
}

void RoutingDbOp::ResumeJobs( const long deferedQueue )
{
	DEBUG_GLOBAL( "Resuming jobs for queue : " << deferedQueue );	
	Database* data = getData();
				
	ParametersVector resParams;
	DataParameterBase *queueParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	queueParam->setLong( deferedQueue );
	resParams.push_back( queueParam );
	
	try
	{
		data->BeginTransaction();
		data->ExecuteNonQuery( DataCommand::SP, "ResumeJobs", resParams );
	}
	catch( const DBWarningException &error )
	{
		DEBUG_GLOBAL( "Warning available in resume jobs [" << error.getMessage() << "]" );
	}
	catch( ... )
	{
		data->EndTransaction( TransactionType::ROLLBACK );
		throw;
	}
	data->EndTransaction( TransactionType::COMMIT );
}

BatchManagerBase::BATCH_STATUS RoutingDbOp::BatchJob( const string& jobId, const string& sequence, const string& batchId, 
	const string& correlId, const string& feedback, const string& xformItem, const string& amountBDP, const string& amountADP )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Batching job : " << jobId );	

	ParametersVector params;
	DataParameterBase *jobidParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	jobidParam->setDimension( jobId.length() );
	jobidParam->setString( jobId );
	params.push_back( jobidParam );

	DataParameterBase *sequenceParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	sequenceParam->setLong( StringUtil::ParseLong( sequence ) );
	params.push_back( sequenceParam );

	DataParameterBase *batchidParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	batchidParam->setDimension( batchId.length() );
	batchidParam->setString( batchId );
	params.push_back( batchidParam );
	
	DataParameterBase *correlidParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	correlidParam->setDimension( correlId.length() );
	correlidParam->setString( correlId );
	params.push_back( correlidParam );

	DataParameterBase *feedbackParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	feedbackParam->setDimension( feedback.length() );
	feedbackParam->setString( feedback );
	params.push_back( feedbackParam );

	DataParameterBase *xformItemParam = m_DatabaseProvider->createParameter( DataType::LARGE_CHAR_TYPE );
	xformItemParam->setDimension( xformItem.length() );
	xformItemParam->setString( xformItem );
	params.push_back( xformItemParam );
	
	DataParameterBase *amountBDPParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	amountBDPParam->setDimension( amountBDP.length() );
	amountBDPParam->setString( amountBDP );
	params.push_back( amountBDPParam );
	
	DataParameterBase *amountADPParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	amountADPParam->setDimension( amountADP.length() );
	amountADPParam->setString( amountADP );
	params.push_back( amountADPParam );

	DataParameterBase *batchStatusParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE, DataParameterBase::PARAM_OUT );
	batchStatusParam->setLong( -1 );
	params.push_back( batchStatusParam );
	
	data->ExecuteNonQueryCached( DataCommand::SP, "BatchJob", params );

	return ( BatchManagerBase::BATCH_STATUS )batchStatusParam->getLong();
}

BatchManagerBase::BATCH_STATUS RoutingDbOp::GetBatchStatus( const string& batchId, const string& batchUID, string& comBatchId,
	const int userId, const unsigned long batchCount, const string& batchAmount, const long serviceId, const string& routingPoint )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Query batch status for [" << batchId << "]" );	

	ParametersVector params;
	DataParameterBase *batchIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	batchIdParam->setDimension( batchId.length() );
	batchIdParam->setString( batchId );
	params.push_back( batchIdParam );

	DataParameterBase *userIdParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	userIdParam->setInt( userId );
	params.push_back( userIdParam );

	DataParameterBase *batchCountParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	batchCountParam->setLong( batchCount );
	params.push_back( batchCountParam );
	
	DataParameterBase *amountParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	amountParam->setDimension( batchAmount.length() );
	amountParam->setString( batchAmount );
	params.push_back( amountParam );
	
	DataParameterBase *serviceIdParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	serviceIdParam->setLong( serviceId );
	params.push_back( serviceIdParam );

	DataParameterBase *rpParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	rpParam->setDimension( routingPoint.length() );
	rpParam->setString( routingPoint );
	params.push_back( rpParam );

	DataParameterBase *batchUidParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	batchUidParam->setDimension( batchUID.length() );
	batchUidParam->setString( batchUID );
	params.push_back( batchUidParam );

	DataParameterBase *batchStatusParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE, DataParameterBase::PARAM_OUT );
	batchStatusParam->setLong( -1 );
	params.push_back( batchStatusParam );
	
	DataParameterBase *comBatchIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE, DataParameterBase::PARAM_OUT );
	comBatchIdParam->setDimension( 35 );
	params.push_back( comBatchIdParam );

	data->ExecuteNonQueryCached( DataCommand::SP, "GetBatchStatus", params );

	comBatchId = StringUtil::Trim( comBatchIdParam->getString() );
	return ( BatchManagerBase::BATCH_STATUS )batchStatusParam->getLong();
}

string RoutingDbOp::GetBatchType( const string& batchId, const string& tableName, const string& sender )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Query batch type for [" << batchId << "]" );	

	ParametersVector params;
	DataParameterBase *batchIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	batchIdParam->setDimension( batchId.length() );
	batchIdParam->setString( batchId );
	params.push_back( batchIdParam );
	
	DataParameterBase *tableNameParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	tableNameParam->setDimension( tableName.length() );
	tableNameParam->setString( tableName );
	params.push_back( tableNameParam );

	DataParameterBase *senderParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	senderParam->setDimension( sender.length() );
	senderParam->setString( sender );
	params.push_back( senderParam );

	DataParameterBase *batchTypeParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE, DataParameterBase::PARAM_OUT );
	batchTypeParam->setDimension( 50 );
	params.push_back( batchTypeParam );

	data->ExecuteNonQueryCached( DataCommand::SP, "GetBatchType", params );
	if ( batchTypeParam->isNULLValue() )
		throw RoutingExceptionAggregationFailure( "BATCHID", batchId );

	return StringUtil::Trim( batchTypeParam->getString() );
}

void RoutingDbOp::UpdateBatchCode( const string& batchId, const string& code )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Updating batch code for batch [" << batchId << "] to [" << code << "]" );	

	ParametersVector params;
	DataParameterBase *batchIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	batchIdParam->setDimension( batchId.length() );
	batchIdParam->setString( batchId );
	params.push_back( batchIdParam );

	DataParameterBase *codeParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	codeParam->setDimension( code.length() );
	codeParam->setString( code );
	params.push_back( codeParam );

	data->ExecuteNonQueryCached( DataCommand::SP, "UpdateBatchCode", params );
}

void RoutingDbOp::InsertIncomingBatch( const string& batchId, const string& messageId, const string& messageNamespace )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Inserting incoming batch [" << batchId << "]" );	
	
	ParametersVector params;
	DataParameterBase *batchIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	batchIdParam->setDimension( batchId.length() );
	batchIdParam->setString( batchId );
	params.push_back( batchIdParam );

	DataParameterBase *messageIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	messageIdParam->setDimension( messageId.length() );
	messageIdParam->setString( messageId );
	params.push_back( messageIdParam );

	DataParameterBase *messageNspParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	messageNspParam->setDimension( messageNamespace.length() );
	messageNspParam->setString( messageNamespace );
	params.push_back( messageNspParam );

	data->ExecuteNonQueryCached( DataCommand::SP, "InsertIncomingBatch", params );
}

string RoutingDbOp::GetOriginalRef( const string& reference, const string& batchId )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Obtaining original ref for trn [" << reference << "] and batch [" << batchId << "]" );	
	
	ParametersVector params;
	DataParameterBase *referenceParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	referenceParam->setDimension( reference.length() );
	referenceParam->setString( reference );
	params.push_back( referenceParam );

	DataParameterBase *batchIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	batchIdParam->setDimension( batchId.length() );
	batchIdParam->setString( batchId );
	params.push_back( batchIdParam );

	DataParameterBase *orgRefParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE, DataParameterBase::PARAM_OUT );
	orgRefParam->setDimension( 16 );
	params.push_back( orgRefParam );

	data->ExecuteNonQueryCached( DataCommand::SP, "GetOriginalRef", params );
	string orgRef = StringUtil::Trim( orgRefParam->getString() );
	return orgRef;
}

void RoutingDbOp::TerminateRapidBatch( const string& batchId, const int userId, const string& tableName, const string& responder )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Terminating rapid batch [" << batchId << "]" );	

	ParametersVector params;
	DataParameterBase *batchIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	batchIdParam->setDimension( batchId.length() );
	batchIdParam->setString( batchId );
	params.push_back( batchIdParam );

	DataParameterBase *userIdParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	userIdParam->setInt( userId );
	params.push_back( userIdParam );

	DataParameterBase *responderParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	responderParam->setDimension( responder.length() );
	responderParam->setString( responder );
	params.push_back( responderParam );

	DataParameterBase *tableNameParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	tableNameParam->setDimension( tableName.length() );
	tableNameParam->setString( tableName );
	params.push_back( tableNameParam );
	

	try
	{
		data->ExecuteNonQuery( DataCommand::SP, "TerminateRBatch", params );
	}
	catch( ... )
	{
		// delete from temp batchjobs gives a warning when there are no temp jobs
		TRACE( "An error occured in TerminateBatch. This may be expected if the first message failed." );
	}
}

void RoutingDbOp::TerminateBatch( const string& batchId, const string& batchType, const BatchManagerBase::BATCH_STATUS status, const string& reason )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Terminating batch [" << batchId << "]" );	

	ParametersVector params;
	DataParameterBase *batchIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	batchIdParam->setDimension( batchId.length() );
	batchIdParam->setString( batchId );
	params.push_back( batchIdParam );

	DataParameterBase *batchTypeParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	batchTypeParam->setDimension( batchType.length() );
	batchTypeParam->setString( batchType );
	params.push_back( batchTypeParam );

	DataParameterBase *batchStatusParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	batchStatusParam->setLong( ( long )status );
	params.push_back( batchStatusParam );

	DataParameterBase *reasonParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	reasonParam->setDimension( reason.length() );
	reasonParam->setString( reason );
	params.push_back( reasonParam );

	try
	{
		data->ExecuteNonQuery( DataCommand::SP, "TerminateBatch", params );
	}
	catch( ... )
	{
		// delete from temp batchjobs gives a warning when there are no temp jobs
		TRACE( "An error occured in TerminateBatch. This may be expected if the first message failed." );
	}
}

/*
DataSet* RoutingDbOp::GetBatchJobs( const string batchId )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Query batch status for [" << batchId << "]" );	

	ParametersVector params;
	DataParameterBase *batchIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	batchIdParam->setDimension( batchId.length() );
	batchIdParam->setString( batchId );
	params.push_back( batchIdParam );

	return data->ExecuteQuery( DataCommand::SP, "GetBatchJobs", params );
}*/

// COT
DataSet* RoutingDbOp::GetCOTMarkers()
{
	Database* config = getConfig();
	
	DEBUG_GLOBAL( "Getting COT markers ... " );
	DataSet* result = NULL;
	
	try
	{
		config->BeginTransaction( true );
		result = config->ExecuteQueryCached( DataCommand::SP, "GetTimeLimits" );
	}
	catch( ... )
	{
		config->EndTransaction( TransactionType::ROLLBACK );
		if ( result != NULL )
		{
			delete result;
			result = NULL;
		}
		throw;
	}
	config->EndTransaction( TransactionType::COMMIT );
	return result;
}

// Routing rules
string RoutingDbOp::GetActiveRoutingSchemaName()
{
	Database* config = getConfig();
	DataSet* result = NULL;
	string retVal = "";
	
	try
	{
		result = config->ExecuteQuery( DataCommand::SP, "GetActiveSchema" );
	
		if ( ( result == NULL ) || ( result->size() == 0 ) )
			throw runtime_error( "Unable to obtain default schema name from the DB" );

		retVal = result->getCellValue( 0, "NAME" )->getString();
		DEBUG_GLOBAL( "Schema name : ["  << retVal << "]" );
		
		// release resources
		if( result != NULL )
			delete result;
	}	
	catch( ... )
	{
		// release resources
		if( result != NULL )
			delete result;
		throw;
	}
	
	return retVal;
}

DataSet* RoutingDbOp::GetActiveRoutingSchemas()
{
	Database* config = getConfig();
	DataSet* result = NULL;
	
	//read this machine time and pass to oracle 
	string currentTime = TimeUtil::Get( "%H:%M:%S", 8 );
	DEBUG( "Current time used to get active schemas : [" << currentTime << "]" )
	
	ParametersVector params;
	DataParameterBase *timeParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	timeParam->setDimension( currentTime.length() );
	timeParam->setString( currentTime );
	params.push_back( timeParam );
	
	try
	{
		config->BeginTransaction( true );
		result = config->ExecuteQuery( DataCommand::SP, "GetActiveSchemas", params );
	}
	catch( ... )
	{
		config->EndTransaction( TransactionType::ROLLBACK );
		if ( result != NULL )
		{
			delete result;
			result = NULL;
		}
		throw;
	}
	config->EndTransaction( TransactionType::COMMIT );
	return result;
}

DataSet* RoutingDbOp::ReadRoutingRules( const string& schemaName )
{
	Database* config = getConfig();

	DEBUG_GLOBAL( "Getting routing rules for schema : "  << schemaName );	
	
	ParametersVector params;
	DataParameterBase *schemaParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	schemaParam->setDimension( schemaName.length() );
	schemaParam->setString( schemaName );
	
	params.push_back( schemaParam );
	
	DataSet* result = NULL;
	try
	{
		config->BeginTransaction( true );
		result = config->ExecuteQuery( DataCommand::SP, "GetRulesForSchema", params );
	}
	catch( ... )
	{
		config->EndTransaction( TransactionType::ROLLBACK );
		if ( result != NULL )
		{
			delete result;
			result = NULL;
		}
		throw;
	}
	config->EndTransaction( TransactionType::COMMIT );

	DEBUG_GLOBAL( "[" << result->size() << "] rules in schema [" << schemaName << "]" );
	
	return result;
}

DataSet* RoutingDbOp::ReadRoutingRule( const long ruleId )
{
	Database* config = getConfig();

	DEBUG_GLOBAL( "Getting routing rule : "  << ruleId );	
	
	ParametersVector params;
	DataParameterBase *ruleidParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	ruleidParam->setLong( ruleId );
	
	params.push_back( ruleidParam );
	
	DataSet* result = NULL;
	try
	{
		config->BeginTransaction( true );
		result = config->ExecuteQuery( DataCommand::SP, "GetRule", params );
	}
	catch( ... )
	{
		config->EndTransaction( TransactionType::ROLLBACK );
		if ( result != NULL )
		{
			delete result;
			result = NULL;
		}
		throw;
	}
	config->EndTransaction( TransactionType::COMMIT );

	return result;
}

//Keywords
DataSet* RoutingDbOp::GetKeywordMappings()
{
	Database* config = getConfig();

	DEBUG_GLOBAL( "Getting keyword mappings" )
	DataSet* result = NULL;
	
	try
	{
		config->BeginTransaction( true );
		ParametersVector tempParams;
		result = config->ExecuteQuery( DataCommand::SP, "GetKeywordMappings", tempParams );
	}
	catch( ... )
	{
		config->EndTransaction( TransactionType::ROLLBACK );
		if ( result != NULL )
		{
			delete result;
			result = NULL;
		}
		throw;
	}
	config->EndTransaction( TransactionType::COMMIT );
	
	return result;
}

DataSet* RoutingDbOp::GetKeywords()
{
	Database* config = getConfig();

	DEBUG_GLOBAL( "Getting keywords" )
	DataSet* result = NULL;

	try
	{
		config->BeginTransaction( true );
		result = config->ExecuteQuery( DataCommand::SP, "GetKeywords" );
	}
	catch( ... )
	{
		config->EndTransaction( TransactionType::ROLLBACK );
		if ( result != NULL )
		{
			delete result;
			result = NULL;
		}
		throw;
	}
	config->EndTransaction( TransactionType::COMMIT );
	
	return result;
}

// service
// returns a list of ids 
map< string, bool > RoutingDbOp::GetDuplicateServices()
{
	map< string, bool > duplicateChecks;
	Database* config = getConfig();

	DEBUG_GLOBAL( "Getting services that have a duplicate check set..." )
	DataSet* result = NULL;

	try
	{
		config->BeginTransaction( true );
		result = config->ExecuteQuery( DataCommand::SP, "GetDuplicateSettings" );

		for ( unsigned int i=0; i<result->size(); i++ )
		{
			duplicateChecks.insert( pair< string, bool >( 
				StringUtil::ToString( result->getCellValue( i, "NAME" )->getString() ),
				( result->getCellValue( i, "DUPLICATECHECK" )->getInt() == 0 ) ? false : true ) );
		}
		config->EndTransaction( TransactionType::COMMIT );
	}
	catch( const std::exception& ex )
	{
		config->EndTransaction( TransactionType::ROLLBACK );
		TRACE( "There was an error obtaining the duplicate settings for connectors [" << ex.what() << ". No duplicate checks will be performed." );
	}
	catch( ... )
	{
		config->EndTransaction( TransactionType::ROLLBACK );
		TRACE( "There was an error obtaining the duplicate settings for connectors. No duplicate checks will be performed." );
	}	
	if ( result != NULL )
	{
		delete result;
		result = NULL;
	}
	
	return duplicateChecks;
}

int RoutingDbOp::GetDuplicates( const string& serviceId, const string& messageId )
{
	DEBUG_GLOBAL( "Getting the number of messages with the same hash for service [" << serviceId << "]" );
	
	Database* data = getData();
	
	ParametersVector params;

	DataParameterBase *serviceIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	serviceIdParam->setDimension( serviceId.length() );
	serviceIdParam->setString( serviceId );
	serviceIdParam->setName( "connectorId" );
	params.push_back( serviceIdParam );

	DataParameterBase *messageIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	messageIdParam->setDimension( messageId.length() );
	messageIdParam->setString( messageId );
	messageIdParam->setName( "messageId" );
	params.push_back( messageIdParam );
	
	DataParameterBase *countParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE, DataParameterBase::PARAM_OUT );
	countParam->setName( "hashCount" );
	countParam->setLong( 0 );
	params.push_back( countParam );
	
	data->ExecuteNonQuery( DataCommand::SP, "GetHash", params );
	
	return countParam->getInt();
}

void RoutingDbOp::PurgeHashes( const int hours )
{
	DEBUG_GLOBAL( "Purging hashes older that [" << hours << "] hours" );
	
	Database* data = getData();
	
	ParametersVector params;

	DataParameterBase *hoursParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	hoursParam->setInt( hours );
	hoursParam->setName( "hours" );
	params.push_back( hoursParam );

	data->BeginTransaction();
	try
	{
		data->ExecuteNonQueryCached( DataCommand::SP, "PurgeHashes", params );
	}
	catch( ... )
	{
		data->EndTransaction( TransactionType::ROLLBACK );
		throw;
	}

	data->EndTransaction( TransactionType::COMMIT );
}

// queue functions
string RoutingDbOp::GetDuplicateQueue( const string& senderApp )
{
	// just make sure we throw the same error if the queue cache is not initialized
	( void )GetDuplicateQueueCache(); // this will throw if the cache is empty
	
	map< string, string >::const_iterator queueFinder = m_DuplicateQueueCache->find( senderApp );
	if ( queueFinder != m_DuplicateQueueCache->end() )
	{
		DEBUG_GLOBAL( "Reply queue name ( from cache ) : [" << queueFinder->second << "]" );
		return queueFinder->second;
	}

	DEBUG( "Duplicate queue for application [" << senderApp << "] was not found." );
	return "";
}

// queue functions
string RoutingDbOp::GetReplyQueue( const string& senderApp )
{
	// just make sure we throw the same error if the queue cache is not initialized
	( void )GetReplyQueueCache(); // this will throw if the cache is empty
	
	map< string, string >::const_iterator queueFinder = m_ReplyQueueCache->find( senderApp );
	if ( queueFinder != m_ReplyQueueCache->end() )
	{
		DEBUG_GLOBAL( "Reply queue name ( from cache ) : [" << queueFinder->second << "]" );
		return queueFinder->second;
	}
	else
	{
		stringstream errorMessage;
		errorMessage << "Reply queue for application [" << senderApp << "] was not found.";
		throw runtime_error( errorMessage.str() );
	}
}

string RoutingDbOp::GetDuplicateReplyQueue( const string& senderApp )
{
	// just make sure we throw the same error if the queue cache is not initialized
	( void )GetDuplicateReplyQueueCache(); // this will throw if the cache is empty
	
	map< string, string >::const_iterator queueFinder = m_DuplicateReplyQueueCache->find( senderApp );
	if ( queueFinder != m_DuplicateReplyQueueCache->end() )
	{
		DEBUG_GLOBAL( "Reply queue name ( from cache ) : [" << queueFinder->second << "]" );
		return queueFinder->second;
	}

	DEBUG( "Duplicate replies queue for application [" << senderApp << "] was not found." );
	return "";
}

string RoutingDbOp::GetDelayedReplyQueue( const string& senderApp )
{
	// just make sure we throw the same error if the queue cache is not initialized
	( void )GetDelayedReplyQueueCache(); // this will throw if the cache is empty
	
	map< string, string >::const_iterator queueFinder = m_DelayedReplyQueueCache->find( senderApp );
	if ( queueFinder != m_DelayedReplyQueueCache->end() )
	{
		DEBUG_GLOBAL( "Delayed reply queue name ( from cache ) : [" << queueFinder->second << "]" );
		return queueFinder->second;
	}

	DEBUG( "Delayed replies queue for application [" << senderApp << "] was not found." );
	return "";
}

void RoutingDbOp::GetQueues()
{
	if ( m_QueueIdCache != NULL )
	{
		delete m_QueueIdCache;
		m_QueueIdCache = NULL;
	}

	if ( m_ReplyQueueCache != NULL )
	{
		delete m_ReplyQueueCache;
		m_ReplyQueueCache = NULL;
	}
	
	if ( m_DuplicateQueueCache != NULL )
	{
		delete m_DuplicateQueueCache;
		m_DuplicateQueueCache = NULL;
	}
	
	if ( m_DuplicateReplyQueueCache != NULL )
	{
		delete m_DuplicateReplyQueueCache;
		m_DuplicateReplyQueueCache = NULL;
	}

	if ( m_DelayedReplyQueueCache != NULL )
	{
		delete m_DelayedReplyQueueCache;
		m_DelayedReplyQueueCache = NULL;
	}
	
	DEBUG_GLOBAL( "Getting queues / reply / duplicate queues..." )	
	m_QueueIdCache = new map< long, RoutingQueue >();
	m_ReplyQueueCache = new map< string, string >();
	m_DuplicateQueueCache = new map< string, string >();
	m_DuplicateReplyQueueCache = new map< string, string >();
	m_DelayedReplyQueueCache = new map< string, string >();

	Database* config = getConfig();		
	DataSet* result = NULL;
	
	string queueName = "", queueEP = "", serviceName = "", duplicateQueue = "", duplicateReplyQueue = "", delayedReplyQueue = "";
	long serviceId;
	
	try
	{
		config->BeginTransaction( true );

		result = config->ExecuteQuery( DataCommand::SP, "GetQueues" );
		if ( result == NULL )
			throw runtime_error( "Unable to obtain queue definitions from the database [unable to get queue definitions from database]" );

		if ( result->size() == 0 )
			throw runtime_error( "Unable to obtain queue definitions from the database [no queue definitions found in database]" );
			
		for ( unsigned int i=0; i<result->size(); i++ )
		{
			long queueId = result->getCellValue( i, "ID" )->getLong();
			queueName = StringUtil::Trim( result->getCellValue( i, "NAME" )->getString() );
			queueEP = StringUtil::Trim( result->getCellValue( i, "EXITPOINT" )->getString() );
			serviceName = StringUtil::Trim( result->getCellValue( i, "SERVICENAME" )->getString() );
			serviceId = result->getCellValue( i, "SERVICEMAPID" )->getLong();

			// compatibility check ( don't fail if the getqueues doesn't return duplicateq )
			try
			{
				duplicateQueue = StringUtil::Trim( result->getCellValue( i, "DUPLICATEQUEUE" )->getString() );
			}
			catch( ... )
			{
				duplicateQueue = "";
				TRACE( "GETQUEUES doesn't return the duplicateq setting ... obsolete SP used" );
			}
			long holdStatus = result->getCellValue( i, "HOLDSTATUS" )->getLong();
			
			//keep compatibility with obsolet service map implementation
			DataColumnBase* workColumn = NULL;
			try
			{
				workColumn = result->getCellValue( i, "DUPLICATENOTIFICATIONQUEUE" );
				if( workColumn != NULL )
					duplicateReplyQueue = StringUtil::Trim( workColumn->getString() );
			}
			catch( ... )
			{
				TRACE( "GETQUEUES doesn't return the DUPLICATENOTIFQ setting ... obsolete \"Service Map\" implementation" );
			}

			try
			{
				workColumn = result->getCellValue( i, "DELAYEDNOTIFICATIONQUEUE" );
				if( workColumn != NULL )
					delayedReplyQueue = StringUtil::Trim( workColumn->getString() );
			}
			catch( ... )
			{
				TRACE( "GETQUEUES doesn't return the DELAYEDNOTIFQ setting ... obsolete \"Service Map\" implementation" );
			}

			//add to cache
			DEBUG_GLOBAL( "Adding queue [" << queueName << "] ( " << queueId << ", " << queueEP << ", " << serviceName << ", " << holdStatus << " ) to cache ..." );
			( void )m_QueueIdCache->insert( pair< long, RoutingQueue >( queueId, RoutingQueue( queueId, queueName, serviceName, serviceId, queueEP, holdStatus ) ) );

			if ( serviceName.length() > 0 )
			{
				if ( m_ReplyQueueCache->find( serviceName ) != m_ReplyQueueCache->end() )
				{
					TRACE( "Service [" << serviceName << "] has more than one reply queues! : [" << queueName << "]. Using previous value [" << ( *m_ReplyQueueCache )[ serviceName ] << "]" );
				}
				else
				{
					DEBUG_GLOBAL( "Adding reply queue [" << queueName << "] for service [" << serviceName << "]" );
					( void )m_ReplyQueueCache->insert( pair< string, string >( serviceName, queueName ) );
				}

				if ( m_DuplicateQueueCache->find( serviceName ) == m_DuplicateQueueCache->end() )
				{
					DEBUG_GLOBAL( "Adding duplicate queue [" << duplicateQueue << "] for service [" << serviceName << "]" );
					( void )m_DuplicateQueueCache->insert( pair< string, string >( serviceName, duplicateQueue ) );
				}

				if ( m_DuplicateReplyQueueCache->find( serviceName ) == m_DuplicateReplyQueueCache->end() )
				{
					DEBUG_GLOBAL( "Adding duplicate reply queue [" << duplicateReplyQueue << "] for service [" << serviceName << "]" );
					( void )m_DuplicateReplyQueueCache->insert( pair< string, string >( serviceName, duplicateReplyQueue ) );
			}

				if ( m_DelayedReplyQueueCache->find( serviceName ) == m_DelayedReplyQueueCache->end() )
				{
					DEBUG_GLOBAL( "Adding delayed reply queue [" << delayedReplyQueue << "] for service [" << serviceName << "]" );
					( void )m_DelayedReplyQueueCache->insert( pair< string, string >( serviceName, delayedReplyQueue ) );
				}
			}
		}
			
		if( result != NULL )
			delete result;
	}
	catch( ... )
	{
		config->EndTransaction( TransactionType::ROLLBACK );

		TRACE_GLOBAL( "Error in queue ["  << queueName << "] definition." );
		if ( result != NULL )
		{
			delete result;
			result = NULL;
		}
		
		throw;
	}
	
	config->EndTransaction( TransactionType::COMMIT );

	DEBUG_GLOBAL( "Queue cache created" );
}

RoutingQueue& RoutingDbOp::GetQueue( const long queueId )
{
	// just make sure we throw the same error if the queue cache is not initialized
	( void )GetQueueCache(); // this will throw if the cache is empty
	return ( *m_QueueIdCache )[ queueId ];
}

RoutingQueue& RoutingDbOp::GetQueue( const string& queueName )
{
	long queueId = GetQueueId( queueName );
	return ( *m_QueueIdCache )[ queueId ];
}

string RoutingDbOp::GetQueueName( const long queueId )
{
	// just make sure we throw the same error if the queue cache is not initialized
	( void )GetQueueCache(); // this will throw if the cache is empty
		
	// try to return from cache
	if ( m_QueueIdCache->find( queueId ) != m_QueueIdCache->end() )
	{
		string cachedQueue = ( *m_QueueIdCache )[ queueId ].getName();
		DEBUG2( "Cached value found : [" << cachedQueue << "]" );
		return cachedQueue;
	}
	
	// no queue id for this queue name
	stringstream errorMessage;
	errorMessage << "Could not find a queue with id [" << queueId << "]";
	throw logic_error( errorMessage.str() );	
}

long RoutingDbOp::GetQueueId( const string& queueName )
{	
	// just make sure we throw the same error if the queue cache is not initialized
	( void )GetQueueCache(); // this will throw if the cache is empty

	DEBUG2( "Getting queueId for queue [" << queueName << "]" );
	map< long, RoutingQueue >::const_iterator queueFinder = m_QueueIdCache->begin();
	while ( queueFinder != m_QueueIdCache->end() )
	{
		if( queueFinder->second.getName() == queueName )
		{
			DEBUG_GLOBAL( "Cached value found for queue [" << queueName << "] = " << queueFinder->second.getId() );
			return queueFinder->second.getId();
		}
		
		queueFinder++;
	}
	
	// no queue id for this queue name
	stringstream errorMessage;
	errorMessage << "Could not find a queue with name [" << queueName << "]";
	throw logic_error( errorMessage.str() );	
}

void RoutingDbOp::ChangeQueueHoldStatus( const string& queueName, const bool holdStatus )
{
	RoutingQueue queue = GetQueue( queueName );
	
	DEBUG_GLOBAL( "Changing queue [" << queueName << "] hold status to " << holdStatus );
	if( queue.getHeld() == holdStatus )
	{
		DEBUG_GLOBAL( "Same status as previous... skipping action" );
		return;
	}
	
	map< long, RoutingQueue >::iterator queueFinder = m_QueueIdCache->begin();
	while ( queueFinder != m_QueueIdCache->end() )
	{
		if( queueFinder->second.getName() == queueName )
		{
			queueFinder->second.setHeld( holdStatus );
			break;
		}
		queueFinder++;
	}
	
	Database* config = getConfig();
	
	ParametersVector params;
	
	DataParameterBase *crtQueueParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	crtQueueParam->setLong( queue.getId() );
	params.push_back( crtQueueParam );
	
	DataParameterBase *statusParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	statusParam->setLong( holdStatus ? 1 : 0 );
	params.push_back( statusParam );
	
	try
	{
		config->BeginTransaction();
		config->ExecuteNonQuery( DataCommand::SP, "ChangeQueueStatus", params );
	}
	catch( ... )
	{
		config->EndTransaction( TransactionType::ROLLBACK );
		throw;
	}
	config->EndTransaction( TransactionType::COMMIT );
	
	// if the queue was held, but now it was released, resume jobs
	if ( !holdStatus )
	{
		DEBUG_GLOBAL( "Resuming jobs for queue [" << queueName << "] ..." );
		ResumeJobs( queue.getId() );
	}
	
	DEBUG_GLOBAL( "Queue [" << queueName << "] is now " << ( GetQueue( queueName ).getHeld() ? "held" : "released" ) );
}

unsigned int RoutingDbOp::GetNextSequence( const long serviceId )
{
	DEBUG_GLOBAL( "Getting next sequence for service [" << serviceId << "]" );
	
	Database* data = getData();
	
	ParametersVector params;
	DataParameterBase *serviceIdParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	serviceIdParam->setLong( serviceId );
	serviceIdParam->setName( "serviceId" );
	params.push_back( serviceIdParam );
	
	DataParameterBase *seqParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE, DataParameterBase::PARAM_OUT );
	seqParam->setName( "sequence" );
	seqParam->setLong( 0 );
	params.push_back( seqParam );
	
	data->ExecuteNonQuery( DataCommand::SP, "GetNextServiceSequence", params );
	
	return seqParam->getInt();
}

// call runtime procedures
// SP signature : procName( messageId in varchar2, result out number )
bool RoutingDbOp::CallBoolProcedure( const string& procName, const string& messageId, const string& messageTable )
{
	DEBUG_GLOBAL( "Calling boolean SP [" << procName << "]" );
	
	Database* data = getData();
	
	ParametersVector params;
	DataParameterBase *messageIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	messageIdParam->setDimension( messageId.length() );
	messageIdParam->setString( messageId );
	params.push_back( messageIdParam );

	stringstream msgTable;
	msgTable << m_DataConnectionString.getUserName() << '.' << messageTable;
	
	DataParameterBase *tableParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	tableParam->setDimension( msgTable.str().length() );
	tableParam->setString( msgTable.str() );
	params.push_back( tableParam );

	DataParameterBase *resultParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE, DataParameterBase::PARAM_OUT );
	resultParam->setName( "result" );
	resultParam->setLong( 0 );
	params.push_back( resultParam );
	
	data->ExecuteNonQuery( DataCommand::SP, procName, params );
	
	return ( resultParam->getInt() == 0 );
}

DataSet* RoutingDbOp::GetDelayedMessages( const string& tableName, const string& tokenId )
{
	DEBUG( "Getting all delayed messages in table " << tableName << " for delayedId = [" << tokenId << "]" );
	Database* data = getData();

	ParametersVector params;
	DataParameterBase *tableNameParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	tableNameParam->setDimension( tableName.length() );
	tableNameParam->setString( tableName );
	
	params.push_back( tableNameParam );
	
	DataParameterBase *tokenIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	tokenIdParam->setDimension( tokenId.length() );
	tokenIdParam->setString( tokenId );
	
	params.push_back( tokenIdParam );

	DataSet* result = NULL;
	
	// build sp name
	stringstream spName;
	spName << "Get" << tableName << "Messages";

	try
	{
		result = data->ExecuteQueryCached( DataCommand::SP, spName.str(), params );
	}
	catch( ... )
	{
		if ( result != NULL )
		{
			delete result;
			result = NULL;
		}
		throw;
	}	
	return result;
}

void RoutingDbOp::UpdateDelayedId( const string& tableName, const string& messageId, const string& delayedId )
{
	Database* data = getData();

	DEBUG_GLOBAL( "Updating delayed notification [" << messageId << "] in queue [" << tableName << "]" );	
	
	ParametersVector params;
	DataParameterBase *messageIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	messageIdParam->setDimension( messageId.length() );
	messageIdParam->setString( messageId );
	params.push_back( messageIdParam );
	
	DataParameterBase *delayedIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	delayedIdParam->setDimension( delayedId.length() );
	delayedIdParam->setString( delayedId );
	params.push_back( delayedIdParam );

	// build sp name
	stringstream spName;
	spName << "UpdateID" << tableName;
	
	data->ExecuteNonQueryCached( DataCommand::SP, spName.str(), params );
}
