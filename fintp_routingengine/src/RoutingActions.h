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

#ifndef ROUTINGACTIONS_H
#define ROUTINGACTIONS_H

#include "StringUtil.h"
#include "XSLT/XSLTFilter.h"

#include "RoutingEngineMain.h"
#include "RoutingExceptions.h"
#include "RoutingMessage.h"
#include "RoutingDbOp.h"

#define ENRICHFIELDSCOUNT 10

class ExportedTestObject RoutingAction
{
#if defined( TESTDLL_EXPORT ) || defined ( TESTDLL_IMPORT )
	friend class RoutingActionsTest;
#endif

	friend class RoutingSchema;
	friend class RoutingStructures;
	friend class RoutingQueue;

	public :

		// datatype type
		typedef enum
		{
			MOVETO,
			COMPLETE,
			REACTIVATE,
			NOACTION,

			CHANGEHOLDSTATUS,
			CHANGEPRIORITY,
			CHANGEVALUEDATE,
			TRANSFORM,
			SENDREPLY,
			UPDATELIQUIDITIES,

			WAITON,

			ASSEMBLE,
			DISASSEMBLE,
			ENRICH,

			HOLDQUEUE,
			RELEASEQUEUE,

			AGGREGATE,
			SETMATCH
		} ROUTING_ACTION;

		// returns a friendly name of the action
		static string ToString( RoutingAction::ROUTING_ACTION type );
		static RoutingAction::ROUTING_ACTION Parse( const string& action );

		// .ctor
		RoutingAction() : m_Action( NOACTION ), m_Param( "" )
		{
		}

		RoutingAction( const RoutingAction::ROUTING_ACTION action, const string& param ) :
			m_Action( action ), m_Param( param )
		{
		}

		RoutingAction( const string& text );

		// perform the action
		string Perform( RoutingMessage* message, const int userId, bool bulk = false ) const;

		string ToString() const;

		// accessors
		RoutingAction::ROUTING_ACTION getRoutingAction() const { return m_Action; }
		string getParam() const { return m_Param; }

		void setText( const string& text );
		void setSessionCode( const string& sessionCode ) { m_SessionCode = sessionCode; }

		static void CreateXSLTFilter();

	private :

		ROUTING_ACTION m_Action;
		string m_Param;
		string m_SessionCode;

		static XSLTFilter* m_XSLTFilter;

		static RoutingExceptionMoveInvestig internalPerformMoveToInvestigation( RoutingMessage* message, const string& reason = "Invalid message sent to investigation queue", bool investigIn = false );
		static RoutingExceptionMoveInvestig internalPerformMoveDuplicate( RoutingMessage* message, const string& reason = "Possible duplicate message sent to investigation queue" );
		static void internalPerformMoveTo( RoutingMessage* message, const string& queue );
		static void internalPerformTransformMessage( RoutingMessage* message, const string& xsltFilename, const string& sessionCode )
		{
			internalPerformTransformMessage( message, xsltFilename, sessionCode, NameValueCollection() );
		}
		static void internalPerformTransformMessage( RoutingMessage* message, const string& xsltFilename, const string& sessionCode, const NameValueCollection& addParams );
		static void internalPerformAggregate( RoutingMessage* message, const bool dummy );
		static void internalPerformReactivate( RoutingMessage* message, const string& tableName );
		static void internalPerformReactivate( RoutingMessage* message, const string& tableName, const RoutingAggregationCode& reactAgregationCode );
		static void internalPerformSendReply( RoutingMessage* message, const string& tableName, bool bulk = false );
		static void internalPerformChangeHoldStatus( RoutingMessage* message, const bool value );
		static void internalInsertBMInfo( RoutingMessage* message );
		static void internalAggregateBMInfo( RoutingMessage* message );
		
		//static void internalPerformComplete( RoutingMessage* message, const string code );

		void internalPerformWaitOn( RoutingMessage* message ) const;

		void internalPerformAssemble( RoutingMessage* message ) const;
		void internalPerformDisassemble( RoutingMessage* message ) const;
		void internalPerformEnrichMessage( RoutingMessage* message ) const;

		//MoveTo from routing rules
		void internalPerformMoveTo( RoutingMessage* message ) const
		{
			internalPerformMoveTo( message, m_Param );
		}

