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

#ifndef XMLUTIL_H
#define XMLUTIL_H

#include "DllMainUtils.h"
#include <map>
#include <string>
#include <pthread.h>

#ifdef WIN32
	#define __MSXML_LIBRARY_DEFINED__
#endif

#include <xercesc/util/XMLString.hpp>
#include <xercesc/dom/DOM.hpp>
//#include <xercesc/dom/DOMBuilder.hpp>
//#include <xercesc/parsers/DOMBuilderImpl.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/framework/MemBufFormatTarget.hpp>
#include <xercesc/framework/Wrapper4InputSource.hpp>
#include <xercesc/framework/StdOutFormatTarget.hpp>
#include <xercesc/dom/DOMErrorHandler.hpp>
#include <xercesc/util/XercesDefs.hpp>
#include <xercesc/sax/ErrorHandler.hpp>
#include <xercesc/util/TransService.hpp>

using namespace std;
XERCES_CPP_NAMESPACE_USE

namespace FinTP
{
	//Contains:
	//		StrX, XStr					- transcoding
	//		XmlUtil 					- serialize / deserialize to/from string
	//		DOMWriterTreeErrorHandler	- error handler for DOMWriter
	//		XercesDOMTreeErrorHandler	- error handler for XERCESDOMParser

	// ---------------------------------------------------------------------------
	//  This is a simple class that lets us do easy (though not terribly efficient)
	//  trancoding of char* data to XMLCh data.
	// ---------------------------------------------------------------------------
	class ExportedUtilsObject XStr
	{
		public :
			//  Constructors and Destructor
			explicit XStr( const char* toTranscode )
			{
				// Call the private transcoding method
#if ( XERCES_VERSION_MAJOR >= 3 )
				if ( m_SourceEncoding.empty() || toTranscode == NULL )
					fUnicodeForm = XMLString::transcode( toTranscode );
				else
				{
					TranscodeFromStr transcoder( reinterpret_cast<const XMLByte*>( toTranscode ), strlen( toTranscode ), m_SourceEncoding.c_str() );
					fUnicodeForm = transcoder.adopt();
				}
#else
				fUnicodeForm = XMLString::transcode( toTranscode );
#endif
			}
		    
			explicit XStr( const std::string& toTranscode )
			{
				// Call the private transcoding method
#if ( XERCES_VERSION_MAJOR >= 3 )
				if ( m_SourceEncoding.empty() || toTranscode.empty()  )
					fUnicodeForm = XMLString::transcode( toTranscode.c_str() );
				else
				{
					TranscodeFromStr transcoder( reinterpret_cast<const XMLByte*>( toTranscode.c_str() ), toTranscode.size(), m_SourceEncoding.c_str() );
					fUnicodeForm = transcoder.adopt();
				}
#else
				fUnicodeForm = XMLString::transcode( toTranscode.c_str() );
#endif
			}
			
			~XStr()
			{
				try
				{
					XMLString::release( &fUnicodeForm );
				}catch(...){};
			}
		
			//  Getter methods
			const XMLCh* uniForm() const
			{
				return fUnicodeForm;
			}
		    
			static string m_SourceEncoding;

		private :
			//  Private data members
			//  fUnicodeForm - This is the Unicode XMLCh format of the string.
			XMLCh* fUnicodeForm;
			XStr() : fUnicodeForm( NULL ) {};
	};

	class StrX
	{
		public :
			// -----------------------------------------------------------------------
			//  Constructors and Destructor
			// -----------------------------------------------------------------------
			explicit StrX( const XMLCh* toTranscode )
			{
				// Call the private transcoding method
				fLocalForm = XMLString::transcode( toTranscode );
			}
		
			~StrX()
			{
				try
				{
					XMLString::release( &fLocalForm );
				}catch( ... ){};
			}
		
		
			// -----------------------------------------------------------------------
			//  Getter methods
			// -----------------------------------------------------------------------
			const char* locForm() const
			{
				return fLocalForm;
			}
		
