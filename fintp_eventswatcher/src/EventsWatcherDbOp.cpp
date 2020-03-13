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

#include "EventsWatcherDbOp.h"
#include "Trace.h"

#include "AppSettings.h"
#include "StringUtil.h"
#include "Collaboration.h"

#include <algorithm>
#include <sstream>

Database* EventsWatcherDbOp::m_DataDatabase = NULL;
Database* EventsWatcherDbOp::m_ConfigDatabase = NULL;

ConnectionString EventsWatcherDbOp::m_ConfigConnectionString;
ConnectionString EventsWatcherDbOp::m_DataConnectionString;

DatabaseProviderFactory* EventsWatcherDbOp::m_DatabaseProvider = NULL;

EventsWatcherDbOp::EventsWatcherDbOp()
{
}

EventsWatcherDbOp::~EventsWatcherDbOp()
{
}

void EventsWatcherDbOp::Terminate()
{
	if ( EventsWatcherDbOp::m_DataDatabase != NULL )
		delete EventsWatcherDbOp::m_DataDatabase;
		
	if ( EventsWatcherDbOp::m_ConfigDatabase != NULL )
		delete EventsWatcherDbOp::m_ConfigDatabase;
		
	if ( EventsWatcherDbOp::m_DatabaseProvider != NULL )
		delete EventsWatcherDbOp::m_DatabaseProvider;
}

void EventsWatcherDbOp::SetConfigDataSection( const NameValueCollection& dataSection )
{
	if ( dataSection.ContainsKey( "provider" ) )
	{
		string provider = dataSection[ "provider" ];

		// create a provider if necessary
		if ( m_DatabaseProvider == NULL )
			m_DatabaseProvider = DatabaseProvider::GetFactory( provider );
	}

	m_DataConnectionString.setDatabaseName( dataSection[ "database" ] );
	m_DataConnectionString.setUserName( dataSection[ "user" ] );
	m_DataConnectionString.setUserPassword( dataSection[ "password" ] );
}				

void EventsWatcherDbOp::SetConfigCfgSection( const NameValueCollection& cfgSection )
{
	if ( cfgSection.ContainsKey( "provider" ) )
	{
		string provider = cfgSection[ "provider" ];

		// create a provider if necessary
		if ( m_DatabaseProvider == NULL )
			m_DatabaseProvider = DatabaseProvider::GetFactory( provider );
	}

	m_ConfigConnectionString.setDatabaseName( cfgSection[ "database" ] );
	m_ConfigConnectionString.setUserName( cfgSection[ "user" ] );
	m_ConfigConnectionString.setUserPassword( cfgSection[ "password" ] );
}

Database* EventsWatcherDbOp::getData()
{
	// create a provider if necessary
	if ( m_DatabaseProvider == NULL )
	{
		throw runtime_error( "Database provider must be set. Check config file" );
	}
	
	// create the db if necessary
	if ( m_DataDatabase == NULL ) 
	{
		m_DataDatabase = m_DatabaseProvider->createDatabase();
	}
	
	// don't reconnect if already connected
	if ( m_DataDatabase->IsConnected() )
	{
		DEBUG_GLOBAL( "Already connected to Data database" );
		return m_DataDatabase;
	}
		
	m_DataDatabase->Connect( m_DataConnectionString );
	return m_DataDatabase;
}

Database* EventsWatcherDbOp::getConfig()
{
	// create a provider if necessary
	if ( m_DatabaseProvider == NULL )
	{
		throw runtime_error( "Database provider must be set. Check config file" );
	}
	
	// create the db if necessary
	if ( m_ConfigDatabase == NULL ) 
	{
		m_ConfigDatabase = m_DatabaseProvider->createDatabase();
	}
	
	// don't reconnect if already connected
	if ( m_ConfigDatabase->IsConnected() )
	{
		DEBUG_GLOBAL( "Already connectd to Config database" );
		return m_ConfigDatabase;
	}

	m_ConfigDatabase->Connect( m_ConfigConnectionString );
	return m_ConfigDatabase;
}

