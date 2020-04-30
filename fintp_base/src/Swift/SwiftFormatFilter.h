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

#ifndef SWIFTFORMATFILTER_H
#define SWIFTFORMATFILTER_H

#include "../AbstractFilter.h"
#include "XmlUtil.h"
#include <unicode/uchriter.h>

namespace FinTP
{
	class SwiftMediator
	{
		public:

			virtual ~SwiftMediator(){};
			void virtual FetchPreparation( ManagedBuffer* inputBuffer, ManagedBuffer* outputBuffer, const string& key, const string& digest ) = 0;
			void virtual PublishPreparation( ManagedBuffer* inputBuffer, ManagedBuffer* outputBuffer, const string& key, const string& digest ) = 0;

	};

	class FINMediator : public SwiftMediator
	{
		public:
			/**
			 * Security check only. ( no leading/trailing characters format )
			 */
			void FetchPreparation( ManagedBuffer* inputBuffer, ManagedBuffer* outputBuffer, const string& key, const string& digest );
			void PublishPreparation( ManagedBuffer* inputBuffer, ManagedBuffer* outputBuffer, const string& key, const string& digest );

		private:
			
			/**
			 * Mandatory to filter all legitimate messages for security check
			 * Clean FIN      : Check security markers
			 * Prefixed FIN   : Skip security markers check, return input
			 * Not FIN        : Skip security markers check, return input
			 */
			bool isSuportedFormat( const string& inputMessage );

	};

	class PCCMediator : public SwiftMediator
	{
		public:
			
			void FetchPreparation( ManagedBuffer* inputBuffer, ManagedBuffer* outputBuffer, const string& key, const string& digest );
			void PublishPreparation( ManagedBuffer* inputBuffer, ManagedBuffer* outputBuffer, const string& key, const string& digest );

		private:

			/**
			 * PCC trailing blanc spaces
			 */
			string addWhitespaces( const string& inputString );

	};

	class StrictFormatMediator : public SwiftMediator
	{
		public:

			void FetchPreparation( ManagedBuffer* inputBuffer, ManagedBuffer* outputBuffer, const string& key, const string& digest );
			void PublishPreparation( ManagedBuffer* inputBuffer, ManagedBuffer* outputBuffer, const string& key, const string& digest );

		private:

			/**
			 * Mandatory to filter all legitimate messages for security check
			 * PAYLOAD and PDU   : Check security markers
			 * PAYLOAD           : Skip security markers check, return input
			 * PDU               : Skip security markers check, return input
			 */
			bool isStrictFormat( const ManagedBuffer& managedBuffer );

	};

	class MXMediator : public SwiftMediator
	{
		public:

			/**
			 * Security check only. ( no leading/trailing characters format )
			 */
			void FetchPreparation( ManagedBuffer* inputBuffer, ManagedBuffer* outputBuffer, const string& key, const string& digest );
			void PublishPreparation( ManagedBuffer* inputBuffer, ManagedBuffer* outputBuffer, const string& key, const string& digest );

		private:

			string Canonicalize( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc ) const;
			bool CheckLAU( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* DataPDU, const string& key ) const;
			/**
			 * Mandatory to filter all legitimate messages for security check
			 * PAYLOAD and PDU   : Check security markers
			 * PAYLOAD           : Skip security markers check, return input
			 * PDU               : Skip security markers check, return input
			 */
			bool isSuportedFormat( const string& payloadDigest );
			

	};

	class ExportedObject IPMediator : public SwiftMediator
	{
		public:

			enum AlgorithmType
			{
				UKN_ALGORITHM = 0,
				RSA_ALGORITHM = 1,
				DSA_ALGORITHM = 2,
				DH_ALGORITHM = 3,
				ECC_ALGORITHM =	4
			};

			/**
			* Security check only. ( no leading/trailing characters format )
			*/
			void FetchPreparation( ManagedBuffer* inputBuffer, ManagedBuffer* outputBuffer, const string& key, const string& digest );
			void PublishPreparation( ManagedBuffer* inputBuffer, ManagedBuffer* outputBuffer, const string& key, const string& digest );

		private:

			string Canonicalize( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc ) const;
			bool CheckSignature( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* messageData, const string& key ) const;
			/**
			* Mandatory to filter all legitimate messages for security check
			* PAYLOAD and PDU   : Check security markers
			* PAYLOAD           : Skip security markers check, return input
			* PDU               : Skip security markers check, return input
			*/
			bool isSuportedFormat( const string& payloadDigest );
	};
	class ExportedObject SwiftFormatFilter : public AbstractFilter
	{
		public:

			static const string LAU_KEY;
			static const string SERVICE_KEY;
			static const string PAYLOAD_DIGEST;
			static const string CERTIFICATE_PATH;
			/* APP_KEY values */
			static const string	SAA_FILEACT;
			static const string	SAA_FILEACTXML;
			static const string	SAA_FILEACTMX;
			static const string	SAA_FILEACTIP;
			static const string SAA_FIN;
			static const string SAA_FINPCC;
			static const string	SAE;


			SwiftFormatFilter(): AbstractFilter( FilterType::SWIFTFORMAT ) {}
			~SwiftFormatFilter() {}

			bool canLogPayload() { return false; }

			bool isMethodSupported( FilterMethod method, bool asClient );

			FilterResult ProcessMessage( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* inputOutputData, NameValueCollection& transportHeaders, bool asClient );

			FilterResult ProcessMessage( unsigned char* inputData, XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* outputData, NameValueCollection& transportHeaders, bool asClient );

			FilterResult ProcessMessage( const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* inputData, AbstractFilter::buffer_type outputData, NameValueCollection& transportHeaders, bool asClient );

			FilterResult ProcessMessage( const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* inputData, unsigned char** outputData, NameValueCollection& transportHeaders, bool asClient );

			FilterResult ProcessMessage( AbstractFilter::buffer_type inputData, XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* outputData, NameValueCollection& transportHeaders, bool asClient );

			FilterResult ProcessMessage( AbstractFilter::buffer_type inputData, AbstractFilter::buffer_type outputData, NameValueCollection& transportHeaders, bool asClient );

			FilterResult ProcessMessage( AbstractFilter::buffer_type inputData, unsigned char** outputData, NameValueCollection& transportHeaders, bool asClient );

			static UnicodeString EscapeNonSwiftChars( UnicodeString& in, const string& replacement = "."  );
			static void EscapeNonSwiftChars( DOMElement* currentItemRoot, const string& replacement = "."  );
			static string EscapeNonSwiftChars( const string& in, const string& replacement = "."  );

		private:

			bool isStrictFormat( const ManagedBuffer& managedBuffer );

			auto_ptr<SwiftMediator> GetSwiftMediator(const string& service);

			void ValidateProperties( NameValueCollection& transportHeaders );
	};
}


#endif