		private :
			// -----------------------------------------------------------------------
			//  Private data members
			//
			//  fLocalForm
			//      This is the local code page form of the string.
			// -----------------------------------------------------------------------
			char* fLocalForm;
			StrX() : fLocalForm( NULL ) {};
	};

	#define unicodeForm( str ) ( XStr( str ).uniForm() )
	#define localForm( str ) ( StrX( str ).locForm() )

	class ExportedUtilsObject XercesDOMTreeErrorHandler : public ErrorHandler
	{
		public:
			// -----------------------------------------------------------------------
			//  Constructors and Destructor
			// -----------------------------------------------------------------------
			XercesDOMTreeErrorHandler() : m_SawErrors( false )
			{
			}
		
			~XercesDOMTreeErrorHandler()
			{
			}
		
		
			// -----------------------------------------------------------------------
			//  Implementation of the error handler interface
			// -----------------------------------------------------------------------
			void warning( const SAXParseException& toCatch );
			void error( const SAXParseException& toCatch );
			void fatalError( const SAXParseException& toCatch );
			void resetErrors();
		
			// -----------------------------------------------------------------------
			//  Getter methods
			// -----------------------------------------------------------------------
			bool getSawErrors() const { return m_SawErrors; }

			string getLastError() const { return m_LastError; }
		
		private :
			// -----------------------------------------------------------------------
			//  Private data members
			//
			//  fSawErrors
			//      This is set if we get any errors, and is queryable via a getter
			//      method. Its used by the main code to suppress output if there are
			//      errors.
			// -----------------------------------------------------------------------
			bool m_SawErrors;
			string m_LastError;
	};

	class DOMWriterTreeErrorHandler : public DOMErrorHandler
	{
		public:
		
			DOMWriterTreeErrorHandler(){};
			~DOMWriterTreeErrorHandler(){};
		
			/** @name The error handler interface */
			bool handleError( const DOMError& domError );
			void resetErrors() const{};
		
		private :
			/* Unimplemented constructors and operators */
			explicit DOMWriterTreeErrorHandler( const DOMErrorHandler& );
			DOMWriterTreeErrorHandler& operator=( const DOMWriterTreeErrorHandler& );
	};

	class ExportedUtilsObject XmlUtil
	{
		public:
			
			static string SerializeToString( const XERCES_CPP_NAMESPACE_QUALIFIER DOMNode* doc, XERCES_CPP_NAMESPACE_QUALIFIER DOMImplementationLS* impl, int prettyPrint = 0 );
			static string SerializeToString( const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* document );
			static string SerializeToString( const XERCES_CPP_NAMESPACE_QUALIFIER DOMNode* node );
			static string SerializeNodeToString( const XERCES_CPP_NAMESPACE_QUALIFIER DOMNode* node, int prettyPrint = 0 );
			
			static XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* DeserializeFromFile( const string& filename );
			static XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* DeserializeFromString( const string& buffer );
			static XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* DeserializeFromString( const unsigned char* buffer, const unsigned long bufferSize );
			
			static void printDOM( const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc );
			
			static string getNamespace( const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc );

			static string XMLChtoString( const XMLCh* inputXml );
			~XmlUtil();
			
			static void Terminate();
			
		private :
		
			XmlUtil();
		
			XercesDOMParser* m_Parser;
			XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument *m_ParseDocument;	
			XercesDOMTreeErrorHandler* m_ErrorReporter;
			XMLTranscoder* m_UTF8Transcoder;
		
			//static pthread_mutex_t InstanceSyncMutex;	
			//static map< pthread_t, XmlUtil* > m_Instance;
			static pthread_once_t KeysCreate;
			static pthread_key_t InstanceKey;

			static void CreateKeys();
			static void DeleteInstances( void* data );

			// the one and only instance
			static XmlUtil Instance;
			static bool m_Initialized;

			static XmlUtil* getInstance();
			
			XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* internalDeserialize( const unsigned char* buffer, const unsigned long bufferSize );
			XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* internalDeserialize( const string& filename );

			void ReleaseParser();
			void ReleaseUTF8Transcoder();
			void CreateParser();
			void CreateUTF8Transcoder();
	};
}

#endif // XMLUTIL_H