DataSet* EventsWatcherDbOp::ReadServiceStatus()
{
	Database* config = getConfig();

	config->BeginTransaction();

	DEBUG_GLOBAL( "Reading service status for all services" );
	DataSet *retStatus = config->ExecuteQuery( DataCommand::SP, "GetServiceState" );

	config->EndTransaction( TransactionType::ROLLBACK );

	return retStatus;
}

void EventsWatcherDbOp::UpdateServiceVersion( const string& serviceName, const string& name, const string& version, const string& machine, const string& hash )
{
	Database* config = getConfig();
	
	ParametersVector params;
	
	DataParameterBase *serviceIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	serviceIdParam->setDimension( serviceName.size() );
	serviceIdParam->setString( serviceName );
	params.push_back( serviceIdParam );
	
	DataParameterBase *nameParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	nameParam->setDimension( name.size() );
	nameParam->setString( name );
	params.push_back( nameParam );	

	DataParameterBase *versionParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	versionParam->setDimension( version.size() );
	versionParam->setString( version );
	params.push_back( versionParam );	

	DataParameterBase *machineParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	machineParam->setDimension( machine.size() );
	machineParam->setString( machine );
	params.push_back( machineParam );	

	DataParameterBase *hashParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	hashParam->setDimension( hash.size() );
	hashParam->setString( hash );
	params.push_back( hashParam );	
	
	try
	{
		config->BeginTransaction();
		config->ExecuteNonQuery( DataCommand::SP, "UPDATEVERSION", params );
	}
	catch( const std::exception& ex )
	{
		config->EndTransaction( TransactionType::ROLLBACK );
		TRACE( "Update service version failed [" << ex.what() << "] for [" << serviceName << "]" );
		
		throw;
	}
	catch( ... )
	{
		config->EndTransaction( TransactionType::ROLLBACK );
		TRACE( "Update service version failed [unknown reason] for [" << serviceName << "]" );

		throw;
	}

	config->EndTransaction( TransactionType::COMMIT );
}

void EventsWatcherDbOp::UpdateServiceState( const long serviceId, const long newState, const string& sessionId )
{
	Database* config = getConfig();
	
	ParametersVector params;
	
	DataParameterBase *serviceIdParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	serviceIdParam->setLong( serviceId );
	params.push_back( serviceIdParam );
	
	DataParameterBase *newStateParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	newStateParam->setLong( newState );
	params.push_back( newStateParam );

	DataParameterBase *sessionIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	sessionIdParam->setDimension( sessionId.size() );
	sessionIdParam->setString( sessionId );
	params.push_back( sessionIdParam );	
	
	try
	{
		config->BeginTransaction();
		config->ExecuteNonQuery( DataCommand::SP, "UPDATESERVICESTATE", params );
	}
	catch( const std::exception& ex )
	{
		config->EndTransaction( TransactionType::ROLLBACK );
		TRACE( "Update service state failed [" << ex.what() << "] for [" << serviceId << "]" );
		
		throw;
	}
	catch( ... )
	{
		config->EndTransaction( TransactionType::ROLLBACK );
		TRACE( "Update service state failed [unknown reason] for [" << serviceId << "]" );

		throw;
	}

	config->EndTransaction( TransactionType::COMMIT );
}

