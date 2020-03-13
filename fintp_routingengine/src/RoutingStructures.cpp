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

#include "Base64.h"
#include "StringUtil.h"
#include "Trace.h"
#include "LogManager.h"

#include "RoutingStructures.h"
#include "RoutingDbOp.h"
#include "RoutingActions.h"
#include "RoutingAggregationManager.h"
#include "RoutingEngine.h"
#include "RoutingActions.h"

//RoutingExitpoint implementation
RoutingExitpoint::RoutingExitpoint( const string& serviceName, long serviceId, const string& exitpointDef ) : RoutingState(),
	m_Queue( "" ), m_QueueManager( "" ), m_TransportURI( "" ), m_HelperType( TransportHelper::NONE ), m_ReplyToQueue( "" ), m_BatchConfig( "" ),
	m_ReplyOptions(), m_MessageOptions( RoutingMessageOptions::MO_NONE ), m_ServiceId( serviceId ),
	m_ServiceName( serviceName ),  m_MessageTransform( "" ), m_MessageTrailer( "" ), m_ValidationSchema( "" ), 
	m_ValidationSchemaNamespace( "" ), m_Definition( exitpointDef ), m_MessageSkipHeaderLength( 0 ), 
	m_TransportHelper( NULL ), m_TransformFilter( NULL ), m_BatchManager( NULL ) 
{
	parseExitpointDefinition( exitpointDef );
}

void RoutingExitpoint::parseExitpointDefinition( const string& exitpointDef )
{
	string exitpointDefinition = exitpointDef;
	if ( exitpointDefinition.length() == 0 )
		return;
	
	DEBUG( "EP Definition : [" << exitpointDefinition << "]" );
		
	// equiv regex for definition : 
	// Q=(?<queue>[^\s|;]+);QM=(?<queueManager>[^\s|;]*)(?:;RTQ=(?<replyQ>[^\s|;]+);RO=(?<replyOpt>.+)){0,1}
	StringUtil myDefinition( exitpointDefinition );
	myDefinition.Split( ";" );

	bool gotQueue = false, gotQM = false, gotHelperType = false;
	
	// interpret each split
	string crtValue = myDefinition.NextToken();
	while ( crtValue.length() > 0 )
	{
		DEBUG( "Current token : ["  << crtValue << "]" );
		// queue name following
		if( crtValue.find( "MQT=" ) == 0 )
		{
			gotHelperType = true;
			m_HelperType = TransportHelper::parseTransportType( crtValue.substr( 4 ) );
		}
		if( crtValue.find( "Q=" ) == 0 )
		{
			gotQueue = true;
			m_Queue = crtValue.substr( 2 );
		}
		else if ( crtValue.find( "CH=" ) == 0 )
		{
			gotQueue = true;
			m_TransportURI = crtValue.substr( 3 );
		}
		// queue manager name following
		else if( crtValue.find( "QM=" ) == 0 )
		{
			gotQM = true;
			m_QueueManager = crtValue.substr( 3 );
		}
		// reply to queue following
		else if( crtValue.find( "RTQ=" ) == 0 )
		{
			m_ReplyToQueue = crtValue.substr( 4 );
		}
		// reply options following
		else if( crtValue.find( "RO=" ) == 0 )
		{
			m_ReplyOptions = TransportReplyOptions( crtValue.substr( 3 ) );
		}
		// reply options following
		else if( crtValue.find( "MO=" ) == 0 )
		{
			m_MessageOptions = RoutingMessageOptions::Parse( crtValue.substr( 3 ) );
			DEBUG( "Message options for this queue are [" << RoutingMessageOptions::ToString( m_MessageOptions ) << "]" );
		}
		// message header transform ( xslt to apply to the message to create a header )
		else if( crtValue.find( "MHT=" ) == 0 )
		{
			m_MessageTransform = crtValue.substr( 4 );
			DEBUG( "Message transform for this queue is [" << m_MessageTransform << "]" );
		}
		// message trailer character ( !!! banktrade specific )
		else if( crtValue.find( "MTC=" ) == 0 )
		{
			string messageTrailer = crtValue.substr( 4 );
			m_MessageTrailer = string( 1, StringUtil::ParseInt( messageTrailer ) );
			DEBUG( "Message trailer for this queue is [\\" << messageTrailer << "]" );
		}
		// message header length ( how many chars to skip in the message to find the payload )
		else if( crtValue.find( "MHL=" ) == 0 )
		{
			m_MessageSkipHeaderLength = StringUtil::ParseInt( crtValue.substr( 4 ) );
		}
		else if( crtValue.find( "S=" ) == 0 )
		{
			m_ValidationSchema = crtValue.substr( 2 );
		}
		else if( crtValue.find( "NS=" ) == 0 )
		{
			m_ValidationSchemaNamespace = crtValue.substr( 3 );
		}
		
		crtValue = myDefinition.NextToken();
	}
	
	// required definitions missing ( queue / queue manager ) only if complete is not set
	if( !gotHelperType )
	{
		throw runtime_error( "MQT=\"WMQ | AMQ\" setting is mandatory for exitpoint definition" );
	}
	if( ( !gotQueue || !gotQM ) && ( ( m_MessageOptions & RoutingMessageOptions::MO_COMPLETE ) != RoutingMessageOptions::MO_COMPLETE ) )
	{
		throw runtime_error( "Missing required setting [Q=..., QM=... ] in exitpoint definition" );
	}
}

