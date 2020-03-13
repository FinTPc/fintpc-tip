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

#ifdef WIN32
	/*#ifdef _DEBUG
		#define _CRTDBG_MAP_ALLOC
		#include <stdlib.h>
		#include <crtdbg.h>
	#endif*/
	#define __MSXML_LIBRARY_DEFINED__
	#include "windows.h"
	#define sleep(x) Sleep( (x)*1000 )
#else
	#include <unistd.h>
#endif

#include <sstream>
//#include <iostream>

using namespace std;

#include "AppSettings.h"
#include "Trace.h"
#include "PlatformDeps.h"
#include "XmlUtil.h"
#include "StringUtil.h"
#include "ConnectionString.h"
#include "Collaboration.h"
#include "MQ/MqFilter.h"
#include "XSLT/ExtensionUrl.h"

#include "DbFetcher.h"

DbFetcher* DbFetcher::m_Me = NULL;

//constructor
DbFetcher::DbFetcher() : Endpoint(), m_Watcher( DbFetcher::NotificationCallback ),
m_CurrentRowId( "" ), m_UncommitedTrns( 0 ), m_MaxUncommitedTrns( -1 ), m_CurrentMessage( NULL ), m_CurrentMessageStr( NULL ),
	m_NotificationTypeXML( false ), m_DatabaseProvider( "" ), m_DatabaseName( "" ), m_UserName( "" ), m_UserPassword( "" ),
	m_TableName( "" ), m_SPmarkforprocess( "" ), m_SPselectforprocess( "" ), m_SPmarkcommit( "" ), m_SPmarkabort( "" ),	m_SPWatcher( "" ),
	m_CurrentDatabase( NULL ), m_CurrentProvider( NULL ), m_Rollback( false ), m_SavedMessage( NULL ), m_DatabaseToXmlTrimm ( true )
{	
	DEBUG2( "CONSTRUCTOR" );
}

//destructor
DbFetcher::~DbFetcher()
{
	DEBUG2( "DESTRUCTOR" );
	try
	{
		if ( m_CurrentDatabase != NULL )
		{
			delete m_CurrentDatabase;
			m_CurrentDatabase = NULL;
		}
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while releasing current database" );
		} catch( ... ){}
	}

	try
	{
		if ( m_CurrentProvider != NULL )
		{
			delete m_CurrentProvider;
			m_CurrentProvider = NULL;
		}
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while releasing current database provider" );
		} catch( ... ){}
	}

	try
	{
		if ( m_CurrentMessage != NULL )
		{
			delete m_CurrentMessage;
			m_CurrentMessage = NULL;
		}
		if ( m_SavedMessage != NULL )
		{
			delete m_SavedMessage;
			m_SavedMessage = NULL;
		}
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while releasing current message" );
		} catch( ... ){}
	}
}

