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

#include <boost/regex.hpp>
#include <xalanc/XercesParserLiaison/XercesDocumentWrapper.hpp>

#include "DBFilter.h"
#include "../XPathHelper.h"
#include "Trace.h"
#include "ConnectionString.h"
#include "StringUtil.h"
#include "XmlUtil.h"

XALAN_USING_XALAN( XercesDocumentWrapper );
XALAN_USING_XALAN( XalanDocument );
#ifdef XALAN_1_9
XALAN_USING_XERCES( XMLPlatformUtils )
#endif
using namespace FinTP;

//Constructor
DbFilter::DbFilter() : AbstractFilter( FilterType::DB ), m_Provider(NULL), m_TransactionStarted( false )
{
}

//Destructor 
DbFilter::~DbFilter()
{
}

AbstractFilter::FilterResult DbFilter::ProcessMessage( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* inputOutputData, NameValueCollection& transportHeaders, bool asClient )
{
	if ( inputOutputData == NULL )
		throw runtime_error( "Input document is empty" );

	if ( asClient )
	{
#ifdef XALAN_1_9
		XercesDocumentWrapper docWrapper( *XMLPlatformUtils::fgMemoryManager, inputOutputData, true, true, true );
#else
		XercesDocumentWrapper docWrapper( inputOutputData, true, true, true );
#endif

		string theRequestor = XPathHelper::SerializeToString( XPathHelper::Evaluate( "/qPCMessageSchema/Message/RequestorService/child::text()", &docWrapper ) );

		UploadMessage( "", XmlUtil::SerializeToString( inputOutputData ), theRequestor+"Queue", "" );
	}
	else
		throw runtime_error( "asServer not implemented" );

	return AbstractFilter::Completed;
}

AbstractFilter::FilterResult DbFilter::ProcessMessage( AbstractFilter::buffer_type inputData, AbstractFilter::buffer_type outputData, NameValueCollection& transportHeaders, bool asClient )
{
	if ( asClient )
	{
		XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument *doc = XmlUtil::DeserializeFromString( inputData.get()->str() );
#ifdef XALAN_1_9
		XercesDocumentWrapper docWrapper( *XMLPlatformUtils::fgMemoryManager, doc, true, true, true );
#else
		XercesDocumentWrapper docWrapper( doc, true, true, true );
#endif

		string theRequestor = XPathHelper::SerializeToString( XPathHelper::Evaluate( "/qPCMessageSchema/Message/RequestorService/child::text()", &docWrapper ) );

		UploadMessage( "", inputData.get()->str(), theRequestor+"Queue", "" );
	}
	else
		throw runtime_error( "asServer not implemented" );	

	return AbstractFilter::Completed;
}

bool DbFilter::canLogPayload()
{
	return false;
}

