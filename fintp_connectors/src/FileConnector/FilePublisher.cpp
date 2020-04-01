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

#include <string>
//#include <iostream>
#include <sstream>
#include <iomanip>

//Boost Regular expression 
#include <boost/regex.hpp>
#include <boost/filesystem.hpp>
#ifdef __GNUC__
	#include <errno.h>
	#include <sys/stat.h>
	#include <bits/sigthread.h>
#endif

#include "FilePublisher.h"
#include "../Connector.h"
#include "../Message.h"

#include "AppSettings.h"
#include "MQ/MqFilter.h"
#include "Trace.h"
#include "StringUtil.h"
#include "PlatformDeps.h"
#include "AppExceptions.h"

#include "XmlUtil.h"
#include "XPathHelper.h"
#include "Base64.h"
#include "XSLT/XSLTFilter.h"
#include "Collaboration.h"
#include <xalanc/XercesParserLiaison/XercesDocumentWrapper.hpp>
#include <boost/regex.hpp>
#include "BatchManager/Storages/BatchXMLfileStorage.h"

#ifdef WIN32
	#define __MSXML_LIBRARY_DEFINED__
	
	#include <windows.h>
	#include <io.h>
	#define sleep(x) Sleep( (x)*1000 )
	
#ifdef CRT_SECURE
	#define access( exp1, exp2 ) _access_s( exp1, exp2 )
#else
	#define access(x,y) _access( x, y )
#endif
	
#else
	#include <unistd.h> // because of access function
#endif

using namespace std;

#ifdef LAU
	#include "SSL/HMAC.h"
#endif

FilePublisher* FilePublisher::m_Me = NULL;

string FilePublisher::m_DestinationPath = "";
string FilePublisher::m_ReplyDestinationPath = "";
string FilePublisher::m_Pattern = "";
string FilePublisher::m_ReplyPattern = "";
string FilePublisher::m_ReplyFeedback ="";
string FilePublisher::m_AppInterfaceReplyXSLT = "";

string FilePublisher::m_TransformFile = "";
string FilePublisher::m_TempDestinationPath = "";

//constructor
FilePublisher::FilePublisher() : Endpoint(), m_Watcher( NULL ), m_WatchQueue( "" ), 
	m_WatchQueueManager( "" ), m_WatchTransportURI( "" ), m_WMQBatchFilter( false ), m_MoreMessages( true ), m_AggregateMessages( false ),
	m_StrictSwiftFormat( false ), m_Metadata(), m_First( true ), m_BatchId( "" ), m_RenameFilePattern( "" ),
	m_BlobLocator( "" ), m_BlobFilePattern( "" ), m_MaxMessagesPerFile( -1 ), m_CurrentMessageIndex( 0 ), m_CommitTransport ( false )
{
	DEBUG2( "CONSTRUCTOR" );
	
#ifdef USING_REGULATIONS
	m_ParamFileXslt = "";
	m_LAUCertFile = "";
#endif //USING_REGULATIONS
}

//destructor
FilePublisher::~FilePublisher()
{
	delete m_Watcher;
}

void FilePublisher::Init()
{
	DEBUG( "INIT" );

	m_DestinationPath = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::FILEDESTPATH );
	m_TransformFile = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::FILEXSLT, "" );
	m_Pattern = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::FILEFILTER );
	m_RenameFilePattern = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::RENAMEPATTERN, "" );
	m_ReplyPattern = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::RPLFILEFILTER, m_Pattern );
	m_ReplyDestinationPath = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::RPLDESTTPATH, m_DestinationPath );
	m_ReplyFeedback = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::RPLFEEDBACK, "" );
	m_AppInterfaceReplyXSLT = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::APPINTRPLXSLT, "" );

	// Read Blob handling options
	// Feature: To save BLOB base64 content field to disk on message publishing 
	m_BlobLocator = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::BLOBLOCATOR, "" );
	m_BlobFilePattern = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::BLOBPATTERN, "" );	

	string strictSwiftFormat = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::STRICTSWIFTFMT, "false" );
	m_StrictSwiftFormat = ( strictSwiftFormat == "true" );

	m_TempDestinationPath = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::FILETEMPDESTPATH, "" );

#ifdef USING_REGULATIONS
	m_ParamFileXslt = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::PARAMFXSLT, "" );
	m_LAUCertFile = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::LAUCERTIFICATE, "" );
#endif //USING_REGULATIONS

	//TODO check delete rights on m_WatchQueue - can't delete source otherwise
	if ( haveGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::BATCHMGRTYPE ) )
	{
		string batchManagerType = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::BATCHMGRTYPE );		
		try
		{
			if ( batchManagerType == "Flatfile" )
			{
				m_BatchManager = BatchManagerBase::CreateBatchManager( BatchManagerBase::Flatfile );
			}
			else if ( batchManagerType == "XMLfile" )
			{
				m_BatchManager = BatchManagerBase::CreateBatchManager( BatchManagerBase::XMLfile );
				BatchManager< BatchXMLfileStorage >* batchManager = static_cast< BatchManager< BatchXMLfileStorage >* >( m_BatchManager );
				if ( haveGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::BATCHMGRXSLT ) )
				{
					string batchXsltFile = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::BATCHMGRXSLT );

					//Set BatchXsltFile
					batchManager->storage().setXslt( batchXsltFile );
				}
#if XERCES_VERSION_MAJOR >= 3
				else if ( haveGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::BATCHMGRXPATH ) )
				{
					string batchXPath = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::BATCHMGRXPATH );
			
					//Set BatchXPath
					batchManager->storage().setXPath( batchXPath );
					batchManager->storage().setXPathCallback( FilePublisher::XPathCallback );
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

	if( haveGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::REPLYBATCHMGRTYPE ) )
	{
		string replyBatchManagerType = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::REPLYBATCHMGRTYPE);
		try
		{
			if ( replyBatchManagerType == "Flatfile" )
			{
				m_ReplyBatchManager = BatchManagerBase::CreateBatchManager( BatchManagerBase::Flatfile );
			}
			else if ( replyBatchManagerType == "XMLfile" )
			{
				m_ReplyBatchManager = BatchManagerBase::CreateBatchManager( BatchManagerBase::XMLfile );
				BatchManager< BatchXMLfileStorage >* batchManager = static_cast< BatchManager< BatchXMLfileStorage >* >( m_ReplyBatchManager );
				if ( haveGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::REPLYBATCHMGRXSLT ) )
				{
					string replyBatchXsltFile = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::REPLYBATCHMGRXSLT );
					batchManager->storage().setXslt( replyBatchXsltFile );
				}
			}
			else
			{
				TRACE( "Reply Batch manager type [" << replyBatchManagerType << "] not implemented. No batch processing will be performed." );
			}
		}	
		catch( const std::exception& ex )
		{
			TRACE( "Reply Batch manager could not be created. [" << ex.what() << "]" );
			if ( m_ReplyBatchManager != NULL )
			{
				delete m_ReplyBatchManager;
				m_ReplyBatchManager = NULL;
			}
			throw;
		}
		catch( ... )
		{
			TRACE( "Reply Batch manager could not be created. [unknown exception]" );
			if ( m_ReplyBatchManager != NULL )
			{
				delete m_ReplyBatchManager;
				m_ReplyBatchManager = NULL;
			}
			throw;
		}
	}
	else {
		m_ReplyBatchManager = NULL;
	}
	if ( m_TempDestinationPath.length() == 0 )
	{
		TRACE( "Temporary destination folder [ config key = \"MQSeriesToApp.TempDestinationPath\" ] is not used." );
	}