void DbFetcher::Init()
{
	//TODO check delete rights on m_WatchPath - can't delete source otherwise
	DEBUG( "INIT" );
	INIT_COUNTERS( &m_Watcher, DBFetcherWatcher );

	// Read application settings 
	m_DatabaseProvider = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::DBPROVIDER );
	m_DatabaseName = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::DBNAME );
		
	m_UserName = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::DBUSER );
	m_UserPassword = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::DBPASS );
	m_TableName = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::TABLENAME );
	
	FunctionUrl::SSLCertificateFileName = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::SSLCERTIFICATEFILENAME, "" );
	FunctionUrl::SSLCertificatePasswd = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::SSLCERTIFICATEPASSWD, "" );

	string dateFormat = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::DATEFORMAT, "" );
	if( dateFormat.length() > 0 ) 
		Database::DateFormat = dateFormat;

	string timestampFormat = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::TIMESTAMPFORMAT, "" );
	if( timestampFormat.length() > 0 ) 
		Database::TimestampFormat = timestampFormat;

	m_SPselectforprocess = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::SPSELECT, "" );
	m_SPmarkforprocess = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::SPMARK, "" );

	string maxUncommitedTrns = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::UNCMTMAX, "" );
	m_MaxUncommitedTrns = ( maxUncommitedTrns.length() > 0 ) ? StringUtil::ParseInt( maxUncommitedTrns ) : -1;

	string notificationType = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::NOTIFTYPE, "XML" );
	m_NotificationTypeXML = ( notificationType == "XML" );

	string dbResultSetTrimm = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::DBTOXMLTRIMM, "true" );
	m_DatabaseToXmlTrimm = ( dbResultSetTrimm == "true" );

	// if mark for process is missing, the watcher will return the full object, not just the notification
	if( m_SPmarkforprocess.length() == 0 )
	{
		m_Watcher.setFullObjectNotification( true );
		m_Watcher.setCallback( NULL );
		m_Watcher.setNotificationPool( &m_NotificationPool );
	}

	m_SPmarkcommit = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::SPMARKCOMMIT );
	m_SPmarkabort = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::SPMARKABORT );
	m_SPWatcher = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::SPWATCHER );
	
	//Set Watcher
	m_Watcher.setProvider( m_DatabaseProvider );
	m_Watcher.setConnectionString( ConnectionString( m_DatabaseName, m_UserName, m_UserPassword ) );
	m_Watcher.setSelectSPName( m_SPWatcher );

	if ( m_NotificationTypeXML )
	{
		m_Watcher.setNotificationType( AbstractWatcher::NotificationObject::TYPE_XMLDOM );
	}
	else
	{
		m_Watcher.setNotificationType( AbstractWatcher::NotificationObject::TYPE_CHAR );
		m_Watcher.setWatchOptions( DbWatcher::ReturnCursorXml );
	}
	
	try
	{
		//Create Database Provider
		m_CurrentProvider = DatabaseProvider::GetFactory( m_DatabaseProvider );

		//Create Database
		m_CurrentDatabase = m_CurrentProvider->createDatabase();
	}
	catch( const std::exception& error )
	{
		DEBUG( "Create database error : [" << error.what() << "]" );
		throw;
	}
	catch( ... )
	{
		DEBUG( "Unknown error while creating database" );
		throw;
	}
}

void DbFetcher::internalStart()
{
	m_Me = this;
	DEBUG( "Starting watcher... " );

	m_Watcher.setEnableRaisingEvents( true );

	TRACE_SERVICE( "[" << m_ServiceThreadId << "] starting to process... " );

	// if the mark for process is missing from config, the connector will use the notificatin pool
	// instead of the callback
	if ( m_SPmarkforprocess.length() == 0 ) 
	{
		try
		{
			while( m_Running )
			{
				DEBUG( "Fetcher [" << m_SelfThreadId << "] waiting for notifications in pool" );
				WorkItem< AbstractWatcher::NotificationObject > notification = m_NotificationPool.removePoolItem();
				
				AbstractWatcher::NotificationObject *notificationObject = notification.get();
				
				m_CurrentRowId = notificationObject->getObjectId();

				bool succeeded = true;
				do
				{
					if( m_NotificationTypeXML )
					{
						if( !m_Rollback )
						{
							m_CurrentMessage = ( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument * )( notificationObject->getObject() );
							m_CurrentMessageStr = NULL;
							m_SavedMessage = ( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument * )m_CurrentMessage->cloneNode( true ); //copy document
						}
						else
						{
							m_Rollback = false;
							m_CurrentMessage = ( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument * )m_SavedMessage->cloneNode( true );
							m_CurrentMessageStr = NULL;
						}
						DEBUG2( "Message : [" << XmlUtil::SerializeToString( m_CurrentMessage ) << "]" );
					}
					else
					{
						m_SavedMessage= NULL;
						m_CurrentMessage = NULL;
						m_CurrentMessageStr = ( ManagedBuffer* )notificationObject->getObject();
					}

					//m_CurrentMessageLength = notificationObject->getObjectSize();
			
					DEBUG( "Notified : [" << m_CurrentRowId << "] in group [" << notificationObject->getObjectGroupId() << "]" );
			
					//TODO throw only on fatal error. The connector should respawn this thread
					succeeded = true;
					try
					{
						DEBUG( "Performing message loop ... " );
						succeeded = PerformMessageLoop( m_BatchManager != NULL );
						DEBUG( "Message loop finished ok. " );
					}
					catch( const AppException& ex )
					{
						string exceptionType = typeid( ex ).name();
						string errorMessage = ex.getMessage();
						
						TRACE_GLOBAL( exceptionType << " encountered while processing message : " << errorMessage );
						succeeded = false;
					}
					catch( const std::exception& ex )
					{
						string exceptionType = typeid( ex ).name();
						string errorMessage = ex.what();
						
						TRACE_GLOBAL( exceptionType << " encountered while processing message : " << errorMessage );
						succeeded = false;
					}
					catch( ... )
					{
						TRACE_GLOBAL( "[unknown exception] encountered while processing message. " );
						succeeded = false;
					}

					if( !m_Rollback && ( m_SavedMessage != NULL ) )
					{
						m_SavedMessage->release();
						m_SavedMessage = NULL;
					}

					if( m_CurrentMessage != NULL )
					{
						m_CurrentMessage->release();
						m_CurrentMessage = NULL;
					}

					if ( !succeeded && m_Running )
					{
						TRACE( "Sleeping 10 seconds before next attempt( previous message failed )" );
						sleep( 10 );
					}
					// repeat untill we abort
				} while ( !succeeded && m_Running && m_Rollback );
			} //while true
		}
		catch( const WorkPoolShutdown& shutdownError )
		{
			TRACE_GLOBAL( shutdownError.what() );
		}
		catch( ... )
		{
			TRACE_GLOBAL( "Unhandled exception encountered while processing. " );
		}
	}
}