		void internalPerformAggregate( RoutingMessage* message ) const
		{
			internalPerformAggregate( message, true );
		}

		void internalPerformComplete( RoutingMessage* message ) const;
		void internalPerformChangeHoldStatus( RoutingMessage* message ) const
		{
			internalPerformChangeHoldStatus( message, ( m_Param == "true" ) || ( m_Param == "1" ) );
		}

		void internalPerformChangePriority( RoutingMessage* message ) const
		{
			message->setPriority( StringUtil::ParseULong( m_Param ) );
		}

		void internalPerformChangeValueDate( RoutingMessage* message ) const;

		void internalPerformTransformMessage( RoutingMessage* message, bool bulk = false ) const;

		void internalPerformSendReply( RoutingMessage* message, bool bulk = false ) const
		{
			internalPerformSendReply( message, m_Param, bulk );
		}

		void internalPerformHoldQueue( const RoutingMessage* message, bool holdStatus ) const
		{
			RoutingDbOp::ChangeQueueHoldStatus( message->getTableName(), holdStatus );
		}

		void internalPerformReactivate( RoutingMessage* message ) const
		{
			message->setTableName( m_Param );
			internalPerformReactivate( message, m_Param );
		}

		void internalPerformUpdateLiquidities( const RoutingMessage* message ) const
		{
			RoutingDbOp::UpdateLiquidities( message->getCorrelationId() );
		}
		
		static RoutingExceptionMoveInvestig internalPerformeMoveToDuplicateReply( RoutingMessage* message, const string& reason = "A reply was received, but original message has already been replied." );

		void internalPerformSetMatch(RoutingMessage* message) const;
};

class EnrichTemplate
{
	
	protected:
		RoutingMessageEvaluator* m_MessageEvaluator;
		string m_XsltFileName;
		
		static const int m_EnrichFields[ ENRICHFIELDSCOUNT ];
		static const string m_EnrichFieldsName[ ENRICHFIELDSCOUNT ];

		inline EnrichTemplate( const string& xsltFile, RoutingMessageEvaluator* evaluator ): m_XsltFileName( xsltFile ), m_MessageEvaluator( evaluator ) {};
		virtual string getIdFilterValue() = 0;
		virtual XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* getEnrichData( const string& idValue ) = 0;

	public:

		static EnrichTemplate* GetEnricher( RoutingMessageEvaluator* evaluator, const string& param );
		inline virtual ~EnrichTemplate(){};
		/**
		 * Four step Template method.
		 * 1. Get filter value
		 * 2. Get enrich data using filter value
		 * 3. Enrich message payload  
		 * 4. Override business messages  
		**/
		void enrich( RoutingMessage* message );

};

class EnrichFromList : public EnrichTemplate
{
	private:

		string m_TableName;
		string m_IdPath;

	public:

		inline EnrichFromList( const string& xsltFile, const string& table, const string& idPath, RoutingMessageEvaluator* evaluator ): EnrichTemplate( xsltFile, evaluator),
			m_TableName( table ), m_IdPath( idPath + "/text()" ){};

	protected:
		inline string getIdFilterValue(){ return m_MessageEvaluator->getCustomXPath( m_IdPath ); }; 
		XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* getEnrichData( const string& idFilterValue );

};

class EnrichFromBusiness : public EnrichTemplate
{
	public:

		inline EnrichFromBusiness( const string& xsltFile, RoutingMessageEvaluator* evaluator ): EnrichTemplate( xsltFile, evaluator ){};

	protected:

		inline string getIdFilterValue(){ return m_MessageEvaluator->getField( InternalXmlPayload::ORGTXID ); }; 
		XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* getEnrichData( const string& idFilterValue );

};
/**
 * \class	SimpleEnricher
 * \brief	Created when "INNER" keyword is provided as source for enrich data. 
 * 			This is, when the enrich data is already in the queue payload or when is got through xslt itself
 * 			
 */
class SimpleEnricher : public EnrichTemplate
{
	public:

		inline SimpleEnricher( const string& xsltFile, RoutingMessageEvaluator* evaluator ): EnrichTemplate( xsltFile, evaluator ){};

	protected:

		XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* getEnrichData( const string& idFilterValue = "" );
		inline string getIdFilterValue(){ return "0"; }; 
};
#endif // ROUTINGACTIONS_H
