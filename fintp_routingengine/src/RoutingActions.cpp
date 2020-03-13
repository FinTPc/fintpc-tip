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

#include <iomanip>

#include "StringUtil.h"
#include "TimeUtil.h"
#include "Trace.h"
#include "LogManager.h"
#include "BatchManager/BatchItem.h"
#include "BatchManager/Storages/BatchXMLfileStorage.h"

#include "RoutingActions.h"
#include "RoutingDbOp.h"
#include "RoutingAggregationManager.h"
#include "RoutingEngine.h"
#include "RoutingDbOp.h"

XSLTFilter *RoutingAction::m_XSLTFilter = NULL;
const int EnrichTemplate::m_EnrichFields[ENRICHFIELDSCOUNT] = { InternalXmlPayload::SENDER, InternalXmlPayload::RECEIVER, InternalXmlPayload::CURRENCY,
									InternalXmlPayload::VALUEDATE, InternalXmlPayload::IBAN, InternalXmlPayload::IBANPL,
									InternalXmlPayload::SENDERCORR, InternalXmlPayload::RECEIVERCORR, InternalXmlPayload::ORGINSTRID
									};

const string EnrichTemplate::m_EnrichFieldsName[ENRICHFIELDSCOUNT] = { "SENDER", "RECEIVER", "CURRENCY", "CURRENCYDATE", "IBAN", "IBANPL", "SENDERCORRESP",
								"RECEIVERCORRESP", "ORGINSTRID" };

//RoutingAction implementation
string RoutingAction::ToString() const 
{
	stringstream result;
	result << RoutingAction::ToString( m_Action ) << "[" << m_Param << "]"; 
	return result.str();
}

string RoutingAction::ToString( ROUTING_ACTION action )
{
	switch( action )
	{
		case RoutingAction::MOVETO :
			return "MoveTo";
		case RoutingAction::COMPLETE :
			return "Complete";
		case RoutingAction::REACTIVATE :
			return "Reactivate";
		case RoutingAction::NOACTION :
			return "NoAction";
		case RoutingAction::CHANGEHOLDSTATUS :
			return "ChangeHoldStatus";	
		case RoutingAction::CHANGEPRIORITY :
			return "ChangePriority";
		case RoutingAction::CHANGEVALUEDATE :
			return "ChangeValueDate";
		case RoutingAction::TRANSFORM :
			return "TransformMessage";
		case RoutingAction::SENDREPLY :
			return "SendReply";
		case RoutingAction::UPDATELIQUIDITIES :
			return "UpdateLiquidities";
		case RoutingAction::ASSEMBLE :
			return "Assemble";
		case RoutingAction::DISASSEMBLE :
			return "Disassemble";
		case RoutingAction::ENRICH :
			return "Enrich";
		case RoutingAction::WAITON :
			return "WaitOn";
			
		case RoutingAction::HOLDQUEUE :
			return "HoldQueue";
		case RoutingAction::RELEASEQUEUE :
			return "ReleaseQueue";
		case RoutingAction::AGGREGATE :
			return "Aggregate";

		default :
			throw invalid_argument( "Invalid action required" );
	}
}

void RoutingAction::CreateXSLTFilter()
{
	if ( m_XSLTFilter == NULL )
		m_XSLTFilter = new XSLTFilter();
}

RoutingAction::ROUTING_ACTION RoutingAction::Parse( const string& action )
{
	if( action == "MoveTo" )
		return RoutingAction::MOVETO;
		
	if( action == "Complete" )
		return RoutingAction::COMPLETE;

	if( action == "Reactivate" )
		return RoutingAction::REACTIVATE;		

	if( action == "NoAction" )
		return RoutingAction::NOACTION;

	if( action == "ChangeHoldStatus" )
		return RoutingAction::CHANGEHOLDSTATUS;

	if( action == "ChangePriority" )
		return RoutingAction::CHANGEPRIORITY;

	if( action == "ChangeValueDate" )
		return RoutingAction::CHANGEVALUEDATE;
			
	if( action == "TransformMessage" )
		return RoutingAction::TRANSFORM;
		
	if( action == "SendReply" )
		return RoutingAction::SENDREPLY;

	if( action == "UpdateLiquidities" )
		return RoutingAction::UPDATELIQUIDITIES;

	if( action == "Assemble" )
		return RoutingAction::ASSEMBLE;

	if( action == "Disassemble" )
		return RoutingAction::DISASSEMBLE;

	if( action == "Enrich" )
		return RoutingAction::ENRICH;

	if( action == "WaitOn" )
		return RoutingAction::WAITON;
		
	if( action == "HoldQueue" )
		return RoutingAction::HOLDQUEUE;
	
	if( action == "ReleaseQueue" )
		return RoutingAction::RELEASEQUEUE;
	
	if( action == "Aggregate" )
		return RoutingAction::AGGREGATE;

	throw invalid_argument( "Invalid action required" );
}

RoutingAction::RoutingAction( const string& text ) : m_Param( "" )
{
	setText( text );
}

void RoutingAction::setText( const string& text )
{
	//expecting <action>[(<param>)]
	StringUtil splitter( text );
	splitter.Split( "(" );
	
	string action = splitter.NextToken();
	
	// missing routing action ? 
	if( action.size() == 0 )
		throw invalid_argument( "Routing action empty" );
		
	//read param 	
	m_Action = RoutingAction::Parse( action );
	
	DEBUG( "Action is [" << RoutingAction::ToString( m_Action ) << "]" );
	string param = splitter.NextToken();
	
	// no param
	if( param.size() == 0 )
		m_Param = "";
	else
	{
		// look for closing bracket
		StringUtil paramSplitter( param );
		paramSplitter.Split( ")" );
		
		m_Param = StringUtil::Trim( paramSplitter.NextToken() );
		
		DEBUG( "Param is [" << m_Param << "]" );
	}
}

string RoutingAction::Perform( RoutingMessage* message, const int userId, bool bulk ) const
{
	switch( m_Action )
	{
		case RoutingAction::MOVETO :
			internalPerformMoveTo( message );
			break;

		case RoutingAction::COMPLETE :
			internalPerformComplete( message );
			break;
			
		case RoutingAction::REACTIVATE :
			internalPerformReactivate( message );
			break;

		case RoutingAction::CHANGEHOLDSTATUS :
			internalPerformChangeHoldStatus( message );
			break;

		case RoutingAction::CHANGEPRIORITY :
			internalPerformChangePriority( message );
			break;

		case RoutingAction::CHANGEVALUEDATE :
			internalPerformChangeValueDate( message );
			break;

		case RoutingAction::TRANSFORM :
			internalPerformTransformMessage( message, bulk );
			break;
			
		case RoutingAction::SENDREPLY :
			internalPerformSendReply( message, bulk );
			break;

		case RoutingAction::UPDATELIQUIDITIES :
			internalPerformUpdateLiquidities( message );
			break;

		case RoutingAction::ASSEMBLE :
			internalPerformAssemble( message );
			break;

		case RoutingAction::DISASSEMBLE :
			internalPerformDisassemble( message );
			break;

		case RoutingAction::WAITON :
			internalPerformWaitOn( message );
			break;

		case RoutingAction::HOLDQUEUE :
			internalPerformHoldQueue( message, true );
			break;

		case RoutingAction::RELEASEQUEUE :
			internalPerformHoldQueue( message, false );
			break;

		case RoutingAction::AGGREGATE :
			internalAggregateBMInfo( message );
			break;

		case RoutingAction::ENRICH :
			internalPerformEnrichMessage( message );
			break;

		default :
			throw invalid_argument( "Invalid action required" );
	}
	
	stringstream actionPerformed;
	
	actionPerformed << "Action [" << ToString( m_Action ) << "] ";
	if ( m_Param.length() > 0 )
		actionPerformed << "with param [" << m_Param << "] ";

	actionPerformed << "successfully performed";
	if ( userId > 0 )
		actionPerformed << " on behalf of user " << userId;

	actionPerformed << ". Queue is [" << message->getTableName() << "]";

	return actionPerformed.str();
}