void DbFetcher::internalStop()
{
	DEBUG( "STOP" );
	
	// ensure watcher is dead ( the watcher will lock on pool and wait until it is empty )
	m_Watcher.setEnableRaisingEvents( false );

	// wait for fetcher to remove all items if the thread is running
	//if ( 0 == pthread_kill( m_SelfThreadId, 0 ) )
	//{
		//m_NotificationPool.waitForPoolEmpty();
	//}
	// the endpoint must now be locked on pool

	// the watcher must be dead by now...
	m_Running = false;

	m_NotificationPool.ShutdownPool();
	
	TRACE( m_ServiceThreadId << " joining endpoint ..." );
	int joinResult = pthread_join( m_SelfThreadId, NULL );
	if ( 0 != joinResult )
	{
		TRACE( "Joining self thread ended in error [" << joinResult << "]" );
	}

	//Disconnect from database
	if ( m_CurrentDatabase != NULL )
		m_CurrentDatabase->Disconnect();
}

string DbFetcher::Prepare()
{
	//sleep( 5 ); //TODO remove this when everything works ok
	
	//TODO check connection to currentDatabase
	DEBUG2( "PREPARE" );
		
	if ( m_CurrentDatabase == NULL )
	{
		TRACE( "Prepare failed : Database not configured" );
		throw runtime_error( "Prepare failed : Database not configured" );
	}
	
	DEBUG2( "Database name : [" << m_DatabaseName << "]" );
		
	//TODO : remove this construct ( one time only .. the connection string doesn't change )
	ConnectionString connString = ConnectionString( m_DatabaseName, m_UserName, m_UserPassword );

	if ( !m_CurrentDatabase->IsConnected() )
	{
		DEBUG( "Connecting to database ... " );
		m_CurrentDatabase->Connect( connString );
	}
	else
	{
		DEBUG2( "Already connected to database ... " );
	}
		
	// if mark for process is defined in config, run it ( compatibility only )
	if ( m_SPmarkforprocess.length() > 0 )
	{
		if ( m_CurrentProvider == NULL )
			throw runtime_error( "Database provider not initialized" );

		//Mark the record for processing
		//Get the rowid or the primary key to unique identify the record
		DataParameterBase *rowId = m_CurrentProvider->createParameter( DataType::CHAR_TYPE, DataParameterBase::PARAM_OUT );
		
		ParametersVector myParams;
		rowId->setDimension( 20 );
		myParams.push_back( rowId );
			
		m_CurrentDatabase->BeginTransaction();

		try
		{
			//mark for process one message; get the message RowId
			//TODO: implement this for DB2
			m_CurrentDatabase->ExecuteNonQueryCached( DataCommand::SP, m_SPmarkforprocess, myParams );
			m_CurrentDatabase->EndTransaction( TransactionType::COMMIT );
		}
		catch( ... )
		{
			try
			{
				m_CurrentDatabase->EndTransaction( TransactionType::ROLLBACK );
			}
			catch( ... ){}
			throw;
		}
			
		m_CurrentRowId = StringUtil::Trim( rowId->getString() );
		DEBUG( "Prepare : ROWID [" << m_CurrentRowId << "]" );
	}
		
	return m_CurrentRowId;
}

