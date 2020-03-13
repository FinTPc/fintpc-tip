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
		//#define _CRTDBG_MAP_ALLOC
		#include <stdlib.h>
		#include <crtdbg.h>
	#endif*/
#endif

#include <string>
#include <sstream>
#include <tiffio.h>
//#include <iostream>

#include "WorkItemPool.h"
#include "AbstractWatcher.h"
#include "MqPublisher.h"
#include "../Connector.h"
#include "AppSettings.h"

#include "TransportHelper.h"
#include "MQ/MqFilter.h"

#include "Trace.h"
#include "PlatformDeps.h"
#include "AppExceptions.h"
#include "XmlUtil.h"
#include "XPathHelper.h"
#include "Base64.h"
#include "BatchManager/Storages/BatchXMLfileStorage.h"
#include "StringUtil.h"
#include "SSL/SSLFilter.h"
#include "CommonExceptions.h"

#ifndef NO_DB
	#include "DataParameter.h"
	#include "ConnectionString.h"
#endif

#include <xalanc/XercesParserLiaison/XercesDocumentWrapper.hpp>

#ifdef WIN32
#define __MSXML_LIBRARY_DEFINED__
#include "windows.h"
#define sleep(x) Sleep( (x)*1000 )
#else
#include <unistd.h>
#endif

using namespace std;

//extern Connector *ConnectorInstance;
MqPublisher* MqPublisher::m_Me = NULL;

/*string MqPublisher::m_MessageFormat = "";
int MqPublisher::m_PrevBatchItem = 0;*/
const string MqPublisher::CHQ_NAMESPACE = "urn:swift:xs:CoreBlkChq";
const string MqPublisher::PRN_NAMESPACE = "urn:swift:xs:CoreBlkPrmsNt";
const string MqPublisher::BLX_NAMESPACE = "urn:swift:xs:CoreBlkBillXch";

//constructor
MqPublisher::MqPublisher() : Endpoint(), m_Watcher( NULL ), m_WatchQueue( "" ), m_WatchQueueManager( "" ), 
	m_WatchTransportURI( "" ), m_WMQBatchFilter( false ), m_AppQueue( "" ), m_AppQmgr( "" ), m_AppTransportURI( "" ), 
	m_SSLKeyRepos( "" ), m_SSLCypherSpec( "" ), m_SSLPeerName( "" ), m_CertificateFileName( "" ), m_CertificatePasswd( "" ),
	m_RepliesQueue( "" ), m_NotificationTypeXML( true ), m_CurrentHelper( NULL ), m_Metadata(), m_SAAFilter( NULL ),
	m_BatchManagerID( BatchManagerBase::ZIP, BatchResolution::SYNCHRONOUS ), m_IsIDsEnabled( false ), m_IsCurrentBatchID( false ), m_CurrentSequence( 0 ),
	m_DatabaseProvider( "" ), m_DatabaseName( "" ), m_UserName( "" ), m_UserPassword( "" ),
	m_IDCertificateFileName( "" ), m_IDCertificatePasswd( "" ), m_LastIdImageInZip( "" )
{	
#ifndef NO_DB
	m_CurrentDatabase = NULL;
	m_CurrentProvider = NULL;
#endif

	DEBUG2( "CONSTRUCTOR" );	
}

//destructor
MqPublisher::~MqPublisher()
{
	DEBUG2( "DESTRUCTOR" );

	delete m_Watcher;
	delete m_SAAFilter;

#ifndef NO_DB
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
#endif
}

void MqPublisher::Init()
{
	DEBUG( "INIT" );

	TransportHelper::TRANSPORT_HELPER_TYPE currentHelperType = TransportHelper::parseTransportType ( getGlobalSetting( EndpointConfig::MQToApp, EndpointConfig::MQSERVERTYPE ) );
	m_CurrentHelper = TransportHelper::CreateHelper( currentHelperType );

	m_Metadata.setFormat( getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::WMQFMT, TransportHelper::TMT_STRING ) );
	m_AppQueue = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::APPQUEUE );
	m_AppQmgr = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::WMQQMGR );
	m_AppTransportURI = getGlobalSetting( EndpointConfig::MQToApp, EndpointConfig::MQURI );
	
	m_SSLKeyRepos = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::WMQKEYREPOS, "" );
	m_SSLCypherSpec = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::WMQSSLCYPHERSPEC, "" );
	m_SSLPeerName = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::WMQPEERNAME, "" );
	
	m_CertificateFileName = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::CERTIFICATEFILENAME, "" );
	m_CertificatePasswd = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::CERTIFICATEPASSWD, "" );

	m_RepliesQueue = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::RPLYQUEUE, m_AppQueue );
	
	string notificationType = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::NOTIFTYPE, "XML" );

	m_ParamFileXslt = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::PARAMFXSLT, "" );
	m_TransformFile = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::FILEXSLT, "" );
	m_StrictSwiftFormat = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::STRICTSWIFTFMT, "" );
	m_LAUKey = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::LAUCERTIFICATE, "" );

	if( m_StrictSwiftFormat == SwiftFormatFilter::SAA_FIN )
	{
		if( m_SAAFilter != NULL  )
			delete m_SAAFilter;
		m_SAAFilter = new SwiftFormatFilter();
	}
	else if ( m_StrictSwiftFormat == SwiftFormatFilter::SAA_FILEACT )
	{
		if ( m_ParamFileXslt.empty() )
			throw logic_error( "Missing MQSeriesToApp.ParamFileXsltkey from .config file" );

		boost::shared_ptr< BatchManager< BatchMQStorage > > batchManager( new BatchManager< BatchMQStorage > ( BatchManagerBase::MQ, BatchResolution::SYNCHRONOUS ) );
		batchManager->storage().initialize( currentHelperType );
		batchManager->storage().setQueue( m_AppQueue );
		batchManager->storage().setQueueManager( m_AppQmgr );
		batchManager->storage().setTransportURI( m_AppTransportURI );
		batchManager->storage().setReplyOptions( "MQRO_NAN+MQRO_PAN+MQRO_COPY_MSG_ID_TO_CORREL_ID" );
		batchManager->storage().setReplyQueue( m_RepliesQueue );
		if( m_SAAFilter != NULL  )
			delete m_SAAFilter;
		m_SAAFilter = new SAAFilter( batchManager );
	}

	// ID config
	string isIDEnabled = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::ISIDENABLED, "false" );
	m_IsIDsEnabled = ( isIDEnabled == "true" );
	
	// ID certificate
	m_IDCertificateFileName = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::IDCERTIFICATEFILENAME, "" );
	m_IDCertificatePasswd = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::IDCERTIFICATEPASSWD, "" );
	
	// ID DB config
	m_DatabaseProvider = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::DBPROVIDER, "" );
	m_DatabaseName = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::DBNAME, "" );
	m_UserName = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::DBUSER, "" );
	m_UserPassword = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::DBPASS, "" );
	
	m_NotificationTypeXML = ( notificationType == "XML" );
	
	if ( haveGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::BATCHMGRTYPE ) )
	{
		string batchManagerType = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::BATCHMGRTYPE );		
		try
		{
			if ( batchManagerType == "XMLfile" )
			{
				m_BatchManager = BatchManagerBase::CreateBatchManager( BatchManagerBase::XMLfile );
				if( haveGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::BATCHMGRXSLT ) )
				{
					string batchXsltFile = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::BATCHMGRXSLT );

					BatchManager< BatchXMLfileStorage >* batchManager = dynamic_cast< BatchManager< BatchXMLfileStorage >* >( m_BatchManager );
					if ( batchManager == NULL )
						throw logic_error( "Bad type : batch manager is not of XML type" );

					batchManager->storage().setXslt( batchXsltFile );
				}