void RoutingAction::internalPerformSendReply( RoutingMessage* message, const string& tableName, bool bulk )
{
	RoutingAggregationCode aggCode = message->getAggregationCode();
		
		if ( !bulk )
		{
			// obtain original payload
			string payload = "";

			try
			{
				payload = RoutingAggregationManager::GetAggregationField( aggCode, RoutingMessageEvaluator::AGGREGATIONTOKEN_PAYLOAD );
				if ( payload.length() == 0 )
				{
					if ( message->getPayload() != NULL )
					{
						TRACE( "Original payload is empty. Using current payload..." );
						payload = message->getPayload()->getTextConst();
					}
					else
					{
						TRACE( "Original payload is empty. Current payload as well..." );
						throw runtime_error( "returned payload is empty" );
					}
				}
				else
				{
					DEBUG( "Got original payload for reply [...]" );
				}
			}
			catch( const std::exception& ex )
			{
				AppException aex( "Unable to retrieve original payload from database.", ex );

				aex.addAdditionalInfo( "Possible reason", "Message database archived" );
				aex.setSeverity( EventSeverity::Fatal );
				throw aex;
			}
			catch( ... )
			{
				AppException aex( "Unable to retrieve original payload from database." );

				aex.addAdditionalInfo( "Possible reason", "Message database archived" );
				aex.setSeverity( EventSeverity::Fatal );
				throw aex;
			}

			// change payload
			//message->getPayload()->setText( payload, RoutingMessagePayload::BASE64 );
			message->getPayload()->setText( payload, RoutingMessagePayload::AUTO );

			// reset payload evaluator
			//TODO the payload has changed, the payload evaluator still references the destroyed payload doc
			message->destroyPayloadEvaluator();
			message->setFastpath( true );

			// set Feedback ( this message will not be reevaluated )
			// if the correltoken is FinTP ( message rejected by user or rejected in ACH )
			// only if the agg code contains something else
			if ( message->getFeedback().getCorrelToken() != RoutingMessageEvaluator::AGGREGATIONTOKEN_FTPID )
			{
				DEBUG( "Setting aggregation code for non-FinTP message" );
				if( aggCode.Size() > 0 )
					message->setFeedback( aggCode );
				else
				{
					DEBUG( "Attempted to set an empty aggregation code ... falling back to feedback" );
					message->getFeedback().Dump();
				}
			}

			// message->setFeedback( feedback.getIdentifier() + string( "|" ) + feedback.getFeedback() );
		}
		else
		{
			DEBUG( "Aggregating bulk replies ..." );
			
			try
			{
				// aggregation not performed (?) on this message, so...
				if( ( message->getPayload() != NULL ) && !message->getPayloadEvaluator()->isBusinessFormat() )
					RoutingAggregationManager::AddRequest( aggCode, RoutingAggregationManager::UpdateOrFail );
			}
			catch( const DBWarningException &ex )
			{
				AppException aex = internalPerformMoveToInvestigation( message, "A batch reply was received, but no original message found." );
				// move to investig will throw , so the following line should never get executed;

				aex.addAdditionalInfo( "Correlation token", aggCode.getCorrelToken() );
				aex.addAdditionalInfo( "Correlation id", aggCode.getCorrelId() );

				aex.setSeverity( EventSeverity::Fatal );

				TRACE( "Warning while aggregating request [" << ex.what() << "]" );
				throw aex;
			}
			catch( const std::exception& ex )
			{
				AppException aex( "Unable to update reply info for batch.", ex );
				aex.addAdditionalInfo( "Possible reason", "Message database archived" );
				aex.addAdditionalInfo( "Possible reason 2", "Not a sent batch" );

				aex.setSeverity( EventSeverity::Fatal );
				throw aex;
			}
			catch( ... )
			{
				AppException aex( "Unable to update reply info for batch." );
				aex.addAdditionalInfo( "Possible reason", "Message database archived" );
				aex.addAdditionalInfo( "Possible reason 2", "Not a sent batch" );

				aex.setSeverity( EventSeverity::Fatal );
				throw aex;
			}
		}

		// move reply to destination
		// if m_Param is not set, find the reply queue from originating message
		if( tableName.length() == 0 )
		{
			// if this isReplyForIncomingMessage, don't reply to REQUESTOR, but throw an error( expect a param )
			if ( ( message->getPayloadEvaluator() != NULL ) && message->getPayloadEvaluator()->isOriginalIncomingMessage() )
			{
				AppException aex( "Unable to send reply for incoming batch" );
				aex.addAdditionalInfo( "Corrective action", "add a queue param to sendreply() rule" );
			
				aex.setSeverity( EventSeverity::Fatal );
				throw aex;
			}
			// find out original message batchid and original requestor
			RoutingAggregationCode newRequest( aggCode.getCorrelToken(), aggCode.getCorrelId() );

			newRequest.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_BATCHID, "" );
			newRequest.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_REQUESTOR, "" );

			RoutingAggregationManager::GetAggregationFields( newRequest );
			RoutingAggregationFieldArray fields = newRequest.getFields();

			string origBatchId = fields[ RoutingMessageEvaluator::AGGREGATIONTOKEN_BATCHID ];
			DEBUG( "Acquired BatchId of the original message [" << origBatchId << "]" );
			message->setBatchId( origBatchId );

			//read rref, and mur from bm, and update feedbackagg tfdcode of the orig message if msg type approved
			/*if( ( aggCode.containsAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_TFDCODE ) ) &&
				( aggCode.getAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_TFDCODE ) == RoutingMessageEvaluator::FEEDBACKFTP_APPROVE ) )
				RoutingAggregationManager::UpdataAggregationField( newRequest );
			*/
			// aggregate refusal
			if( message->getPayloadEvaluator() != NULL )
			{
				if( message->getPayloadEvaluator()->updateRelatedMessages() )
				{
					DEBUG( "Updating feedback for original transactions" );
					RoutingDbOp::UpdateOriginalBatchMessages( origBatchId );
				}
			}
			else
			{
				TRACE( "Unable to update feedback for original transactions ( no payload evaluator for this message )" );
			}
			//update

			string origRequestor = fields[ RoutingMessageEvaluator::AGGREGATIONTOKEN_REQUESTOR ];
			DEBUG( "Acquired Requestor of the original message [" << origRequestor << "]" );

			internalPerformMoveTo( message, RoutingDbOp::GetReplyQueue( origRequestor ) );
		}
		else
		{
			// find out original message batchid
			RoutingAggregationCode newRequest( aggCode.getCorrelToken(), aggCode.getCorrelId() );

			newRequest.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_BATCHID, "" );

			RoutingAggregationManager::GetAggregationFields( newRequest );
			RoutingAggregationFieldArray fields = newRequest.getFields();

			string origBatchId = fields[ RoutingMessageEvaluator::AGGREGATIONTOKEN_BATCHID ];
			DEBUG( "Acquired BatchId of the original message [" << origBatchId << "]" );
			message->setBatchId( origBatchId );

			if( message->getPayloadEvaluator() != NULL )
			{
				if( message->getPayloadEvaluator()->updateRelatedMessages() )
				{
					DEBUG( "Updating feedback for original transactions" );
					RoutingDbOp::UpdateOriginalBatchMessages( origBatchId );
				}
			}
			else
			{
				TRACE( "Unable to update feedback for original transactions ( no payload evaluator for this message )" );
			}

			internalPerformMoveTo( message, tableName );
		}

		if ( bulk )
		{
			DEBUG( "Bulk reply successfull" );
		}
}

void RoutingAction::internalPerformMoveTo( RoutingMessage* message, const string& queue )
{
	//reset sequence
	message->setRoutingSequence( 0 );

	RoutingDbOp::MoveRoutingMessage( message->getTableName(), queue, message->getMessageId(), 
			message->getPayload()->getText( RoutingMessagePayload::AUTO ), message->getBatchId(), message->getCorrelationId(),
			message->getSessionId(), message->getRequestorService(), message->getResponderService(),
			RoutingMessage::ToString( message->getRequestType() ), message->getPriority(),
			message->isHeld(), message->getRoutingSequence(), message->getFeedbackString() );
	
	// change the name of the table (allow routing engine to continue applying rules)
	message->setTableName( queue );
}

void RoutingAction::internalPerformComplete( RoutingMessage* message ) const
{
	string param;
	if ( m_Param.empty() && m_Action == RoutingAction::COMPLETE )
		param = "FTP39";
	else
		param = m_Param;

	// set the correct feedback 
	RoutingAggregationCode aggCode = message->getAggregationCode();
	aggCode.Clear();
		
	aggCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_FTPCODE, param );
		
	message->setFeedback( aggCode );

	RoutingAggregationManager::AddRequest( aggCode );
				
	// we could skip this after everything is ok
	RoutingDbOp::DeleteRoutingMessage( message->getTableName(), message->getMessageId() );

}

