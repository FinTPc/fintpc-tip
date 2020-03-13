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

#ifndef ROUTINGEVALUATORS_H
#define ROUTINGEVALUATORS_H

#include "RoutingMessage.h"
#include "RoutingKeyword.h"
#include "Currency.h"

class ExpressionEvaluator
{
	public :
		typedef enum 
		{
			NOOP,
			EQUALITY, 	// ==
			DIFF,		// !=
			IN_SET,		// IN
			GREATER,	// >
			LESS,		// <
			GREATER_EQ,	// >=
			LESS_EQ,	// <=
			LIKE
		} OPERATOR_TYPE;
		
	private :
		string m_FirstOp;
		OPERATOR_TYPE m_Operator;
		string m_SecondOp;
		
	public :
		//ctor
		ExpressionEvaluator() : m_Operator( ExpressionEvaluator::NOOP ){};
		ExpressionEvaluator( const string& text );
		
		// accessors
		string getFirstOp() const { return m_FirstOp; }
		string getSecondOp() const { return m_SecondOp; }
		OPERATOR_TYPE getOperator() const { return m_Operator; }
		
		// eval methods 
		bool EvaluateBool( const bool actualValue ) const;
		bool EvaluateString( const string& actulValue ) const;
		bool EvaluateDate( const string& actualValue ) const;
		bool EvaluateCurrency( const string& actualValue ) const;

		bool Evaluate( const string& actualValue, const RoutingKeyword::EVALUATOR_TYPE evalType ) const;
};

class ExportedTestObject RoutingCondition
{
	public :

		// datatype type	
		typedef enum 
		{
			ALWAYS = 1,
			MESSAGE = 2,
			FUNCTION = 4,
			METADATA = 8
		} ROUTING_CONDITION;
		
		typedef enum
		{
			VALIDATE_TO_XSD,
			VALIDATE,
			IS_ACK,
			IS_NACK,
			IS_REPLY
		} SUPPORTED_FUNCTION_EVALS;
	
		typedef enum
		{
			MT,
			ORIGINALMT,
			KEYWORD,
			FEEDBACK,
			XPATH,
			FINCOPY
		} SUPPORTED_MESSAGE_EVALS;
		
		typedef enum
		{
			REQUESTOR,
			RESPONDER,
			ORIGINALREQUESTOR,
			FEEDBACKCODE,
			NAMESPACE,
			REQUESTTYPE
		} SUPPORTED_METADATA_EVALS;	
		
		// returns a friendly name of the condition
		static string ToString( const ROUTING_CONDITION type );
		string ToString() const;
		
		// .ctor
		RoutingCondition( const ROUTING_CONDITION conditionType, const string& text );
		~RoutingCondition(){};

		// accessors
		ROUTING_CONDITION getConditionType() const { return m_ConditionType; }
		
		// evaluate condition on a message
		bool Eval( RoutingMessage& message ) const;

		// used by plans to evaluate if a condition must be fullfilled or not
		const bool expectedResult() const { return m_ExpectedResult; }
		void setExpectedResult( const bool result ) { m_ExpectedResult = result; }

		bool Validate( const string& options, RoutingMessage& message ) const;
		
	private :
	
		bool internalEvalMessage( RoutingMessage& message ) const;
		bool internalEvalFunction( RoutingMessage& message ) const;
		bool internalEvalMetadata( RoutingMessage& message ) const;
	
		ROUTING_CONDITION m_ConditionType;
		string m_Text;

		bool m_ExpectedResult;
		
		SUPPORTED_FUNCTION_EVALS m_EvalFunction;
		vector< string > m_EvalFunctionParams;
		
		SUPPORTED_MESSAGE_EVALS m_EvalMessage;
		SUPPORTED_METADATA_EVALS m_EvalMetadata;
		ExpressionEvaluator m_Evaluator;
};

#endif
