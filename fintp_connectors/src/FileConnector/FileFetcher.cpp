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
#include <sstream>
//#include <iostream>
#include "PlatformDeps.h"

#include "FileFetcher.h"
#include "../Connector.h"
#include "AppSettings.h"
#include "Trace.h"
#include "StringUtil.h"
#include "Collaboration.h"
#include "MQ/MqFilter.h"

#include "AppExceptions.h"
#include "LogManager.h"

#include "BatchManager/Storages/BatchFlatfileStorage.h"
#include "BatchManager/Storages/BatchXMLfileStorage.h"

#ifdef USING_REGULATIONS
#include "RoutingRegulations.h"
#include "WSRM/SequenceFault.h"
#include "LogManager.h"
#endif // USING_REGULATIONS

#ifdef LAU
#include "SSL/HMAC.h"
#endif

#ifdef WIN32
	#define __MSXML_LIBRARY_DEFINED__
	
	#include <windows.h>
	#include <io.h>

#ifdef CRT_SECURE
	#define access( exp1, exp2 ) _access_s( exp1, exp2 )
#else
	#define access(x,y) _access( x, y )
#endif

	#define sleep(x) Sleep( (x)*1000 )

#else
	#include <unistd.h> // because access function
#endif

#ifdef LINUX
	#include <errno.h>
#endif

using namespace std;

FileFetcher* FileFetcher::m_Me = NULL;
int FileFetcher::m_PrevBatchItem = -1;

//constructor
FileFetcher::FileFetcher() : Endpoint(), m_Watcher( FileFetcher::NotificationCallback ),
	m_CurrentFilename( "" ), m_CurrentFileSize( 0 ), m_WatchPath( "" ), m_SuccessPath( "" ), m_ErrorPath( "" ),
	m_BatchXsltFile( "" ), m_CurrentBatchId( "BATCH01" )
{	
	DEBUG2( "CONSTRUCTOR" );

#ifdef USING_REGULATIONS
	m_RepliesPath = "";
	m_Regulations = "";
	m_ParamFilePattern = "";
	m_ParamFileXslt = "";
	m_ReplyServiceName = "";
	m_RejectLAUKey="";
#endif// USING_REGULATIONS
}
	
//destructor
FileFetcher::~FileFetcher()
{
	try
	{
		DEBUG( "DESTRUCTOR" );
	} catch( ... ){}
}

void FileFetcher::Init()
{
	DEBUG( "INIT" );
	
	m_WatchPath = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::FILESOURCEPATH );
	m_SuccessPath = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::FILEDESTPATH );
	m_ErrorPath = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::FILEERRPATH );
	m_ReconSource = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::RECONSOURCE, "" );
	string watchFilter = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::FILEFILTER );

#ifdef USING_REGULATIONS
	m_RepliesPath = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::RPLDESTTPATH, "" );
	m_Regulations = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::REGULATIONS, "" );
	m_ParamFilePattern = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::PARAMFPATTERN, "{file}.dat" );
	m_ParamFileXslt = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::PARAMFXSLT, "" );
	m_ReplyServiceName = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::RPLSERVICENAME );
	//LAU key for sending reject messages
	m_RejectLAUKey = getGlobalSetting( EndpointConfig::WMQToApp, EndpointConfig::LAUCERTIFICATE, "" );
