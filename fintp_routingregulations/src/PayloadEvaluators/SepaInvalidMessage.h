#pragma once
#include "../DllMainRoutingRegulations.h"

#include "../MessageEvaluator.h"

class ExportedRRObject SepaInvalidMessage : public MessageEvaluator
{
	public :
		SepaInvalidMessage( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* document, const string& errorCode, const string& message, const string& messageFilename );
		~SepaInvalidMessage();

	private:

		unsigned int m_AckType; // tristate value ( 0-notack, 1-ack, 2, not evaluated )

	protected :
		
		string internalToString(){ return ""; }
		string internalGetFieldName( const int field ){ return ""; }
		string internalGetField( const int field ){ return ""; }

	public :
			
		bool isReply() { return true; }
		bool isAck() { return false; }
		bool isNack() { return true; }

		bool isBatch() { return true; }

		string getSchema() const { return ""; }

		// standards version
		wsrm::SequenceResponse* getSequenceResponse() { return m_SequenceResponse; }
};