void RoutingAction::internalPerformChangeHoldStatus( RoutingMessage* message, const bool value )
{
	if ( value )
	{
		message->setHeld( true );
		
		// update message as it will stay here for an undefinite period
		RoutingDbOp::UpdateRoutingMessage( message->getTableName(), message->getMessageId(), 
			message->getPayload()->getText( RoutingMessagePayload::AUTO ), message->getBatchId(), message->getCorrelationId(),
			message->getSessionId(), message->getRequestorService(), message->getResponderService(),
			RoutingMessage::ToString( message->getRequestType() ), message->getPriority(),
			message->isHeld(), message->getRoutingSequence(), message->getFeedbackString() );
	}
	else
	{
		message->setHeld( false );
	}
}

RoutingExceptionMoveInvestig RoutingAction::internalPerformMoveDuplicate( RoutingMessage* message, const string& reason )
{
	RoutingExceptionMoveInvestig aex( reason );
	aex.setSeverity( EventSeverity::Fatal );

	try
	{
		aex.addAdditionalInfo( "Message buffer", message->getPayload()->getTextConst() );
	}
	catch( ... )
	{
		aex.addAdditionalInfo( "Message buffer", "Failed to get original payload" );
	}
	aex.addAdditionalInfo( "Original queue", message->getTableName() );
	
	TRACE( aex.getMessage() );

	string duplicateQueue = RoutingDbOp::GetDuplicateQueue( message->getRequestorService() );
	if ( duplicateQueue.length() == 0 ) 
		duplicateQueue = "InvestigInQueue";

	try
	{
		internalPerformMoveTo( message, duplicateQueue );
		aex.addAdditionalInfo( "Next action", "Move to duplicate queue successful." );
	}
	catch( const AppException& ex )
	{
		stringstream errorMessage;
		errorMessage << "Move to investigation queue failed [" << ex.getMessage() << "]";
		aex.addAdditionalInfo( "Next action", errorMessage.str() );
	}
	catch( const std::exception& ex )
	{
		stringstream errorMessage;
		errorMessage << "Move to investigation queue failed [" << ex.what() << "]";
		aex.addAdditionalInfo( "Next action", errorMessage.str() );
	}
	catch( ... )
	{
		aex.addAdditionalInfo( "Next action", "Move to investigation queue failed [unknown reason]" );
	}

	return aex;
}

RoutingExceptionMoveInvestig RoutingAction::internalPerformMoveToInvestigation( RoutingMessage *message, const string& reason, bool investigIn )
{
	RoutingExceptionMoveInvestig aex( reason );
	aex.setSeverity( EventSeverity::Fatal );

	try
	{
		aex.addAdditionalInfo( "Message buffer", message->getPayload()->getTextConst() );
	}
	catch( ... )
	{
		aex.addAdditionalInfo( "Message buffer", "Failed to get original payload" );
	}
	aex.addAdditionalInfo( "Original queue", message->getTableName() );
	
	TRACE( aex.getMessage() );

	try
	{
		if ( investigIn )
			internalPerformMoveTo( message, "InvestigInQueue" );
		else
			internalPerformMoveTo( message, "InvestigOutQueue" );

		aex.addAdditionalInfo( "Next action", "Move to investigation queue successful." );
	}
	catch( const AppException& ex )
	{
		stringstream errorMessage;
		errorMessage << "Move to investigation queue failed [" << ex.getMessage() << "]";
		aex.addAdditionalInfo( "Next action", errorMessage.str() );
	}
	catch( const std::exception& ex )
	{
		stringstream errorMessage;
		errorMessage << "Move to investigation queue failed [" << ex.what() << "]";
		aex.addAdditionalInfo( "Next action", errorMessage.str() );
	}
	catch( ... )
	{
		aex.addAdditionalInfo( "Next action", "Move to investigation queue failed [unknown reason]" );
	}

	return aex;
}

void RoutingAction::internalPerformAggregate( RoutingMessage* message, const bool dummy )
{
	RoutingMessageEvaluator* evaluator = message->getPayloadEvaluator();
	RoutingAggregationCode reply = message->getAggregationCode();
	string initCorrelationId = message->getCorrelationId();
	bool isReply = false;
	bool needsTrim = false;
	
	if( ( evaluator != NULL ) && ( message->isReply() ) )
	{
		isReply = true;
		DEBUG2( "The message is a reply to another message-> Aggregating reply..." );
		RoutingAggregationCode feedback = message->getFeedback();
		
		DEBUG( "Aggregating reply values with [" << reply.getCorrelToken() << "] = [" << reply.getCorrelId() << "]" );
		
		// if we don't have the necessary fields to make a request : stop here
		if ( ( reply.getCorrelToken().length() == 0 ) || reply.getCorrelId().length() == 0 )
		{
			AppException aex( "No fields in the reply can be used to identify original message." );
			aex.setSeverity( EventSeverity::Fatal );
			aex.addAdditionalInfo( "Message buffer", message->getPayload()->getTextConst() );
			aex.addAdditionalInfo( "Correlation token", reply.getCorrelToken() );
			aex.addAdditionalInfo( "Correlation id", reply.getCorrelId() );

			throw aex;
		}

		// if this message is not a reject (reject=FinTP reject?)
		if ( feedback.getCorrelToken() != RoutingMessageEvaluator::AGGREGATIONTOKEN_FTPCODE )
		{
			// find out original message correlationid and original requestor
			RoutingAggregationCode newRequest( reply );
			// clear fields
			newRequest.Clear();
	
			newRequest.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_FTPID, "" );
			newRequest.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_REQUESTOR, "" );
			newRequest.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_OSESSION, "" );
			

			
			string origCorrelationId = "", origRequestor = "", origSession = "";

			try
			{
				//if this throw the original message is not present
				needsTrim = RoutingAggregationManager::GetAggregationFields( newRequest );

				RoutingAggregationFieldArray fields = newRequest.getFields();
				origCorrelationId = fields[ RoutingMessageEvaluator::AGGREGATIONTOKEN_FTPID ];
				DEBUG( "Acquired CorrelationId of the original message [" << origCorrelationId << "]" );
			
				origRequestor = fields[ RoutingMessageEvaluator::AGGREGATIONTOKEN_REQUESTOR ];
				DEBUG( "Acquired Requestor of the original message [" << origRequestor << "]" );

				origSession = fields[ RoutingMessageEvaluator::AGGREGATIONTOKEN_OSESSION ];
				DEBUG( "Acquired Output session of the original message [" << origSession << "]" );
			}
			catch( ... )
			{
				RoutingExceptionMoveInvestig aex( "A reply was received, but the original message cannot be found." );
				string queue = RoutingDbOp::GetDelayedReplyQueue( message->getRequestorService() );
				//for now, only RTGS 012, 019 replies
				if( ( evaluator->delayReply() ) && ( queue.length() > 0 ) )
				{
					internalPerformMoveTo( message, queue );
					RoutingDbOp::UpdateDelayedId( queue, message->getMessageId(), reply.getCorrelId() );
					aex.addAdditionalInfo( "Next action", " Move to queue " + queue + " successful.");
				}
				else
					aex = internalPerformMoveToInvestigation( message, "A reply was received, but the original message cannot be found." );
					// move to investig will throw , so the following line should not get executed;

				aex.addAdditionalInfo( "Correlation token", reply.getCorrelToken() );
				aex.addAdditionalInfo( "Correlation id", reply.getCorrelId() );
				throw aex;
			}

			message->setCorrelationId( origCorrelationId );
			message->setOriginalRequestorService( origRequestor );
			message->setOutputSession( origSession );
		}
	}
	else
	{
		DEBUG( "The message is not a reply. Creating aggregation request..." );
	}	
	
	try
	{
		// optimistic update( only by mistake, a reply is received, but the original message is not there )
		// if we have a reply, most likely it will update some field
		// duplicate replies rise exception when updates are executed
		if ( isReply )
		{	
			try
			{
				if ( needsTrim )
				{
					// special case when the id length is < 26 
					RoutingAggregationManager::AddRequest( reply, RoutingAggregationManager::OptimisticUpdateTrimmed );
				}
				else
					RoutingAggregationManager::AddRequest( reply, RoutingAggregationManager::OptimisticUpdate );
				}
			catch( const DBNoUpdatesException&  )
			{
				//write back correlationId for replies  that did't correlate
				message->setCorrelationId( initCorrelationId );
				RoutingExceptionMoveInvestig aex = internalPerformeMoveToDuplicateReply( message, "A reply was received, but original message was already replied." );
			
				aex.addAdditionalInfo( "Correlation token", reply.getCorrelToken() );
				aex.addAdditionalInfo( "Correlation id", reply.getCorrelId() );
				aex.setSeverity( EventSeverity::Fatal );
				DEBUG( "Warning while aggregating request [" << aex.what() << "]" );
				throw aex;
			}
			//for ack/nak with MIR aggregation field, may be delayed replies in delay queue
			if( reply.containsAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_MIR ) )
				message->setDelayedReplyId( reply.getAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_MIR ) );
			
		}
		// if we have a non-reply message, try first to insert 
		else
		{
			RoutingAggregationManager::AddRequest( reply, RoutingAggregationManager::OptimisticInsert );
		}
	}
	catch( ... )
	{
		TRACE( "Exception occured while aggregating message fields" ); 
		reply.Dump();
		throw;
	}
}