#ifdef USING_REGULATIONS
	if ( m_ParamFileXslt.length() == 0 )
	{
		TRACE( "Parameter file will not be generated. [ config key = \"MQSeriesToApp.ParamFileXslt\" ] is not defined." );
	}
#endif //USING_REGULATIONS

	// expect the first filter to be of WMQ type
	AbstractFilter* firstFilter = ( *m_FilterChain )[ 0 ];
	if ( ( firstFilter == NULL ) || ( firstFilter->getFilterType() != FilterType::MQ ) )
		throw logic_error( "First filter in a publisher's chain should be of MQ type. Please check the config file." );

	MqFilter* getterFilter = dynamic_cast< MqFilter* >( firstFilter );
	if ( getterFilter == NULL )
		throw logic_error( "First filter in a publisher's chain should be of MQ type. Please check the config file." );

	m_Watcher = AbstractWatcher::createWatcher( *firstFilter, &m_NotificationPool );
	m_WMQBatchFilter = getterFilter->isBatch();

	// only add one record/batch, only add unique records to pool
	if( m_WMQBatchFilter )
		m_Watcher->setWatchOptions( MqWatcher::NotifyGroups | MqWatcher::NotifyUnique );

	if ( m_WMQBatchFilter && m_MessageThrottling > 0 )
		throw runtime_error( "Error: Message throttling for MQ group messages" );

	if ( !m_WMQBatchFilter && m_MessageThrottling <= 0 && m_BatchManager != NULL )
		throw runtime_error( "Please provide MessageThrottling configuration key" );

	if( m_MessageThrottling && m_BatchManager )
	{
		m_AggregateMessages = true;
		// limit messages only for aggregate mode (batch mode is limited on RE)
		string maxMessagesPerFile = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::MAXFILESPERFILE, "-1" );
		m_MaxMessagesPerFile = StringUtil::ParseInt( maxMessagesPerFile );
	}

	if ( !m_WMQBatchFilter && m_BatchManager && m_MaxMessagesPerFile <= 0 )
		throw runtime_error( "Please provide MaxFilesPerFile configuration key" );

	m_Watcher->setMessageThrottling( m_MessageThrottling );
}

void FilePublisher::internalStart()
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
			
			/*m_CurrentMessageId = notificationObject->getObjectId();
			m_CurrentMessageLength = notificationObject->getObjectSize();
			m_CurrentGroupId = Base64::decode( notificationObject->getObjectGroupId() );
	
			DEBUG( "Notified : [" << m_CurrentMessageId << "] in group [" << notificationObject->getObjectGroupId()
				<< "] size [" << m_CurrentMessageLength << "]" );
			*/
			m_Metadata.setId( notificationObject->getObjectId() );
			m_Metadata.setLength( notificationObject->getObjectSize() );
			m_Metadata.setGroupId( Base64::decode( notificationObject->getObjectGroupId() ) );
			
			DEBUG( "Notified : [" << m_Metadata.id() << "] in group [" <<  notificationObject->getObjectGroupId() << "] size [" << m_Metadata.length() << "]" );

			//TODO throw only on fatal error. The connector should respawn this thread
			bool succeeded = true;
			m_IsLast = false;
			m_First = true;
			bool aggregateMessages = false;

			try
			{
				if ( ( m_BatchManager != NULL ) && ( m_BatchManager->getStorageCategory() == BatchManagerBase::XMLfile ) )
				{
					if( m_MessageThrottling > 0 )
					{
						DEBUG_GLOBAL( "Use BatchXMLfileStorage to aggregate messages" );
						aggregateMessages = true;
						m_MoreMessages = true;
						m_CurrentMessageIndex = 0;
					}
					else
						DEBUG_GLOBAL( "Use BatchXMLfileStorage to batch messages" );

					DEBUG_GLOBAL( "First batch item ... opening storage" );
					// open batch ( next iteration it will already be open )
					m_BatchManager->open( m_Metadata.groupId(), ios_base::out );

					if ( (m_ReplyBatchManager != NULL ) && (m_ReplyBatchManager->getStorageCategory() == BatchManagerBase::XMLfile ) )
						m_ReplyBatchManager->open( m_Metadata.groupId(), ios_base::out );
				}
				
				DEBUG( "Performing message loop ... " );
				succeeded = PerformMessageLoop( m_WMQBatchFilter || aggregateMessages );
				DEBUG( "Message loop finished ok. " );
			}
			catch( const AppException& ex )
			{
				string exceptionType = typeid( ex ) .name();
				string errorMessage = ex.getMessage();
				
				TRACE_GLOBAL( exceptionType << " encountered while processing message : " << errorMessage );
				succeeded = false;
			}
			catch( const std::exception& ex )
			{
				string exceptionType = typeid( ex ) .name();
				string errorMessage = ex.what();
				
				TRACE_GLOBAL( exceptionType << " encountered while processing message : " << errorMessage );
				succeeded = false;
			}
			catch( ... )
			{
				TRACE_GLOBAL( "[unknown exception] encountered while processing message. " );
				succeeded = false;
			}

			if ( !succeeded && m_Running )
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

void FilePublisher::internalStop()
{
	DEBUG( "STOP" );
	
	// ensure watcher is dead ( the watcher will lock on pool and wait until it is empty )
	m_Watcher->setEnableRaisingEvents( false );

	// wait for fetcher to remove all items if the thread is running
	//if ( 0 == pthread_kill( m_SelfThreadId, 0 ) )
	//{
	//	m_NotificationPool.waitForPoolEmpty();
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
}

bool FilePublisher::moreMessages() const
{
	if ( ( m_AggregateMessages && ( !m_MoreMessages || ( m_CurrentMessageIndex == m_MaxMessagesPerFile ) ) ) || ( ( m_WMQBatchFilter && ( m_IsLast || !m_MoreMessages ) ) ) )
		return false;
	else
		return true;
}

