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

#include "Trace.h"
#include "StringUtil.h"

#include "XSD/XSDFilter.h"
#include "XSD/XSDValidationException.h"

#include "RoutingEngine.h"
#include "RoutingEvaluators.h"

#ifdef USING_REGULATIONS
#include "RoutingRegulations.h"
#endif // USING_REGULATIONS

ExpressionEvaluator::ExpressionEvaluator( const string& text )
{
	m_Operator = ExpressionEvaluator::NOOP;
	
	int opLen = 0;
	
	// try ==
	string::size_type opPos = text.find( "==" );
	if ( opPos != string::npos )
	{
		m_Operator = ExpressionEvaluator::EQUALITY;
		opLen = 2;
	}
	if ( m_Operator == ExpressionEvaluator::NOOP )
	{
		DEBUG( "NOT == " );
		// try !=
		opPos = text.find( "!=" );

		if ( opPos != string::npos )
		{
			m_Operator = ExpressionEvaluator::DIFF;
			opLen = 2;
		}
	}
	if ( m_Operator == ExpressionEvaluator::NOOP )
	{
		DEBUG( "NOT != " );
		// try <
		opPos = text.find( "<=" );

		if ( opPos != string::npos )
		{
			m_Operator = ExpressionEvaluator::LESS_EQ;
			opLen = 2;
		}
	}
	if ( m_Operator == ExpressionEvaluator::NOOP )
	{
		// try <=
		opPos = text.find( "<" );
	
		if ( opPos != string::npos )
		{
			m_Operator = ExpressionEvaluator::LESS;
			opLen = 1;
		}
	}
	if ( m_Operator == ExpressionEvaluator::NOOP )
	{
		// try >=
		opPos = text.find( ">=" );
	
		if ( opPos != string::npos )
		{
			m_Operator = ExpressionEvaluator::GREATER_EQ;
			opLen = 2;
		}
	}
	if ( m_Operator == ExpressionEvaluator::NOOP )
	{
		// try >
		opPos = text.find( ">" );
	
		if ( opPos != string::npos )
		{
			m_Operator = ExpressionEvaluator::GREATER;
			opLen = 1;
		}
	}
	if ( m_Operator == ExpressionEvaluator::NOOP )
	{
		// try IN or in
		opPos = StringUtil::CaseInsensitiveFind( text, " IN " );
	
		if ( opPos != string::npos )
		{
			m_Operator = ExpressionEvaluator::IN_SET;
			opLen = 4;
		}
	}
	if ( m_Operator == ExpressionEvaluator::NOOP )
	{
		// try IN or in
		opPos = StringUtil::CaseInsensitiveFind( text, " LIKE " );
			
		if ( opPos != string::npos )
		{
			m_Operator = ExpressionEvaluator::LIKE;
			opLen = 6;
		}
	}
	if ( m_Operator == ExpressionEvaluator::NOOP )
	{
		string errorMessage( "No operator found in condition : [" );
		errorMessage += text;
		errorMessage.append( "]" );

		TRACE( errorMessage );
		throw logic_error( errorMessage );
	}
	
	//TODO trim operands
	m_FirstOp = StringUtil::Trim( text.substr( 0, opPos ) );
	m_SecondOp = StringUtil::Trim( text.substr( opPos + opLen ) );

	DEBUG( "Condition : [" << m_FirstOp << "] [" << m_Operator << "] [" << m_SecondOp << "]" );
}

bool ExpressionEvaluator::EvaluateBool( const bool actualValue ) const
{
	DEBUG( "Bool comparison" );
	bool ruleValue = ( m_SecondOp == "true" ) ? true : false;
	
	switch ( m_Operator )
	{
		case EQUALITY :
			return ( actualValue == ruleValue );
		case DIFF :
			return ( actualValue != ruleValue );
		default :
			throw logic_error( "Invalid comparison between bool types" );
	}
}