void RoutingAction::internalPerformTransformMessage( RoutingMessage* message, bool bulk ) const
{
	// don't transform bulk, put postpone transform for the individual batch items
	if ( bulk || message->isBulk() )
	{
		message->DelayTransform( m_Param, m_SessionCode, 0 );
		return;
	}
	// partial amount settled 
	else if ( message->isReply() ) 
	{
		RoutingAggregationCode feedback = message->getFeedback();
		if ( feedback.containsAggregationField( "~AMOUNT" ) ) 
		{
			string replyAmount = feedback.getAggregationField( "~AMOUNT" );
			NameValueCollection addParams;
			addParams.Add( "XSLTPARAMSTTLDAMT", replyAmount );
			internalPerformTransformMessage( message, m_Param, m_SessionCode, addParams );
			return;
		}
	}
	internalPerformTransformMessage( message, m_Param, m_SessionCode );
}
		
void RoutingAction::internalPerformTransformMessage( RoutingMessage* message, const string& xsltFilename, const string& sessionCode, const NameValueCollection& addParams )
{
	RoutingMessageEvaluator* evaluator = message->getPayloadEvaluator();

	// added ISO as internal message
	bool origIsBusiness = ( evaluator == NULL ) ? false : evaluator->isBusinessFormat();

	if ( origIsBusiness && ( evaluator != NULL ) )
	{
		evaluator->UpdateMessage( message );
	}
	DEBUG( ( origIsBusiness ? "The message is in SWIFT XML format" : "The message is not in SWIFT XML format" ) );
	
	XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc = message->getPayload()->getDoc();
	if ( doc == NULL )
		throw runtime_error( "Unable to transform message ( payload is not a valid XML document )" );
		
	NameValueCollection transportHeaders;
	if( m_XSLTFilter == NULL )
		throw runtime_error( "XSLTFilter instance was not created" );
	// this was not thread safe
	//m_XSLTFilter = new XSLTFilter();

	// not thread safe : m_XSLTFilter->addProperty( XSLTFilter::XSLTFILE, xsltFilename );
	// add all additional params
	unsigned int xlstParamsCount = addParams.getCount();
	if ( xlstParamsCount > 0 )
  	{
		DEBUG( "Adding " << xlstParamsCount << " more params to transform ..." );
  		vector< DictionaryEntry > params = addParams.getData();

		// add all params to the transform
		for( unsigned int i=0; i<xlstParamsCount; i++ )
		{
			transportHeaders.Add( params[ i ].first.data(), params[ i ].second.data() );
		}
  	}

	transportHeaders.Add( XSLTFilter::XSLTFILE, xsltFilename );
	transportHeaders.Add( XSLTFilter::XSLTUSEEXT, "true" );
	transportHeaders.Add( "XSLTPARAMREQUESTOR", StringUtil::Pad( message->getRequestorService(), "\'", "\'" ) );
	transportHeaders.Add( "XSLTPARAMFEEDBACK", StringUtil::Pad( message->getFeedback().getAggregationField( 0 ), "\'","\'" ) );
	transportHeaders.Add( "XSLTPARAMSESSION", StringUtil::Pad( message->getOutputSession(), "\'", "\'" ) );
	transportHeaders.Add( "XSLTPARAMISESSION", StringUtil::Pad( message->getInputSession(), "\'", "\'" ) );
	transportHeaders.Add( "XSLTPARAMCORRELID", StringUtil::Pad( message->getCorrelationId(), "\'", "\'" ) );
	
	bool isBatch = ( ( message->getMessageOptions() & RoutingMessageOptions::MO_BATCH ) == RoutingMessageOptions::MO_BATCH );
	bool generateId = ( ( message->getMessageOptions() & RoutingMessageOptions::MO_GENERATEID ) == RoutingMessageOptions::MO_GENERATEID );

	// add some parms if in batch and first
	transportHeaders.Add( "XSLTPARAMBATCHID", StringUtil::Pad( message->getBatchId(), "\'", "\'" ) );
	if ( isBatch && ( message->getBatchSequence() == BatchItem::FIRST_IN_SEQUENCE ) )
	{
		transportHeaders.Add( "XSLTPARAMBATCHCOUNT", StringUtil::Pad( StringUtil::ToString( message->getBatchTotalCount() ), "\'", "\'" ) );
		transportHeaders.Add( "XSLTPARAMBATCHAMOUNT", StringUtil::Pad( message->getBatchTotalAmount(), "\'", "\'" ) );		
	}
	
	char formattedDate[ 7 ], formattedTime[ 7 ];
	struct tm ptr;
	time_t tm;
	
	tm = time( NULL );
	ptr = *localtime( &tm );
	strftime( formattedDate, 7, "%y%m%d", &ptr );
	formattedDate[ 6 ] = 0;
	string crtDate = string( formattedDate );
	
	transportHeaders.Add( "XSLTPARAMDATE", StringUtil::Pad( crtDate, "\'", "\'" ) );
	DEBUG2( "Current date : [" << crtDate << "]" );
	
	if ( generateId )
	{
		DEBUG( "Generating message id ... " );
		RoutingQueue crtQueue = RoutingDbOp::GetQueue( message->getTableName() );

		stringstream genMessageId;

		strftime( formattedTime, 7, "%H%M%S", &ptr );
		formattedTime[ 6 ] = 0;
		
		unsigned int nextSequence = RoutingDbOp::GetNextSequence( crtQueue.getServiceId() );
		genMessageId << formattedTime << setfill( '0' ) << setw( 6 ) << nextSequence;

		transportHeaders.Add( "XSLTPARAMMESSAGEID", StringUtil::Pad( genMessageId.str(), "\'", "\'" ) );
	}

	if( RoutingEngine::shouldTrackMessages() )
	{
		//string messageText(  )= theMessage->getPayload()!= NULL ? theMessage->getPayload()->getTextConst() : "No payload available";
		AppException trackAppEx(  "Tracking", EventType::Management );
		//trackAppEx.setCorrelationId( theMessage->getCorrelationId() );
		trackAppEx.setAdditionalInfo( "Payload", XmlUtil::SerializeToString( doc ) );	
		trackAppEx.setAdditionalInfo( "Headers", transportHeaders.ConvertToXml("Headers") );
		trackAppEx.setAdditionalInfo( "RoutingEngine_ProcessTime", TimeUtil::Get( "%d/%m/%Y %H:%M:%S", 19 ) );
		LogManager::Publish( trackAppEx );
	}
	//unsigned char outputXslt[ 8000 ]; 
	
	unsigned char **outXslt = new ( unsigned char * );
	unsigned char *outputXslt = NULL;
	
	try
	{
		m_XSLTFilter->ProcessMessage( doc, outXslt, transportHeaders, true );
		outputXslt = *outXslt;
		
		DEBUG2( "Transformed : [" << outputXslt << "]" );
		
		// check if the result is xml
		message->getPayload()->setText( string( ( char* )outputXslt ), RoutingMessagePayload::PLAINTEXT );
		
	}
	catch( ... )
	{
		if ( outputXslt != NULL )
		{
			delete[] outputXslt;
			outputXslt = NULL;
		}

		if ( outXslt != NULL )
		{
			delete outXslt;
			outXslt = NULL;
		}
			
		throw;
	}

	if ( outputXslt != NULL )
	{
		delete[] outputXslt;
		outputXslt = NULL;
	}

	if ( outXslt != NULL )
	{
		delete outXslt;
		outXslt = NULL;
	}
	
	// ouput methos should be XML to continue reevaluating message
	string outputFormat = transportHeaders[ XSLTFilter::XSLTOUTPUTFORMAT ];
	if ( outputFormat != XSLTFilter::OUTPUT_METHOD_XML )
	{
		if ( message->getPayloadEvaluator() != NULL )
			message->getPayloadEvaluator()->setValid( false );
	
		// set message output session when performing transforms that may end up in mq
		message->setOutputSession( sessionCode );
		return;
	}
	
	evaluator = message->getPayloadEvaluator( true );
	
	if ( evaluator == NULL )
	{
		// set message output session when performing transforms that may end up in mq
		message->setOutputSession( sessionCode );
		return;
	}

	bool trfIsBusiness = evaluator->isBusinessFormat();

	if ( origIsBusiness && !trfIsBusiness )
	{
		// set message output session when performing transforms that may end up in mq
		message->setOutputSession( sessionCode );

		return;
	}

	//added EnrichDocument as message wrapper
	bool origIsEnrich = ( evaluator == NULL ) ? false : evaluator->CheckPayloadType( RoutingMessageEvaluator::FINTPENRCH );

	//enriched message are not insert using Transform
	if ( origIsEnrich )
		return;

	// if the message wasn't swiftxml format but now is, insert a report line		
	if ( !origIsBusiness && trfIsBusiness )
	{
		internalAggregateBMInfo( message );
	}
}