string FilePublisher::Prepare()
{
	DEBUG2( "PREPARE" );
	
	//sleep( 5 ); //TODO remove this when everything works ok

	const char* strTemp = m_DestinationPath.c_str();
	
	// check write access to m_DestinationPath
	if ( ( access( strTemp, 2 ) ) == -1 )
	{	
		stringstream errorMessage;
		errorMessage << "Don't have write permission to the destination folder [" << m_DestinationPath << "]" << endl;
		TRACE( errorMessage.str() );
		TRACE( "Sleeping 30 seconds before next attempt" );
		sleep( 30 );

		throw AppException( errorMessage.str() );		
	}
	
	// check write access to m_TempDestinationPath
	if ( m_TempDestinationPath.length() != 0 )
	{
		if ( ( access( m_TempDestinationPath.c_str(), 2 ) ) == -1 )
		{	
			stringstream errorMessage;
			errorMessage << "Don't have write permission to the temporary destination folder [" << m_TempDestinationPath << "]" << endl;
			TRACE( errorMessage.str() );
			TRACE( "Sleeping 30 seconds before next attempt" );
			sleep( 30 );
		
			throw AppException( errorMessage.str() );
		}
	}
	//TODO check if enough disk space is available ... 
	// not for the moment

	// open the file in exclusive mode 
	string destFilename = Path::Combine( m_DestinationPath, m_Pattern );
	string replyDestFilename = Path::Combine( m_ReplyDestinationPath, m_ReplyPattern );
	
	int tempDestLength = m_TempDestinationPath.length();
	destFilename = Path::Combine( ( tempDestLength == 0 ) ? m_DestinationPath : m_TempDestinationPath, m_Pattern );
	
	// batchid is set for the first message in an xmlfile only
	if ( m_BatchManager != NULL )
	{
		if( m_First )
		{
			m_BatchId = Collaboration::GenerateMsgID();
			if( m_AggregateMessages )
			{
				m_Metadata.setGroupId( m_BatchId );
				DEBUG( "Generated aggregation  id is [" << m_BatchId << "]" );
			}
		}
	}
	else
		m_BatchId = "";
	m_MessageId = Collaboration::GenerateMsgID();

	static const boost::regex timeFormatRegex("\\{time:(.*?)\\}");
	m_CurrentMessageIndex++;

	stringstream millisecondsSinceEpoch;

	if ( m_First || m_CurrentMessageIndex == 1 )
	{
		  if ( m_MaxMessagesPerFile > 0 )
		  {
			  if ( m_CurrentMessageIndex == 1 )
				  m_TimeSinceEpoch = TimeUtil::GetTimeSinceEpoch();

			  if ( m_CurrentMessageIndex > m_MaxMessagesPerFile )
			  {
				  m_TimeSinceEpoch = TimeUtil::GetTimeSinceEpoch();
				  m_CurrentMessageIndex = 1;
			  }
		  }
		  else
			m_TimeSinceEpoch = TimeUtil::GetTimeSinceEpoch();
	}

	millisecondsSinceEpoch << m_TimeSinceEpoch.total_milliseconds();
	
	// look for {values} and replace fields
	if ( ( m_Pattern.find( "{" ) == string::npos ) || ( m_Pattern.find( "}" ) == string::npos )  )
	{
		// Append to name the messageId ( if we're not running in batch mode )
		m_DestFileName = ( m_BatchManager == NULL ) ? destFilename + m_MessageId : destFilename + m_BatchId;
	}
	else
	{
		m_DestFileName = StringUtil::Replace( destFilename, "{msgid}", m_MessageId );
		m_DestFileName = StringUtil::Replace( m_DestFileName, "{batchid}", m_BatchId );
		m_DestFileName = StringUtil::Replace( m_DestFileName, "{millisecond}", millisecondsSinceEpoch.str() );
		m_DestFileName = StringUtil::Replace( m_DestFileName, "{mqbatchid}", m_Metadata.groupId() );

		boost::smatch matches;
		if (boost::regex_search( m_DestFileName, matches, timeFormatRegex ))
		{
			const string time = TimeUtil::ToString( TimeUtil::ToPtime(m_TimeSinceEpoch), matches[1] );
			m_DestFileName = StringUtil::Replace( m_DestFileName, matches[0], time );
		}
	}
		
	if ( ( m_ReplyPattern.find( "{" ) == string::npos ) || ( m_ReplyPattern.find( "}" ) == string::npos )  )
	{
		// Append to name the messageId ( if we're not running in batch mode )
		m_ReplyDestFileName = ( m_BatchManager == NULL ) ? replyDestFilename + m_MessageId : destFilename + m_BatchId;
	}
	else
	{
		m_ReplyDestFileName = StringUtil::Replace( replyDestFilename, "{msgid}", m_MessageId );
		m_ReplyDestFileName = StringUtil::Replace( m_ReplyDestFileName, "{batchid}", m_BatchId );
		m_ReplyDestFileName = StringUtil::Replace( m_ReplyDestFileName, "{millisecond}", millisecondsSinceEpoch.str() );

		boost::smatch matches;
		if (boost::regex_search(m_ReplyDestFileName, matches, timeFormatRegex))
		{
			const string time = TimeUtil::ToString( TimeUtil::ToPtime(m_TimeSinceEpoch), matches[1] );
			m_ReplyDestFileName = StringUtil::Replace( m_ReplyDestFileName, matches[0], time );
		}
	}
		
	DEBUG( "Opening output file [" << m_DestFileName << "]. Reply file is [" << m_ReplyDestFileName << "]" );
	bool openNeeded = true;

	m_DestFile.exceptions( ifstream::failbit | ifstream::badbit );

	try
	{
		if ( m_BatchManager != NULL || m_MaxMessagesPerFile > 0 )
		{
			if ( !m_DestFile.is_open() )
				m_DestFile.open( m_DestFileName.data(), ios::binary | ios::out | ios::app );
			else
			{
				openNeeded = false;
				DEBUG( "Output file [" << m_DestFileName << "] already opened" );
			}
		}
		else
			m_DestFile.open( m_DestFileName.data(), ios::binary | ios::out | ios::trunc );

		if ( openNeeded && !m_DestFile.is_open() )
			throw runtime_error( "unknown reason" );
	}
	catch( const std::exception& ex )
	{
		stringstream errorMessage;
		errorMessage << "Can't open destination file [" << m_DestFileName << "] [" << typeid( ex ).name() << " - " << ex.what() << "]" << endl;
		TRACE( errorMessage.str() );
		
		//sleep here until a way to abort from prepare is found.
		TRACE( "Sleeping 10 seconds before next attempt( previous message failed )" );
		sleep( 10 );
		throw AppException( errorMessage.str() );
	}
	catch( ... )
	{
		stringstream errorMessage;
		errorMessage << "Can't open destination file [" << m_DestFileName << "] [unknown reason]" << endl;
		TRACE( errorMessage.str() );
		
		//sleep here until a way to abort from prepare is found.
		TRACE( "Sleeping 10 seconds before next attempt( previous message failed )" );
		sleep( 10 );
		throw AppException( errorMessage.str() );
	}

	//m_First = false;
	return m_Metadata.id(); //m_CurrentMessageId;
}

