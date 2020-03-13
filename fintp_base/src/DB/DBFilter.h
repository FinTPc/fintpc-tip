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

#ifndef DBFILTER_H
#define DBFILTER_H

#include "../AbstractFilter.h"
#include "DatabaseProvider.h"

namespace FinTP
{
	class ExportedObject DbFilter : public AbstractFilter
	{
		public:
			
			DbFilter();
			~DbFilter();
			
			// return true if the filter supports logging payload to a file
			// note for overrides : return true/false if the payload can be read without rewinding ( not a stream )
			bool canLogPayload();
			
			// return true if the filter can execute the requested operation in client/server context
			bool isMethodSupported( FilterMethod method, bool asClient );
			
			// if DOMDocument/buffer is validated returns DOMDocument
			// if not throw an exception
			FilterResult ProcessMessage( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* inputOutputData, NameValueCollection& transportHeaders, bool asClient );
			FilterResult ProcessMessage( unsigned char* inputData, XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* outputData, NameValueCollection& transportHeaders, bool asClient )
			{
				throw FilterInvalidMethod( AbstractFilter::BufferToXml );
			}
			
			FilterResult ProcessMessage( const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* inputData, AbstractFilter::buffer_type outputData, NameValueCollection& transportHeaders, bool asClient )
			{
				throw FilterInvalidMethod( AbstractFilter::XmlToBuffer );
			}

			FilterResult ProcessMessage( const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* inputData, unsigned char** outputData, NameValueCollection& transportHeaders, bool asClient )
			{
				throw FilterInvalidMethod( AbstractFilter::XmlToBuffer );
			}
			
			FilterResult ProcessMessage( AbstractFilter::buffer_type inputData, XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* outputData, NameValueCollection& transportHeaders, bool asClient )
			{
				throw FilterInvalidMethod( AbstractFilter::BufferToXml );
			}

			FilterResult ProcessMessage( AbstractFilter::buffer_type inputData, AbstractFilter::buffer_type outputData, NameValueCollection& transportHeaders, bool asClient );
			
			FilterResult ProcessMessage( AbstractFilter::buffer_type inputData, unsigned char** outputData, NameValueCollection& transportHeaders, bool asClient )
			{
				throw FilterInvalidMethod( AbstractFilter::BufferToBuffer );
			}

			void Init();
			void Rollback();
			void Commit();
			void Abort();

			string getTransportURI() const { return m_URI; }
			string getSelectSPName() const { return m_SPWatcher; }
			string getDatabaseName() const { return m_DatabaseName; }
			string getUserName() const { return m_UserName; }
			string getUserPassword() const { return m_UserPassword; }
			string getProvider() const { return m_DatabaseProvider; }
			ConnectionString getConnectionString() const { return m_ConnectionString; }

		private:

			void ValidateProperties( NameValueCollection& transportHeaders );
			void UploadMessage( const string& theDadFileName, const string& xmlData, const string& xmlTable, const string& hash );

			bool m_TransactionStarted;

			Database* m_Database;
			DatabaseProviderFactory* m_Provider;

			string m_SPmarkcommit, m_SPmarkabort, m_SPWatcher;
			string m_SPinsertXmlData;
			string m_URI;

			string m_DatabaseProvider, m_DatabaseName, m_UserName, m_UserPassword;
			ConnectionString m_ConnectionString;
	};
}


#endif