void RoutingAction::internalAggregateBMInfo( RoutingMessage* message )
{
	RoutingMessageEvaluator* evaluator = message->getPayloadEvaluator();

	// added ISO as internal message
	bool isBusiness = ( evaluator == NULL ) ? false : evaluator->isBusinessFormat();

	if( isBusiness )
	{
		internalInsertBMInfo( message );

		internalPerformAggregate( message, "" );
		if ( evaluator->updateRelatedMessages() )
		{
			RoutingAggregationCode aggBusinessCode = evaluator->getBusinessAggregationCode();
			if( aggBusinessCode.Size() > 0 )
				RoutingAggregationManager::AddRequest( aggBusinessCode, RoutingAggregationManager::OptimisticUpdateTrimmed );
		}

		// allow changes by evaluator
		evaluator->UpdateMessage( message );

		if( message->isDuplicate() )
		{
			// a duplicate ... investigate
			RoutingExceptionMoveInvestig aex = internalPerformMoveDuplicate( message, "The message was marked as a possible duplicate." );
			throw aex;
		}
	}
	else if( ( evaluator != NULL ) && ( evaluator->isReply() ) )
	{
		internalPerformAggregate( message, "" );
	}
	else
	{
		if( message->getFastpath() )
			TRACE( "The message is fastpath message there cannot be evaluated." )
		else
			TRACE( "The message is not in business format nor is a reply, so aggregation was skipped.");
	}
}

void RoutingAction::internalInsertBMInfo( RoutingMessage* message )
{
	RoutingMessageEvaluator* evaluator = message->getPayloadEvaluator();
	
	// unable to evaluate fields
	if ( evaluator == NULL )
		return;

	// unable to evaluate non swift messages
	if ( !evaluator->isBusinessFormat() )
		return;
	//TODO: Add entity as RoutingMessage field if required
	string entity = message->isOutgoing() ? evaluator->getField( InternalXmlPayload::SENDER ) : evaluator->getField( InternalXmlPayload::RECEIVER );
	if( !message->isReply() )
	{
		string messageType = evaluator->getField( InternalXmlPayload::MESSAGETYPE );

		vector<string> keywords = evaluator->getKeywordNames();
		vector<string> keywordValues;
		for ( size_t i = 0; i < keywords.size(); i++ )
			keywordValues.push_back( evaluator->getField( keywords[i] ) );

		try
		{
			RoutingDbOp::InsertBusinessMessage( messageType, entity,
			message->getRequestorService(),
			message->getResponderService(),
			message->getMessageId(),
			message->getCorrelationId(),
			keywords,
			keywordValues );
		}
		catch( const std::exception& ex )
		{
			// if we cannot obtain all business data/values don't match... investigate
			RoutingExceptionMoveInvestig aex = internalPerformMoveToInvestigation( message, "Could not obtain all business data from message.", !message->isOutgoing() );

			aex.addAdditionalInfo( "Error_type", typeid( ex ).name() );
			aex.addAdditionalInfo( "Insert_error", ex.what() );
			throw aex;
		}
		catch( ... )
		{
			// if we cannot obtain all business data/values don't match... investigate
			RoutingExceptionMoveInvestig aex = internalPerformMoveToInvestigation( message, "Could not obtain all business data from message.", !message->isOutgoing() );
			throw aex;
		}
	}
}

void RoutingAction::internalPerformReactivate( RoutingMessage *message, const string& tableName )
{
	string correlationId = message->getCorrelationId();
		
	DEBUG( "Obtaining business data about original message with correlation id [" << correlationId << "]" );
	
	RoutingAggregationCode aggregationCode( RoutingMessageEvaluator::AGGREGATIONTOKEN_FTPID, correlationId );
	aggregationCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_PAYLOAD, "" );
	aggregationCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_BATCHID, "" );
	aggregationCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_REQUESTOR, "" );
	aggregationCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_OSESSION, "" );
	
	RoutingAggregationManager::GetAggregationFields( aggregationCode );
	
	if ( message->getMessageId().length() == 0 )
	{
		string newMessageId = Collaboration::GenerateGuid();
		message->setMessageId( newMessageId );
	}
	
	string orgPayload = aggregationCode.getAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_PAYLOAD );
	DEBUG( "Got original payload... " );
	message->setPayload( orgPayload );
	
	string orgBatchId = aggregationCode.getAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_BATCHID );
	DEBUG( "Got original batchId [" << orgBatchId << "]" );
	message->setBatchId( orgBatchId );
	
	string orgRequestor = aggregationCode.getAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_REQUESTOR );
	DEBUG( "Got original requestor [" << orgRequestor << "]" );
	message->setRequestorService( orgRequestor );

	string orgSession = aggregationCode.getAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_OSESSION );
	DEBUG( "Got original session [" << orgSession << "]" );
	message->setOutputSession( orgSession );

	message->setTableName( tableName );
	if ( tableName.length() > 0 )
	{
		DEBUG( "Reactivating message [" << message->getMessageId() << "] in queue [" << tableName << "]" );
		RoutingDbOp::MoveRoutingMessage( "", tableName, message->getMessageId(), orgPayload, orgBatchId, 
			correlationId, orgSession, orgRequestor, "" /* responder */,
			"SingleMessage", 0 /*priority*/, 0 /*holdstatus*/, 0 /*sequence*/, message->getFeedbackString() /*feedback*/ );
	}
}

void RoutingAction::internalPerformReactivate( RoutingMessage* message, const string& tableName, const RoutingAggregationCode& reactConditions )
{
	string correlationId = message->getCorrelationId();
	bool isReactCondition = true;
		
	DEBUG( "Obtaining business data about original message with correlation id [" << correlationId << "]" );
	
	RoutingAggregationCode aggregationCode( RoutingMessageEvaluator::AGGREGATIONTOKEN_FTPID, correlationId );
	aggregationCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_PAYLOAD, "" );
	aggregationCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_BATCHID, "" );
	aggregationCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_REQUESTOR, "" );
	aggregationCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_OSESSION, "" );
	if ( reactConditions.Size() > 0 )
	{
		for( unsigned int i = 0; i < reactConditions.Size(); i++ )
		{
			aggregationCode.addAggregationField( reactConditions.getAggregationFieldName( i ), "" );
		}
	}

	RoutingAggregationManager::GetAggregationFields( aggregationCode );
	
	if ( reactConditions.Size() > 0 )
	{
		//before rectivate check all conditions for reactivation
		for( unsigned int i = 0; i < reactConditions.Size(); i++ )
		{
			string fieldToken = reactConditions.getAggregationFieldName( i );
			string expectedValue = reactConditions.getAggregationField( i );
			string currentValue = aggregationCode.getAggregationField( fieldToken );
			if ( currentValue != expectedValue )
			{
				DEBUG( "Message [" << message->getMessageId() << "] is not reactivated in queue [" << tableName << "]. Current token " << fieldToken << "=[" << currentValue << "], expected [" << expectedValue << "]" );
				return;
			}
		}
	}

	if ( message->getMessageId().length() == 0 )
	{
		string newMessageId = Collaboration::GenerateGuid();
		message->setMessageId( newMessageId );
	}
	
	string orgPayload = aggregationCode.getAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_PAYLOAD );
	DEBUG( "Got original payload... " );
	message->setPayload( orgPayload );
	
	string orgBatchId = aggregationCode.getAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_BATCHID );
	DEBUG( "Got original batchId [" << orgBatchId << "]" );
	message->setBatchId( orgBatchId );
	
	string orgRequestor = aggregationCode.getAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_REQUESTOR );
	DEBUG( "Got original requestor [" << orgRequestor << "]" );
	message->setRequestorService( orgRequestor );

	string orgSession = aggregationCode.getAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_OSESSION );
	DEBUG( "Got original session [" << orgSession << "]" );
	message->setOutputSession( orgSession );

	message->setTableName( tableName );
	if ( tableName.length() > 0 )
	{
		DEBUG( "Reactivating message [" << message->getMessageId() << "] in queue [" << tableName << "]" );
		RoutingDbOp::InsertRoutingMessage( tableName, message->getMessageId(), orgPayload, orgBatchId, 
			correlationId, orgSession, orgRequestor, "" /* responder */,
			"SingleMessage", 0 /*priority*/, 0 /*holdstatus*/, 0 /*sequence*/, message->getFeedbackString() /*feedback*/ );
	}
}