void FilePublisher::Process( const string& correlationId )
{	
	DEBUG2( "PROCESS" );
	DEBUG( "Current message length : " << m_Metadata.length() );
	
	FinTPMessage::Message message( m_Metadata );
	
	// reuse member collection 
	m_TransportHeaders.Clear();
	try
	{
		m_TransportHeaders.Add( MqFilter::MQGROUPID, m_Metadata.groupId() );
		m_TransportHeaders.Add( MqFilter::MQMSGSIZE, StringUtil::ToString( m_Metadata.length() ) );	
		m_TransportHeaders.Add( XSLTFilter::XSLTUSEEXT, "true" );
		
		// as the message wil end up in MQ, no output is needed
		try
		{
			m_FilterChain->ProcessMessage( message.dom(), m_TransportHeaders, false );
		}
		catch ( const AppException& exception )
		{
			// if aggregate, the reason failure doesn't matter
			if( m_AggregateMessages && !m_First )
			{
				//serialize XML 
				if( m_BatchManager->getStorageCategory() == BatchManagerBase::XMLfile )
				{
					BatchManager< BatchXMLfileStorage >* batchManager = dynamic_cast< BatchManager< BatchXMLfileStorage >* >( m_BatchManager );
					string messageOutput = batchManager->storage().getSerializedXml();
					ProcessFile( messageOutput );

				}
				else //BatchManagerBase::Flatfile
					MoveFile( m_DestFileName );

				m_BatchManager->close( m_Metadata.groupId() );
				m_MoreMessages = false;
				return;
			}
			else
			{
				m_MoreMessages = false;
				Rollback();
				return;
			}
		}

		// simulate an error at every 10 messages ( test only )
		//if( COUNTER( TRN_ATTEMPTS ) % 10 == 9 )
		//throw logic_error( "test error" );
	}
	catch( const std::exception& ex )
	{
		// release buffer
		message.releaseDom();
			
		// format error
		stringstream errorMessage;
		errorMessage << "Can't read the message [" << m_Metadata.id() << 
			"] from queue [" << m_WatchQueue << "]. Check inner exception for details." << endl;
			
		TRACE( errorMessage.str() );
		TRACE( ex.what() );
		
		//TODO check this for returning a pointer to a const ref
		throw AppException( errorMessage.str(), ex );
	}
	catch( ... ) //TODO put specific catches before this
	{
		// release buffer
		message.releaseDom();
		
		// format error
		stringstream errorMessage;
		errorMessage << "Filter can't process message." << endl;
		
		TRACE( errorMessage.str() );
		throw AppException( errorMessage.str() );		
	}

	if ( m_TransportHeaders.ContainsKey( MqFilter::MQLASTMSG ) )
		m_IsLast = ( m_TransportHeaders[ MqFilter::MQLASTMSG ] == "1" );

	//Add message to batch or write to file
	DEBUG( "Message is in buffer; size is : " << m_Metadata.length() );

	string payload = "", feedback = "", theCorrelationId = "";
	message.getInformation( FinTPMessage::Message::FILECONNECTOR );
	theCorrelationId = message.correlationId();
	if ( theCorrelationId.length() != 0 )
		setCorrelationId( theCorrelationId );
	payload = message.payload();
	feedback = message.feedback();

	string payloadTooLong = ( payload.length() > 100 ) ? payload.substr( 0, 100 ) : payload;
	DEBUG( "Payload is ( first 100 bytes ) : [" << payloadTooLong );
	
	string messageOutput = Base64::decode( payload );
	string replyMessageOutput = messageOutput;

	if ( ( m_BatchManager != NULL ) )
	{
		BatchItem batchItem;
		batchItem.setPayload( messageOutput );
		batchItem.setBatchId( m_Metadata.groupId() );
		batchItem.setMessageId( m_Metadata.id() );

		DEBUG( "Add message to batch " );
		*m_BatchManager << batchItem;

		if ( !moreMessages() )
		{
			DEBUG( "LAST message" );

			//serialize XML 	
			if( m_BatchManager->getStorageCategory() == BatchManagerBase::XMLfile )	
			{
				BatchManager< BatchXMLfileStorage >* batchManager = dynamic_cast< BatchManager< BatchXMLfileStorage >* >( m_BatchManager );
				messageOutput = batchManager->storage().getSerializedXml();
			}

			m_BatchManager->close( m_Metadata.groupId() );
		}
	}
	string messageFileName = "";
	if ( !m_AppInterfaceReplyXSLT.empty() )
	{
#ifdef WIN32
		messageFileName = StringUtil::Replace( m_DestFileName, m_TempDestinationPath + "\\", "");
#else
		messageFileName = StringUtil::Replace( m_DestFileName, m_TempDestinationPath + "/", "");
#endif// WIN32

		m_ReplyTransportHeaders.Clear();

		m_ReplyTransportHeaders.Add( XSLTFilter::XSLTFILE, m_AppInterfaceReplyXSLT );
		m_ReplyTransportHeaders.Add( XSLTFilter::XSLTUSEEXT, "true");
		m_ReplyTransportHeaders.Add( "XSLTPARAMBATCHID", StringUtil::Pad( m_Metadata.groupId(), "\'", "\'" ) );
		m_ReplyTransportHeaders.Add( "XSLTPARAMMSGID", StringUtil::Pad( m_MessageId, "\'", "\'"));
		m_ReplyTransportHeaders.Add( "XSLTPARAMFILENAME", StringUtil::Pad( messageFileName, "\'", "\'"));
		m_ReplyTransportHeaders.Add( "XSLTPARAMREQUESTOR", StringUtil::Pad( getServiceName(), "\'", "\'" ) );

		XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* inData = XmlUtil::DeserializeFromString( replyMessageOutput );

		unsigned char **outXslt = new (unsigned char *);

		try
		{
			XSLTFilter m_XSLTFilter;

			m_XSLTFilter.ProcessMessage( inData, outXslt, m_ReplyTransportHeaders, true );
			replyMessageOutput = (char *)*outXslt;

			delete outXslt;
			outXslt = NULL;
		}
		catch( ... )
		{
			if ( outXslt != NULL )
			{
				delete outXslt;
				outXslt = NULL;
			}

			throw;
		}
	}
	if ( m_ReplyBatchManager != NULL )
	{
		try
		{
			BatchItem replyBatchItem;
			replyBatchItem.setPayload( replyMessageOutput );
			replyBatchItem.setBatchId( m_Metadata.groupId() );
			replyBatchItem.setMessageId( m_Metadata.id() );

			DEBUG("Add reply message to batch ");
			*m_ReplyBatchManager << replyBatchItem;

			if (!moreMessages())
			{
				DEBUG("LAST reply message");

				//serialize XML 	
				if ( m_ReplyBatchManager->getStorageCategory() == BatchManagerBase::XMLfile )
				{
					BatchManager< BatchXMLfileStorage >* batchManager = dynamic_cast<BatchManager< BatchXMLfileStorage >*>( m_ReplyBatchManager );
					replyMessageOutput = batchManager->storage().getSerializedXml();
				}

				m_ReplyBatchManager->close( m_Metadata.groupId() );
			}
		}
		catch( ... )
		{
			throw;
		}

	}
	// if there is no batchmanager, write it to file, if there is and this is the last message write it
	if ( ( m_BatchManager == NULL ) || !moreMessages() )
	{
		ProcessFile( messageOutput, feedback );

		if ( !messageFileName.empty() )
		{
			string fileExtension = m_Pattern.substr( m_Pattern.find_last_of( "." ) );
			m_DestFileName = StringUtil::Replace( m_DestFileName, fileExtension, "_reply" + fileExtension );

			ProcessFile( replyMessageOutput, feedback );
		}
	}
	else
	{
		DEBUG( "There are more messages to process [not written to file]" );
	}

	m_First = false;
}