bool ExpressionEvaluator::EvaluateString( const string& actualValue ) const
{
	DEBUG( "String comparison [" << actualValue << "] vs [" << m_SecondOp << "]" );
	switch( m_Operator )
	{
		case LIKE :
			return ( actualValue.find( m_SecondOp ) == 0 );

		case IN_SET : 
			{
				if( actualValue.length() > 0 )
				{
					StringUtil condTokens( m_SecondOp );
					condTokens.Split( "," );
					while ( condTokens.MoreTokens() )
					{
						string currentToken = condTokens.NextToken();
						if ( currentToken.find( actualValue ) != string::npos )
							return true;
					}
				}
				return false;
			}
			
		case EQUALITY :
			if ( actualValue == m_SecondOp )
				return true;
			// the string may be padded with 's
			
			DEBUG( "Attempting padded string match [" << StringUtil::Pad( actualValue, "'", "'" ) << "]" );
			return ( StringUtil::Pad( actualValue, "'", "'" ) == m_SecondOp );

		case DIFF :
			return ( actualValue != m_SecondOp );

		default :
			throw logic_error( "Invalid comparison between string types" );
	}
}

bool ExpressionEvaluator::EvaluateDate( const string& actualValue ) const
{
	DEBUG( "Date comparison" );
	return false;
}

bool ExpressionEvaluator::EvaluateCurrency( const string& actualValue ) const
{
	//expect xxx,xx type of ammount
	StringUtil commaSep( actualValue );
	commaSep.Split( "," );

	long firstRulePart = StringUtil::ParseLong( m_SecondOp );
	long firstPart = StringUtil::ParseLong( commaSep.NextToken() );

	DEBUG( "Currency comparison " << firstPart << " vs " << firstRulePart );
	
	switch( m_Operator )
	{
		case EQUALITY :
			return ( firstPart == firstRulePart );
		case DIFF :
			return ( firstPart != firstRulePart );
		case GREATER :
			return ( firstPart > firstRulePart );
		case LESS :
			return ( firstPart < firstRulePart );
		case GREATER_EQ :
			return ( firstPart >= firstRulePart );
		case LESS_EQ :
			return ( firstPart <= firstRulePart );
		default:
			return false;
	}
	
	return false;
}	

bool ExpressionEvaluator::Evaluate( const string& actualValue, const RoutingKeyword::EVALUATOR_TYPE evalType ) const
{
	switch( evalType )
	{
		case RoutingKeyword::STRING :
			return EvaluateString( actualValue );
		case RoutingKeyword::DATE :
			return EvaluateDate( actualValue );
		case RoutingKeyword::CURRENCY :
			return EvaluateCurrency( actualValue );
	}
	return false;
}
	
//RoutingCondition implementation
string RoutingCondition::ToString( const ROUTING_CONDITION cond )
{
	switch( cond )
	{
		case RoutingCondition::ALWAYS :
			return "Always";
	
		case RoutingCondition::MESSAGE :
			return "Message";
			
		case RoutingCondition::FUNCTION :
			return "Function";
		
		case RoutingCondition::METADATA :
			return "Metadata";	
			
		default :
			throw invalid_argument( "cond" );
	}
}

string RoutingCondition::ToString() const
{
	string report = ToString( m_ConditionType );
	report.append( " [" );
	report += m_Text;
	report.append( "]" );
	
	return report; 
}

