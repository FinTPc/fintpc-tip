#pragma once
#include "DllMainRoutingRegulations.h"

#include "MessageEvaluator.h"
/*
<PrevalidationChecks>
	-- choice <Sequence>, <SequenceFault>, <SequenceAcknowledgement> .. from here it's like wsrm.xsd
	wsrm.xsd will be modified to include SEPA fault codes, but the structure will be the same
</PrevalidationCheks>
*/

class ExportedRRObject SepaRules
{
	public :

		typedef enum
		{
			NONE = 0,
		} SEPA_RULES;
			
		SepaRules( const string& options ){ m_Options = Parse( options ); }
		SepaRules( int options = SepaRules::NONE ){ m_Options = options; }
		
		static int Parse( const string& options );
		static string ToString( int options );
		
		int getValue() const { return m_Options; }

	private :

		// this is based on the XSLT setting a flag that can be easily checked in the code
		bool isAttributePresent( const string& xpath );
		
		int m_Options;
};

class ExportedRRObject SepaOptions
{
	public :

		typedef enum
		{
			NONE = 0,
		} SEPA_OPTIONS;
			
		SepaOptions( const string& options ){ m_Options = Parse( options ); }
		SepaOptions( int options = SepaOptions::NONE ){ m_Options = options; }
		
		static int Parse( const string& options );
		static string ToString( int options );
		
		int getValue() const { return m_Options; }

	private :
		
		int m_Options;
};

class ExportedRRObject ValidationActions
{
	public :

		typedef enum
		{
			NONE = 0,
		} VALIDATION_ACTIONS;
			
		ValidationActions( const string& options ){ m_Options = Parse( options ); }
		ValidationActions( int options = ValidationActions::NONE ){ m_Options = options; }
		
		static int Parse( const string& options );
		static string ToString( int options );
		
		int getValue() const { return m_Options; }

	private :
		
		int m_Options;
};

class ExportedRRObject Regulations
{
	public :
		Regulations(){};

		// makes no assumptions on the message ( may be invalid XML or not compliant with the TVS Schema 
		bool Type1RejectValidation( const string& options, const string& message );
		bool Type2RejectValidation( const string& options, const string& message );

		MessageEvaluator* Validate( const string& options, const string& message, const string& messageFilename );

	protected :

		XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* ValidateString( const string& options, const string& message );
		MessageEvaluator* Validate( const string& options, XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* message, const string& messageFilename );
};