void RoutingAction::internalPerformChangeValueDate( RoutingMessage* message ) const
{
	internalPerformTransformMessage( message, RoutingEngine::getUpdateDateXSLT(), m_SessionCode );
	RoutingDbOp::UpdateBusinessMessageValueDate( message->getCorrelationId(), TimeUtil::Get( "%y%m%d", 6 ) );

	// update feedbackagg payload 
	RoutingAggregationCode request( RoutingMessageEvaluator::AGGREGATIONTOKEN_FTPID, message->getCorrelationId() );
	request.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_PAYLOAD, message->getPayload()->getText( RoutingMessagePayload::AUTO ) );
	
	// optimistic update( we know the message is in there ;) )
	RoutingAggregationManager::AddRequest( request, RoutingAggregationManager::OptimisticUpdate );
}

void RoutingAction::internalPerformAssemble( RoutingMessage* message ) const
{
	// must be after a change hold status ( all messages in the same queue )
	// must be set only on header message types
	BatchManager< BatchXMLfileStorage >* batchManager = NULL;
	DataSet* batchSet = NULL;
	string batchId = "";
	string xsltFileName = "";
	string getSPName = "";
	string delSPName = "";
	int messageNo = 0;
	
	try
	{
		RoutingMessageEvaluator* evaluator = message->getPayloadEvaluator();
		if ( evaluator == NULL )
			throw runtime_error( "Invalid message format for TRN evaluation" );

		// m_Param means an XSLT will be applied on the message ( m_Param ) to create the header
		if ( m_Param.length() > 0 )
		{
			bool autoBatch = false;
			stringstream autoBatchOption;
			autoBatchOption << "Assemble." << m_Param << ".AutoBatch";
			if( RoutingEngine::TheRoutingEngine->GlobalSettings.getSettings().ContainsKey( autoBatchOption.str() ) )
				autoBatch = ( RoutingEngine::TheRoutingEngine->GlobalSettings[ autoBatchOption.str() ] == "true" );

			DEBUG( "AutoBatch option for [" << m_Param << "] set to [" << ( ( autoBatch ) ? string( "true]" ) : string( "false]" ) ) );

			// messages with no-batch options will be "assembled" in a diff. way  : change hold status
			if ( !autoBatch && ( ( message->getMessageOptions() & RoutingMessageOptions::MO_BATCH ) != RoutingMessageOptions::MO_BATCH ) )
			{
				// perform hold
				internalPerformChangeHoldStatus( message, true );				
				return;
			}

			// for autobatch, we use the messageid as batchid
			batchId = ( autoBatch ) ? message->getMessageId() : message->getBatchId();

			stringstream settingXsltName, settingGetSPName;
			settingXsltName << "Assemble." << m_Param << ".Xslt";
			settingGetSPName << "Assemble." << m_Param << ".GetSP";
			if( RoutingEngine::TheRoutingEngine->GlobalSettings.getSettings().ContainsKey( settingXsltName.str() ) )
			{
				xsltFileName = RoutingEngine::TheRoutingEngine->GlobalSettings[ settingXsltName.str() ];
				DEBUG( "Assemble XSLT filename : [" << xsltFileName << "]" );
			}
			else
			{
				// for autobatch, the xslt doesn't have to exist ( no xform )
				if ( !autoBatch )
				{
					xsltFileName = m_Param;
					TRACE( "Assemble XSLT filename not set in config. Using param as xslt [" << m_Param << "]" );
				}
			}

			if( RoutingEngine::TheRoutingEngine->GlobalSettings.getSettings().ContainsKey( settingGetSPName.str() ) )
			{
				getSPName = RoutingEngine::TheRoutingEngine->GlobalSettings[ settingGetSPName.str() ];
				DEBUG( "Assemble Get SP name : [" << getSPName << "]" );
			}
			else
			{
				getSPName = "GetMessagesInAssembly";
				TRACE( "Assemble Get SP name not set in config. Using default value [" << getSPName << "]" );
			}			

			// only apply it if it's not an autobatch or if it explicitly set
			if ( !autoBatch || ( xsltFileName.length() > 0 ) )
				internalPerformTransformMessage( message, xsltFileName, "" );

			// so that the queue will not see this message as a batch
			message->setMessageOptions( RoutingMessageOptions::MO_NONE );

			// don't care
			messageNo = -1;
		}
		else
		{
			// use ref of the header as batch id
			if ( !evaluator->isBusinessFormat() )
				throw runtime_error( "Invalid message format for TRN evaluation ( expected internal format )" );

			batchId = evaluator->getField( InternalXmlPayload::TRN );
			if ( batchId.length() == 0 )
				throw runtime_error( "Unable to evaluate TRN for use as batch correlation id" );
			messageNo = StringUtil::ParseInt( evaluator->getField( InternalXmlPayload::RELATEDREF ) ) - 1;
		}

		// build a batch manager
		batchManager = dynamic_cast< BatchManager< BatchXMLfileStorage >* >( BatchManagerBase::CreateBatchManager( BatchManagerBase::XMLfile ) );
		if ( batchManager == NULL )
			throw logic_error( "Bad type : batch manager is not of BatchXMLfileStorage type" );
		batchManager->open( batchId, ios_base::out );

		// enqueue header
		BatchItem batchHeader;
		batchHeader.setPayload( message->getPayload()->getTextConst() );
		batchHeader.setBatchId( batchId );
		batchHeader.setMessageId( message->getMessageId() );

		*batchManager << batchHeader;

		// get all messages from the current queue
		if ( getSPName.length() > 0 )
			batchSet = RoutingDbOp::GetBatchMessages( batchId, message->getTableName(), getSPName );
		else
			batchSet = RoutingDbOp::GetBatchMessages( batchId, message->getTableName() );

		// check if we have all messages
		if ( ( messageNo > 0 ) && ( messageNo != batchSet->size() ) )
		{
			stringstream errorMessage;
			errorMessage << "Not all message parts found. Expected " << messageNo << ", but found " << batchSet->size();
			throw runtime_error( errorMessage.str() );
		}

		// enqueue all messages
		for ( unsigned int i=0; i<batchSet->size(); i++ )
		{
			BatchItem batchItem;
			batchItem.setPayload( StringUtil::Trim( batchSet->getCellValue( i, "PAYLOAD" )->getString() ) );
			batchItem.setBatchId( batchId );
			batchItem.setMessageId( StringUtil::Trim( batchSet->getCellValue( i, "ID" )->getString() ) );

			*batchManager << batchItem;
		}

		// get result
		string messageOutput = batchManager->storage().getSerializedXml();
		batchManager->close( batchId );

		DEBUG( "Result batch is [" << messageOutput << "]" );
		message->setPayload( messageOutput );
		message->setBatchId( batchId );

		// set request type to batch ( the messages come as "SingleMessage" and change here to "Batch" )
		message->setRequestType( RoutingMessage::Batch );
		message->destroyPayloadEvaluator();

		// delete parts
		stringstream settingDelSPName;
		settingDelSPName << "Assemble." << m_Param << ".DelSP";
		if( RoutingEngine::TheRoutingEngine->GlobalSettings.getSettings().ContainsKey( settingDelSPName.str() ) )
		{
			delSPName = RoutingEngine::TheRoutingEngine->GlobalSettings[ settingDelSPName.str() ];
			DEBUG( "Assemble Del SP name : [" << delSPName << "]" );
		}
		else
		{
			delSPName = "DeleteMessagesInAssembly";
			TRACE( "Assemble Del SP name not set in config. Using default value [" << delSPName << "]" );
		}
		RoutingDbOp::RemoveBatchMessages( batchId, message->getTableName(), delSPName );
	}
	catch( ... )
	{
		TRACE( "Error in assemble" );
		if ( batchManager != NULL )
		{
			batchManager->close( batchId );

			delete batchManager;
			batchManager = NULL;
		}

		if ( batchSet != NULL )
		{
			delete batchSet;
			batchSet = NULL;
		}

		throw;
	}

	if ( batchManager != NULL )
	{
		delete batchManager;
		batchManager = NULL;
	}

	if ( batchSet != NULL )
	{
		delete batchSet;
		batchSet = NULL;
	}
}