void EventsWatcherDbOp::InsertEvent( const string& dadbuffer, const string& message )
{
	Database* data = getData();
	
	ParametersVector params;

	DataParameterBase *dadParam = m_DatabaseProvider->createParameter( DataType::LARGE_CHAR_TYPE );
	dadParam->setDimension( dadbuffer.length() );
	dadParam->setString( dadbuffer );
	dadParam->setName( "DAD_buffer" );
	params.push_back( dadParam );

	DataParameterBase *messageParam = m_DatabaseProvider->createParameter( DataType::LARGE_CHAR_TYPE );
	messageParam->setDimension( message.length() );
	messageParam->setName( "XML_buffer" );
	messageParam->setString( message );

	params.push_back( messageParam );

	DataParameterBase *paramReturnCode = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE, DataParameterBase::PARAM_OUT );
	DataParameterBase *paramReturnMessage = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE, DataParameterBase::PARAM_OUT );

	// return code from upload proc
	paramReturnCode->setDimension( 11 );
	params.push_back( paramReturnCode );
		
	// return message from upload proc
	paramReturnMessage->setDimension( 1024 );
	params.push_back( paramReturnMessage );

	data->BeginTransaction();
	try
	{
		data->ExecuteNonQueryCached( DataCommand::SP, "INSERTEVENTSBATCH", params );

		string returnCode = StringUtil::Trim( paramReturnCode->getString() );
		DEBUG( "Insert XML data return code : [" << returnCode << "]" );	 	
		if ( returnCode != "0" )
	 	{
	 		stringstream messageBuffer;
	  		messageBuffer << "Error inserting XML. Message : [" << paramReturnMessage->getString() << "]";
			TRACE( messageBuffer.str() );
			TRACE( "Original_buffer [" << message << "]" );

	 		throw runtime_error( messageBuffer.str() );
	 	}
	}
	catch( const std::exception& ex )
	{
		TRACE( "Insert event failed [" << ex.what() << "]" );
	}
	catch( ... )
	{
		TRACE( "Insert event failed [unknown reason]" );
	}

	data->EndTransaction( TransactionType::COMMIT );
	DEBUG2( "done" );
}

void EventsWatcherDbOp::InsertEvent( const long serviceId, const string& correlationId, const string& sessionId,
	const string& evtype, const string& machine, const string& date, const string& messageBuffer, 
	const string& event_class, const string& additionalInfo, const string& innerException )
{
	Database* data = getData();
	
	ParametersVector params;
	
	string guid = Collaboration::GenerateGuid();
	DEBUG2( "guidParam [" << guid << "]" );
	DataParameterBase *guidParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	guidParam->setDimension( guid.size() );
	guidParam->setString( guid );
	params.push_back( guidParam );
		
	DEBUG2( "serviceIdParam [" << serviceId << "]" );
	DataParameterBase *serviceIdParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	serviceIdParam->setLong( serviceId );
	params.push_back( serviceIdParam );

	DEBUG2( "correl [" << correlationId << "]" );
	DataParameterBase *correlIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	correlIdParam->setDimension( correlationId.size() );
	correlIdParam->setString( correlationId );
	params.push_back( correlIdParam );
		
	DEBUG2( "session [" << sessionId << "]" );
	DataParameterBase *sessionIdParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	sessionIdParam->setDimension( sessionId.size() );
	sessionIdParam->setString( sessionId );
	params.push_back( sessionIdParam );	

	DEBUG2( "type [" << evtype << "]" );
	DataParameterBase *typeParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	typeParam->setDimension( evtype.size() );
	typeParam->setString( evtype );
	params.push_back( typeParam );	
	
	DEBUG2( "machine [" << machine << "]" );
	DataParameterBase *machineParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	machineParam->setDimension( machine.size() );
	machineParam->setString( machine );
	params.push_back( machineParam );
	
	DEBUG2( "eventdate [" << date << "]" );
	DataParameterBase *dateParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	dateParam->setDimension( date.size() );
	dateParam->setString( date );
	params.push_back( dateParam );

	DEBUG2( "message" );
	DataParameterBase *messageParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	string::size_type messageBufferLength = messageBuffer.length();
	if( messageBufferLength > 256 )
	{
		messageParam->setDimension( 256 );
		messageParam->setString( messageBuffer.substr( 0, 256 ) );
	}
	else
	{
		messageParam->setDimension( messageBufferLength );
		messageParam->setString( messageBuffer );
	}
	params.push_back( messageParam );

	DEBUG2( "class [" << event_class << "]" );
	DataParameterBase *classParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	classParam->setDimension( event_class.size() );
	classParam->setString( event_class );
	params.push_back( classParam );
		
	DEBUG2( "addinfo [" << additionalInfo << "]" );
	DataParameterBase *addInfoParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	if( ( additionalInfo.length() == 0 ) && ( messageBufferLength > 256 ) )
	{
		addInfoParam->setDimension( messageBufferLength );
		addInfoParam->setString( messageBuffer );
	}
	else
	{
		if ( additionalInfo.length() > 2999 )
		{
			addInfoParam->setDimension( 2999 );
			addInfoParam->setString( additionalInfo.substr( 0, 2999 ) );
		}
		else
		{
			addInfoParam->setDimension( additionalInfo.length() );
			addInfoParam->setString( additionalInfo );
		}
	}
	params.push_back( addInfoParam );	

	DEBUG2( "innerex [" << innerException << "]" );
	DataParameterBase *innerExParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );

	if( innerException.length() > 3499 )
	{
		innerExParam->setDimension( 3499 );
		innerExParam->setString( innerException.substr( 0, 3499 ) );
	}
	else
	{
		innerExParam->setDimension( innerException.length() );
		innerExParam->setString( innerException );
	}
	params.push_back( innerExParam );	
	
	DataParameterBase *userParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	userParam->setInt( 0 );
	params.push_back( userParam );

	string queueName = "";
	DataParameterBase *queueNameParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	queueNameParam->setDimension( queueName.length() );
	queueNameParam->setString( queueName );
	queueNameParam->setName( "QueueName" );

	params.push_back( queueNameParam );

	data->BeginTransaction();
	try
	{
		data->ExecuteNonQueryCached( DataCommand::SP, "INSERTEVENT", params );
	}
	catch( const std::exception& ex )
	{
		TRACE( "Insert event failed [" << ex.what() << "]" );
	}
	catch( ... )
	{
		TRACE( "Insert event failed [unknown reason]" );
	}

	data->EndTransaction( TransactionType::COMMIT );
	DEBUG_GLOBAL( "done" );
}