void FilePublisher::Commit()
{
	if ( !moreMessages() )
	{
		DEBUG( "Final commit." );
		
		//TODO check return code
		m_FilterChain->Commit();
		
		if( m_DestFile.is_open() )
			m_DestFile.close();
	}
	else if( m_BatchManager != NULL )
		DEBUG( "Partial commit for batch item." );

}

void FilePublisher::Abort()
{
	//TODO move the message to dead letter queue or backout queue
	//for now, commit message got
	//do this only if not batch or last message in batch 
	if ( !moreMessages() || ( m_BatchManager == NULL ) )
	{
		
		m_FilterChain->Abort();
		if( m_DestFile.is_open() )
			m_DestFile.close();
			
		if( remove( m_DestFileName.data() ) == -1 )
		{
			int errCode = errno;
#ifdef CRT_SECURE
			char errBuffer[ 95 ];
			strerror_s( errBuffer, sizeof( errBuffer ), errCode );
			TRACE( "Unable to remove destination file [" << m_DestFileName << "]. Error code [" << errCode << "] : " << errBuffer ); 
#else
			TRACE( "Unable to remove destination file [" << m_DestFileName << "]. Error code [" << errCode << "] : " << strerror( errCode ) ); 
#endif	
			if( errCode == ENOENT ) //No such file or directory
			{
				if( remove( m_ReplyDestFileName.data() ) != 0 )
				{
					errCode = errno; 
#ifdef CRT_SECURE
					char errBuffer2[ 95 ];
					strerror_s( errBuffer2, sizeof( errBuffer2 ), errCode );
					TRACE( "Unable to remove reply file [" << m_ReplyDestFileName << "]. Error code [" << errCode << "] : " << errBuffer2 );
#else
					TRACE( "Unable to remove reply file [" << m_ReplyDestFileName << "]. Error code [" << errCode << "] : " << strerror( errCode ) );
#endif	
				}
				else 
				{
					DEBUG( "Reply file [" << m_ReplyDestFileName << "] deleted" );
				}
			}
		}
		else 
		{
			DEBUG( "Destination file [" << m_DestFileName << "] deleted" );
		}
	}
}

void FilePublisher::Rollback()
{
	
	DEBUG( "ROLLBACK" );
	//m_CommitTransport = false;
	if ( !moreMessages() || ( m_BatchManager == NULL ) )
	{
		m_CurrentMessageIndex--;

		//TODO check return code...
		m_FilterChain->Abort();
		if( m_DestFile.is_open() )
			m_DestFile.close();
			
		if( remove( m_DestFileName.data() ) == -1 )
		{
			int errCode = errno;
#ifdef CRT_SECURE
			char errBuffer[ 95 ];
			strerror_s( errBuffer, sizeof( errBuffer ), errCode );
			TRACE( "Unable to remove destination file [" << m_DestFileName << "]. Error code [" << errCode << "] : " << errBuffer ); 
#else
			TRACE( "Unable to remove destination file [" << m_DestFileName << "]. Error code [" << errCode << "] : " << strerror( errCode ) ); 
#endif	
			if( errCode == ENOENT ) //No such file or directory
			{
				if( remove( m_ReplyDestFileName.data() ) != 0 )
				{
					errCode = errno; 
#ifdef CRT_SECURE
					char errBuffer2[ 95 ];
					strerror_s( errBuffer2, sizeof( errBuffer2 ), errCode );
					TRACE( "Unable to remove reply file [" << m_ReplyDestFileName << "]. Error code [" << errCode << "] : " << errBuffer2 );
#else
					TRACE( "Unable to remove reply file [" << m_ReplyDestFileName << "]. Error code [" << errCode << "] : " << strerror( errCode ) );
#endif	
				}
				else 
				{
					DEBUG( "Reply file [" << m_ReplyDestFileName << "] deleted" );
				}
			}
		}
		else 
		{
			DEBUG( "Destination file [" << m_DestFileName << "] deleted" );
		}
	}
}

// private helper functions

void FilePublisher::WriteMessage( const char* message, const streamsize length )
{
	DEBUG2( "Trying to write message to file " );
	/*try
	{*/
		if ( m_StrictSwiftFormat )
		{
			string output = "";

			output.push_back( ( char )0x01 );
			for( streamsize i=0; i<length; i++ )
			{
				if ( message[ i ] == '\x0A' )
				{
					if ( ( i > 1 ) && ( message[ i-1 ] != '\x0D' ) )
						output.push_back( '\x0D' );
				}
				output.push_back( message[ i ] );
			}

			unsigned int remainder = 512 - ( ( output.length() + 1 ) % 512 ); // + 0x03
			if ( remainder == 512 )
				remainder = 0;
			
			DEBUG( "Strict SWIFT format required. Need to append " << remainder << " blanks ... " );
			output.push_back( ( char )0x03 );
			output.append( string( remainder, ' ' ) );

			assert( output.length() % 512 == 0 );

			m_DestFile.write( output.c_str(), output.length() );
		}
		else
		{
			m_DestFile.write( message, length );
		}

		DEBUG2( "Message succesfully writen to file" );
	/*}
	catch( ... )
	{
		stringstream errorMessage;
		errorMessage << "An error occured while writing message to file";
			
		TRACE( errorMessage.str() );
		
		throw runtime_error( errorMessage.str() );
	}*/
}

string FilePublisher::Serialize( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument *doc, const string& xsltFilename, const string& filename, const unsigned long fileSize, const string& HMAC )
{
	unsigned char **outXslt = new ( unsigned char * );
	unsigned char *outputXslt = NULL;
	string result;

	XSLTFilter xsltFilter;
	
	NameValueCollection trHeaders;
	trHeaders.Add( XSLTFilter::XSLTFILE, xsltFilename );
	trHeaders.Add( XSLTFilter::XSLTUSEEXT, "true" );
	trHeaders.Add( "XSLTPARAMMSGID", StringUtil::Pad( Collaboration::GenerateGuid(), "\'", "\'" ) );
	trHeaders.Add( "XSLTPARAMFILENAME", StringUtil::Pad( filename, "\'", "\'" ) );
	trHeaders.Add( "XSLTPARAMFILESIZE", StringUtil::Pad( StringUtil::ToString( fileSize ), "\'", "\'" ) );
#ifdef LAU
	trHeaders.Add( "XSLTPARAMDATASIGNITURE", StringUtil::Pad( HMAC, "\'", "\'" ) );
#endif

	try
	{
		xsltFilter.ProcessMessage( doc, outXslt, trHeaders, true );
		outputXslt = *outXslt;
				
		// check if the result is xml
		result = string( ( char* )outputXslt );
	}
	catch( ... )
	{
		delete[] outputXslt;
		outputXslt = NULL;
		delete outXslt;
		outXslt = NULL;

		throw;
	}

	delete[] outputXslt;
	outputXslt = NULL;
	delete outXslt;
	outXslt = NULL;

	return result;
}