void RoutingAction::internalPerformDisassemble( RoutingMessage* message ) const
{
	// just set the message as bulk, the queue will disassemble the batch
	if ( message->isReply() )
		message->setBulk( true );
	else
	{
		RoutingMessageEvaluator* evaluator = message->getPayloadEvaluator();
		if ( ( evaluator == NULL ) || ( !evaluator->isBusinessFormat() ) )
			throw runtime_error( "Invalid message format for TRN evaluation" );

		// use ref of the header as batch id
		string messageTRN = evaluator->getField( InternalXmlPayload::TRN );
		if ( messageTRN.length() == 0 )
			throw runtime_error( "Unable to evaluate TRN for use as batch correlation id" );

		// skip reactivated messages
		// dissasemblying an MT102 results in 1 MT102 and several MT102E
		// the idea is to skip the first part 
		if ( messageTRN == message->getBatchId() )
			return;

		BatchManager< BatchXMLfileStorage >* batchManager = NULL;
		
		try
		{			
			batchManager = dynamic_cast< BatchManager< BatchXMLfileStorage >* >( BatchManagerBase::CreateBatchManager( BatchManagerBase::XMLfile ) );
			if ( batchManager == NULL )
				throw logic_error( "Bad type : batch manager is not of BatchXMLfileStorage type" );

			//Set batch Xml message 
			batchManager->storage().setSerializedXml( message->getPayload()->getTextConst() );
			batchManager->storage().setXslt( m_Param );
			batchManager->open( messageTRN, ios_base::in );
		
			bool isLast = false;
			RoutingAggregationCode request = message->getAggregationCode(); 

			do
			{
				BatchItem item;
				*batchManager >> item;

				// update feedback batchid, payload, seq
				request.setCorrelToken( RoutingMessageEvaluator::AGGREGATIONTOKEN_FTPID );
				request.setAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_BATCHID, item.getBatchId() );
				request.setAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_BATCHSEQ, StringUtil::ToString( item.getSequence() ) );
				request.setAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_PAYLOAD, item.getPayload() );

				// for the 1st item, the bm is already inserted
				if ( item.getSequence() > BatchItem::FIRST_IN_SEQUENCE )
				{
					string messageId = Collaboration::GenerateGuid();
					string correlationId = Collaboration::GenerateGuid();
					//insert part in queue; the message part will be routed starting from crt. sequence
					DEBUG( "Inserting message split [" << messageId << "] in queue [" << message->getTableName() << "]" );
					RoutingDbOp::InsertRoutingMessage( message->getTableName(), messageId, item.getPayload(), item.getBatchId(), 
					correlationId, message->getOutputSession(), message->getRequestorService(), message->getResponderService(),
					"SingleMessage", 0 /*priority*/, 0 /*holdstatus*/, message->getRoutingSequence() /*sequence*/, message->getFeedbackString() );

					// insert part in RoutedMessages
					RoutingMessage itemMessage( *message );

					itemMessage.setMessageId( messageId );
					itemMessage.setCorrelationId( correlationId );
					itemMessage.setBatchId( item.getBatchId() );
					itemMessage.setPayload( item.getPayload() );

					internalInsertBMInfo( &itemMessage );
					
					// insert part in FeedbackAgg
					request.setCorrelId( correlationId );
					RoutingAggregationManager::AddRequest( request, RoutingAggregationManager::OptimisticInsert );
				}
				else
				{
					//update payload for initial message
					message->setPayload( item.getPayload() );
					DEBUG( "Update first message split [" << message->getMessageId() << "] in queue [" << message->getTableName() << "]" );
					RoutingDbOp::UpdateRoutingMessage( message->getTableName(), message->getMessageId(), item.getPayload(), item.getBatchId(), 
					message->getCorrelationId(), message->getOutputSession(), message->getRequestorService(), message->getResponderService(),
					"SingleMessage", 0 /*priority*/, 0 /*holdstatus*/, message->getRoutingSequence() /*sequence*/, message->getFeedbackString() );
					
					// optimistic update( we know the message is in there ;) )
					request.setCorrelId( message->getCorrelationId() );
					RoutingAggregationManager::AddRequest( request, RoutingAggregationManager::OptimisticUpdate );
				}
	
				isLast = item.isLast();

			} while ( !isLast );

			batchManager->close( messageTRN );
		}
		catch( ... )
		{
			TRACE( "Error in disassemble" );
			if ( batchManager != NULL )
			{
				batchManager->close( messageTRN );

				delete batchManager;
				batchManager = NULL;
			}
			throw;
		}

		if ( batchManager != NULL )
		{
			delete batchManager;
			batchManager = NULL;
		}

		// HACK : not a nice way to use exceptions in a normal condition, 
		// redesign is needed.. don't have time...
		RoutingExceptionMessageDeleted ex;
		throw ex;
	}
}

// will wait on a db condition 
// to perform async wait use this pattern : return false from SP, do whatever processing is required, insert a job to further route the message
void RoutingAction::internalPerformWaitOn( RoutingMessage* message ) const
{
	bool shouldWait = RoutingDbOp::CallBoolProcedure( m_Param, message->getMessageId(), message->getTableName() );
	if ( shouldWait )
	{
		DEBUG( "The message [" << message->getMessageId() << "] will wait on " << m_Param << " to become true" );
	}
	else
	{
		DEBUG( "Condition " << m_Param << " satisfied. The message will be sent to the next rule" );
	}

	internalPerformChangeHoldStatus( message, shouldWait );
}

RoutingExceptionMoveInvestig RoutingAction::internalPerformeMoveToDuplicateReply( RoutingMessage* message, const string& reason )
{
	RoutingExceptionMoveInvestig aex( reason);
	aex.setSeverity( EventSeverity::Fatal );
	string investigQueue = "InvestigOutQueue";
	try
	{
		aex.addAdditionalInfo( "Message buffer", message->getPayload()->getTextConst() );
	}
	catch( ... )
	{
		aex.addAdditionalInfo( "Message buffer", "Failed to get original payload" );
	}
	aex.addAdditionalInfo( "Original queue", message->getTableName() );
	
	TRACE( aex.getMessage() );
	
	string duplicateQueue = RoutingDbOp::GetDuplicateReplyQueue( message->getRequestorService() );
	if( duplicateQueue.length() > 0 ) 
		investigQueue = duplicateQueue;
	try
	{
		internalPerformMoveTo( message, investigQueue );
		stringstream message;
		message << " Move to queue " <<  investigQueue << " successful.";
		aex.addAdditionalInfo( "Next action", message.str() );

	}
	catch( const AppException& ex )
	{
		stringstream errorMessage;
		errorMessage << "Move to " << investigQueue << " queue failed [" << ex.getMessage() << "]";
		aex.addAdditionalInfo( "Next action", errorMessage.str() );
	}
	catch( const std::exception& ex )
	{
		stringstream errorMessage;
		errorMessage << "Move to " << investigQueue << " queue failed [" << ex.what() << "]";
		aex.addAdditionalInfo( "Next action", errorMessage.str() );
	}
	catch( ... )
	{
		aex.addAdditionalInfo( "Next action", "Move to investigation queue failed [unknown reason]" );
	}

	return aex;
}

void RoutingAction::internalPerformEnrichMessage( RoutingMessage* message ) const
{
	RoutingMessageEvaluator* evaluator = message->getPayloadEvaluator();
	if ( evaluator == NULL )
		throw runtime_error( "The message is not in known format" );

	
	EnrichTemplate* enricher = EnrichTemplate::GetEnricher( evaluator, m_Param );
	try
	{
		enricher->enrich( message );

		delete enricher;
		enricher = NULL;
	}
	catch( const runtime_error& rex)
	{
		if( enricher != NULL )
		{
			delete enricher;
			enricher = NULL;
		}
		AppException aex = internalPerformMoveToInvestigation( message, rex.what() );
		throw aex;
	}
	catch( ... )
	{
		string errorMessage = "Enrich message fail!";
		TRACE( errorMessage );
		if( enricher != NULL )
		{
			delete enricher;
			enricher = NULL;
		}
		AppException aex = internalPerformMoveToInvestigation( message, errorMessage );
		throw aex;
	}
}