#if XERCES_VERSION_MAJOR >= 3
				else if( haveGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::BATCHMGRXPATH ) )
				{
					string batchXPath = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::BATCHMGRXPATH );
										
					BatchManager< BatchXMLfileStorage >* batchManager = dynamic_cast< BatchManager< BatchXMLfileStorage >* >( m_BatchManager );
					if ( batchManager == NULL )
						throw logic_error( "Bad type : batch manager is not of XML type" );
			
					//Set BatchXPath
					batchManager->storage().setXPath( batchXPath );
					batchManager->storage().setXPathCallback( MqPublisher::XPathCallback );
				}
#endif
			}
			else
			{
				TRACE( "Batch manager type [" << batchManagerType << "] not implemented. No batch processing will be performed." );
			}
		}
		catch( const std::exception& ex )
		{
			TRACE( "Batch manager could not be created. [" << ex.what() << "]" );
			if ( m_BatchManager != NULL )
			{
				delete m_BatchManager;
				m_BatchManager = NULL;
			}
			throw;
		}
		catch( ... )
		{
			TRACE( "Batch manager could not be created. [unknown exception]" );
			if ( m_BatchManager != NULL )
			{
				delete m_BatchManager;
				m_BatchManager = NULL;
			}
			throw;
		}
	}
	else
	{
		DEBUG( "No batch manager specified in config file." );
	}

	AbstractFilter* firstFilter = ( *m_FilterChain )[ 0 ];
	if ( ( firstFilter == NULL ) )
		throw logic_error( "No filter in publisher's chain." );

	m_Watcher = AbstractWatcher::createWatcher( *firstFilter, &m_NotificationPool );
	
#ifdef WMQTOAPP_BACKUP
	string backupQueue = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::BAKQUEUE, "" );
	if ( backupQueue.length() )
		m_CurrentHelper->setBackupQueue( backupQueue );
#endif

	//ID configs
	if ( m_IsIDsEnabled )
	{
		if ( ( m_IDCertificateFileName.length() == 0 ) || ( m_IDCertificatePasswd.length() == 0 ) )
		{
			stringstream errorMessage;
			errorMessage << "Certificate filename [" << m_IDCertificateFileName << "] and/or password [" << m_IDCertificatePasswd.length() << "] settings missing. Please check the config file.";
			TRACE( errorMessage.str() );
			throw logic_error( errorMessage.str() );
		}
		if ( m_DatabaseProvider.length() == 0 )
		{
			TRACE( "Unable to find database provider definition. Please check the config file." );
			throw logic_error( "Unable to find database provider definition. Please check the config file." );
		}
		if ( m_DatabaseName.length() == 0 )
		{
			TRACE( "Unable to find database SID definition. Please check the config file." );
			throw logic_error( "Unable to find database SID definition. Please check the config file." );
		}
		if ( ( m_UserPassword.length() == 0 ) || ( m_UserName.length() == 0 ) ) 
		{
			TRACE( "Unable to find database credentials definition. Please check the config file." );
			throw logic_error( "Unable to find database credentials definition. Please check the config file." );
		}

#ifndef NO_DB
		// everything is defined for ID processing
		try
		{
			//Create Database Provider
			m_CurrentProvider = DatabaseProvider::GetFactory( m_DatabaseProvider );

			//Create Database
			if ( m_CurrentProvider != NULL )
			{
				m_CurrentDatabase = m_CurrentProvider->createDatabase();
				if ( m_CurrentDatabase == NULL )
					throw runtime_error( "Unable to create database " );
			}
			else
			{
				throw runtime_error( "Unable to create database provider" );
			}
		}
		catch( const std::exception& error )
		{
			if ( m_CurrentProvider != NULL )
				delete m_CurrentProvider;
			if ( m_CurrentDatabase != NULL )
				delete m_CurrentDatabase;

			DEBUG( "Create database error : [" << error.what() << "]" );
			throw;
		}
		catch( ... )
		{
			if ( m_CurrentProvider != NULL )
				delete m_CurrentProvider;
			if ( m_CurrentDatabase != NULL )
				delete m_CurrentDatabase;

			DEBUG( "Unknown error while creating database" );
			throw;
		}
#endif
	}
}