string FilePublisher::XPathCallback( const string& itemNamespace )
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

string FilePublisher::SaveBlobToFile( const string& xmlData )
{
	string result = xmlData;
	XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument *doc = NULL;
	//stream nedeed to put image to disk
	ofstream outf;
	string destinationBlobFilename;
	try
	{
		doc = XmlUtil::DeserializeFromString( xmlData );
		if ( doc == NULL ) 
			return result;

		XALAN_USING_XALAN( XercesDocumentWrapper );
		XALAN_USING_XALAN( XalanDocument );

		// map xerces dom to xalan document
#ifdef XALAN_1_9
		XALAN_USING_XERCES( XMLPlatformUtils )
		XercesDocumentWrapper docWrapper( *XMLPlatformUtils::fgMemoryManager, doc, true, true, true );
#else
		XercesDocumentWrapper docWrapper( doc, true, true, true );
#endif
		XalanDocument* theDocument = ( XalanDocument* )&docWrapper;	

		{ // Xalan required block
			string theBlob = XPathHelper::SerializeToString( XPathHelper::Evaluate( m_BlobLocator + "/child::text()", theDocument ) );
			if ( theBlob.length() <= 0 )
			{
				if( doc != NULL )
				{
					doc->release();
					doc = NULL;
				}  
				return result;
			}

			//parse the blob locator xpath and remove the blob from doc
			vector< string > locatorBits;
			StringUtil pathSplitter( m_BlobLocator );
			pathSplitter.Split( "/" );
			while( pathSplitter.MoreTokens() )
			{
				string crtParam = StringUtil::Trim( pathSplitter.NextToken() );
				if ( crtParam.length() > 0 )
					locatorBits.push_back( crtParam );
			};

			//iterate through the locator bits and find the content
			string pathSoFar = "";
			DOMElement* root = doc->getDocumentElement();

			// skip root node
			vector< string >::const_iterator bitsWalker = locatorBits.begin();
			bitsWalker++;

			for( ; bitsWalker != locatorBits.end(); bitsWalker++ )
			{
				DOMNodeList* nextNodes = root->getElementsByTagName( unicodeForm( *bitsWalker ) );
				if ( ( nextNodes == NULL ) || ( nextNodes->getLength() == 0 ) )
					throw runtime_error( "Missing required [] element in [] document" );

				root = dynamic_cast< DOMElement* >( nextNodes->item( 0 ) );
				if ( root == NULL )
					throw logic_error( "Bad type : [/[0]] should be and element" );
			}

			// remove blob node
			if ( root->getFirstChild() != NULL )
			{
				DOMNode* theRemovedChild = root->removeChild( root->getFirstChild() );
				theRemovedChild->release();
				root->setTextContent( unicodeForm( "[removed]" ) );
			}
			else
				throw logic_error( "Could not find payload in []" );

			//find tokens in blobfilepattern
			vector< string > patternTokens;
			string::size_type lastSeparator = 0;

			string::size_type nextOpenSeparator = m_BlobFilePattern.find_first_of( '{', lastSeparator );
			string::size_type nextCloseSeparator = m_BlobFilePattern.find_first_of( '}', lastSeparator );

			while ( ( nextOpenSeparator != string::npos ) && ( nextCloseSeparator != string::npos ) )
			{
				patternTokens.push_back( m_BlobFilePattern.substr( nextOpenSeparator + 1, nextCloseSeparator - nextOpenSeparator - 1 ) );
				lastSeparator = nextCloseSeparator + 1;

				nextOpenSeparator = m_BlobFilePattern.find_first_of( '{', lastSeparator );
				nextCloseSeparator = m_BlobFilePattern.find_first_of( '}', lastSeparator );
			}

			//find tokens in message and replace in path
			destinationBlobFilename = m_BlobFilePattern;
			for( vector< string >::const_iterator tokensWalker = patternTokens.begin(); tokensWalker != patternTokens.end(); tokensWalker++ )
			{
				string theField = XPathHelper::SerializeToString( XPathHelper::Evaluate( ( *tokensWalker ) + "/child::text()", theDocument ) );
				destinationBlobFilename = StringUtil::Replace( destinationBlobFilename, "{" + *tokensWalker + "}", theField );
			}

			//StringUtil::SerializeToFile( destinationBlobFilename, theBlob );
			string decodedBlob = Base64::decode( theBlob );
			
			outf.exceptions( ifstream::failbit | ifstream::badbit );
			outf.open( destinationBlobFilename.c_str(), ios_base::out | ios_base::binary );
			outf.write( decodedBlob.c_str(), decodedBlob.size() );
			outf.close();
			
			result = XmlUtil::SerializeToString( doc );
		}
	}
	catch( ofstream::failure& ex )
	{
		if( doc != NULL )
		{
			doc->release();
			doc = NULL;
		}
		stringstream errorMessage;
		errorMessage << "Error writing image [" << destinationBlobFilename << "] to repository";
		TRACE( errorMessage.str() << "Error is:[" << ex.what() << "]" );
		try
		{
			outf.close();
		}catch( ... ){};
		
		throw AppException( errorMessage.str(), ex );		
	}
	catch( ... )
	{
		if( doc != NULL )
		{
			doc->release();
			doc = NULL;
		}
		throw;
	}
	if( doc != NULL )
	{
		doc->release();
		doc = NULL;
	}  
	return result;
}


void FilePublisher::internalCleanup()
{
	try
	{
		if ( m_MaxMessagesPerFile > 0 && m_CurrentMessageIndex > 1 )
		{
			if ( m_DestFile.is_open() )
				m_DestFile.close();
			m_First = true;
			m_CurrentMessageIndex = 0;

			string finalDestinationFilename = Path::GetFilename( m_LastFilename );
			finalDestinationFilename = Path::Combine( m_DestinationPath, finalDestinationFilename );
			if ( rename( m_LastFilename.c_str(), finalDestinationFilename.c_str() ) != 0 )
			{
				int errCode = errno;
				stringstream errorMessage;
	#ifdef CRT_SECURE
				char errBuffer[ 95 ];
				strerror_s( errBuffer, sizeof( errBuffer ), errCode );
				errorMessage << "Rename out file [" << m_LastFilename << "] -> [" << finalDestinationFilename << "] failed with code : " << errCode << " " << errBuffer;
	#else
				errorMessage << "Rename out file [" << m_LastFilename << "] -> [" << finalDestinationFilename << "] failed with code : " << errCode << " " << strerror( errCode );
	#endif
				TRACE( errorMessage.str() );
			}
		}
	}
	catch (...)
	{
	}
}