void EventsWatcherDbOp::InsertPerformanceInfo( long serviceId, long sessionId, const string& timestamp,
	long minTT, long maxTT, long meanTT, long sequenceNo, long ioIdentifier, long commitedNo )
{
	// executes the SP UPDATEINSERTSERVICE ( IN in_serviceID INTEGER, IN in_sessionID INTEGER,
	//		IN in_timeSt TIMESTAMP, IN in_minTT DOUBLE, IN in_maxTT DOUBLE, IN in_meanTT DOUBLE,
	// 		IN in_sequence INTEGER, IN in_ioIdentifier INTEGER, 
	//		IN in_commited INTEGER )

	Database* config = getConfig();
	
	ParametersVector params;
	
	DataParameterBase *serviceIdParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	serviceIdParam->setLong( serviceId );
	params.push_back( serviceIdParam );
	
	DataParameterBase *sessionIdParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	sessionIdParam->setLong( sessionId );
	params.push_back( sessionIdParam );
	
	DataParameterBase *timestampParam = m_DatabaseProvider->createParameter( DataType::CHAR_TYPE );
	timestampParam->setDimension( timestamp.size() );
	timestampParam->setString( timestamp );
	params.push_back( timestampParam );
	
	DataParameterBase *minTTParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	minTTParam->setLong( minTT );
	params.push_back( minTTParam );
	
	DataParameterBase *maxTTParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	maxTTParam->setLong( maxTT );
	params.push_back( maxTTParam );
	
	DataParameterBase *meanTTParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	meanTTParam->setLong( meanTT );
	params.push_back( meanTTParam );
	
	DataParameterBase *sequenceNoParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	sequenceNoParam->setLong( sequenceNo );
	params.push_back( sequenceNoParam );
	
	DataParameterBase *ioIdentifierParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	ioIdentifierParam->setLong( ioIdentifier );
	params.push_back( ioIdentifierParam );
	
	DataParameterBase *commitedParam = m_DatabaseProvider->createParameter( DataType::LONGINT_TYPE );
	commitedParam->setLong( commitedNo );
	params.push_back( commitedParam );

	config->ExecuteNonQuery( DataCommand::SP, "UPDATEINSERTSERVICE", params );
}