// EnrichTemplate implementation
EnrichTemplate* EnrichTemplate::GetEnricher( RoutingMessageEvaluator* evaluator, const string& param )
{
	EnrichTemplate* enricher = NULL;
	StringUtil allParams( param );
	allParams.Split( "," );
	string xsltFile = allParams.NextToken();
	if( xsltFile.empty() )
		throw logic_error( "Enrich rule should have at least one xslt file parameters" );
	
	string listName = allParams.NextToken();
	if( listName.empty() )
		enricher = new EnrichFromBusiness ( xsltFile, evaluator );
	else if( listName == "INNER" )
	{
		enricher = new SimpleEnricher( xsltFile, evaluator );
	}
	else
	{
		string path = allParams.NextToken();
		//enricher = new EnrichFromList( xsltFile, "bankcodebicmaps", "//x:TxInf/x:RtrRsnInf/x:Orgtr/x:Id/x:OrgId/x:BICOrBEI/text()", evaluator );
		enricher = new EnrichFromList( xsltFile, listName, path, evaluator );
	}

	return enricher; 
	
}

void EnrichTemplate::enrich( RoutingMessage* message )
{
	
	string idFilterValue = getIdFilterValue();
	if( idFilterValue.length() == 0 )
	{
		string errorMessage = "Failed to get id of enrich data";
		TRACE( errorMessage );
		throw runtime_error( errorMessage );
	}

	// add original payload to message
	XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* relatedDoc = NULL;
	try
	{			
		relatedDoc = getEnrichData( idFilterValue );
		if( relatedDoc == NULL )
		{
			string errorMessage = "Empty enrich data ";
			TRACE( errorMessage );
			throw runtime_error( errorMessage );
		}

		DEBUG( "Enrich message, got original payload..." );

		//Get the root element for the related document
		DOMElement* relatedRootElement  = relatedDoc->getDocumentElement();
		if ( relatedRootElement == NULL )
		{
			TRACE( "Root element of the related message is NULL " );
			throw runtime_error( "Root element of the related message is NULL " );
		}	
		XERCES_CPP_NAMESPACE_QUALIFIER DOMNode* key = relatedRootElement->getFirstChild();
		for ( ; key != 0; key=key->getNextSibling() )
		{
			if ( key->getNodeType() != DOMNode::ELEMENT_NODE )
				continue;
			else
				break;
		}
				
		XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc = message->getPayload()->getDoc();
		DOMElement* theRootElement  = doc->getDocumentElement();
		theRootElement->appendChild( doc->importNode( key , true ) );

		DEBUG( "Before enrich transform...[" << XmlUtil::SerializeToString( message->getPayloadEvaluator()->getDocument() ) << "]" );

		NameValueCollection transportHeaders;		
		transportHeaders.Add( XSLTFilter::XSLTFILE, m_XsltFileName );
		WorkItem< ManagedBuffer > outBuffer( new ManagedBuffer() );
		ManagedBuffer* outTransformedBuffer = outBuffer.get();

		XSLTFilter transformFilter;
		transformFilter.ProcessMessage( doc, outBuffer, transportHeaders, true );
		message->getPayload()->setText(  outTransformedBuffer->str(), RoutingMessagePayload::PLAINTEXT );

		relatedDoc->release();
	}
	catch( const DOMException& e )
  	{
		if( relatedDoc != NULL )
			relatedDoc->release();
			
  		TRACE_GLOBAL( "DOMException " << e.code );
  		string message;
  		
  		const unsigned int maxChars = 2047;
		
		XMLCh errText[ maxChars + 1 ];
      
		// attemt to read message text
    	if ( DOMImplementation::loadDOMExceptionMsg( e.code, errText, maxChars ) )
    	{
			message = localForm( errText );
		}
  		else
  		{
  			// format error code
  			stringstream messageBuffer;
  			messageBuffer << "DOMException error code : [ " << ( int )e.code << "]";
    		message = messageBuffer.str();
    	}		
 		throw runtime_error( message );
 	}
	catch( const std::exception& e )
	{
		if( relatedDoc != NULL )
			relatedDoc->release();
  			
		stringstream messageBuffer;
	  	messageBuffer << typeid( e ).name() << " exception [" << e.what() << "]";
		TRACE_GLOBAL( messageBuffer.str() );

    	//throw runtime_error( "Parse error" );
    	throw runtime_error( messageBuffer.str() );
  	}
	catch( ... )
	{
		if( relatedDoc != NULL )
			relatedDoc->release();
		TRACE_GLOBAL( "Unhandled exception" );
		
		throw;
	}  

	// update new payload to FinTP
	RoutingAggregationCode payloadCode( RoutingMessageEvaluator::AGGREGATIONTOKEN_FTPID, message->getCorrelationId() );
	payloadCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_PAYLOAD, message->getPayload()->getTextConst() );
	RoutingAggregationManager::AddRequest( payloadCode, RoutingAggregationManager::OptimisticUpdate );
	
	// reset evaluator
	m_MessageEvaluator = message->getPayloadEvaluator( true );

	// update message to FinTP
	RoutingAggregationCode businessCode( RoutingMessageEvaluator::AGGREGATIONTOKEN_CORRELID, message->getCorrelationId() );	

	vector<string> enrichNames = m_MessageEvaluator->getKeywordNames();
	vector<string> enrichValues;

	vector<string> tableNames;
	vector<string> tableValues;

	//Set 1st parameter "inmsgtype"
	tableNames.push_back( "Message Type" );
	tableValues.push_back( message->getOriginalMessageType() );

	//Set 2nd parameter "insenderapp"
	tableNames.push_back( "Sender application" );
	tableValues.push_back( message->getRequestorService() );

	//Set 3rd parameter "inreceiverapp"
	tableNames.push_back( "Reciver application" );
	tableValues.push_back( message->getResponderService() );

	//Set 4th parameter "inguid"
	tableNames.push_back( "Guid" );
	tableValues.push_back( message->getMessageId() );

	//Set 5th parameter "incorrelid"
	tableNames.push_back( "Correlation id" );
	tableValues.push_back( message->getCorrelationId() );

	//Set 6'th parameter "inkwnames character varying[]"
	for ( size_t i = 0; i < enrichNames.size(); i++ )
		enrichValues.push_back( m_MessageEvaluator->getField( enrichNames[i] ) );

	//Set 7'th parameter inentity
	tableNames.push_back("Entity");
	tableValues.push_back( message->isOutgoing() ? m_MessageEvaluator->getField( InternalXmlPayload::SENDER ) : m_MessageEvaluator->getField( InternalXmlPayload::RECEIVER ) );

	RoutingDbOp::EnrichMessageData( tableNames, tableValues, enrichNames, enrichValues );
}

// Concrete enricher inplementation
XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* EnrichFromList::getEnrichData( const string& idValue )
{
	DataSet* result = NULL;
	XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* relatedDoc = NULL;
	try
	{
		//result = RoutingDbOp::GetEnrichData( m_TableName, "CITI" );
		result = RoutingDbOp::GetEnrichData( m_TableName, idValue );
		relatedDoc = Database::ConvertToXML( result );

		if( result != NULL )
		{
			delete result;
			result = NULL;
		}
	}
	catch( ... )
	{
		string errorMessage = "Failed to get list data while performing enrich message";
		TRACE( errorMessage );

		if( result != NULL )
		{
			delete result;
			result = NULL;
		}

		throw runtime_error( errorMessage );
	}

	return relatedDoc;
}

XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* EnrichFromBusiness::getEnrichData( const string& idValue )
{

	DEBUG( "Obtaining payload for enriching message: transaction id=[" << idValue << "]" );
	
	RoutingAggregationCode aggregationCode( RoutingMessageEvaluator::AGGREGATIONTOKEN_TRN, idValue );
	aggregationCode.addAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_PAYLOAD, "" );
	aggregationCode.addAggregationCondition( RoutingMessageEvaluator::AGGREGATIONTOKEN_BATCHID, m_MessageEvaluator->getField( InternalXmlPayload::RELATEDREF) );
	RoutingAggregationManager::GetAggregationFields( aggregationCode );

	string relatedData = aggregationCode.getAggregationField( RoutingMessageEvaluator::AGGREGATIONTOKEN_PAYLOAD );
	if( relatedData.length() == 0 )
	{
		string errorMessage = "Failed to get original payload while performing enrich message";
		TRACE( errorMessage );
		throw runtime_error( errorMessage );
	}
	
	XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* relatedDoc = XmlUtil::DeserializeFromString( relatedData );

	return relatedDoc;
}

XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* SimpleEnricher::getEnrichData( const string& idValue )
{

	DEBUG( "Getting empty enriching data. Simple enrichement" );

	XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* relatedDoc = XmlUtil::DeserializeFromString( "<?xml version=\"1.0\"?><Empty_root><Empty_elem/></Empty_root>" );

	return relatedDoc;
}