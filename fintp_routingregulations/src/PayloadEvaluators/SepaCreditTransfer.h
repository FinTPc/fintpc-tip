#pragma once
#include "../DllMainRoutingRegulations.h"
#include "../MessageEvaluator.h"

class ExportedRRObject SepaCreditTransfer : public MessageEvaluator
{
	public :
		SepaCreditTransfer( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* document, const string& messageFilename  );
		~SepaCreditTransfer();

	private:

		unsigned int m_AckType; // tristate value ( 0-notack, 1-ack, 2, not evaluated )

	protected :
		
		string internalToString(){ return ""; }
		string internalGetFieldName( const int field ){ return ""; }
		string internalGetField( const int field ){ return ""; }	

	public :
			
		bool isReply() { return false; }
		bool isAck() { return false; }
		bool isNack() { return false; }

		bool isBatch() { return true; }

		string getSchema() const { return "../lib/SEPA/$pacs.008.001.01.xsd"; }

		bool Validate();

		// standards version
		wsrm::SequenceResponse* getSequenceResponse() { return m_SequenceResponse; }
};