RoutingCondition::RoutingCondition( const ROUTING_CONDITION conditionType, const string& text )
{
	if ( text.length() > 0 )
		DEBUG( "Condition text : [" << text << "]" );

	//DEBUG_STREAM( "Size : " ) << text.length() << "; first char : " << ( ( int )( text[ 0 ] ) ) << endl;
	m_ConditionType = conditionType;
	m_Text = text;

	switch( m_ConditionType )
	{
		case ALWAYS :
			break;

		case METADATA :
			
			DEBUG( "ConditionType : METADATA" );
			m_Evaluator = ExpressionEvaluator( text );
			
			{ // local vars block
				string cond = StringUtil::ToUpper( m_Evaluator.getFirstOp() );
				if ( cond == "REQUESTOR" )
				{
					m_EvalMetadata = RoutingCondition::REQUESTOR;
				}
				else if ( cond == "RESPONDER" )
				{
					m_EvalMetadata = RoutingCondition::RESPONDER;
				}
				else if ( cond == "ORIGINALREQUESTOR" )
				{
					m_EvalMetadata = RoutingCondition::ORIGINALREQUESTOR;
				}
				else if ( cond == "FEEDBACKCODE" )
				{
					m_EvalMetadata = RoutingCondition::FEEDBACKCODE;
				}
				else if ( cond == "REQUESTTYPE" )
				{
					m_EvalMetadata = RoutingCondition::REQUESTTYPE;
				}
				else if ( cond == "NS" )
				{
					m_EvalMetadata = RoutingCondition::NAMESPACE;
				}
				else
				{
					stringstream errorMessage;
					errorMessage << "Unsupported message condition : [" << cond << "]";
					
					TRACE( errorMessage.str() );
					throw logic_error( errorMessage.str() );
				}
			}
			
			break;
						
		case MESSAGE :
		
			DEBUG( "ConditionType : MESSAGE" );
			// expect MT in '103, 102, ... '
			m_Evaluator = ExpressionEvaluator( text );
			
			{ // local vars block
				// first token is function( param [,param] )
				string cond = StringUtil::ToUpper( m_Evaluator.getFirstOp() );
				if ( cond == "MT" )
				{
					m_EvalMessage = RoutingCondition::MT;
				}
				else if ( cond.substr( 0, 8 ) == "KEYWORD " )
				{
					m_EvalMessage = RoutingCondition::KEYWORD;
				}
				else if ( cond == "FINCOPY" )
				{
					m_EvalMessage = RoutingCondition::FINCOPY;
				}
				else if ( cond.substr( 0, 6 ) == "XPATH " )
				{
					m_EvalMessage = RoutingCondition::XPATH;
				}
				else if ( cond == "ORIGINALMT" )
				{
					m_EvalMessage = RoutingCondition::ORIGINALMT;
				}
				else
				{
					stringstream errorMessage;
					errorMessage << "Unsupported message condition : [" << cond << "]";
					
					TRACE( errorMessage.str() );
					throw logic_error( errorMessage.str() );
				}
			}
			
			break;
			
		case FUNCTION :
	
			DEBUG ( "ConditionType : FUNCTION" );
			// expect function( param [,param] ) = true/false
			m_Evaluator = ExpressionEvaluator( text );
			
			{ // local vars block
				// first token is function( param [,param] )
				StringUtil functionSplitter( m_Evaluator.getFirstOp() );
				functionSplitter.Split( "(" );
				
				string functionName = StringUtil::ToUpper( functionSplitter.NextToken() );
				
				// check function name here
				if ( functionName == "VALIDATETOXSD" )
					m_EvalFunction = RoutingCondition::VALIDATE_TO_XSD;
				else if ( functionName == "VALIDATE" )
					m_EvalFunction = RoutingCondition::VALIDATE;
				else if ( functionName == "ISACK" )
					m_EvalFunction = RoutingCondition::IS_ACK;
				else if ( functionName == "ISNACK" )
					m_EvalFunction = RoutingCondition::IS_NACK;		
				else if ( functionName == "ISREPLY" )
					m_EvalFunction = RoutingCondition::IS_REPLY;		
				else
				{
					stringstream errorMessage;
					errorMessage << "Unsupported function : [" << functionName << "]";
					
					TRACE( errorMessage.str() );
					throw logic_error( errorMessage.str() );
				}
				
				// second token are the params :  param [,param] )
				string strParams = StringUtil::Trim( functionSplitter.NextToken() );
				
				// remove last )
				StringUtil paramSplitter( strParams.substr( 0, strParams.length() - 1 ) );
				paramSplitter.Split( "," );
				while( paramSplitter.MoreTokens() )
				{
					string crtParam = StringUtil::Trim( paramSplitter.NextToken() );
					if ( crtParam.length() > 0 )
						m_EvalFunctionParams.push_back( crtParam );
				};
			}
			
			break;
			
		default :
			throw invalid_argument( "condition type" );
	}
}

bool RoutingCondition::Eval( RoutingMessage& message ) const
{
	switch( m_ConditionType )
	{
		case ALWAYS :
			// always returns true for every evaluation
			return true;
		case MESSAGE :
			return internalEvalMessage( message );
		case FUNCTION :
			return internalEvalFunction( message );
		case METADATA :
			return internalEvalMetadata( message );
		default :
			throw invalid_argument( "condition type" );
	}
}