#endif// USING_REGULATIONS
	
	if( haveGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::BATCHMGRTYPE ) )
	{
		string batchManagerType = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::BATCHMGRTYPE );
		
		try
		{
			if( batchManagerType == "Flatfile" )
			{
				m_BatchManager = BatchManagerBase::CreateBatchManager( BatchManagerBase::Flatfile );
				BatchManager< BatchFlatfileStorage >* batchManager = dynamic_cast< BatchManager< BatchFlatfileStorage >* >( m_BatchManager );
				if( batchManager == NULL )
					throw logic_error( "Bad type : batch manager is not of FlatFile type" );

				string templateName = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::BATCHMGRTMPL, "" );	
				if ( templateName.length() > 0 )
					batchManager->storage().setTemplate( templateName );
				string headerRegex = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::HEADERREGEX, "" );
				batchManager->storage().setHeaderRegex( headerRegex );
			}
			else if( batchManagerType == "XMLfile" )
			{
				m_BatchManager = BatchManagerBase::CreateBatchManager( BatchManagerBase::XMLfile );
				if( haveGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::BATCHMGRXSLT ) )
				{
					m_BatchXsltFile = getGlobalSetting( EndpointConfig::AppToWMQ, EndpointConfig::BATCHMGRXSLT );

					BatchManager< BatchXMLfileStorage >* batchManager = dynamic_cast< BatchManager< BatchXMLfileStorage >* >( m_BatchManager );
					if( batchManager == NULL )
						throw logic_error( "Bad type : batch manager is not of XML type" );

					//Set BatchXsltFile
					batchManager->storage().setXslt( m_BatchXsltFile );
				}
			}
			else
			{
				TRACE( "Batch manager type [" << batchManagerType << "] not implemented. No batch processing will be performed." );
			}
		}	
		catch( const std::exception& ex )
		{
			TRACE( "Batch manager could not be created. [" << ex.what() << "]" );
			if( m_BatchManager != NULL )
				delete m_BatchManager;
			throw;
		}
		catch( ... )
		{
			TRACE( "Batch manager could not be created. [unknown exception]" );
			if( m_BatchManager != NULL )
				delete m_BatchManager;
			throw;
		}
	}
	else
	{
		DEBUG( "No batch manager specified in config file." );
	}
	
	m_Watcher.setPath( m_WatchPath );
	StringUtil sourcePaths = StringUtil( m_WatchPath ); 
	sourcePaths.Split( ";" );
	// set first path from config
	m_WatchPath = sourcePaths.NextToken();
	m_Watcher.setFilter( watchFilter );
	m_Watcher.setWatchOptions( FsWatcher::SkipEmptyFiles );
}

void FileFetcher::internalStart()
{
	m_Me = this;
	DEBUG( "Starting watcher... " );
	m_Watcher.setEnableRaisingEvents( true );

	TRACE_SERVICE( "[" << m_ServiceThreadId << "] starting to process... " );
}

void FileFetcher::internalStop()
{
	DEBUG( "STOP" );
	
	m_Watcher.setEnableRaisingEvents( false );
}

bool FileFetcher::moreMessages() const
{
	DEBUG( "Checking for more messages ..." );
	if( m_BatchManager == NULL )
	{	
		DEBUG( "Batch Manager is NULL .." );
		return false;
	}
	else
	{	
		DEBUG( "Batch Manager is not NULL .." );
		return m_BatchManager->moreMessages();
	}
}

string FileFetcher::Prepare()
{
	DEBUG( "PREPARE" );
	
	//sleep( 5 ); //TODO remove this when everything works ok

	// check delete rights on m_WatchPath ( = SourcePath ) - can't delete source otherwise
	char* strTemp;

	strTemp = ( char* )m_WatchPath.c_str();
	if( ( access( strTemp, 2 ) ) == -1 )
	{	// you don't have rights to access this folder
		stringstream errorMessage;
		errorMessage << "Don't have write permission to the source folder[" << m_WatchPath << "].";
		TRACE( errorMessage.str() );
		TRACE( "Sleeping 30 seconds before next attempt" );
		sleep( 30 );

		throw AppException( errorMessage.str() );		
	}

	// check write access to m_SuccessPath
	strTemp = ( char* )m_SuccessPath.c_str();
	if( ( access( strTemp, 2 ) ) == -1 )
	{	
		stringstream errorMessage;
		errorMessage << "Don't have write permission to the success folder [" << m_SuccessPath << "].";
		TRACE( errorMessage.str() );
		TRACE( "Sleeping 30 seconds before next attempt" );
		sleep( 30 );
		
		throw AppException( errorMessage.str() );		
	}
	
	// write acces to m_ErrorPath is checked in NotificationCallback
	// the endpoint will move the file there first
	
	//TODO  check if enough disk space is available ... 
	
	if( m_BatchManager == NULL )
	{
		// open Source File	in exclusive mode 
		// get filesize ( ios::ate - at the end of file; binary - don't interpret crlf....)

		m_CurrentFile.open( m_CurrentFilename.c_str(), ios::in | ios::binary | ios::ate );
	
		if( !m_CurrentFile.is_open() )
		{
			stringstream errorMessage;
			errorMessage << "Can't open source file [" << m_CurrentFilename << "]." << endl;
			TRACE( errorMessage.str() );
			
			throw AppException( errorMessage.str() );
		}
	
		m_CurrentFileSize =	m_CurrentFile.tellg();
		// rewind  - poz to begin of the file
		m_CurrentFile.seekg( 0, ios::beg );
		
		return m_CurrentFilename;
	}
	else
	{
		stringstream batchIdTrn;
		batchIdTrn << m_CurrentFilename << "#" << m_CrtBatchItem;
		
		DEBUG( "Current batch item id : " << batchIdTrn.str() );
		
		return batchIdTrn.str();
	}
}