void FilePublisher::MoveFile( const string& destFilename )
{
	// move to final destination path
	// !!! path-ul contine si "/" si "\"
	if ( m_TempDestinationPath.length() > 0 )
	{
		DEBUG( "Moving files to destination path..." );
		int errCode = 0;
		try
		{
			// move param file first from temp->destination
#ifdef USING_REGULATIONS
			if ( m_ParamFileXslt.length() > 0 )
			{
				string finalParamFilename = Path::GetFilename( paramFilename );
				finalParamFilename = Path::Combine( m_DestinationPath, finalParamFilename );
				if ( rename( paramFilename.c_str(), finalParamFilename.c_str() ) != 0 )
				{ 
					errCode = errno;
					stringstream errorMessage;
#ifdef CRT_SECURE
					char errBuffer[ 95 ];
					strerror_s( errBuffer, sizeof( errBuffer ), errCode );
					errorMessage << "Rename param file [" << paramFilename << "] -> [" << finalParamFilename << "] failed with code : " << errCode << " " << errBuffer;
#else
					errorMessage << "Rename param file [" << paramFilename << "] -> [" << finalParamFilename << "] failed with code : " << errCode << " " << strerror( errCode );
#endif
					TRACE( errorMessage.str() );
					throw runtime_error( errorMessage.str() );
				}
			}
#endif // USING_REGULATIONS

			// move output file from temp->destination
			string finalDestinationFilename = Path::GetFilename( destFilename );
			finalDestinationFilename = Path::Combine( m_DestinationPath, finalDestinationFilename );
			finalDestinationFilename = RenameFile( destFilename, finalDestinationFilename );
		}
		catch( const std::exception& ex )
		{
			// if file rename fails, file will be removed from TempDestinationPath by qPIFilePublisher::Rollback() or Abort()
			// if file rename succeede there is no operations to determine further exceptions and needs to remove from final destination path
			// need to remove only already generated .param file
			TRACE( "A [" << typeid( ex ).name() << "] has occured [" << ex.what() << "] while moving files to destination" );
			throw;
		}
		catch( ... )
		{
			TRACE( "An [unknown exception] has occured while moving files to destination" );
			throw;
		}
	}
}