void DbFetcher::Process( const string& correlationId )
{
	//This will change after batch support is in place
	DEBUG2( "PROCESS" );
	
	if ( m_CurrentDatabase == NULL )
		throw runtime_error( "Database not initialized" );

	//Select data from the table
	DataSet *theDataSet = NULL;
	
	// if select for process is not in config, the watcher notifies this with the full doc, so skip all processing
	if ( m_SPselectforprocess.length() > 0 )
	{
		try
		{
			if ( m_CurrentProvider == NULL )
				throw runtime_error( "Database provider not initialized" );

			DEBUG( "Select data from table ..." );

			ParametersVector myParams;

			DataParameterBase *rowId = m_CurrentProvider->createParameter( DataType::CHAR_TYPE, DataParameterBase::PARAM_IN );
			rowId->setDimension( m_CurrentRowId.length() );
			rowId->setString( m_CurrentRowId );
			myParams.push_back( rowId );

			m_CurrentDatabase->BeginTransaction();

			//select for process only the messages identified by m_CurrentRowId
			// TODO: implement this for DB2
			// 		 use a false parameter if not possible
			theDataSet = m_CurrentDatabase->ExecuteQuery( DataCommand::SP, m_SPselectforprocess, myParams );
		}
		catch( const DBConnectionLostException& error )
		{
			stringstream messageBuffer;
		  	messageBuffer << typeid( error ).name() << " exception [" << error.what() << "]";
			TRACE( messageBuffer.str() );
			
			// nu trebuie delete ?
			if ( theDataSet != NULL )
				delete theDataSet;
				
			throw;
		}
		catch( const std::exception& error )
		{
			stringstream messageBuffer;
	  		messageBuffer << typeid( error ).name() << " exception [" << error.what() << "]";
			TRACE( messageBuffer.str() );
			
			if ( theDataSet != NULL )
				delete theDataSet;
				
			throw;
		}
		catch( ... )
		{
			TRACE( "Unhandled exception !" );
			if ( theDataSet != NULL )
				delete theDataSet;
				
			throw;
		}
	
		//Convert Data to XML DOM format
		try
		{
			m_CurrentMessage = Database::ConvertToXML( theDataSet, m_DatabaseToXmlTrimm );
			
			if ( m_CurrentMessage == NULL ) 
				throw runtime_error( "Conversion to XML resulted in an empty document" );
				
			if ( theDataSet != NULL )
			{
				delete theDataSet;
				theDataSet = NULL;
			}
		}
		catch( const std::exception& error )
		{
			if( m_CurrentMessage != NULL )
			{
				m_CurrentMessage->release();
				m_CurrentMessage = NULL;
			}
  			if ( theDataSet != NULL )
			{
				delete theDataSet;
				theDataSet = NULL;
			}
	  			
			stringstream messageBuffer;
	  		messageBuffer << typeid( error ).name() << " exception [" << error.what() << "]";
			TRACE( messageBuffer.str() );
			
			throw;
		}
		catch( ... )
		{
			if( m_CurrentMessage != NULL )
			{
				m_CurrentMessage->release();
				m_CurrentMessage = NULL;
			}
			if ( theDataSet != NULL )
			{
				delete theDataSet;
				theDataSet = NULL;
			}

			TRACE( "Unhandled exception !" );
			throw;
		}
	}
	// no select for process, get the dom from the notification object
	else
	{
		// we may encounter a connection lost when recovering from a db shutdown
		try
		{
			// so that we can rollback/commit
			// if the optional config is not set( -1 ) or we are not in a transaction
			if ( ( m_MaxUncommitedTrns < 0 ) || ( m_UncommitedTrns == 0 ) )
			{
				m_CurrentDatabase->BeginTransaction();
			}
		}
		catch( const DBConnectionLostException& ex )
		{
			// set nothrow on endtransaction since we are in a catch block
			m_CurrentDatabase->EndTransaction( TransactionType::ROLLBACK, false );

			TRACE_GLOBAL( "Connection lost. " << ex.getMessage() << ". Attempting to reconnect at 10s intervals..." );

			// isconnected will attempt to reconnect
			while ( !m_CurrentDatabase->IsConnected() && m_Running )
			{
				// still disconnected
				sleep( 10 );
			}

			// throw the error, because we were stopped and not able to reconnect
			if ( !m_CurrentDatabase->IsConnected() ) 
				throw ex;
			else
			{
				// try again to start a transaction
				m_CurrentDatabase->BeginTransaction();
			}
		}

		if ( m_NotificationTypeXML && ( m_CurrentMessage == NULL ) )
		{
			AppException aex( "Conversion to XML resulted in an empty document", EventType::Error );
			aex.setSeverity( EventSeverity::Fatal );
			throw aex;
		}
	}

	//string bDomDoc= XmlUtil::SerializeToString( doc );
	//DEBUG( "DOMDocument before process ..." << bDomDoc );
	
	//Process XML Data 
	//Delegate work to the chain of filters.
	DEBUG( "Processing XML Data ..." );
	AbstractFilter::FilterResult result;
	try
	{
		m_TransportHeaders.Clear();
		
		m_TransportHeaders.Add( "XSLTPARAMBATCHID", "'BATCH01'" );
		m_TransportHeaders.Add( "XSLTPARAMGUID", StringUtil::Pad( Collaboration::GenerateGuid(), "\'", "\'" ) );
	  	m_TransportHeaders.Add( "XSLTPARAMCORRELID", StringUtil::Pad( correlationId, "\'","\'" ) );
	  	m_TransportHeaders.Add( "XSLTPARAMSESSIONID", "'0001'" );
	  	m_TransportHeaders.Add( "XSLTPARAMREQUESTOR", StringUtil::Pad( getServiceName(), "\'", "\'" ) );
	  	m_TransportHeaders.Add( "XSLTPARAMREQUEST", "'SingleMessage'" );
		m_TransportHeaders.Add( "XSLTPARAMFEEDBACK", StringUtil::Pad( string( "W|" ) + m_CurrentRowId + string( "|0" ), "\'","\'" ) );
		m_TransportHeaders.Add( MqFilter::MQAPPNAME, getServiceName() );
		
		// as the message wil end up in MQ, no output is needed
		if ( m_NotificationTypeXML )
			result = m_FilterChain->ProcessMessage( m_CurrentMessage, m_TransportHeaders, true );
		else
		{
			result = m_FilterChain->ProcessMessage( AbstractFilter::buffer_type( m_CurrentMessageStr ), AbstractFilter::buffer_type( NULL ), m_TransportHeaders, true );
		}
	}
	catch( ... ) //TODO put specific catches before this
	{	
		// if we read the whole message in the watcher, don't discard the document until abort
		if( ( m_CurrentMessage != NULL ) && ( m_SPselectforprocess.length() > 0 ) )
		{
			m_CurrentMessage->release();
			m_CurrentMessage = NULL;
		}
		//TODO add details to the error 
		throw;
	}
	
	//string aDomDoc= XmlUtil::SerializeToString( doc );
	//DEBUG( "DOMDocument after process..." << aDomDoc );
	if( m_CurrentMessage != NULL )
	{
		m_CurrentMessage->release();
		m_CurrentMessage = NULL;
	}
	if( m_SavedMessage != NULL )
	{
		m_SavedMessage->release();
		m_SavedMessage = NULL;
	}
}