bool RoutingCondition::internalEvalMessage( RoutingMessage& message ) const
{
	DEBUG( "Evaluating message" );
	switch ( m_EvalMessage )
	{
		case RoutingCondition::FINCOPY :
			DEBUG( "Requesting FINCopy evaluation ..." );
			{
				RoutingMessageEvaluator* evaluator = message.getPayloadEvaluator();
				if ( ( evaluator == NULL ) || ( !evaluator->isBusinessFormat() ) )
				{
					DEBUG( "Condition failed because the message is not in the correct format for FINCopy evaluation" );
					return false;
				}
					
				string resultFC = evaluator->getField( InternalXmlPayload::FINCOPY );
				return m_Evaluator.EvaluateString( resultFC );
			}
				
		case RoutingCondition::XPATH :
			// supports Xpath <xpath> <op> value
			return false;
			
		case RoutingCondition::MT :
			// supports MT <op> value/set
		
			DEBUG( "Requesting MT evaluation ..." );
			{
				RoutingMessageEvaluator* evaluator = message.getPayloadEvaluator();
				if ( ( evaluator == NULL ) || ( !evaluator->isBusinessFormat() ) )
				{
					DEBUG( "Condition failed because the message is not in the correct format for MT evaluation" );
					return false;
				}
					
				string resultMT = evaluator->getField( InternalXmlPayload::MESSAGETYPE );
			
				// removed for CQ/PE/BN message types ...
				/*if( resultMT.length() < 3 )
				{
					DEBUG( "Condition failed because the message type [" << resultMT << "] is not valid ( \\d{3} )" );
					return false;
				}*/
				
				return m_Evaluator.EvaluateString( resultMT );
			}

		case RoutingCondition::ORIGINALMT :
			// supports ORIGINALMT <op> value/set
				
			DEBUG( "Requesting ORIGINALMT evaluation ..." );
			{
				RoutingMessageEvaluator* evaluator = message.getPayloadEvaluator();
				if ( ( evaluator == NULL ) || ( !evaluator->isBusinessFormat() ) || !message.isReply() )
				{
					DEBUG( "Condition failed because the message is not in the correct format for ORIGINALMT evaluation" );
					return false;
				}
				//string result = evaluator->getField( InternalXmlPayload::ORIGMESSAGETYPE );
				//return m_Evaluator.EvaluateString( result );
				return false;
			}

		case RoutingCondition::KEYWORD :
		
			// supports Keyword
			DEBUG( "Requesting Keyword eval" );
			
			{ // local switch block 
				// skip Keyword eyecatcher
				string keyword = m_Evaluator.getFirstOp().substr( 8 );
				string::size_type commaIndex = keyword.find( "," );
				
				// if no field, only keyword
				string evalKeyword = keyword;
				string field = "value";
				
				if ( commaIndex != string::npos )
				{
					evalKeyword = keyword.substr( 0, commaIndex );
					field = keyword.substr( commaIndex+1 );
				}
				DEBUG( "Keyword is [" << evalKeyword << "] field is [" << field << "]" );
				
				RoutingMessageEvaluator* evaluator = message.getPayloadEvaluator();
				if ( ( evaluator == NULL ) || ( !evaluator->isBusinessFormat() ) )
				{
					DEBUG( "Condition failed because the message is not in the correct format for MT evaluation" );
					return false;
				}
				
				string resultMT = evaluator->getField( InternalXmlPayload::MESSAGETYPE );
				string xpath = evaluator->GetKeywordXPath( resultMT, evalKeyword );
				string actualValue = evaluator->getCustomXPath( xpath );
				
				pair< string, RoutingKeyword::EVALUATOR_TYPE > interField = evaluator->Evaluate( actualValue, evalKeyword, field );
				return m_Evaluator.Evaluate( interField.first, interField.second );
				
			}
			
		default :
			// this should not happen ( message condition is parsed in ctor )
			throw runtime_error( "Invalid message condition" );
	}
	return false;
}

