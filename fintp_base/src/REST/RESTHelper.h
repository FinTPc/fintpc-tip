/*
* FinTP - Financial Transactions Processing Application
* Copyright (C) 2013 Business Information Systems (Allevo) S.R.L.
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* ( at your option ) any later version.
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

#ifndef RESTHELPER_H
#define RESTHELPER_H


#include <string>
#include <vector>
#include <curl/curl.h>

#include "../DllMain.h"
#include "WorkItemPool.h"


using namespace std;


namespace FinTP
{
	/**
	 * \brief Purpose:  Single Host Server REST operations
	 * 		  Strategy: Operates without preparations and react if error. 
	 * 					Throws after 3 fails. Try to reset everything on FATAL
	 * 					One connect for all operations
	 *
	 */
	class ExportedObject RESTcURLHelper
	{
		private :

			static const char CA_CERT_FILE[];
		
			//vector<unsigned char> m_ResponseBuffer;
			//vector<unsigned char> m_RequestBuffer;
			vector<string> m_HeaderBuffer;
									
			//cURL settings
			map<string, string> m_HeaderFields;
			//map<string, string> m_CustomHeaderFields;
			string m_HttpContentType;
			bool m_SSLEnabled;

			//string m_CertificatePhrase;
			//string m_ClientCertificate;
			double m_LastRequestTime;

			static int writerCallback( char *data, size_t size, size_t nmemb, void* writerData );
			static int headerCallback( char *data, size_t size, size_t nmemb, void* headerBuffer );
			void closeConnection();
			void setTLSOptions();

		protected:

			CURL* m_ConnsHandle;
			curl_slist* m_RequestHeaderFields;
			char m_ErrorBuffer[CURL_ERROR_SIZE];

			/**
			 * Http channel https://<host>:<port>/<resource_root>
			 */
			string m_HttpChannel;

			int internalPerform();

		public :

			enum HttpContentType
			{
				UNMGT = 0, 
				TXT = 1,
				XML = 2,
				SOAP = 3,
				JSON = 4
			};

			static RESTcURLHelper::HttpContentType parseContentType( const string httpContentType );

			RESTcURLHelper();
			~RESTcURLHelper();

			void connect( const string& httpChannel, const string& certFile = "", const string& certPhrase = "" , bool force = false );
			void disconnect();
			
			/**
			 * interface declared get messages
			 */
			virtual int getOne( const string& resourcePath, vector<unsigned char>& outputBuffer );
			/**
			* interface declared put messages
			*/
			int putOne( const string& resourcePath, ManagedBuffer* inputBuffer, ManagedBuffer* outputBuffer );
			/**
			* interface declared post messages but no response body write out
			*/
			int postOne( const string& resourcePath, ManagedBuffer* inputBuffer, const bool headerReset = true );
			/**
			 * interface declared post messages
			 */
			int postOne( const string& resourcePath, ManagedBuffer* inputBuffer, vector<unsigned char>& responseBuffer, const bool headerReset = true  );
			
			double getLastRetrievingTime(){ return m_LastRequestTime; };
			void setContentType( const HttpContentType contentType = RESTcURLHelper::UNMGT );
			void SSLDisable() { m_SSLEnabled = false; };
			//TODO: set basic authentication from url content
			void setRequestHeaderItem( const string& headerItem, const bool force = false );
			string getHeaderField( const string& field );
			/**
			 * Using the same CURL handle with different headers requires reset on each http method switch
			 */
			virtual void resetHeader();
			
			bool commit();
			bool abort();
	};

	class ExportedObject IPSHelper: public RESTcURLHelper
	{
		private :

			enum HeaderFields
			{ 
				REQ_STS = 0,
				REQ_DUPLICATE = 1,
				MSG_SEQ  = 2,
				MSG_TYPE = 3,
				VERSION = 4
			};


			static const int HEADER_FIELDS = 7;
			/**
			 * IP( IPS-Application ) Headers
			 */
			static const char* m_HeaderFields[HEADER_FIELDS];
			static const char REQUEST_CHANNEL[];
			static const char m_ConfirmationBody[];
			static const string REQUEST_VERSION;
			static const string REQUEST_AKN_SEQUENCE;

			string m_ClientId;
		
		public :

			IPSHelper() :  m_ClientId( "" ){};
			void resetHeader();
			string getLastRequestSequence(){ return getHeaderField( m_HeaderFields[IPSHelper::MSG_SEQ] ); };
			string getLastMessageType(){ return getHeaderField( m_HeaderFields[IPSHelper::MSG_TYPE] ); };
			void setClientId( const string& clientId ) { m_ClientId = clientId; };
			/**
			* interface declared request/confirmation
			*/
			//int postOneReply( const string& resourcePath, ManagedBuffer* inputBuffer, ManagedBuffer* outputBuffer );
			int postOneConfirmation( const string& resourcePath, const string& messageId );
			int getOne( const string& resourcePath, vector<unsigned char>& outputBuffer );
			/**
			* Specialized behaviors related to last request
			* !!! Not used yet, in current design
			*/
			/*
			bool requiresSecurityCheck();
			bool requiresConfirmation();
			bool requiresReply();
			*/
	};
}

#endif