void MqPublisher::internalStart()
{
	m_Me = this;

	DEBUG( "Starting watcher... " );
	m_Watcher->setEnableRaisingEvents( true );

	TRACE_SERVICE( "[" << m_ServiceThreadId << "] starting to process... " );

	try
	{
		while( m_Running )
		{
			DEBUG( "Publisher [" << m_SelfThreadId << "] waiting for notifications in pool" );
			WorkItem< AbstractWatcher::NotificationObject > notification = m_NotificationPool.removePoolItem();
			
			AbstractWatcher::NotificationObject *notificationObject = notification.get();
			
			// suppose Message.Message m_CurrentMessage
			// m_CurrentMessage = Message.MessageFactory.createMessage( id, grpId, length.. )

			/*m_CurrentMessageId = notificationObject->getObjectId();
			m_CurrentMessageLength = notificationObject->getObjectSize();
			string newGroupId = Base64::decode( notificationObject->getObjectGroupId() );

			DEBUG( "Notified : [" << m_CurrentMessageId << "] in group [" << notificationObject->getObjectGroupId()
				<< "] size [" << m_CurrentMessageLength << "]" );
			*/
			m_Metadata.setId( notificationObject->getObjectId() );
			m_Metadata.setLength( notificationObject->getObjectSize() );
			string newGroupId =  Base64::decode( notificationObject->getObjectGroupId() );

			DEBUG( "Notified : [" << m_Metadata.id() << "] in grup [" << newGroupId.c_str() << "] size [" << m_Metadata.length() << "]" );
			
			// if we're getting batches, and the previous batch is the same as this batch, delete notification
			if ( ( m_BatchManager != NULL ) && m_LastOpSucceeded && ( m_Metadata.groupId() == newGroupId ) )
			{
				TRACE( "This batch [" << m_Metadata.groupId() << "] was already processed. Skipping ..." );
				continue;
			}
			else
				m_Metadata.setGroupId( newGroupId );

			m_IsLast = false;
			m_CurrentSequence = 0;
			m_IsCurrentBatchID = false;
	
			//TODO throw only on fatal error. The connector should respawn this thread
			m_LastOpSucceeded = true;
			try
			{
				//open BatchXMLFile
				if ( m_BatchManager != NULL )
				{
					DEBUG_GLOBAL( "First batch item ... opening storage" );
					
					// open batch ( next iteration it will already be open )
					m_BatchManager->open( m_Metadata.groupId(), ios_base::out );
				}

				DEBUG( "Performing message loop ... " );
				m_LastOpSucceeded = PerformMessageLoop( m_WMQBatchFilter );
				DEBUG( "Message loop finished ok. " );
			}
			catch( const AppException& ex )
			{
				string exceptionType = typeid( ex ).name();
				string errorMessage = ex.getMessage();
				
				TRACE_GLOBAL( exceptionType << " encountered while processing message : " << errorMessage );
				m_LastOpSucceeded = false;
			}
			catch( const std::exception& ex )
			{
				string exceptionType = typeid( ex ).name();
				string errorMessage = ex.what();
				
				TRACE_GLOBAL( exceptionType << " encountered while processing message : " << errorMessage );
				m_LastOpSucceeded = false;
			}
			catch( ... )
			{
				TRACE_GLOBAL( "[unknown exception] encountered while processing message. " );
				m_LastOpSucceeded = false;
			}

			if ( !m_LastOpSucceeded && m_Running )
			{
				TRACE( "Sleeping 10 seconds before next attempt( previous message failed )" );
				sleep( 10 );
			}
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

void MqPublisher::internalStop()
{
	DEBUG( "STOP" );
	
	// ensure watcher is dead ( the watcher will lock on pool and wait until it is empty )
	m_Watcher->setEnableRaisingEvents( false );

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

	// close queue, disconnect
	m_CurrentHelper->closeQueue();
	m_CurrentHelper->disconnect();
}

string MqPublisher::Prepare()
{
	DEBUG( "PREPARE" );
	//sleep( 5 ); //TODO remove this when everything works ok
	
	// try to connect and open the queue
	// this ops. will throw if an error occurs ... no plan B for now

	// connect to destination
	
	/*NameValueCollection sslOptions;
	
	if ( m_SSLKeyRepos.length() > 0 )
		sslOptions.Add( "KEYREPOS", m_SSLKeyRepos);
	if ( m_SSLCypherSpec.length() > 0 )
		sslOptions.Add( "SSLCYPHERSPEC", m_SSLCypherSpec );
	if ( m_SSLPeerName.length() > 0 )
		sslOptions.Add( "SSLPEERNAME", m_SSLPeerName );
	*/
	if ( m_SSLKeyRepos.length() > 0 )
//	if( sslOptions.getCount() >  0 ) 
		m_CurrentHelper->connect( m_AppQmgr, m_AppTransportURI, /*sslOptions*/m_SSLKeyRepos, m_SSLCypherSpec, m_SSLPeerName );
	else
		m_CurrentHelper->connect( m_AppQmgr, m_AppTransportURI );

	// appqueue may not be in m_WatchQueueManager
	m_CurrentHelper->openQueue( m_AppQueue );

	return m_Metadata.id();//m_CurrentMessageId;
}
/*
//for deletion : just for finde memory leaks
void MqPublisher::GetIDImage( string groupId, string correlationId, string &imgRef, ManagedBuffer* outputSignedBuffer )
{
	// e dezalocat ~WorkItem<>
	ManagedBuffer* inputUnsignedBuffer = NULL; 

	DataSet* result = NULL;
	string base64img = "";
	string imgData = "";
	try
	{
		if ( m_CurrentDatabase == NULL )
			throw logic_error( "Database not created" );

		while ( !m_CurrentDatabase->IsConnected() )
		{
			DEBUG( "Connecting to database ... " );
			ConnectionString connString = ConnectionString( m_DatabaseName, m_UserName, m_UserPassword );
			m_CurrentDatabase->Connect( connString );

			if ( !m_CurrentDatabase->IsConnected() )
			{
				TRACE( "Still not connected to database. Sleeping 30 seconds before retrying..." );
				sleep( 30 );
			}
		}
		
		ParametersVector myParams;
		DataParameterBase *paramCorrelationId = m_CurrentProvider->createParameter( DataType::CHAR_TYPE );
		paramCorrelationId->setDimension( correlationId.length() );	  	 
		paramCorrelationId->setString( correlationId );
		paramCorrelationId->setName( "in_correlationid" );
		myParams.push_back( paramCorrelationId );

		result = m_CurrentDatabase->ExecuteQuery( DataCommand::SP, "GETIMAGEFORTFD_TEST", myParams );
		// daca result este NULL ExecuteQuery face throw
		base64img = StringUtil::Trim( result->getCellValue( 0, "PAYLOAD" )->getString() );
		imgRef = StringUtil::Trim( result->getCellValue( 0, "IMAGEREFERENCE" )->getString() );
		imgData = Base64::decode( Base64::decode( base64img ) );
	}
	catch( const std::exception& ex )
	{
		if ( result != NULL )
		{
			delete result;
			result = NULL;
		}
		string exceptionType = typeid( ex ).name();
		TRACE( exceptionType << " encountered while extracting ID image : " << ex.what() );
		throw;
	}
	catch( ... )
	{
		if ( result != NULL )
		{
			delete result;
			result = NULL;
		}

		TRACE( "Unknown error encountered while extracting ID image" );
		throw;
	}
		if ( result != NULL )
		{
			delete result;
			result = NULL;
		}
	outputSignedBuffer->copyFrom( imgData );
}
*/

#ifndef NO_DB
void MqPublisher::GetIDImage( const string& groupId, const string& correlationId, string &imgRef, ManagedBuffer* outputSignedBuffer )
{
	// e dezalocat ~WorkItem<>
	ManagedBuffer* inputUnsignedBuffer = NULL; 

	DataSet* result = NULL;
	string base64img = "";
	string imgData = "";
	try
	{
		if ( m_CurrentDatabase == NULL )
			throw logic_error( "Database not created" );

		while ( !m_CurrentDatabase->IsConnected() )
		{
			DEBUG( "Connecting to database ... " );
			ConnectionString connString = ConnectionString( m_DatabaseName, m_UserName, m_UserPassword );
			m_CurrentDatabase->Connect( connString );

			if ( !m_CurrentDatabase->IsConnected() )
			{
				TRACE( "Still not connected to database. Sleeping 30 seconds before retrying..." );
				sleep( 30 );
			}
		}
		DEBUG( " Extracting ID image... " );
		ParametersVector myParams;
		DataParameterBase *paramCorrelationId = m_CurrentProvider->createParameter( DataType::CHAR_TYPE );
		paramCorrelationId->setDimension( correlationId.length() );	  	 
		paramCorrelationId->setString( correlationId );
		paramCorrelationId->setName( "in_correlationid" );
		myParams.push_back( paramCorrelationId );

		m_CurrentDatabase->BeginTransaction();
		result = m_CurrentDatabase->ExecuteQuery( DataCommand::SP, "getimageforcsm", myParams );
		m_CurrentDatabase->EndTransaction( TransactionType::COMMIT );
		DEBUG( "Get image for ID message. Image correlation id is [" << correlationId << "]" );
		
		// daca result este NULL ExecuteQuery face throw
		base64img = StringUtil::Trim( result->getCellValue( 0, "PAYLOAD" )->getString() );
		imgRef = StringUtil::Trim( result->getCellValue( 0, "IMAGEREFERENCE" )->getString() );
		imgData = Base64::decode( Base64::decode( base64img ) );
		if ( result != NULL )
		{
			delete result;
			result = NULL;
		}
	}
	catch( const std::exception& ex )
	{
		if ( result != NULL )
		{
			delete result;
			result = NULL;
		}
		string exceptionType = typeid( ex ).name();
		stringstream errorMessage;
		errorMessage << exceptionType << " encountered while extracting ID image : " << ex.what();	
		throw runtime_error( errorMessage.str() );
	}
	catch( ... )
	{
		if ( result != NULL )
		{
			delete result;
			result = NULL;
		}
		stringstream errorMessage;
		errorMessage << "Unknown error encountered while extracting ID image";	
		throw runtime_error( errorMessage.str() );
	}

	string tiffFilename;
	tiffFilename = "temp" + imgRef + ".tiff";
	TIFF* tif = NULL;

	try
	{
		ofstream outf;
		outf.open( tiffFilename.c_str(), ios_base::out | ios_base::binary );
		outf.write( imgData.c_str(), imgData.length() );
		outf.close();
		
		DEBUG( "Check and write to IMAGEDESCRIPTION TAG of ID image" );
		// Daca open rateaza, continutul probabil nu este tiff.. pentru a permite transmiterea mai departe a datelor se ignora eroarea
		try
		{
			tif = TIFFOpen( tiffFilename.c_str(), "r+" );
		}
		catch( ... )
		{
			TRACE( "An error was encountered when trying to open tiff ID image. Possible non-tiff data being passed." );
			tif = NULL;
		}

		char* ownDescription = NULL;
		bool isTiffRewrite = false;
		if ( tif != NULL )
		{
			// scrie tagul IMAGEDESCRIPTION
			if ( !TIFFGetField( tif, TIFFTAG_IMAGEDESCRIPTION, &ownDescription ) )
			{
				if ( !TIFFSetField( tif, TIFFTAG_IMAGEDESCRIPTION, imgRef.c_str() ) )
				{
					TIFFClose( tif );
					ownDescription = NULL;
					stringstream errorMessage;
					errorMessage << "An error occured while writing IMAGEDESCRIPTION tag [" << TIFFTAG_IMAGEDESCRIPTION << "]";	
					TRACE( errorMessage.str() );
					throw runtime_error( errorMessage.str() );
				}

				TIFFRewriteDirectory( tif );
				isTiffRewrite = true;

				DEBUG( "Tag [" << TIFFTAG_IMAGEDESCRIPTION << "] written to image data. Value is [" << imgRef << "]" );
			}

			TIFFClose( tif );
			ownDescription = NULL;
		}
		if ( isTiffRewrite )
		{
			ifstream inf( tiffFilename.c_str(), ios_base::in | ios_base::binary | ios::ate );
			long tiffFileSize = inf.tellg();
			inf.seekg( 0, ios::beg );
			unsigned char* tiffBuffer = NULL;
			
			try
			{
				tiffBuffer = new unsigned char[ tiffFileSize ];
				inf.read( ( char * )tiffBuffer, tiffFileSize );
				inf.close();
			}
			catch( ... )
			{
				if ( tiffBuffer != NULL )
				{
					delete[] tiffBuffer;
					tiffBuffer = NULL;
				}
				throw;
			}
			inputUnsignedBuffer = new ManagedBuffer( tiffBuffer, ManagedBuffer::Adopt, tiffFileSize );
		}
		else
		{
			inputUnsignedBuffer = new ManagedBuffer();
			inputUnsignedBuffer->copyFrom( imgData );
		}
		
		remove( tiffFilename.c_str() );
	}
	catch( const std::exception& ex )
	{
		string exceptionType = typeid( ex ).name();
		TRACE( exceptionType << " encountered while checking/writing IMAGEDESCRIPTION TAG [" << imgRef << "] : " << ex.what() );	

		if ( inputUnsignedBuffer != NULL )
		{
			delete inputUnsignedBuffer;
			inputUnsignedBuffer = NULL;
		}
		if ( tif != NULL )
			TIFFClose( tif );
		remove( tiffFilename.c_str() );
		
		throw;
	}
	catch( ... )
	{
		TRACE( "Unknown error encountered while checking/writing IMAGEDESCRIPTION TAG [" << imgRef << "]" );	
		
		if ( inputUnsignedBuffer != NULL )
		{
			delete inputUnsignedBuffer;
			inputUnsignedBuffer =NULL;
		}
		if ( tif !=NULL )
			TIFFClose( tif );
		remove( tiffFilename.c_str() );
		
		throw;
	}

	DEBUG( "Signing ID image data..." );
	SSLFilter* sslFilterID = NULL;
	AbstractFilter::buffer_type inputUnsignedDataBuffer( inputUnsignedBuffer );

	try
	{
		NameValueCollection transportHeaders;
		ManagedBuffer* outputSignedImage = new ManagedBuffer();

		AbstractFilter::buffer_type outputSignedDataBuffer( outputSignedImage );

		sslFilterID = new SSLFilter();
		sslFilterID->addProperty( SSLFilter::SSLCERTFILENAME, m_IDCertificateFileName );
		sslFilterID->addProperty( SSLFilter::SSLCERTPASSWD, m_IDCertificatePasswd );
		sslFilterID->ProcessMessage( inputUnsignedDataBuffer, outputSignedDataBuffer, transportHeaders, false );

		outputSignedBuffer->copyFrom( outputSignedImage );

		if ( sslFilterID != NULL )
		{
			delete sslFilterID;
			sslFilterID = NULL;
		}
	}
	catch( const std::exception& ex )
	{
		string exceptionType = typeid( ex ).name();
		TRACE( exceptionType << " encountered while signing ID image [" << imgRef << "] : " << ex.what() );	

		if ( sslFilterID != NULL )
		{
			delete sslFilterID;
			sslFilterID = NULL;
		}

		throw;
	}
	catch( ... )
	{
		TRACE( "Unknown error encountered while signing ID image [" << m_Metadata.id() << "]" );
		if ( sslFilterID != NULL )
		{
			delete sslFilterID;
			sslFilterID = NULL;
		}

		throw;
	}
}
#endif

void MqPublisher::Process( const string& correlationId )
{
	DEBUG( "PROCESS" );	
	
	// this will change after batch support is in place
	DEBUG( "Current message length : " << m_Metadata.length() );

	string payload = "", messageOutput = "";
	ManagedBuffer* messageOutputID = NULL;
	
	m_TransportHeaders.Clear();
	
	FinTPMessage::Message message( m_Metadata ); 
	
	try
	{
		// don't rely on wmq ordering to return the same message
		if ( !m_WMQBatchFilter )
			m_TransportHeaders.Add( MqFilter::MQMSGID, m_Metadata.id() );
		m_TransportHeaders.Add( MqFilter::MQGROUPID, m_Metadata.groupId() );
		m_TransportHeaders.Add( MqFilter::MQMSGSIZE, StringUtil::ToString( m_Metadata.length() ) );
		m_TransportHeaders.Add( MqFilter::MQAPPNAME, getServiceName() );
		
		// delegate work to the chain of filters.

		//m_FilterChain->ProcessMessage( m_CurrentMessage.dom(), transportHeaders, false );
		if( m_NotificationTypeXML )
		{
			m_FilterChain->ProcessMessage( message.dom(), m_TransportHeaders, false );
		
			DEBUG( "Message is in buffer; size is : " << m_Metadata.length() );
		
			message.getInformation( FinTPMessage::Message::MQCONNECTOR );
		
			if ( message.correlationId().length() != 0 )
				setCorrelationId( message.correlationId() );

			payload = message.payload();

			if( payload.size() > 100 )
			{
				DEBUG( "Payload is ( first 100 bytes ): [" << payload.substr( 0, 100 ) << "]" );
			}
			else
			{
				DEBUG( "Payload is : [" << payload << "]" );
			}
				
			messageOutput = Base64::decode( payload );

			message.releaseDom();
		}
		else
		{
			ManagedBuffer* outputBuffer = new ManagedBuffer( );
			AbstractFilter::buffer_type outputDataBuffer( outputBuffer );
			
			m_FilterChain->ProcessMessage( message.dom(), outputDataBuffer, m_TransportHeaders, false );
			
			if ( outputBuffer->size() > 100 )
			{
				string partialBuffer = string( ( char* )outputBuffer->buffer(), 100 );
				DEBUG( "Final message [" << outputBuffer->str() << "]" )
			}
			else
			{
				DEBUG( "Final message [" << outputBuffer->str() << "]" )
			}
		
			messageOutput = outputBuffer->str();	 
		}
	}
	catch( const std::exception& ex )
	{
		TRACE( "An exception has occured while processing message in chain ["  << typeid( ex ).name() << "] : " << ex.what() );
		message.releaseDom();
		throw;
	}
	catch( ... ) //TODO put specific catches before this
	{
		TRACE( "An exception has occured while processing message in chain [ reason unknown ]" );
		message.releaseDom();
		throw;
	}

	m_IsLast = true;
	if ( m_TransportHeaders.ContainsKey( MqFilter::MQLASTMSG ) )
		m_IsLast = ( m_TransportHeaders[ MqFilter::MQLASTMSG ] == "1" );
		
	if ( m_IsLast )	
		DEBUG( "Transport headers / batch mode options indicate this is the last message." );

	if ( m_BatchManager != NULL )
	{
		if ( m_IsIDsEnabled )
		{
			//ID namespace test and extract image
			string imgRef = "";
			if( m_CurrentSequence == 0 )
			{
				XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc = NULL;
				try
				{
					doc = XmlUtil::DeserializeFromString( messageOutput );
					string batchNamespace = XmlUtil::getNamespace( doc );
					DEBUG( "First batch sequence checked for ID: [ namespace = " << batchNamespace << "]" );

					if ( doc != NULL )
					{
						doc->release();
						doc = NULL;
					}
					if ( ( batchNamespace == MqPublisher::CHQ_NAMESPACE ) || 
						( batchNamespace == MqPublisher::PRN_NAMESPACE ) || 
						( batchNamespace == MqPublisher::BLX_NAMESPACE ) )
					{
						// reset to false in internalStart() before de PerforMessageLoop(inBatch)
						m_IsCurrentBatchID = true;
						DEBUG_GLOBAL( "First batch ID item ... opening storage" );
						m_BatchManagerID.open( m_Metadata.groupId(), ios_base::out | ios_base::binary );
					}
				}
				catch( const std::exception& ex )
				{
					if ( doc != NULL )
					{
						doc->release();
						doc = NULL;
					}	
					m_BatchManagerID.close( m_Metadata.groupId() );
					stringstream errorMessage;
					errorMessage << "Problem fetching batch namespace [" << ex.what() << "]";
					throw runtime_error( errorMessage.str() );
				}
				catch( ... )
				{
					if ( doc != NULL )
					{
						doc->release();
						doc = NULL;
					}
					m_BatchManagerID.close( m_Metadata.groupId() );
					throw runtime_error( "Problem fetching batch namespace [ unknown reason ]" );
				}

				// reset to 0 in internalStart() before PerformMessageLoop(inBatch)
				m_CurrentSequence++; 
			}
#ifndef NO_DB
			if ( m_IsCurrentBatchID )
			{
				DEBUG( "Start processing ID message... " );
				try
				{
					if ( m_LastIdImageInZip != m_Metadata.id() )
					{
						WorkItem< ManagedBuffer > imgManagedBuffer ( new ManagedBuffer() );
						ManagedBuffer* imgBuffer = imgManagedBuffer.get();

						DEBUG( "Geting ID image..." );
						GetIDImage( m_Metadata.groupId(), correlationId, imgRef, imgBuffer );
						if ( imgBuffer == NULL )
							throw runtime_error( "Error after geting ID image from database [ NULL image ]" );
						if ( imgRef.length() == 0 )
							throw runtime_error( "Error after geting ID image reference from database [ blank image reference ]" );
							
						DEBUG( "Inserting ID image to batch..." );
						BatchItem zipItem;
						zipItem.setBatchId( m_Metadata.groupId() );
						zipItem.setEyecatcher( ( imgRef + ".tiff.p7m" ).c_str() );
						zipItem.setBinPayload( imgBuffer );

						DEBUG( "Add ID image to batch" );
						m_BatchManagerID << zipItem;
					}
					else
					{
						DEBUG( "ID image already added to batch... " );
					}
				}
				catch ( const std::exception& ex )
				{	
					TRACE( "Error encountered processing ID image [ " << ex.what() << " ]" );
					throw;
				}
				catch( ... )
				{
					TRACE( "Error encountered processing ID image" );
					throw ;
				}
			}
#endif
		}
		try
		{
			BatchItem batchItem;
			batchItem.setPayload( messageOutput );
			batchItem.setBatchId( m_Metadata.groupId() );
			batchItem.setMessageId( m_Metadata.id() );
			
			DEBUG( "Add message to batch " );
			*m_BatchManager << batchItem;
			
			m_LastIdImageInZip = "";
		}
		catch( ... )
		{
			m_LastIdImageInZip = m_Metadata.id();
			throw;
		}
		if ( m_IsLast )
		{
			DEBUG( "LAST message" );
			long xmlStorageEnques = 0;		
			
			//serialize XML 		
			if ( m_BatchManager->getStorageCategory() == BatchManagerBase::XMLfile )
			{
				BatchManager< BatchXMLfileStorage >* batchManager = dynamic_cast< BatchManager< BatchXMLfileStorage >* >( m_BatchManager );
				if ( batchManager == NULL )
					throw logic_error( "Bad type : batch manager is not of XML type" );

				// get batch xml document
				messageOutput = batchManager->storage().getSerializedXml();
				if ( !m_TransformFile.empty() )
				{
					NameValueCollection transportHeaders;
					transportHeaders.Add( XSLTFilter::XSLTFILE, m_TransformFile );
					XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* xmlBatchManager = NULL;
					WorkItem< ManagedBuffer > output( new ManagedBuffer() );
					ManagedBuffer* outputBuffer = output.get();
					try
					{
						xmlBatchManager = XmlUtil::DeserializeFromString( messageOutput );
						XSLTFilter xsltFilter;
						xsltFilter.ProcessMessage( xmlBatchManager, output, transportHeaders, true );
						messageOutput = outputBuffer->str();
					}
					catch ( ... )
					{
						if ( xmlBatchManager != NULL )
							xmlBatchManager->release();
						throw;
					}
					xmlBatchManager->release();
				}
				xmlStorageEnques = batchManager->storage().getCrtSequence();
			}

			m_BatchManager->close( m_Metadata.groupId() );
			try
			{
				if ( m_IsCurrentBatchID )
				{
					if ( xmlStorageEnques == ( m_BatchManagerID.storage() ).getCrtSequence() )
					{
						BatchItem zipItem;
						zipItem.setEyecatcher( StringUtil::Trim( m_Metadata.groupId() ) + ".xml" );
						zipItem.setPayload( messageOutput );
						m_BatchManagerID << zipItem;

						messageOutputID = m_BatchManagerID.storage().getBuffer();
						m_BatchManagerID.close( m_Metadata.groupId() );
					}
					else
					{
						stringstream errorMessage ;
						errorMessage << "Number of ID images is different than ID transactions in batch [ images no.=" 
							<< ( m_BatchManagerID.storage() ).getCrtSequence() - 1 << " transactions no.="  << xmlStorageEnques -1 << " ]" ;
						throw logic_error( errorMessage.str() );
					}
					/*
					ofstream outzip;
					outzip.open( "batching.zip", ios_base::out | ios_base::binary );
					outzip.write( ( messageOutputID )->str().c_str(), messageOutputID->size() );
					outzip.close();
					*/
					
				}
			}
			catch( ... )
			{
				if( messageOutputID != NULL )
				{
					delete messageOutputID;			
					messageOutputID = NULL;
				}
				m_BatchManagerID.close( m_Metadata.groupId() );
				
				TRACE( "Error encountered adding xml file to ID batch" );
				throw ;
			}
		}
	}
	
	// if is not batch or if all messages in batch arrived
	// put message in MQ 
	if ( m_IsLast )
	{
		DEBUG( "This is the last message" );
		// do this if not batch processing or if last message in batch 
		
		try
		{
			// Apply SSL Filter if we need to do so 
			if( m_CertificateFileName.length() != 0 )
			{
				DEBUG( "Applying SSL filter..." );
							
				WorkItem< ManagedBuffer > managedInputBuffer( new ManagedBuffer() );
				ManagedBuffer* inputBuffer = managedInputBuffer.get();
								
				inputBuffer->copyFrom( messageOutput  );
			
				SSLFilter* sslFilter = NULL;
				try
				{
					sslFilter = new SSLFilter();
					sslFilter->addProperty( SSLFilter::SSLCERTFILENAME, m_CertificateFileName );
					sslFilter->addProperty( SSLFilter::SSLCERTPASSWD, m_CertificatePasswd );
					
					ManagedBuffer* outputBuffer = new ManagedBuffer( );
					AbstractFilter::buffer_type outputDataBuffer( outputBuffer );
				
					sslFilter->ProcessMessage( managedInputBuffer, outputDataBuffer, m_TransportHeaders, false );
			
					if( sslFilter != NULL )
					{
						delete sslFilter;
						sslFilter = NULL;
					}

					messageOutput = outputBuffer->str();
				}
				catch( const std::exception& ex )
				{
					if ( sslFilter != NULL )
					{
						delete sslFilter;
						sslFilter = NULL;
					}
					throw;
				}
				catch( ... )
				{
					if ( sslFilter != NULL )
					{
						delete sslFilter;
						sslFilter = NULL;
					}
					throw;
				}
				DEBUG( "Current message is : [" << messageOutput << "]" );
			} 
			
			//TODO check return code
			m_CurrentHelper->setMessageId( m_Metadata.id() );
			m_CurrentHelper->setMessageFormat( m_Metadata.format() );
			
			// prepare message options : 
			// type is request;
			DEBUG( "Message type : " << m_TransportHeaders[ MqFilter::MQMESSAGETYPE ] );
			
			if ( m_TransportHeaders.ContainsKey( MqFilter::MQMESSAGETYPE ) && ( m_TransportHeaders[ MqFilter::MQMESSAGETYPE ] != "DATAGRAM" ) )
			{
				if( m_TransportHeaders[ MqFilter::MQMESSAGETYPE ] == "REQUEST" )
				{
					string rtqName="SYSTEM.DEAD.LETTER.QUEUE", rtqmName=""; 
					TransportReplyOptions replyOptions;
					
					if ( m_TransportHeaders.ContainsKey( MqFilter::MQREPLYQUEUE ) )
						rtqName = m_TransportHeaders[ MqFilter::MQREPLYQUEUE ];
					DEBUG( "Reply-to Q : [" << rtqName << "]" );
					
					if ( m_TransportHeaders.ContainsKey( MqFilter::MQREPLYQUEUEMANAGER ) )
						rtqmName = m_TransportHeaders[ MqFilter::MQREPLYQUEUEMANAGER ];
					DEBUG( "Reply-to QM : [" << rtqmName << "]" );
					
					if ( m_TransportHeaders.ContainsKey( MqFilter::MQREPLYOPTIONS ) )
						replyOptions.Parse( m_TransportHeaders[ MqFilter::MQREPLYOPTIONS ] );			
					if ( m_IsCurrentBatchID )
					{
						m_CurrentHelper->setMessageFormat( TransportHelper::TMT_NONE );
						m_CurrentHelper->putOneRequest( messageOutputID->buffer(), messageOutputID->size(), rtqName, rtqmName, replyOptions );
					}
					else
						m_CurrentHelper->putOneRequest( ( unsigned char* )messageOutput.data(), messageOutput.size(), rtqName, rtqmName, replyOptions );
				
				}
				else if( m_TransportHeaders[ MqFilter::MQMESSAGETYPE ] == "REPLY" )
				{
					long feedback = 0;
					if ( m_TransportHeaders.ContainsKey( MqFilter::MQFEEDBACK ) )
						feedback = atol( ( m_TransportHeaders[ MqFilter::MQFEEDBACK ] ).data() );
					
					// prepare opened app queue, now, if we're sending a reply open the replies queue
					if ( m_RepliesQueue != m_AppQueue )
						m_CurrentHelper->openQueue( m_RepliesQueue );		
					if ( m_IsCurrentBatchID )
					{
						m_CurrentHelper->setMessageFormat( TransportHelper::TMT_NONE );
						m_CurrentHelper->putOneReply( messageOutputID->buffer(), messageOutputID->size(), feedback );
					}
					else
						m_CurrentHelper->putOneReply( ( unsigned char* )messageOutput.data(), messageOutput.size(), feedback );
				}
			}
			else
			{
				if ( m_IsCurrentBatchID )
				{
					m_CurrentHelper->setMessageFormat( TransportHelper::TMT_NONE );
					m_CurrentHelper->putOne( messageOutputID->buffer(), messageOutputID->size() );
				}
				//TODO: SAA for DI
				else if ( !m_StrictSwiftFormat.empty() )
				{
					WorkItem< ManagedBuffer > managedOutputBuffer ( new ManagedBuffer( ) );
					ManagedBuffer* outputBuffer = managedOutputBuffer.get();

					NameValueCollection transportHeaders;
					transportHeaders.Add( XSLTFilter::XSLTFILE, m_ParamFileXslt );
					transportHeaders.Add( MqFilter::MQGROUPID, m_Metadata.groupId() );
					transportHeaders.Add( SwiftFormatFilter::LAU_KEY, m_LAUKey );
					transportHeaders.Add( SwiftFormatFilter::SERVICE_KEY, m_StrictSwiftFormat );
					WorkItem<ManagedBuffer> managedInputBuffer( new ManagedBuffer( ( unsigned char* ) messageOutput.data(), ManagedBuffer::Ref, messageOutput.size() ) );

					//with with StrictSwiftFormat::SAA_FILEACT message will be put to MQ (empty outputBuffer)
					m_SAAFilter->ProcessMessage( managedInputBuffer, managedOutputBuffer, transportHeaders, false );
					if( m_StrictSwiftFormat == SwiftFormatFilter::SAA_FIN )
						m_CurrentHelper->putOne( outputBuffer->buffer(), outputBuffer->size() );
				}
				else
					m_CurrentHelper->putOne( ( unsigned char* )messageOutput.data(), messageOutput.size() );
			}
			if( messageOutputID != NULL )
			{
				delete messageOutputID;			
				messageOutputID = NULL;
			}
		}
		catch( const std::exception& ex )
		{		
			if( messageOutputID != NULL )
			{
				delete messageOutputID;			
				messageOutputID = NULL;
			}
			// format error
			stringstream errorMessage;
			errorMessage << "Can't write the message [" << m_Metadata.id() << 
				"] to queue [" << m_WatchQueue << "]. Check inner exception for details." << endl;
				
			TRACE( errorMessage.str() );
			TRACE( ex.what() );
			
			//TODO check this for returning a pointer to a const ref
			throw AppException( errorMessage.str(), ex );
		}
		catch( ... )
		{
			if( messageOutputID != NULL )
			{
				delete messageOutputID;			
				messageOutputID = NULL;
			}
			// format error
			stringstream errorMessage;
			errorMessage << "Can't write the message [" << m_Metadata.id() << 
				"] to queue [" << m_WatchQueue << "]. Reason unknown" << endl;
			
			TRACE( errorMessage.str() );
			
			throw AppException( errorMessage.str() );
		}		
	}
	else
	{
		DEBUG( "This is not the last message in batch." );
	}
}

void MqPublisher::Commit()
{
	//do this only if not batch or last message in batch 
	if ( m_IsLast )
	{
		DEBUG( "Final commit." );

		//TODO check return code
		m_FilterChain->Commit();
		m_CurrentHelper->commit();
		if ( m_SAAFilter )
			m_SAAFilter->Commit();

		//remove any notification with same batch that occurs while processing last batch
		try
		{
			DEBUG( "Trying to remove object already processed from pool" );
			WorkItem< AbstractWatcher::NotificationObject > tmpNotification = m_NotificationPool.removePoolItem( m_Metadata.id() );
		}
		catch( const WorkItemNotFound& ex )
		{
			DEBUG( "WorkItemNotFound while we try to remove item from pool : It's a test for remove duplicate items from pool " );
		}
		catch( const std::exception& ex )
		{
			DEBUG( "Error while removing item from pool after commit : [" << ex.what() << "]" );
		}		
		catch( ... )
		{
			DEBUG( "Unknown error while removing item from pool after commit" );
		}
	}
	else 
	{
		DEBUG( "Partial commit for batch item." );
	}		
}

void MqPublisher::Abort()
{
	DEBUG( "ABORT" );

	//TODO move the message to dead letter queue or backout queue
	//for now, commit message got
	//do this only if not batch or last message in batch 
	/// <summary>On abort mq filter is aborted too. 
	///This action consist in moving all group messages in DEAD.LETTER and commit current MQ transaction
	///</summary>

	m_FilterChain->Abort();
	m_BatchManagerID.close( m_Metadata.groupId() );
	m_BatchManager->close( m_Metadata.groupId() );
	m_CurrentHelper->rollback();
	// close queue, disconnect
	m_CurrentHelper->closeQueue();
	m_CurrentHelper->disconnect();
	if ( m_SAAFilter )
		m_SAAFilter->Rollback();

	m_IsLast = true;
}

void MqPublisher::Rollback()
{
	DEBUG( "ROLLBACK" );
	m_FilterChain->Rollback();
	//TODO check return code...	
	m_BatchManagerID.close( m_Metadata.groupId() );
	m_BatchManager->close( m_Metadata.groupId() );
	m_CurrentHelper->closeQueue();
	m_CurrentHelper->disconnect();
	if ( m_SAAFilter )
		m_SAAFilter->Rollback();

	m_IsLast = true;
}

string MqPublisher::XPathCallback( const string& itemNamespace )
{
	// HACK : fake get key with dynamic name
	stringstream xpathKey;
	string preXPathKey = EndpointConfig::getName( EndpointConfig::WMQToApp, EndpointConfig::BATCHMGRXPATH );
	//xpathKey <<  << << "_" << itemNamespace;
	xpathKey << preXPathKey << "_" << itemNamespace;
	DEBUG( "Looking in config for key [" << xpathKey.str() << "] as XPath on ns [" << itemNamespace << "]" );

	string batchXPath = "";
	if ( m_Me->getGlobalSettings().getSettings().ContainsKey( xpathKey.str() ) )
		batchXPath = m_Me->getGlobalSettings()[ xpathKey.str() ];
	return batchXPath;
}
//void MqPublisher::NotificationCallback( string groupId, string messageId, int messageLength )
//{
//	DEBUG_GLOBAL( "NotificationCallback invoked." );
//	
//	m_CurrentGroupId = groupId;
//	m_CurrentMessageId = messageId;
//	m_CurrentMessageLength = messageLength;
//	
//	DEBUG_GLOBAL( "Notified : group id [" << m_CurrentGroupId << "]; message id [" << m_CurrentMessageId <<
//		"] size : [" << m_CurrentMessageLength << "]" );
//		
//	//groupId is passed as Base64 string
//	//to pass it  forward groupId must be decoded
//	m_CurrentGroupId = Base64::decode( groupId );
//	
//	m_PrevBatchItem = -1;
//	
//	//open BatchXMLFile
//	if ( ConnectorInstance->getPublisher()->getBatchManager() != NULL )
//	{
//		DEBUG_GLOBAL( "First batch item ... opening storage" );
//		// open batch ( next iteration it will already be open )
//		ConnectorInstance->getPublisher()->getBatchManager()->open( m_CurrentGroupId, ios_base::out );
//	}
//
//	ConnectorInstance->getPublisher()->PerformMessageLoop( ConnectorInstance->getPublisher()->getBatchManager() != NULL );		
//}