void DbFetcher::Commit()
{
	DEBUG2( "COMMIT" );

	if( m_SavedMessage != NULL )
	{
		m_SavedMessage->release();
		m_SavedMessage = NULL;
	}

	if ( m_CurrentDatabase == NULL )
		throw runtime_error( "Database not initialized" );	

	try
	{
		m_FilterChain->Commit();

		//Delete processed data from table
  		DEBUG( "Mark data as processed in source table ... " );
		
		if( m_SPselectforprocess.length() > 0 )
			m_CurrentDatabase->ExecuteNonQueryCached( DataCommand::SP, m_SPmarkcommit );
		else
		{
			if ( m_CurrentProvider == NULL )
				throw runtime_error( "Database provider not initialized" );

			ParametersVector myParams;

#ifdef IFX_ONLY
			DataParameterBase *rowId = m_CurrentProvider->createParameter( DataType::LONGINT_TYPE, DataParameterBase::PARAM_IN );
			//rowId->setDimension( );
			rowId->setLong( StringUtil::ParseLong( m_CurrentRowId ) );
			rowId->setName("ROWID");
			myParams.push_back( rowId );

#else
			DataParameterBase *rowId = m_CurrentProvider->createParameter( DataType::CHAR_TYPE, DataParameterBase::PARAM_IN );
			rowId->setDimension( m_CurrentRowId.length() );
			rowId->setString( m_CurrentRowId );
			rowId->setName("ROWID");
			myParams.push_back( rowId );
#endif
			m_CurrentDatabase->ExecuteNonQueryCached( DataCommand::SP, m_SPmarkcommit, myParams );
		}
	}
	catch( const std::exception& ex )
	{
		try
		{
			TRACE( "An error occured in commit [" << ex.what() << "]" );
			m_CurrentDatabase->EndTransaction( TransactionType::COMMIT );
		}
		catch( ... ){}
		throw;
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured in commit [unknown reason]" );
			m_CurrentDatabase->EndTransaction( TransactionType::COMMIT );
		}
		catch( ... ){}
		throw;
	}

	//End Transaction and commit all changes if we are not using the optional uncommit setting 
	//or we have not reached the upper threshold, or there are no more messages waiting
	if ( ( m_MaxUncommitedTrns < 0 ) || ( ++m_UncommitedTrns > m_MaxUncommitedTrns ) || ( m_NotificationPool.getSize() == 0 ) )
	{
		m_CurrentDatabase->EndTransaction( TransactionType::COMMIT );

		DEBUG( "Commited [" << m_UncommitedTrns << "] trns." );
		m_UncommitedTrns = 0;
	}
}