void FileFetcher::Process( const string& correlationId )
{
	DEBUG( "PROCESS" );	
		
	//TODO this will change after batch support is in place

	unsigned char* buffer = NULL;
		
	try
	{
		if( m_BatchManager == NULL )
		{
			buffer = new unsigned char[ m_CurrentFileSize ];
		
			m_CurrentFile.read( ( char * )buffer, m_CurrentFileSize );
			//buffer[ m_CurrentFileSize ]=0;
			m_CurrentFile.close();
		}
		else
		{
			if( m_PrevBatchItem == m_CrtBatchItem )
			{
				DEBUG( "Retrying the same batch item." );
			}
			else
			{	
				*m_BatchManager >> m_LastBatchItem;
				m_PrevBatchItem = m_CrtBatchItem;
			}	
			
			m_CurrentFileSize = m_LastBatchItem.getPayload().length();
			buffer = new unsigned char[ m_CurrentFileSize ];
			memcpy( buffer, m_LastBatchItem.getPayload().c_str(), m_CurrentFileSize );
		}
	}
	catch( const std::exception& ex )
	{
		// release buffer
		if( buffer != NULL )
		{
			delete[] buffer;
			buffer = NULL;
		}
			
		// format error
		stringstream errorMessage;
		errorMessage << "Can't read from file [" << m_CurrentFilename <<
			 "]. Check inner exception for details." << endl;
			
		TRACE( errorMessage.str() );
		TRACE( ex.what() );
		
		throw AppException( errorMessage.str(), ex );
		//DEBUG( "Process additional info count : " << aex.getAppExceptionInfo().getCount() );
	}	
	catch( ... )
	{
		// release buffer
		if( buffer != NULL )
		{
			delete[] buffer;
			buffer = NULL;
		}
			
		// format error
		stringstream errorMessage;
		errorMessage << "Can't read from file [" << m_CurrentFilename << "]. Unknown exception";
		
		TRACE( errorMessage.str() );
		throw AppException( errorMessage.str() );
	}
		
	DEBUG( "File is in buffer; size is : " << m_CurrentFileSize );
	
	// delegate work to the chain of filters.
	try
	{
		string serviceName = StringUtil::Pad( getServiceName(), "\'", "\'" );
	#ifdef USING_REGULATIONS
		if( m_TransportHeaders.ContainsKey( "XSLTPARAMREQUESTOR" ) )
		{
			if( StringUtil::Pad( m_ReplyServiceName, "\'", "\'" ) ==  m_TransportHeaders[ "XSLTPARAMREQUESTOR" ])
			{
				serviceName = m_TransportHeaders[ "XSLTPARAMREQUESTOR" ]; 
			}
		}
	#endif
		m_TransportHeaders.Clear();
		
		m_TransportHeaders.Add( "XSLTPARAMBATCHID", StringUtil::Pad( m_CurrentBatchId, "\'", "\'" ) );
		m_TransportHeaders.Add( "XSLTPARAMGUID", StringUtil::Pad( Collaboration::GenerateGuid(), "\'", "\'" ) );
	  	m_TransportHeaders.Add( "XSLTPARAMCORRELID", StringUtil::Pad( correlationId, "\'", "\'" ) );
	  	m_TransportHeaders.Add( "XSLTPARAMSESSIONID", "'0001'" );
		m_TransportHeaders.Add( "XSLTPARAMREQUESTOR", serviceName );
	  	m_TransportHeaders.Add( "XSLTPARAMREQUEST", "'SingleMessage'" );
		m_TransportHeaders.Add( MqFilter::MQAPPNAME, getServiceName() );
	  	if( m_ReconSource.length() != 0 )
	  		m_TransportHeaders.Add( "XSLTPARAMSOURCE", StringUtil::Pad( m_ReconSource, "\'", "\'" ) );

		stringstream formatFeedback;
		string filenameUsed = Path::GetFilename( m_CurrentFilename ).substr( 0, 32 );

		//FTP00 for RE to identify incomming messages
		formatFeedback << "W|" << filenameUsed << "|0";
		m_TransportHeaders.Add( "XSLTPARAMFEEDBACK", StringUtil::Pad( formatFeedback.str(), "\'", "\'" ) );

		// as the message will end up in MQ, no output is needed
		// if ok then it returns (AbstractFilter::Completed = 1), otherwise it returns other values

		ManagedBuffer* inputBuffer = new ManagedBuffer( buffer, ManagedBuffer::Ref, m_CurrentFileSize );
		DEBUG( "Buffer content : " << inputBuffer->str() );

		AbstractFilter::FilterResult result = m_FilterChain->ProcessMessage( AbstractFilter::buffer_type( inputBuffer ), AbstractFilter::buffer_type( NULL ), m_TransportHeaders, true );

		if( result != AbstractFilter::Completed )
			throw result;
	}
	catch( const AbstractFilter::FilterResult& result )
	{
		// release buffer
		if( buffer )
		{
			delete[] buffer;
			buffer = NULL;
		}

		// format error
		stringstream errorMessage;
		errorMessage << "Process message [" << ( char* )result << "] from [" << m_CurrentFilename << "] file";
		
		TRACE( errorMessage.str() );
		throw AppException( errorMessage.str() );		
	}
	catch( ... ) //TODO put specific catches before this
	{
		// release buffer
		if( buffer )
		{
			delete[] buffer;
			buffer = NULL;
		}

		// format error
		stringstream errorMessage;
		errorMessage << "Filter can't process message from file [" << m_CurrentFilename << "]";
		
		TRACE( errorMessage.str() );
		throw;// AppException( errorMessage.str() );
	}
		
	// release buffer
	if( buffer )
	{
		delete[] buffer;
		buffer = NULL;
	}
}