RoutingExitpoint::~RoutingExitpoint()
{
	try
	{
		if ( m_TransportHelper != NULL )
		{
			delete m_TransportHelper;
			m_TransportHelper = NULL;
		}
	} 
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while deleting the TransportHelper" );
		}catch( ... ){};
	}
		
	try
	{
		if ( m_BatchManager != NULL )
		{
			delete m_BatchManager;
			m_BatchManager = NULL;
		}
	} 
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while deleting the batch manager" );
		}catch( ... ){};
	}

	try
	{
		if ( m_TransformFilter != NULL )
		{
			delete m_TransformFilter;
			m_TransformFilter = NULL;
		}
	} 
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while deleting the transformation filter" );
		}catch( ... ){};
	}
}

string RoutingExitpoint::ProcessMessage( RoutingMessage* message, const int userId, bool bulk, const string& batchType )
{
	//DEBUG( "In process" );
	//RoutingEngine::getMessagePool().Dump();
	bool isMessageBatch = ( ( message->getMessageOptions() & RoutingMessageOptions::MO_BATCH ) == RoutingMessageOptions::MO_BATCH );
	bool isBatch = isMessageBatch || ( ( m_MessageOptions & RoutingMessageOptions::MO_BATCH ) == RoutingMessageOptions::MO_BATCH );
	bool mustInsertBatch = ( ( m_MessageOptions & RoutingMessageOptions::MO_INSERTBATCH ) == RoutingMessageOptions::MO_INSERTBATCH );

	if( mustInsertBatch && !message->isReply() )
	{
		string messageNamespace = "";
		RoutingMessageEvaluator* evaluator = message->getPayloadEvaluator();
		if ( evaluator != NULL )
			messageNamespace = evaluator->getNamespace();
		else
			messageNamespace = RoutingMessageEvaluator::getNamespaceByMessageType( message->getOriginalMessageType() );

		RoutingDbOp::InsertIncomingBatch( message->getBatchId(), message->getMessageId(), messageNamespace );
	}

	BatchManager< BatchMQStorage >* batchManager = NULL;

	if ( isBatch )
	{
		DEBUG( "Using batch manager to process messages" );

		if ( m_BatchManager == NULL )
		{
			DEBUG( "Creating a new batch manager" );
			m_BatchManager = new BatchManager< BatchMQStorage >( BatchManagerBase::MQ, BatchResolution::SYNCHRONOUS );

			m_BatchManager->storage().initialize( m_HelperType );
			m_BatchManager->storage().setQueue( m_Queue );
			m_BatchManager->storage().setQueueManager( m_QueueManager );
			m_BatchManager->storage().setTransportURI( m_TransportURI );		
		}

		batchManager = m_BatchManager;

		batchManager->open( message->getBatchId(), ios_base::out );
	}
	else
	{
		DEBUG( "Using TransportHelper to process messages" );
		if ( m_TransportHelper == NULL )
			m_TransportHelper = TransportHelper::CreateHelper( m_HelperType );
			
		// as client = put	
		m_TransportHelper->connect( m_QueueManager, m_TransportURI );
		m_TransportHelper->openQueue( m_Queue );
	}
	
	//DEBUG( "Before setting responder" );
	//RoutingEngine::getMessagePool().Dump();
	
	message->setResponderService( m_ServiceName );
	bool isReply = message->isReply();

	//DEBUG( "Before update responder" );
	//RoutingEngine::getMessagePool().Dump();
			
	// updating businessmessage responder is not necessary on replies
	//HACK : catch the error instead of dealing with it
	try
	{
		if ( !isReply )
		{
			bool isRapidBatch = ( ( message->getMessageOptions() & RoutingMessageOptions::MO_RBATCH ) == RoutingMessageOptions::MO_RBATCH );
			if ( !isRapidBatch )
				RoutingDbOp::UpdateBusinessMessageResponder( message->getCorrelationId(), m_ServiceName );
			// operation is safe because batchId is generated in FinTP on assemble operation
			// requestType is set as ::Batch only in internalPerformeAssemble()
			if ( message->getRequestType() == RoutingMessage::Batch )
				RoutingDbOp::UpdateBMAssembleResponder( message->getBatchId(), m_ServiceName );
		}
	}
	catch( ... )
	{
		TRACE( "UpdateBusinessMessageReponder failed. This is an expected warning for replies." );
	}
	
	// flat message with payload encoded
	bool bypassMP = ( ( message->getMessageOptions() & RoutingMessageOptions::MO_BYPASSMP ) == RoutingMessageOptions::MO_BYPASSMP );
	if ( !bypassMP && ( ( m_MessageOptions & RoutingMessageOptions::MO_BYPASSMP ) == RoutingMessageOptions::MO_BYPASSMP ) )
		bypassMP = true;

	string messageText = "";
	string messagePayloadHeader = "";

	if ( m_MessageTransform.length() > 0 )
	{
		NameValueCollection trHeaders;

		if ( m_TransformFilter == NULL )
			m_TransformFilter = new XSLTFilter();
		
		trHeaders.Add( XSLTFilter::XSLTFILE, m_MessageTransform );
		trHeaders.Add( XSLTFilter::XSLTUSEEXT, "true" );

		// HACK if we have generateid on the queue, the id will increment by 2 ( if it is transformed )
		unsigned int nextSequence = RoutingDbOp::GetNextSequence( m_ServiceId );

		trHeaders.Add( "XSLTPARAMSEQUENCE", StringUtil::Pad( StringUtil::ToString( nextSequence ), "\'", "\'" ) ); 

		if ( isReply && message->isAck() )
			trHeaders.Add( "XSLTPARAMACK", "\'PAN\'" ); 
		else 
			trHeaders.Add( "XSLTPARAMACK", "\'NAN\'" ); 

		WorkItem< ManagedBuffer > workItem( new ManagedBuffer() );
		m_TransformFilter->ProcessMessage( message->getPayload()->getDocConst(), workItem, trHeaders, true );

		messagePayloadHeader = workItem.get()->str();
	}
	
	if ( bypassMP )
	{
		DEBUG( "Bypassing message processing... [" << RoutingMessageOptions::ToString( message->getMessageOptions() ) << "]" );

		// if we are bypassing message processing, but we have a batch option set,
		// feed the metadata to the batch evaluators and pass the original message as payload
		if ( isBatch )
		{
			RoutingMessageEvaluator* evaluator = message->getPayloadEvaluator();
			string messageType = "";

			if ( ( evaluator != NULL ) && evaluator->isBusinessFormat() )
				messageType = evaluator->getField( InternalXmlPayload::MESSAGETYPE );

			if ( m_BatchManager == NULL )
				throw logic_error( "Batch manager is NULL" );
			*m_BatchManager << BatchManip::BatchMetadata( message->getPayload()->getDocConst(), messageType );
		}

		// embeds the message in an qPCMessageSchema envelope
		if ( m_TransformFilter != NULL )
		{
			string origPayload = message->getOriginalPayload( RoutingMessagePayload::PLAINTEXT );
			if ( m_MessageSkipHeaderLength > 0 )
				origPayload = origPayload.substr( m_MessageSkipHeaderLength );

			//replies already have the header, so replace
			if ( isReply )
			{
				messageText = origPayload.replace( 0, messagePayloadHeader.size(), messagePayloadHeader ) + m_MessageTrailer;
				messageText = message->ToString( Base64::encode( messageText ) );
			}
			else
				messageText = message->ToString( Base64::encode( messagePayloadHeader + origPayload + m_MessageTrailer ) );
		}
		else
		{
			/*string origPayload = message->getOriginalPayload( RoutingMessagePayload::PLAINTEXT );
			origPayload = StringUtil::Replace( origPayload, "RNCBROBU", "SPXAROB0" );
			messageText = message->ToString( Base64::encode( origPayload ) );*/
			messageText = message->ToString( message->getOriginalPayload() );
		}
	}
	else	
	{
		//DEBUG( "Before getting text" );
		//RoutingEngine::getMessagePool().Dump();
	
		// embeds the message in an qPCMessageSchema envelope and Base64-encodes the payload
		messageText = message->ToString( message->getPayload()->getText( RoutingMessagePayload::BASE64 ) );
		
		//DEBUG( "After getting text" );
		//RoutingEngine::getMessagePool().Dump();
	}
	
	// should be set for MT104 messages ( or other batched made in BO )
	bool skipSequence = ( ( m_MessageOptions & RoutingMessageOptions::MO_NOSEQ ) == RoutingMessageOptions::MO_NOSEQ );

	if ( isReply )
	{
		// we could put the reply to mq, but it will break the batch... 
		// TODO allow batch of replies ? is this necessary ?
		if ( isMessageBatch )
			throw runtime_error( "Replies can't be batched/helper NULL" );
		
		if ( m_TransportHelper == NULL )
		{
			m_TransportHelper = TransportHelper::CreateHelper( m_HelperType );
			
			// as client = put	
			m_TransportHelper->connect( m_QueueManager, m_TransportURI );
			m_TransportHelper->openQueue( m_Queue );
		}
		
		bool replyDatagram =  ( ( m_MessageOptions & RoutingMessageOptions::MO_REPLYDATAGRAM ) == RoutingMessageOptions::MO_REPLYDATAGRAM );

		if ( replyDatagram )
			m_TransportHelper->putOne( ( unsigned char* )messageText.data(), messageText.size() );
		else if ( message->isAck() )
			m_TransportHelper->putOneReply( ( unsigned char* )messageText.data(), messageText.size() ); //MQFB_PAN?
		else
			m_TransportHelper->putOneReply( ( unsigned char* )messageText.data(), messageText.size() ); //MQFB_NAN?
	}
	// if reply options set for queue, send a request, else send a datagram/reply
	else
	{
		DEBUG( "The message will be sent as [" << ( isBatch ? "part of batch" : "single message" ) << "] to the WMQ queue" );
		
		// set mq id for request
		// breaking change : if the request type is batch, set batchid as aggregation token
		RoutingAggregationCode request( RoutingMessageEvaluator::AGGREGATIONTOKEN_FTPID, message->getCorrelationId() );
		RoutingAggregationManager::REQUEST_MODE requestMode = RoutingAggregationManager::OptimisticUpdate;

		if ( message->getRequestType() == RoutingMessage::Batch )
		{
			DEBUG( "Using aggregation mode [BATCHID] for batch [" << message->getBatchId() << "] because request type is Batch" );
			request.setCorrelToken( RoutingMessageEvaluator::AGGREGATIONTOKEN_BATCHID );
			request.setCorrelId( message->getBatchId() );

			requestMode = RoutingAggregationManager::OptimisticUpdateTrimmed;
		}
			
		if ( isBatch )
		{
			//DEBUG( "Before batching" );
			//RoutingEngine::getMessagePool().Dump();
			if ( m_BatchManager == NULL )
				throw logic_error( "Batch manager is NULL" );

			if ( !bypassMP )
			{
				unsigned long batchSequence = message->getBatchSequence();

				// if sequence 0 is returned ( not a valid seq ), the message exists to a MO_BATCH exitpoint
				if ( !isMessageBatch && ( batchSequence == 0 ) )
				{
					// send the message to be evaluated ( results batchid/seq/last )
					m_BatchManager->setXPath( "name(node())" );
					*m_BatchManager << BatchManip::BatchMetadata( message->getPayload()->getDocConst() );

					// enqueue actual message 
					// TODO check this
					*m_BatchManager << BatchManip::BatchMetadata( message->getPayload()->getDocConst() );
				}
				else
				{
					string batchId = message->getBatchId();	

					// option to skip appending the sequence to the batch
					//if ( skipSequence ) 
					//	batchId = RoutingEngine::GetReverseMapId( batchId );

					bool isLast = ( batchSequence == message->getBatchTotalCount() );

					BatchItem item( batchSequence, batchId, message->getMessageId(), isLast );
					item.setPayload( messageText );

					*m_BatchManager << item;
					
					//RoutingEngine::UnmapId( batchId );
				}
			}
			else
			{
				*m_BatchManager << messageText;
			}
			
			//DEBUG( "After batching" );
			//RoutingEngine::getMessagePool().Dump();

			if ( batchManager == NULL )
				throw logic_error( "Batch manager is NULL" );
			request.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_MQID, batchManager->getResolution().getItem().getMessageId() );
			request.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_BATCHID, batchManager->getResolution().getItem().getBatchId() );
			request.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_BATCHSEQ, StringUtil::ToString( batchManager->getResolution().getItem().getSequence() ) );
		}
		else
		{
			if ( m_TransportHelper == NULL )
				throw logic_error( "MQ helper is NULL" );
			if ( !m_ReplyOptions.optionsSet() )
				m_TransportHelper->putOne( ( unsigned char* )messageText.data(), messageText.size() );
			else
				m_TransportHelper->putOneRequest( ( unsigned char* )messageText.data(), messageText.size(), m_ReplyToQueue, m_QueueManager, m_ReplyOptions );
			request.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_MQID, m_TransportHelper->getLastMessageId() );
		}
		
		request.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_OSESSION, message->getOutputSession() );

		// optimistic update( we know the message is in there ;) )
		// TODO: maybe updateorfail instead; if we expect the message not beeing there 
		try
		{
			RoutingAggregationManager::AddRequest( request, requestMode );
		}
		catch( const DBNoUpdatesException& ) 
		{
			//if no update performed correlation token was altered on routing and probably didn't care here (e.g Assemble on RID to BORIDOut)
			DEBUG( "Message sent to " << m_Queue << " queue but aggregation for token " << request.getCorrelToken() << " = [" << request.getCorrelId() << "] was not performed" );
		}
	}

	return "";
}