void DbFetcher::Abort()
{
	DEBUG2( "ABORT" );
	
	if( ( m_CurrentMessage != NULL ) && ( m_SPselectforprocess.length() == 0 ) )
	{
		m_CurrentMessage->release();
		m_CurrentMessage = NULL;
	}
	if( m_SavedMessage != NULL )
	{
		m_SavedMessage->release();
		m_SavedMessage = NULL;
	}
	
	//Delete processed data from table
	try
	{
		if( m_CurrentDatabase == NULL )
			throw runtime_error( "Database not initialized" );

		m_FilterChain->Commit();

		//Delete processed data from table
  		DEBUG( "Mark data as aborted in source table ... " );
		
		if( m_SPselectforprocess.length() > 0 )
			m_CurrentDatabase->ExecuteNonQueryCached( DataCommand::SP, m_SPmarkabort );
		else
		{
			if ( m_CurrentProvider == NULL )
				throw runtime_error( "Database provider not initialized" );

			ParametersVector myParams;

			DataParameterBase *rowId = m_CurrentProvider->createParameter( DataType::CHAR_TYPE, DataParameterBase::PARAM_IN );
			rowId->setDimension( m_CurrentRowId.length() );
			rowId->setString( m_CurrentRowId );
			myParams.push_back( rowId );

			m_CurrentDatabase->ExecuteNonQueryCached( DataCommand::SP, m_SPmarkabort, myParams );
		}
	}
	catch( ... )
	{
		m_CurrentDatabase->EndTransaction( TransactionType::COMMIT );
		throw;
	}
	
	//End Transaction and commit all changes
	m_CurrentDatabase->EndTransaction( TransactionType::COMMIT );
}

void DbFetcher::Rollback()
{
	DEBUG2( "ROLLBACK" );

	m_Rollback = true;

	if( m_CurrentMessage != NULL )
	{
		m_CurrentMessage->release();
		m_CurrentMessage = NULL;
	}
	
	m_FilterChain->Rollback();

	//end transaction and rollback all changes. this may fail if a rollback was requested while reconnecting
	if ( m_CurrentDatabase == NULL )
		throw runtime_error( "Database not initialized" );

	m_CurrentDatabase->EndTransaction( TransactionType::ROLLBACK, false );
}

void DbFetcher::NotificationCallback( const AbstractWatcher::NotificationObject *notification )
{
	string currentMessageId = notification->getObjectId();	
	DEBUG( "Notified : [" << currentMessageId << "]" );

	//TODO throw only on fatal error. The connector should respawn this thread
	//bool succeeded = true;
	try
	{
		DEBUG( "Performing message loop ... " );
		m_Me->PerformMessageLoop( m_Me->getBatchManager() != NULL );
		DEBUG( "Message loop finished ok. " );
	}
	catch( const AppException& ex )
	{
		string exceptionType = typeid( ex ).name();
		string errorMessage = ex.getMessage();
		
		TRACE_GLOBAL( exceptionType << " encountered while processing message : " << errorMessage );
	}
	catch( const std::exception& ex )
	{
		string exceptionType = typeid( ex ).name();
		string errorMessage = ex.what();
		
		TRACE_GLOBAL( exceptionType << " encountered while processing message : " << errorMessage );
	}
	catch( ... )
	{
		TRACE_GLOBAL( "[unknown exception] encountered while processing message. " );
	}
}