void FileFetcher::Commit()
{
	DEBUG( "COMMIT" );

	// close source
	if( m_BatchManager == NULL )
	{
		if( m_CurrentFile.is_open() )
			m_CurrentFile.close();
	}
	else
	{
		if( !m_BatchManager->moreMessages() )
			m_BatchManager->close( m_CurrentFilename );
		else
		{
			//mesajul e corect ?????
			DEBUG( "Partial commit done ( current batch item commited ) " );
			return;
		}
	}
	// change path to Success path
	string destName = Path::Combine( m_SuccessPath, Path::GetFilename( m_CurrentFilename ) );
	FinalMove( destName );
		
	m_FilterChain->Commit();
	DEBUG( "Source file [" << m_CurrentFilename << "] moved to [" << destName << "]" );
}

void FileFetcher::Abort()
{
	//!!!!!!!!cine face rollback la filtru mq
	DEBUG( "ABORT" );	
	// close source
	if( m_BatchManager == NULL )
	{
		if( m_CurrentFile.is_open() )
			m_CurrentFile.close();
	}
	else
	{
		if( !m_BatchManager->moreMessages() )
			m_BatchManager->close( m_CurrentFilename );
		else
		{
			DEBUG( "Partial abort done ( current batch item aborted )" );
			return;
		}
	}
	
	//the file is already in error path , it was moved in NotoficationCallback 
}

void FileFetcher::Rollback()
{
	DEBUG( "ROLLBACK" );	
	m_FilterChain->Rollback();
			
	// close source
	if( m_BatchManager == NULL )
	{
		if( m_CurrentFile.is_open() )
			m_CurrentFile.close();
	}
	else
	{
		// the op. will not be retried, so close the batch
		m_BatchManager->close( m_CurrentFilename );
		DEBUG( "Partial rollback done ( current batch item rolled back )" );
	}
}