bool RoutingCondition::internalEvalFunction( RoutingMessage& message ) const
{
	DEBUG( "Evaluating function" );
	switch ( m_EvalFunction )
	{
		case RoutingCondition::VALIDATE_TO_XSD :
			{
				// local case vars block
				// create an XSD filter
				XSDFilter myFilter;
				NameValueCollection headers;
				headers.Add( XSDFilter::XSDFILE, m_EvalFunctionParams[ 0 ] );
				if ( m_EvalFunctionParams.size() > 1 )
					headers.Add( XSDFilter::XSDNAMESPACE, m_EvalFunctionParams[ 1 ] );
			
				bool valid = true;
				try
				{	
					myFilter.ProcessMessage( message.getPayload()->getDoc(), headers, true );
				}
				catch( const XSDValidationException& ex )
				{
					TRACE( ex.getMessage() );
					valid = false;
				}
				
				return m_Evaluator.EvaluateBool( valid );
			}

		case RoutingCondition::VALIDATE :
			{
				bool valid = true;
				string options = "";
				if ( m_EvalFunctionParams.size() > 0 )
					options = m_EvalFunctionParams[ 0 ];
				
				valid = Validate( options,  message );
				return m_Evaluator.EvaluateBool( valid );
			}
			
		case RoutingCondition::IS_ACK :
			{
				if ( m_EvalFunctionParams.size() == 0 )
					return m_Evaluator.EvaluateBool( message.isAck() );

				// hack for dd : 
				RoutingMessageEvaluator* evaluator = message.getPayloadEvaluator();
				if ( ( evaluator != NULL ) && ( evaluator->CheckPayloadType( RoutingMessageEvaluator::ACHBLKACC ) ) )
				{
					return m_Evaluator.EvaluateBool( evaluator->isAck( m_EvalFunctionParams[ 0 ] ) );
				}
				
				DEBUG( "Condition failed because the message is not in the correct format for ACK(param) evaluation" );
				return false;
			}
			
		case RoutingCondition::IS_NACK :
			{
				if ( m_EvalFunctionParams.size() == 0 )
					return m_Evaluator.EvaluateBool( message.isNack() );

				// hack for dd : 
				RoutingMessageEvaluator* evaluator = message.getPayloadEvaluator();
				if ( ( evaluator != NULL ) && ( evaluator->CheckPayloadType( RoutingMessageEvaluator::ACHBLKRJCT ) ) )
				{
					return m_Evaluator.EvaluateBool( evaluator->isNack( m_EvalFunctionParams[ 0 ] ) );
				}
				
				DEBUG( "Condition failed because the message is not in the correct format for NACK(param) evaluation" );
				return false;
			}
			
		case RoutingCondition::IS_REPLY :
			return m_Evaluator.EvaluateBool( message.isReply() );
			
		default :
			// this should not happen ( function is parsed in ctor )
			throw runtime_error( "Invalid function requested for evaluation." );
			break;
	}
	return false;
}

bool RoutingCondition::internalEvalMetadata( RoutingMessage& message ) const
{
	DEBUG( "Evaluating metadata" );
	
	switch ( m_EvalMetadata )
	{
		case RoutingCondition::RESPONDER :
			return m_Evaluator.EvaluateString( message.getResponderService() );
			
		case RoutingCondition::REQUESTOR :
			return m_Evaluator.EvaluateString( message.getRequestorService() );

		case RoutingCondition::ORIGINALREQUESTOR :
			return m_Evaluator.EvaluateString( message.getOriginalRequestorService() );

		case RoutingCondition::REQUESTTYPE :
			return m_Evaluator.EvaluateString( RoutingMessage::ToString( message.getRequestType() ) );
		
		case RoutingCondition::FEEDBACKCODE :
			{
				RoutingAggregationCode reply = message.getFeedback();
				RoutingMessageEvaluator* evaluator = message.getPayloadEvaluator();

				if ( evaluator != NULL )
					reply = evaluator->getAggregationCode( reply );

				if ( reply.getFields().size() == 0 )
				{
					DEBUG( "Feedback code : [missing]... returning no_match" );
					return false;
				}

				DEBUG( "Feedback code : " << reply.getAggregationField( 0 ) );
				return m_Evaluator.EvaluateString( reply.getAggregationField( 0 ) );
			}

		case RoutingCondition::NAMESPACE :
			{
				RoutingMessageEvaluator* evaluator = message.getPayloadEvaluator();
				if ( evaluator == NULL )
				{
					string ns = "";
					if ( ( message.getPayload() != NULL ) && ( message.getPayload()->getDoc( false ) != NULL ) )
						ns = XmlUtil::getNamespace( message.getPayload()->getDoc( false ) );
					return m_Evaluator.EvaluateString( ns );
				}
				else
					return m_Evaluator.EvaluateString( evaluator->getNamespace() );
			}
			
		default :
			// this should not happen ( function is parsed in ctor )
			throw runtime_error( "Invalid metadata condition requested for evaluation." );
			break;
	}
	
	return false;
}	

bool RoutingCondition::Validate( const string& options, RoutingMessage& message ) const
{
#ifdef USING_REGULATIONS
	Regulations r;
	r.Validate( options, message );
	return true;
#else
	TRACE( "Routing engine not built with Regulations support, but Validate() invoked" );
	return true;
#endif //USING_REGULATIONS
}