void RoutingExitpoint::Commit( const string& batchId  )
{
	if ( m_TransportHelper != NULL )
	{
		DEBUG( "Commit transport helper" )
		m_TransportHelper->commit();
	}

	if ( m_BatchManager != NULL )
	{
		DEBUG( "Commit batch manager" );
		m_BatchManager->commit();
		m_BatchManager->close( batchId );
	}
}

void RoutingExitpoint::Rollback( const bool isBatch )
{
	if ( !isBatch )
	{
		if ( m_TransportHelper != NULL )
			m_TransportHelper->rollback();
	}
	else
	{
		// HACK : commit he batch at every message instead at every id unmap 
		if ( m_BatchManager != NULL )
			m_BatchManager->rollback();
	}
}

//RoutingQueue implementation
RoutingQueue::RoutingQueue() : RoutingState(), m_Exitpoint( "", -1, "" ), m_QueueId( -1 ), m_QueueName( "dummyQueue" ), 
	m_HoldStatus( true ), m_BulkOperationInProgress( false )
{
}

RoutingQueue::RoutingQueue( const long queueId, const string& queueName, const string& serviceName, const long serviceId, const string& queueEP, const long holdStatus ) : 
	RoutingState(), m_Exitpoint( serviceName, serviceId, queueEP ), m_QueueId( queueId ), m_QueueName( queueName ), 
	m_HoldStatus( true ), m_BulkOperationInProgress( false )
{
	m_HoldStatus = ( holdStatus != 0 );
	DEBUG( "Routing queue : [" << m_QueueName << "]" );
}