void FileFetcher::NotificationCallback( const AbstractWatcher::NotificationObject *notification )
{
	//m_Me->m_CurrentFilename = Path::Combine( m_Me->m_WatchPath, notification->getObjectId() );
	m_Me->m_CurrentFilename = notification->getObjectId();
	DEBUG( "Notified : [" << m_Me->m_CurrentFilename << "]" );
	
	//test if we have write permission to error folder
	char* strTemp;
	
	strTemp = ( char* )m_Me->m_ErrorPath.c_str();
	if( ( access( strTemp, 2 ) ) == -1 )
	{	
		stringstream errorMessage;
		errorMessage << "Don't have write permission to the error folder [" << m_Me->m_ErrorPath << "].";
		TRACE( errorMessage.str() );
		
		throw AppException( errorMessage.str() );		
	}

	int filenameLength = Path::GetFilename( m_Me->m_CurrentFilename ).length();
	m_Me->m_WatchPath = m_Me->m_CurrentFilename.substr( 0, m_Me->m_CurrentFilename.length() - ( filenameLength + 1 ) );
	strTemp = ( char* )m_Me->m_WatchPath.c_str();
	if( ( access( strTemp, 2 ) ) == -1 )
	{	// you don't have rights to access this folder
		stringstream errorMessage;
		errorMessage << "Don't have write permission to the source folder [" << m_Me->m_WatchPath << "].";
		TRACE( errorMessage.str() );

		throw AppException( errorMessage.str() );		
	}
	
	//move the file in error directory, to see if it's ready to process
	string destName = Path::Combine( m_Me->m_ErrorPath, Path::GetFilename( m_Me->m_CurrentFilename ) );
	
	// rename source ( may fail ! )
	if( rename( m_Me->m_CurrentFilename.data(), destName.data() ) != 0 )
	{
		int errCode = errno;
		stringstream errorMessage;

#ifdef CRT_SECURE
		char errBuffer[ 95 ];
		strerror_s( errBuffer, sizeof( errBuffer ), errCode );
		errorMessage << "Rename [" << m_Me->m_CurrentFilename << "] -> [" << destName << "] failed with code : " << errCode << " " << errBuffer;
#else
		errorMessage << "Rename [" << m_Me->m_CurrentFilename << "] -> [" << destName << "] failed with code : " << errCode << " " << strerror( errCode );
#endif

		TRACE( errorMessage.str() );
	
		sleep( 10 ); //sleep 10 seconds because the file is not ready yet for move
					 // the operation is to be retried in 10 seconds
			
		if( errCode == EEXIST )
		{
			destName.append( Collaboration::GenerateGuid() );
			DEBUG( "Attempting to rename output file because destination already exists. New name is ["  << destName << "]" );
			
			if( rename( m_Me->m_CurrentFilename.data(), destName.data() ) != 0 )
			{
				errCode = errno;
#ifdef CRT_SECURE
				char errBuffer2[ 95 ];
				strerror_s( errBuffer2, sizeof( errBuffer2 ), errCode );
				errorMessage << "Second attempt to rename [" << m_Me->m_CurrentFilename << "] -> [" << destName << "] failed with code : " << errCode << " " << errBuffer2;
#else
				errorMessage << "Second attempt to rename [" << m_Me->m_CurrentFilename << "] -> [" << destName << "] failed with code : " << errCode << " " << strerror( errCode );
#endif
				throw AppException( errorMessage.str() );
			}
		}
		else
			throw AppException( errorMessage.str() );
	}
	
	DEBUG( "Source file [" << m_Me->m_CurrentFilename << "] moved to [" << destName << "]" );
	
	m_Me->m_CurrentFilename = destName;

	m_PrevBatchItem = -1;
	
	//TODO throw only on fatal error. The connector should respawn this thread
	bool succeeded = true;

	try
	{
		string messageText = "";
		if( m_Me->m_BatchManager != NULL )
		{
			int batchManagerType = m_Me->m_BatchManager->getStorageCategory();
			if( batchManagerType == BatchManagerBase::XMLfile )
			{
				messageText = StringUtil::DeserializeFromFile( m_Me->m_CurrentFilename );
			}
		}
#ifdef USING_REGULATIONS
		m_Me->m_TransportHeaders.Clear();
		if( m_Me->m_Regulations.length() > 0 )
		{
			DEBUG( "Starting regulations validation ... " )
			if( messageText.length() == 0 )
			{
				messageText = StringUtil::DeserializeFromFile( m_Me->m_CurrentFilename );
			}	
			Regulations r;
			MessageEvaluator* evaluator = NULL;
			try
			{
				//TODO Move validation and reply thigs elsewhere as all messages will follow the same path
				evaluator = r.Validate( m_Me->m_Regulations, messageText, notification->getObjectId() );
				if( evaluator == NULL )
					throw logic_error( "Validation failed : could not create evaluator" );

				// do the reply thing here
				MessageEvaluator::EvaluatorType evaluatorType = evaluator->getEvaluatorType();
				if( !evaluator->isValid() )
				{
					if( ( evaluatorType == MessageEvaluator::Sepa_CreditTransfer ) )
					{
						string rejectMessage = evaluator->SerializeResponse();
						string hackedRemSNLPath = Path::GetFilename( m_Me->m_CurrentFilename );
						string::size_type start = hackedRemSNLPath.find( ".SNL" );
						if( start != string::npos )
							hackedRemSNLPath = hackedRemSNLPath.substr( 0, start );
						hackedRemSNLPath = hackedRemSNLPath + ".SNLBIS";

						string replyFilename = Path::Combine( m_Me->m_RepliesPath, hackedRemSNLPath );
						StringUtil::SerializeToFile( replyFilename, rejectMessage );
						messageText = rejectMessage;
						if( ( m_Me->m_ParamFilePattern.length() > 0 ) && ( m_Me->m_ParamFileXslt.length() > 0 ) )
						{
#ifdef LAU
							string sha256Hash = HMAC::HMAC_Sha256Gen( rejectMessage, m_Me->m_RejectLAUKey );
							string b64Hash = Base64::encode ( (unsigned char*) sha256Hash.c_str(), 32 );
#endif
							string paramFilename = StringUtil::Replace( m_Me->m_ParamFilePattern, "{file}", hackedRemSNLPath );
							string paramFullFilename = Path::Combine( m_Me->m_RepliesPath, paramFilename );
#ifdef LAU					
							string paramContents = m_Me->Serialize( evaluator->getDocument(), m_Me->m_ParamFileXslt, Path::GetFilename( m_Me->m_CurrentFilename ), rejectMessage.length(), b64Hash );
#else
							string paramContents = m_Me->Serialize( evaluator->getDocument(), m_Me->m_ParamFileXslt, Path::GetFilename( m_Me->m_CurrentFilename ), rejectMessage.length() );
#endif						
							StringUtil::SerializeToFile( paramFullFilename, paramContents );
						}
						m_Me->m_TransportHeaders.Add( "XSLTPARAMREQUESTOR", StringUtil::Pad( m_Me->m_ReplyServiceName, "\'", "\'" ) );
						DEBUG( "Invalid message send to  [ rejected file name is " << replyFilename << " ]" );
					}
					if( ( evaluatorType == MessageEvaluator::Sepa_Return ) || ( evaluatorType == MessageEvaluator::Sepa_Reject ) )
					{
						string sepaTypeMessage = ( evaluatorType == MessageEvaluator::Sepa_Return ? "Sepa_Return" : "Sepa_Reject");
						stringstream eventMessage;
						eventMessage << "Publishing  event for invalid " << sepaTypeMessage << " message [ file name is : " << m_Me->m_CurrentFilename << " ]";
						DEBUG( eventMessage.str() );
						wsrm::SequenceResponse* seqResponse = evaluator->getSequenceResponse();
						XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* doc = NULL;
						AppException aex = AppException( eventMessage.str() );
						aex.setCorrelationId( Collaboration::GenerateGuid() );
						try
						{
							DOMImplementation* impl = DOMImplementationRegistry::getDOMImplementation( unicodeForm( "LS" ) );
							//assign the root element
							doc = impl->createDocument( 0, unicodeForm( "Document" ), 0 );
							if( doc == NULL )
							{
								TRACE( "Publishing  event for invalid message failed : could not create DOM document" );
								throw;
							}
							seqResponse->Serialize( doc->getDocumentElement() );
							//TODO play more nice with fault serialization
							string faults = XmlUtil::SerializeNodeToString( doc->getDocumentElement() );
							aex.addAdditionalInfo( "Faults", faults );
							DEBUG( "Faults for " << m_Me->m_CurrentFilename << " message are: " << faults );
							LogManager::Publish( aex );
						}
						catch( ... )
						{
							string errorMessage = "Unknown error encounter while serializing validation faults for  event";
							TRACE( errorMessage );
							aex.addAdditionalInfo( "Exception", errorMessage );
						}
						if( doc != NULL )
						{
							doc->release();
							doc = NULL;
						}
						if( evaluator != NULL )
						{
							delete evaluator;
							evaluator = NULL;
						}
						// change path to Success path
						string succDestName = Path::Combine( m_Me->m_SuccessPath, Path::GetFilename( m_Me->m_CurrentFilename ) );
						m_Me->FinalMove( succDestName );
						DEBUG( "Source file [" << m_Me->m_CurrentFilename << "] moved to [" << succDestName << "]" );
						return;
					}
					if (( evaluatorType == MessageEvaluator::Sag_StsReport ) || 
						( evaluatorType == MessageEvaluator::Sepa_InvalidMessage ))
					{
						string succDestName = Path::Combine( m_Me->m_SuccessPath, Path::GetFilename( m_Me->m_CurrentFilename ) );
						m_Me->FinalMove( succDestName );
						
						stringstream eventMessage;
						if( evaluatorType == MessageEvaluator::Sag_StsReport )
							eventMessage << "Sag report file invalid. File name is [" << m_Me->m_CurrentFilename << "] with path [" << m_Me->m_SuccessPath << "]";
						else
							eventMessage << "Invalid Sepa message. File name is [" << m_Me->m_CurrentFilename << "] with path [" << m_Me->m_SuccessPath << "]";
					
						DEBUG( eventMessage.str() );
						AppException aex = AppException( eventMessage.str() );
						LogManager::Publish( aex );

						if( evaluator != NULL )
						{
							delete evaluator;
							evaluator = NULL;
						}
						return;
					}
				}
				else//  valid evaluator
				{
					DEBUG( "Extracting batch id for current message ..." );
					if( ( evaluatorType == MessageEvaluator::Sag_StsReport ) )
					{
						string repFilename = evaluator->getCustomXPath( "/x:Report//x:LogicalName/text()", true );
						m_Me->m_CurrentBatchId = repFilename;
						DEBUG( "Message is a FTA SAG Report message. Don't have batch id, using instead filename: [" << repFilename << "]" );
					}
					else
					{
						XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* currentDoc = NULL;
						currentDoc = evaluator->getDocument();
						if( currentDoc == NULL )
						{
							TRACE( "Extracting batch id failed: could not get DOM document" );
							throw;
						}
						const DOMElement* docRoot = currentDoc->getDocumentElement();
						if( docRoot == NULL )
						{
							throw logic_error( "Document is empty" );
						}
						DOMNodeList* headerNodes = docRoot->getElementsByTagName( unicodeForm( "GrpHdr" ) );
						if( ( headerNodes == NULL ) || ( headerNodes->getLength() == 0 ) )
						{
							throw logic_error( "Bad type: [root element should have an GrpHdr descendant]" );
						}
						DOMElement* headerNode = dynamic_cast< DOMElement* >( headerNodes->item( 0 ) );
						if( headerNode == NULL )
							throw logic_error( "Bad type: [GrpHdr should be an element]" );
						DOMNodeList* msgidNodes = headerNode->getElementsByTagName( unicodeForm( "MsgId" ) );
						if ( ( msgidNodes == NULL ) || ( headerNodes->getLength() == 0 ) )
							throw logic_error( "Bad type: [GrpHdr element should have an MsgId element as child]" );
						DOMNode* msgidNode = msgidNodes->item( 0 );
						if( headerNode == NULL )
							throw logic_error( "Bad type: [GrpHdr/MsgId should be an element]" );
						DOMText* reftext = dynamic_cast< DOMText* >( msgidNode->getFirstChild() );
						if( reftext == NULL )
							throw runtime_error( "Missing required [TEXT] element child of [GrpHdr/MsgId]" );

						m_Me->m_CurrentBatchId = localForm( reftext->getData() );
						//m_Me->m_CurrentBatchId = evaluator->getCustomXPath( "//GrpHdr/MsgId/child::text()", true );
					}
				}
			}
			catch( ... )
			{
				if( evaluator != NULL )
				{
					delete evaluator;
					evaluator = NULL;
				}
				throw;
			}
			if( evaluator != NULL )
			{
				delete evaluator;
				evaluator = NULL;
			}
		}
#endif // USING_REGULATIONS	
		if( m_Me->m_BatchManager != NULL )
		{
			DEBUG_GLOBAL( "First batch item ... opening storage" );
			int batchManagerType = m_Me->m_BatchManager->getStorageCategory();
			if( batchManagerType == BatchManagerBase::XMLfile )
			{
				BatchManager< BatchXMLfileStorage >* batchManager = dynamic_cast< BatchManager< BatchXMLfileStorage >* >( m_Me->m_BatchManager );
				if( batchManager == NULL )
					throw logic_error( "Bad type : batch manager is not of XML type" );
				if( messageText.length() == 0 )
				{
					throw runtime_error( "Serialized batchfile is empty" );
				}
				batchManager->storage().setSerializedXml( messageText );
			}
			// open batch ( next iteration it will already be open )
			try
			{
				m_Me->m_BatchManager->open( m_Me->m_CurrentFilename, ios_base::in | ios_base::binary );
			}
			catch ( ... )
			{
				m_Me->m_BatchManager->close( m_Me->m_CurrentFilename );
				throw;
			}
		}
		DEBUG( "Performing message loop ... " );
		m_Me->PerformMessageLoop( m_Me->m_BatchManager != NULL );

		DEBUG( "Message loop finished ok. " );
	}
	catch( const AppException& ex )
	{
		string exceptionType = typeid( ex ).name();
		string errorMessage = ex.getMessage();
		stringstream publishMessage;
		publishMessage << exceptionType << " encountered while processing message : " <<  Path::GetFilename( m_Me->m_CurrentFilename ) << " [Error message is " << errorMessage << "]";
		
		TRACE_GLOBAL( exceptionType << " encountered while processing message : " << errorMessage );
		LogManager::Publish( publishMessage.str() );
		succeeded = false;
	}
	catch( const std::exception& ex )
	{
		string exceptionType = typeid( ex ).name();
		string errorMessage = ex.what();
		stringstream publishMessage;
		publishMessage << exceptionType << " encountered while processing message : " << Path::GetFilename( m_Me->m_CurrentFilename ) <<" [Error message is " << errorMessage << "]";
		
		TRACE( exceptionType << " encountered while processing message : " << errorMessage );
		LogManager::Publish( publishMessage.str() );
		succeeded = false;
	}
	catch( ... )
	{
		stringstream errorMessage;
		errorMessage << "[unknown exception] encountered while processing message: " << Path::GetFilename( m_Me->m_CurrentFilename );
		TRACE_GLOBAL( errorMessage );	
		LogManager::Publish( errorMessage.str() );
		succeeded = false;
	}

	if( !succeeded && ( m_Me->m_Running ) )
	{
		TRACE( "Sleeping 10 seconds before next attempt( previous message failed )" );
		sleep( 10 );
	}
}

