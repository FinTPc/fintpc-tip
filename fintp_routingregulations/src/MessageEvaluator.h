#ifndef RRMESSAGEEVALUATOR_H
#define RRMESSAGEEVALUATOR_H

#include <map>
#include <vector>
#include <string>

using namespace std;
#include "XmlUtil.h"
#include "XPathHelper.h"

#include "WSRM/SequenceResponse.h"

#include "DllMainRoutingRegulations.h"

class ExportedRRObject MessageEvaluator 
{
	public :

#if defined( TESTDLL_EXPORT ) || defined ( TESTDLL_IMPORT )
		friend class PayloadEvalTest;
#endif

		enum EvaluatorType
		{
			Sepa_CreditTransfer,
			Sepa_Return,
			Sepa_Reject,
			Sag_StsReport,
			Sepa_InvalidMessage
		};
		
	protected :
				
		map< int, string > m_Fields;
		map< string, string > m_XPathFields;
					
	protected : 

		MessageEvaluator::EvaluatorType m_EvaluatorType;
		bool m_Valid;
		string m_Namespace;
		string m_MessageFilename;

		virtual string internalToString() = 0;
		virtual string internalGetFieldName( const int field ) = 0;
		virtual string internalGetField( const int field ) = 0;	

		MessageEvaluator& operator=( const MessageEvaluator& source );
		MessageEvaluator( const MessageEvaluator& source );
		MessageEvaluator( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* document, MessageEvaluator::EvaluatorType evaluatorType, const string& messageFilename );
		
		XALAN_CPP_NAMESPACE_QUALIFIER XercesDocumentWrapper *m_DocumentWrapper;
		XALAN_CPP_NAMESPACE_QUALIFIER XalanDocument *m_XalanDocument;
		XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument *m_Document;

		wsrm::SequenceResponse* m_SequenceResponse;

		void setNamespace( const string& namespaceValue ){ m_Namespace = namespaceValue; }
	
	public :
	
		static MessageEvaluator* getEvaluator( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* document, const string& messageFilename );
		static MessageEvaluator* getInvalidEvaluator( const string& errorCode, const string& errorMessage, const string& messageFilename );

		static MessageEvaluator::EvaluatorType Parse( const string& evaluatorType );

		// returns true if the message is in a SEPA XML Format and can be evaluated
		bool CheckPayloadType( MessageEvaluator::EvaluatorType payloadtype ) const { return m_EvaluatorType == payloadtype; }

		bool ValidateToSchema();
		virtual bool ValidateToXslt();
		virtual bool Validate() { return true; }
		virtual bool ValidateCharset();
		
		bool isValid() const { return m_Valid; }
		void setValid( const bool validStatus ) { m_Valid = validStatus; }
		
		string ToString();
		string getField( const int field );
		string getCustomXPath( const string& xpath, const bool noCache = false );

		string getNamespace() const { return m_Namespace; }
		
		virtual bool isReply() = 0;
		virtual bool isAck() = 0;
		virtual bool isNack() = 0;
		virtual bool isAck( const string ns )
		{
			if ( ns.length() == 0 )
				return isAck();
			return ( isAck() && ( ns == m_Namespace ) );
		}
		virtual bool isNack( const string ns )
		{
			if ( ns.length() == 0 )
				return isNack();
			return ( isNack() && ( ns == m_Namespace ) );
		}
		virtual bool isBatch() = 0;
		bool isValid() { return m_Valid; }

		virtual string getSchema() const = 0;
		virtual string getValidationXslt() const { return "../lib/SEPA/validationcheck.xslt"; }
		virtual string getReplyXslt() const { return "../lib/SEPA/SendRejectType1.xslt"; }

		virtual bool updateRelatedMessages() { return false; }
		virtual string getIssuer() { return ""; }
		
		virtual ~MessageEvaluator();
		
		XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* getDocument() const { return m_Document; }

		MessageEvaluator::EvaluatorType getEvaluatorType() const { return m_EvaluatorType; }
		
		// standards version
		virtual wsrm::SequenceResponse* getSequenceResponse() = 0;
		virtual string SerializeResponse();

		// Visitor method on message
		//virtual void UpdateMessage( RoutingMessage* message );
};
#endif