void FilePublisher::ProcessFile( string& messageOutput, string feedback )
{
	XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* document = NULL;

	string destFilename = m_DestFileName;
	try
	{	
		if ( m_TransportHeaders.ContainsKey( MqFilter::MQMESSAGETYPE ) && ( m_TransportHeaders[ MqFilter::MQMESSAGETYPE ] != "DATAGRAM" ) )
		{
			DEBUG( "Message type : " << m_TransportHeaders[ MqFilter::MQMESSAGETYPE ] );
			if( ( m_TransportHeaders[ MqFilter::MQMESSAGETYPE ] == "REPLY" ) && ( m_ReplyDestFileName != m_DestFileName ) )
			{
				// if reply feedback is set in config and feedback exists
				if ( ( m_ReplyFeedback.length() != 0 ) && ( feedback.length() > 0 ) )
				{
					// first part on m_ReplyFeedback( before '?' ) is the regex, the next is the replacement
					string::size_type tempPos = m_ReplyFeedback.find( "?" );
					boost::regex ex( m_ReplyFeedback.substr( 0 , tempPos-1 ) );
					feedback = boost::regex_replace( feedback.substr( 0, 1 ), ex, m_ReplyFeedback.substr( tempPos-1 ), boost::match_default | boost::format_all );

					m_ReplyDestFileName = StringUtil::Replace( m_ReplyDestFileName, "{feedback}", feedback );
				}

				// later it will be reopened with the correct name
				destFilename = m_ReplyDestFileName;
				m_DestFile.close();
				if ( 0 != remove( m_DestFileName.data() ) )
				{
					int errCode = errno;
#ifdef CRT_SECURE
					char errBuffer[ 95 ];
					strerror_s( errBuffer, sizeof( errBuffer ), errno );
					TRACE( "Unable to remove destination file [" << m_DestFileName << "]. Error code [" << errno << "] : " << errBuffer ); 
#else
					TRACE( "Unable to remove destination file [" << m_DestFileName << "]. Error code [" << errno << "] : " << strerror( errno ) ); 
#endif	
				}
			}
		}

		if ( !m_DestFile.is_open() )
		{
			m_DestFile.open( destFilename.data(), ios::binary | ios::out | ios::trunc );
			if ( !m_DestFile.is_open() )
			{
				stringstream errorMessage;
				errorMessage << "Can't open destination file [" << destFilename << "]" << endl;
				TRACE( errorMessage.str() );

				throw AppException( errorMessage.str() );
			}
			DEBUG2( "File : [" << m_DestFile << "] opened " );
		}

#ifdef AIX  

		if ( 0 != chmod( destFilename.data(), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP ) )
		{
			int errCode = errno;
			// this may not be fatal, report it at least
			TRACE( "Chmod u+rw, g+rw on [" << destFilename << "] failed with code : " << errCode << " " << strerror( errCode ) );
		}
#endif
#ifdef LINUX
		//change user rights on output file

		if ( 0 != chmod( destFilename.data(), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH ) )
		{
			int errCode = errno;
			// this may not be fatal, report it at least
			TRACE( "Chmod u+rw, g+rw on [" << destFilename << "] failed with code : " << errCode << " " << strerror( errCode ) );
		}
#endif
		// simulate an error at every 10 messages ( test only )
		//if( COUNTER( TRN_ATTEMPTS ) % 10 == 9 )
		//	throw logic_error( "test error" );

		unsigned long finalMessageLength = messageOutput.length();

		//final transformation
		if ( m_TransformFile.length() > 0 )
		{
			XSLTFilter finalTransform;
			NameValueCollection trpHeaders;

			trpHeaders.Add( XSLTFilter::XSLTUSEEXT, "true" );
			trpHeaders.Add( "XSLTPARAMMSGID", StringUtil::Pad( m_MessageId, "\'","\'" ) );
			trpHeaders.Add( XSLTFilter::XSLTFILE, m_TransformFile );

			document = XmlUtil::DeserializeFromString( messageOutput );
			if( document != NULL )
			{
				WorkItem< ManagedBuffer > outputXslt( new ManagedBuffer() );
				finalTransform.ProcessMessage( document, outputXslt, trpHeaders, true );
				messageOutput = outputXslt.get()->str();
			}

		}
		// extract DI image in separate file
		if ( ( m_BlobLocator.length() > 0 ) && ( m_BlobFilePattern.length() > 0 ) )
			messageOutput = SaveBlobToFile( messageOutput );

		//write message in file
		WriteMessage( messageOutput.c_str(), messageOutput.length() );		
		m_DestFile.close();

		//rename xml messages using inner info
		if ( m_RenameFilePattern.length() > 0 )
		{
			if ( m_RenameFilePattern == "{feedbackid}" )
			{
				StringUtil parseFeedback = StringUtil( feedback );
				parseFeedback.Split( "|" );

				string newFilename;
				if( parseFeedback.MoreTokens() )
				{
					parseFeedback.NextToken();
					newFilename = parseFeedback.NextToken();
				}

				if ( !newFilename.empty() )
				{
					boost::filesystem::path path( destFilename );
					path.remove_filename();
					path = path / newFilename;
					newFilename = path.string();
					newFilename = RenameFile( destFilename, newFilename );
					destFilename = newFilename;
				}
			}
			else
			{
				document = XmlUtil::DeserializeFromString( messageOutput );
				if( document != NULL )
				{
					string newFilename = Serialize( document, m_RenameFilePattern, destFilename );
					if ( rename( destFilename.c_str(), newFilename.c_str() ) != 0 )
					{
						int errCode = errno;
						stringstream errorMessage;

	#ifdef CRT_SECURE
						char errBuffer[ 95 ];
						strerror_s( errBuffer, sizeof( errBuffer ), errCode );
						errorMessage << "Rename [" << destFilename << "] -> [" << newFilename << "] failed with code : " << errCode << " " << errBuffer;
	#else
						errorMessage << "Rename [" << destFilename << "] -> [" << newFilename << "] failed with code : " << errCode << " " << strerror( errCode );
	#endif
						TRACE( errorMessage.str() );
					}
				}
			}
		}

#ifdef USING_REGULATIONS
		//generate .param file
		string paramFilename = ".par";
		if( m_ParamFileXslt.length() > 0 )
		{	
			if ( document == NULL )
				document = XmlUtil::DeserializeFromString( messageOutput );

			string filename = Path::GetFilename( destFilename );
			paramFilename = filename + paramFilename; 

			string paramDestPath = ( m_TempDestinationPath.length() > 0 ) ? m_TempDestinationPath : m_DestinationPath;
			paramFilename = Path::Combine( paramDestPath, paramFilename );

			string paramFileOutput;
#ifdef LAU

			string b64Hash;
			string sha256Hash;
			if ( m_TransformFile.length() > 0 )
			{
				sha256Hash = HMAC::Sha256( outputXslt );
				b64Hash = Base64::encode ( (unsigned char*) sha256Hash.c_str(), 32 );
			}
			else
			{
				sha256Hash = HMAC::Sha256( messageOutput );
				b64Hash = Base64::encode ( (unsigned char*) sha256Hash.c_str(), 32 );
			}
			DEBUG( "Base64 SHA256 is [" << b64Hash << "]" );
			paramFileOutput = Serialize( document, m_ParamFileXslt, paramFilename, finalMessageLength, b64Hash );
#else
			paramFileOutput = Serialize( document, m_ParamFileXslt, paramFilename, finalMessageLength );
#endif
			ofstream paramFileStream;
			paramFileStream.open( paramFilename.c_str(), ios::binary | ios::out | ios::trunc );
			if ( !paramFileStream.is_open() )
			{
				stringstream errorMessage;
				errorMessage << "Can't open .param destination file [" << paramFilename << "]" << endl;
				TRACE( errorMessage.str() );

				throw AppException( errorMessage.str() );
			}
			DEBUG2( "File : [" << paramFileStream << "] opened " );
			paramFileStream.exceptions( ifstream::failbit | ifstream::badbit );
			try
			{
				//prefix needed for LAU 0x1f+0 padded 6bytes length+par signiture.
#ifdef LAU_
				sha256Hash = HMAC::HMAC_Sha256Gen( paramFileOutput, m_LAUCertFile );
				string parHash = Base64::encode ( (unsigned char*) sha256Hash.c_str(), 16 );

				stringstream parPrefix;
				parPrefix << ( unsigned char )0x1f << setfill( '0' ) << setw( 6 ) << ( paramFileOutput.size() + parHash.size() ) << parHash;

				paramFileOutput = parPrefix.str() + paramFileOutput;
#endif
				paramFileStream.write( paramFileOutput.c_str(), paramFileOutput.length());
				paramFileStream.close();
			}
			catch( const iostream::failure& ef )
			{
				remove( paramFilename.c_str() );
				stringstream errorMessage;
				errorMessage << "Can't write coresponding " << paramFilename << ".param file " << ef.what();
				throw runtime_error( errorMessage.str() );
			}	
		}
#endif // USING_REGULATIONS

		MoveFile( destFilename );

		// release buffer
		if( document != NULL )
		{
			document->release();
			document = NULL;
		}
	}
	catch( const std::exception& ex )
	{
		// release buffer
		if( document != NULL )
		{
			document->release();
			document = NULL;
		}

		// format error
		stringstream errorMessage;
		errorMessage << "Can't performe final file processing [" << destFilename <<
			"]. Check inner exception for details. " << ex.what() << endl;

		TRACE( errorMessage.str() );
		TRACE( ex.what() );

		throw AppException( errorMessage.str(), ex );
	}	
	catch( ... )
	{
		// release buffer
		if( document != NULL )
		{
			document->release();
			document = NULL;
		}
		// format error
		stringstream errorMessage;
		errorMessage << "Can't performe final file processing [" << destFilename << "]" << endl;
		TRACE( errorMessage.str() );

		throw AppException( errorMessage.str() );
	}
}

string FilePublisher::RenameFile( const string& oldFilename, const string& newFilename )
{
	string finalFileName = newFilename;
	if ( rename( oldFilename.c_str(), finalFileName.c_str() ) != 0 )
	{
		int errCode = errno;
		stringstream errorMessage;
#ifdef CRT_SECURE
		char errBuffer[ 95 ];
		strerror_s( errBuffer, sizeof( errBuffer ), errCode );
		errorMessage << "Rename out file [" << oldFilename << "] -> [" << newFilename << "] failed with code : " << errCode << " " << errBuffer;
#else
		errorMessage << "Rename out file [" << oldFilename << "] -> [" << newFilename << "] failed with code : " << errCode << " " << strerror( errCode );
#endif
		TRACE( errorMessage.str() );

		if( errCode == EEXIST )
		{
			finalFileName.append( Collaboration::GenerateGuid() );
			DEBUG( "Attempting to rename output file because destination already exists. New name is ["  << finalFileName << "]" );
			if ( rename( oldFilename.c_str(), finalFileName.c_str() ) != 0 )
			{
				errorMessage.str("");
				errorMessage.clear();
				errCode = errno;
#ifdef CRT_SECURE
				strerror_s( errBuffer, sizeof( errBuffer ), errCode );
				errorMessage << "Second attempt to rename [" << oldFilename << "] -> [" << finalFileName << "] failed with code : " << errCode << " " << errBuffer;
#else
				errorMessage << "Second attempt to rename [" << oldFilename << "] -> [" << finalFileName << "] failed with code : " << errCode << " " << strerror( errCode );
#endif
				throw runtime_error( errorMessage.str() );
			}
		}
		else
			throw runtime_error( errorMessage.str() );
	}
	return finalFileName;
}