string FileFetcher::Serialize( XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument *doc, const string& xsltFilename, const string& paramFilename, const long paramFileSize, const string& HMAC )
{
	unsigned char **outXslt = new ( unsigned char * );
	unsigned char *outputXslt = NULL;
	string result;

	XSLTFilter xsltFilter;
	
	NameValueCollection trHeaders;
	trHeaders.Add( XSLTFilter::XSLTFILE, xsltFilename );
	trHeaders.Add( XSLTFilter::XSLTUSEEXT, "true" );
	trHeaders.Add( "XSLTPARAMFILENAME", StringUtil::Pad( paramFilename, "\'", "\'" ) );
	trHeaders.Add( "XSLTPARAMMSGID", StringUtil::Pad( Collaboration::GenerateGuid(), "\'", "\'" ) );
	trHeaders.Add( "XSLTPARAMFILESIZE", StringUtil::Pad( StringUtil::ToString( paramFileSize ), "\'", "\'" ) );
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
		if( outputXslt != NULL )
		{
			delete[] outputXslt;
			outputXslt = NULL;
		}

		if( outXslt != NULL )
		{
			delete outXslt;
			outXslt = NULL;
		}
			
		throw;
	}

	if( outputXslt != NULL )
	{
		delete[] outputXslt;
		outputXslt = NULL;
	}

	if( outXslt != NULL )
	{
		delete outXslt;
		outXslt = NULL;
	}
	return result;
}
void FileFetcher::FinalMove( string& destName )
{	
	// rename source ( may fail ! )
	if( rename( m_CurrentFilename.data(), destName.data() ) != 0 )
	{
		int errCode = errno;
		stringstream errorMessage;
		
#ifdef CRT_SECURE
		char errBuffer[ 95 ];
		strerror_s( errBuffer, sizeof( errBuffer ), errCode );
		errorMessage << "Rename [" << m_CurrentFilename << "] -> [" << destName << "] failed with code : " << errCode << " " << errBuffer;
#else
		errorMessage << "Rename [" << m_CurrentFilename << "] -> [" << destName << "] failed with code : " << errCode << " " << strerror( errCode );
#endif	

		TRACE( errorMessage.str() );
		
		if( errCode == EEXIST )
		{
			destName.append( Collaboration::GenerateGuid() );
			DEBUG( "Attempting to rename output file because destination already exists. New name is [" << destName << "]" );
			
			if( rename( m_CurrentFilename.data(), destName.data() ) != 0 )
			{
				errCode = errno;
#ifdef CRT_SECURE
				char errBuffer2[ 95 ];
				strerror_s( errBuffer2, sizeof( errBuffer2 ), errCode );
				errorMessage << "Second attempt to rename [" << m_CurrentFilename << "] -> [" << destName << "] failed with code : " << errCode << " " << errBuffer2;
#else
				errorMessage << "Second attempt to rename [" << m_CurrentFilename << "] -> [" << destName << "] failed with code : " << errCode << " " << strerror( errCode );
#endif	
				throw AppException( errorMessage.str() );
			}
		}
		else
			throw AppException( errorMessage.str() );
	}
}