string RoutingQueue::ProcessMessage( RoutingMessage* message, const int userId, bool bulk, const string& batchType )
{
	bool isMessageBatch = ( ( message->getMessageOptions() & RoutingMessageOptions::MO_BATCH ) == RoutingMessageOptions::MO_BATCH );
	bool isBatch = isMessageBatch || ( ( m_Exitpoint.getMessageOptions() & RoutingMessageOptions::MO_BATCH ) == RoutingMessageOptions::MO_BATCH );
	bool isReply = message->isReply();
	string batchIssuer = "";

	if ( ( m_Exitpoint.getMessageOptions() & RoutingMessageOptions::MO_COMPLETE ) == RoutingMessageOptions::MO_COMPLETE )
	{
		DEBUG( "Completing message [MO_COMPLETE set on exitpoint]" );
		
		if ( !bulk )
		{
			// get rid of the message
			RoutingDbOp::DeleteRoutingMessage( message->getTableName(), message->getMessageId(), isReply );
		}
		else
		{
			// add aggregation fields

			DataSet* batchItems = NULL;
			try
			{
				DEBUG( "Getting messages in batch [" << message->getBatchId() << "]" );
				
				// get individual messages from db, matching the current batchId
				if( isReply && ( message->getPayloadEvaluator() != NULL ) )
				{
					batchIssuer = message->getPayloadEvaluator()->getIssuer();
					DEBUG( "Batch issuer is [" << batchIssuer << "]" );
				}
				else
				{
					DEBUG( "Unable to check batch issuer [payload evaluator is NULL]" );
				}
				batchItems = RoutingDbOp::GetBatchMessages( message->getBatchId(), isReply, batchIssuer );
				
				if( batchItems != NULL )
				{
					for( unsigned int i=0; i<batchItems->size(); i++ )
					{
						string correlationId = StringUtil::Trim( batchItems->getCellValue( i, "CORRELATIONID" )->getString() );

						// set mq id for request
						RoutingAggregationCode request( RoutingMessageEvaluator::AGGREGATIONTOKEN_FTPID, correlationId );
						
						request.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_BATCHID,  message->getBatchId() );
						//request.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_BATCHSEQ, StringUtil::ToString( batchManager->getResolution().getItem().getSequence() ) );
						request.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_OSESSION, message->getOutputSession() );

						// optimistic update( we know the message is in there ;) )
						RoutingAggregationManager::AddRequest( request, RoutingAggregationManager::OptimisticUpdate );
					}
				}
			}
			catch( const std::exception& ex )
			{
				TRACE( "An error occured during bulk operation [" << ex.what() << "]" );
				if ( batchItems != NULL )
				{
					delete batchItems;
					batchItems = NULL;
				}
				m_Exitpoint.Rollback( isBatch );

				throw;
			}
			catch( ... )
			{
				TRACE( "An error occured during bulk operation [unknown reason]" );
				if ( batchItems != NULL )
				{
					delete batchItems;
					batchItems = NULL;
				}
				m_Exitpoint.Rollback( isBatch );

				throw;
			}

			//DEBUG( "Before commit batch" );
			//RoutingEngine::getMessagePool().Dump();

			if ( batchItems != NULL )
			{
				delete batchItems;
				batchItems = NULL;
			}
			RoutingDbOp::RemoveBatchMessages( message->getBatchId(), message->getTableName() );
		}
		
		return m_QueueName;
	}

	bool bypassMP = ( ( message->getMessageOptions() & RoutingMessageOptions::MO_BYPASSMP ) == RoutingMessageOptions::MO_BYPASSMP );
	bool isRapidBatch = ( ( message->getMessageOptions() & RoutingMessageOptions::MO_RBATCH ) == RoutingMessageOptions::MO_RBATCH );
	string destination = "?";

	// if an EP is defined for this queue, move it 
	if ( m_Exitpoint.isValid() )
	{
		if ( bulk )
		{
			bool noHeaders = ( ( m_Exitpoint.getMessageOptions() & RoutingMessageOptions::MO_NOHEADERS ) == RoutingMessageOptions::MO_NOHEADERS );
			DataSet* batchItems = NULL;
			DataSet::size_type batchItemsSize = 0;

			try
			{
				DEBUG( "Getting messages in batch [" << message->getBatchId() << "]" );
				
				int ddbtReply = 0;
				if( ( message->getPayloadEvaluator() != NULL ) )
				{
					DEBUG( "Payload evaluator not NULL" );
					if ( isReply )
					{
						batchIssuer = message->getPayloadEvaluator()->getIssuer();
						DEBUG( "Batch issuer is [" << batchIssuer << "]" );
					}

					if ( message->getPayloadEvaluator()->isDD() )
					{
						DEBUG( "This reply is for dd, look for dd messages that were not refused only" );
						ddbtReply = RoutingDbOp::DD_Ref_Reply;
					}
					if ( message->getPayloadEvaluator()->isID() )
					{
						DEBUG( "This reply is for id, look for id messages that were not refused only" );
						ddbtReply = RoutingDbOp::DD_Ref_Reply;
					}
					if( message->getPayloadEvaluator()->isBusinessFormat() )
					{
						DEBUG( "This reply is for pacs.008, look for messages ids that were bulk rejected" );
						ddbtReply = RoutingDbOp::Sepa_Sts_Reply;
					}

				}
				else
				{
					DEBUG( "Unable to check reply type/issuer [payload evaluator is NULL]" );
				}	
				
				//DEBUG( "Before getting messages from batch" );
				//RoutingEngine::getMessagePool().Dump();
				
				if ( isRapidBatch )
				{
					// get individual items from memory
					batchItemsSize = RoutingEngine::getRoutingSchema()->GetRBatchItems().size();
				}
				else
				{
					// get individual messages from db, matching the current batchId
						batchItems = RoutingDbOp::GetBatchMessages( message->getBatchId(), isReply, batchIssuer, ddbtReply );
				
					if( ( batchItems == NULL ) || ( batchItems->size() == 0 ) )
					{
						stringstream errorMessage;
						AppException aex;

						if ( isReply )
						{
							errorMessage << "Batch response for batch [" << message->getBatchId() << "] does not match a sent batch";
							aex = RoutingAction::internalPerformMoveToInvestigation( message, errorMessage.str() );
						}
						else
						{
							// this should not happen
							errorMessage << "Batch items were not found for batch [" << message->getBatchId() << "]";
							TRACE( "Teminating batch [" << message->getBatchId() << "]. Reason : " << errorMessage.str() );

							aex = AppException( errorMessage.str() );
						}
						
						throw aex;
					}
					else
					{
						DEBUG( "Performing bulk WMQ put for batch [" << message->getBatchId() << "]. Messages in batch [" << batchItems->size() << "]" );
					}
					batchItemsSize = batchItems->size();
				}
				
				//DEBUG( "Before iterating messages" );
				//RoutingEngine::getMessagePool().Dump();

				for( unsigned int i=0; i<batchItemsSize; i++ )
				{
					m_BulkOperationInProgress = true;
					RoutingMessage batchItem;

					if ( isReply )
					{
						if ( batchItems == NULL )
							throw logic_error( "Batch items allocation failure" );

						// copy batchmessage
						batchItem = *message;

						// replace fields
						string correlationId = StringUtil::Trim( batchItems->getCellValue( i, RoutingMessageEvaluator::AGGREGATIONTOKEN_FTPID )->getString() );
						int sequence = batchItems->getCellValue( i, "BATCHSEQUENCE" )->getInt();
						if ( noHeaders && ( sequence == 1 ) )
						{
							DEBUG( "Header skipped ( MO_NOHEADERS option set on exitpoint [" << message->getTableName() << "] )" );
							continue;
						}
						batchItem.setCorrelationId( correlationId );
						LogManager::setCorrelationId( correlationId );

						batchItem.setMessageId( Collaboration::GenerateGuid() );

						// don't reevaluate if the message is a reply or not, use feedback from the batch
						batchItem.setFastpath( true );
						
						// get original message fields, no insert in DB2
						RoutingAction::internalPerformReactivate( &batchItem, "" );
						batchItem.setTableName( message->getTableName() );

						// if a transform was delayed, apply it now
						string messageRef = StringUtil::Trim( batchItems->getCellValue( i, "REFERENCE" )->getString() );
						NameValueCollection addParams;						
						if ( message->getPayloadEvaluator() != NULL )
						{
							addParams = message->getPayloadEvaluator()->getAddParams( messageRef );
						}

						RoutingAction::internalPerformTransformMessage( &batchItem, batchItem.getDelayedTransform(), batchItem.getDelayedTransformSessCode(), addParams );
					}
					else
					{
						if ( isRapidBatch )
						{
							batchItem = RoutingEngine::getRoutingSchema()->GetRBatchItems()[ i ];
							LogManager::setCorrelationId( batchItem.getCorrelationId() );

							/*if ( batchItem.getBatchSequence() == BatchItem::FIRST_IN_SEQUENCE )
							{
								if ( noHeaders )
								{
									DEBUG( "Header skipped ( MO_NOHEADERS option set on exitpoint [" << batchItem.getTableName() << "] )" );
									continue;
								}
								DEBUG( "FIRST in batch : performing transform" );

								batchItem.setBatchTotalAmount( StringUtil::Trim( batchItems->getCellValue( i, "BATCHAMOUNT" )->getString() ) );
								RoutingAction::internalPerformTransformMessage( &batchItem, RoutingEngine::getBatchXslt(), message->getOutputSession() );
							}*/
						}
						else
						{
							if ( batchItems == NULL )
								throw logic_error( "Batch items allocation failure" );

							// get the transformed message
							// TODO remove useless search in DB
							batchItem.setMessageId( StringUtil::Trim( batchItems->getCellValue( i, "ID" )->getString() ) );
							batchItem.setTableName( StringUtil::Trim( batchItems->getCellValue( i, "ROUTINGPOINT" )->getString() ) );

							batchItem.Read();	
							LogManager::setCorrelationId( batchItem.getCorrelationId() );

							batchItem.setBatchId( message->getBatchId() );
							
							// else we dont need to read the msg again, if we have the information like ( feedback and correlation ) 
							//saved in table tempbatchjobs
						
							//LogManager::setCorrelationId( StringUtil::Trim( batchItems->getCellValue( i, "CORRELATIONID" )->getString() ) );
													
							batchItem.setMessageOption( RoutingMessageOptions::MO_BATCH );
							if ( ( message->getMessageOptions() & RoutingMessageOptions::MO_RBATCH ) == RoutingMessageOptions::MO_RBATCH )
								batchItem.setMessageOption( RoutingMessageOptions::MO_RBATCH );
							batchItem.setOutputSession( message->getOutputSession() );

							unsigned long batchItemSequence = batchItems->getCellValue( i, "SEQUENCE" )->getLong();
							batchItem.setBatchSequence( batchItemSequence );
							batchItem.setBatchTotalCount( batchItems->getCellValue( i, "MESSAGECOUNT" )->getLong() );

							//DEBUG( "Before resetting payload" );
							//RoutingEngine::getMessagePool().Dump();
						
							// first item - build envelope
							if ( batchItemSequence == BatchItem::FIRST_IN_SEQUENCE )
							{
								if ( noHeaders )
								{
									DEBUG( "Header skipped ( MO_NOHEADERS option set on exitpoint [" << batchItem.getTableName() << "] )" );
									continue;
								}
								DEBUG( "FIRST in batch : performing transform" );

								batchItem.setBatchTotalAmount( StringUtil::Trim( batchItems->getCellValue( i, "AMOUNT" )->getString() ) );
								RoutingAction::internalPerformTransformMessage( &batchItem, RoutingEngine::getBatchXslt( batchItem.getTableName() ), message->getOutputSession() );
							}
							else
							{	
								batchItem.setPayload( batchItems->getCellValue( i, "XFORMITEM" )->getString() );
							}
						}
					}
					
					//DEBUG( "Before process message" );
					//RoutingEngine::getMessagePool().Dump();

					//if ( ( m_Exitpoint.getMessageOptions() & RoutingMessageOptions::MO_NOSEQ ) == RoutingMessageOptions::MO_NOSEQ )
					//{
						// 104 or 102
					//}
					//else
					//{
					// send to mq
					ProcessMessage( &batchItem, userId, false, "" );
					//}

					//DEBUG( "Before delete message" );
					//RoutingEngine::getMessagePool().Dump();
					
					if ( ( !isReply ) && ( !isRapidBatch ) )
					{
						//if is rapid batch we delete all msgs at the end of the batch
						// get rid of the batch message ( we've dealt with all messages in batch );
						RoutingDbOp::DeleteRoutingMessage( batchItem.getTableName(), batchItem.getMessageId() );
					}
				}
				m_BulkOperationInProgress = false;
			}
			catch( const std::exception& ex )
			{
				TRACE( "An error occured during bulk operation [" << ex.what() << "]" );
				if ( batchItems != NULL )
				{
					delete batchItems;
					batchItems = NULL;
				}
				m_Exitpoint.Rollback( isBatch );
				m_BulkOperationInProgress = false;

				throw;
			}
			catch( ... )
			{
				TRACE( "An error occured during bulk operation [unknown reason]" );
				if ( batchItems != NULL )
				{
					delete batchItems;
					batchItems = NULL;
				}
				m_Exitpoint.Rollback( isBatch );
				m_BulkOperationInProgress = false;

				throw;
			}

			//DEBUG( "Before commit batch" );
			//RoutingEngine::getMessagePool().Dump();

			if ( batchItems != NULL )
			{
				delete batchItems;
				batchItems = NULL;
			}

			//DEBUG( "After commit batch" );
			//RoutingEngine::getMessagePool().Dump();
			
			DEBUG( "Commiting batch ... message options [" << isMessageBatch << "] ep options [" << isBatch << "]" );
			m_Exitpoint.Commit( message->getBatchId() );

			if ( isReply )
			{
				// get rid of the batch message ( we've dealt with all messages in batch );
				RoutingDbOp::DeleteRoutingMessage( message->getTableName(), message->getMessageId() );
				RoutingDbOp::UpdateBusinessMessageAck( message->getBatchId(), true /*batch*/ );
			}
			else
			{
				RoutingDbOp::TerminateBatch( message->getBatchId(), batchType, BatchManagerBase::BATCH_COMPLETED, "ok" );
			}
			
			return string( "exitpoint " ) + m_Exitpoint.getQueueName();
		}
		else /* not bulk */
		{
			try
			{
				//DEBUG( "Before process message" );
				//RoutingEngine::getMessagePool().Dump();
				
				m_Exitpoint.ProcessMessage( message, userId, bulk, "" );
				bool incomingMessage = false;
				try
				{
					string feedbackcode = message->getFeedback().getAggregationField( 0 );

					// incoming message being sent to a queue
					incomingMessage = ( feedbackcode == RoutingMessageEvaluator::FEEDBACKFTP_MSG );
				}
				catch( ... )
				{
					//ignore errors
				}
				if ( ( isReply && !m_BulkOperationInProgress ) || incomingMessage )
				{
					RoutingDbOp::UpdateBusinessMessageAck( message->getCorrelationId(), false /*not batch*/ );
				}
				/*stringstream msgReport;
				msgReport << "Message successfully sent to exitpoint [" << m_Exitpoint.getQueueName() << "]";
				DEBUG( msgReport.str() );
				AppException ex( msgReport.str(), EventType::Info );
				ex.setCorrelationId( message->getCorrelationId() );
				if ( userId.length() > 0 )
					ex.addAdditionalInfo( "UserId", userId );
				LogManager::Publish( ex );*/
				
				//DEBUG( "After process message" );
				//RoutingEngine::getMessagePool().Dump();
				

				destination = string( "exitpoint " ) + m_Exitpoint.getQueueName();
				
				if ( !m_BulkOperationInProgress ) 
				{
					// sensitive ops here : 2 commits
					try
					{
						// remove the message from DB...  a deadlock may prevent this ...
						RoutingDbOp::DeleteRoutingMessage( message->getTableName(), message->getMessageId(), isReply );
					}
					catch( ... )
					{
						m_Exitpoint.Rollback( isBatch );
						throw;
					}

					// commit put to MQ
					m_Exitpoint.Commit( message->getBatchId() );
				}
			}
			catch( const std::exception& ex )
			{
				stringstream msgReport;
				msgReport << "Unable to send message to exitpoint [" << m_Exitpoint.getQueueName() << "]";
				DEBUG( msgReport.str() );
				AppException aex( msgReport.str(), ex, EventType::Error );
				aex.setCorrelationId( message->getCorrelationId() );
				if ( userId > 0 )
					aex.addAdditionalInfo( "UserId", StringUtil::ToString( userId ) );
				LogManager::Publish( aex );

				if ( !m_BulkOperationInProgress ) 
				{
					m_Exitpoint.Rollback( isBatch );
				}
				throw;
				// rollback transaction
			}
			catch( ... )
			{
				stringstream msgReport;
				msgReport << "Unable to send message to exitpoint [" << m_Exitpoint.getQueueName() << "]";
				DEBUG( msgReport.str() );
				AppException aex( msgReport.str(), EventType::Error );
				aex.setCorrelationId( message->getCorrelationId() );
				if ( userId > 0 )
					aex.addAdditionalInfo( "UserId", StringUtil::ToString( userId ) );
				LogManager::Publish( aex );

				if ( !m_BulkOperationInProgress ) 
				{
					m_Exitpoint.Rollback( isBatch );
				}
				throw;
				// rollback transaction
			}
		}
	}
	else
	{
		DEBUG( "Message should be held ( no more rules )... " );
		// no ep, hold message as all rules have been processed
		
		RoutingAction holdAction( "ChangeHoldStatus( 1 )" );
		holdAction.Perform( message, userId );

		destination = m_QueueName;
	}

	return destination;
}