void DbFilter::UploadMessage( const string& theDadFileName, const string& xmlData, const string& xmlTable, const string& hash )
{
	//Insert data into DB using a stored procedure
	ParametersVector myParams;
	DataParameterBase *paramDadFile = m_Provider->createParameter( DataType::CHAR_TYPE );
	myParams.push_back( paramDadFile );
	DataParameterBase *paramXmlBuffer = m_Provider->createParameter( DataType::LARGE_CHAR_TYPE );
	myParams.push_back( paramXmlBuffer );
	DataParameterBase *paramXmlTable = m_Provider->createParameter( DataType::CHAR_TYPE );
	myParams.push_back( paramXmlTable );
	DataParameterBase *paramLengthBuffer = m_Provider->createParameter( DataType::CHAR_TYPE );
	myParams.push_back( paramLengthBuffer );
	DataParameterBase *paramReturnCode = m_Provider->createParameter( DataType::CHAR_TYPE, DataParameterBase::PARAM_OUT );
	myParams.push_back( paramReturnCode );
	DataParameterBase *paramReturnMessage = m_Provider->createParameter( DataType::CHAR_TYPE, DataParameterBase::PARAM_OUT );
	myParams.push_back( paramReturnMessage );
		
	try
	{
		string dadBuffer = hash;

		DEBUG2( "DAD contents : " << dadBuffer );
		paramDadFile->setDimension( dadBuffer.length() );	  	 
		paramDadFile->setString( dadBuffer );
		paramDadFile->setName( "DAD_content" );

		DEBUG2( "XML buffer: " << xmlData );

		paramXmlBuffer->setDimension( xmlData.length() );
		paramXmlBuffer->setString( xmlData );

		paramXmlBuffer->setName( "XML_message" );

		// 3rd param : Table name used by Oracle only
		paramXmlTable->setDimension( xmlTable.length() );		
		paramXmlTable->setString( xmlTable );
		paramXmlTable->setName( "table_name" );
		
		// 4th param : Buffer size
		string bufferSize = StringUtil::ToString( xmlData.length() );
		paramLengthBuffer->setDimension( bufferSize.length() );
		paramLengthBuffer->setString( bufferSize );
		paramLengthBuffer->setName( "buffer_size" );

		// 5th param : return code from upload proc
		paramReturnCode->setDimension( 11 );
		
		// 6th param : return message from upload proc
		paramReturnMessage->setDimension( 1024 );

		// Begin transaction
		if ( !m_TransactionStarted )
			m_Database->BeginTransaction();
		m_TransactionStarted = true;

		// Execute the Stored Procedure that will call dxxShredXML procedure 
		m_Database->ExecuteNonQueryCached( DataCommand::SP, m_SPinsertXmlData, myParams ); 	
	   
		string returnCode = StringUtil::Trim( paramReturnCode->getString() );

		DEBUG( "Insert XML data return code : [" << returnCode << "]" );
		
		//sqlserver nonquery hack ( no return )
		if ( ( returnCode.length() > 0 ) && ( returnCode != "0" ) )
	 	{
	 		stringstream messageBuffer;
	  		messageBuffer << "Error inserting XML. Message : [" << paramReturnMessage->getString() << "]";
			TRACE( messageBuffer.str() );
			myParams.Dump();

	 		throw runtime_error( messageBuffer.str() );
	 	}	  
	}
	catch( const DBConnectionLostException& error )
	{
		stringstream messageBuffer;
	  	messageBuffer << typeid( error ).name() << " exception [" << error.what() << "]";
		TRACE( messageBuffer.str() );
		TRACE( "Original buffer [" << xmlData << "]" );
		throw;		
	}
	catch( const std::exception& error )
	{
		stringstream messageBuffer;
	  	messageBuffer << typeid( error ).name() << " exception [" << error.what() << "]";
		TRACE( messageBuffer.str() );
		TRACE( "Original buffer [" << xmlData << "]" );
		throw;
	}
	catch( ... )
	{
		TRACE( "An error occured while uploading the message to the database [unknown error]" );
		TRACE( "Original buffer [" << xmlData << "]" );
		throw;
	}
}

bool DbFilter::isMethodSupported( AbstractFilter::FilterMethod method, bool asClient )
{
	switch( method )
	{
	case AbstractFilter::XmlToXml:
				return true;
	case AbstractFilter::BufferToBuffer:
				return true;			
		default:
			return false;
	}
}

void DbFilter::Init( )
{
	if ( m_Properties.ContainsKey( "DatabaseProvider" ) )
	{
		if ( m_Provider != NULL )
			throw std::logic_error("DbFilter already initialized");

		m_DatabaseProvider = m_Properties[ "DatabaseProvider" ];
		m_Provider = DatabaseProvider::GetFactory( m_DatabaseProvider );
		m_Database = m_Provider->createDatabase();

		const string m_DatabaseName = m_Properties[ "DatabaseName" ];
		const string m_UserName = m_Properties[ "UserName" ];
		const string m_UserPassword = m_Properties[ "UserPassword" ];

		m_SPWatcher = m_Properties["SPWatcher"];
		m_SPinsertXmlData = m_Properties["SPinsertXmlData"];

		m_ConnectionString = ConnectionString( m_DatabaseName, m_UserName, m_UserPassword );
		m_Database->Connect( m_ConnectionString );
	}
}

void DbFilter::ValidateProperties( NameValueCollection& transportHeaders )
{
}

void DbFilter::Commit()
{
	if ( !m_SPmarkcommit.empty() )
		m_Database->ExecuteNonQueryCached( DataCommand::SP, m_SPmarkcommit );
	m_Database->EndTransaction( TransactionType::COMMIT );
	m_TransactionStarted = false;
}

void DbFilter::Abort()
{
	if ( !m_SPmarkabort.empty() )
	{
		m_Database->ExecuteNonQueryCached( DataCommand::SP, m_SPmarkabort );
		m_Database->EndTransaction( TransactionType::COMMIT );
	}
	else
		m_Database->EndTransaction( TransactionType::ROLLBACK );
	m_TransactionStarted = false;
}

void DbFilter::Rollback()
{
	m_Database->EndTransaction( TransactionType::ROLLBACK );
	m_TransactionStarted = false;
}

