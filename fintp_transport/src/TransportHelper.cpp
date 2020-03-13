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

#include <ctime>

#include "LogManager.h"
#include "StringUtil.h"
#include "Base64.h"
#include "TransportHelper.h"
#ifdef HAVE_AMQ
#include "ActiveMq/AmqHelper.h"
#endif
#ifdef HAVE_WMQ
#include "WebsphereMq/WMqHelper.h"
#endif
#include "MqLogPublisher.h"
using namespace FinTP;

const string TransportHelper::TMT_STRING = "TMT_STRING";
const string TransportHelper::TMT_RF_HEADER_2 = "TMT_RF_HEADER_2";
const string TransportHelper::TMT_NONE = "";

//implement ReplyOptions
const string TransportReplyOptions::m_ReplyOptionNames[ MAX_REPLY_OPTIONS ] = { "NONE","RO_COD","RO_COA", "RO_NAN", "RO_PAN",
			"RO_COPY_MSG_ID_TO_CORREL_ID", "RO_PASS_CORREL_ID" };

TransportReplyOptions::TransportReplyOptions( const TransportReplyOptions& transportReplyOptions )
{
	m_Options = transportReplyOptions.m_Options;
}

void TransportReplyOptions::Parse( const string& options )
{
	// split the string using '+' as separator
	StringUtil myOptions( options );
	myOptions.Split( "+" );
	string crtToken = myOptions.NextToken();
	if ( crtToken == m_ReplyOptionNames[0] )
	{
		m_Options.push_back( NONE );
		return;
	}
	while( crtToken.length() > 0 )
	{
		// match against known options
		for( int i = 1; i < MAX_REPLY_OPTIONS; i++ )
			if (crtToken == m_ReplyOptionNames[ i ] )
				m_Options.push_back( ( ReplyOption )( i ) );
			else
				DEBUG( "MQ reply option [" << crtToken << "] is not implemented" );

		crtToken = myOptions.NextToken();
	}
}

string TransportReplyOptions::ToString() const
{
		int replySize = m_Options.size();
		stringstream message;
		for( int i = 0; i <= replySize; i++)
			if( i < replySize )
				message << m_ReplyOptionNames[ m_Options[ i ] ] << "+";
			else
				message << m_ReplyOptionNames[ m_Options[ i ] ];

		return message.str();

}

bool TransportReplyOptions::optionsSet() const
{
	return ( m_Options.size() > 0 && m_Options[0] != NONE );
}

TransportHelper::TransportHelper( unsigned int messageIdLength ) : m_QueueName( "" ), m_MessageIdLength( messageIdLength ),
			m_MessageId( messageIdLength, 0 ), m_CorrelationId( messageIdLength, 0 ), m_GroupId( messageIdLength, 0 ), m_GroupSequence( 0 ),
			m_LastInGroup( true ), m_BackupQueueName( "" ), m_SaveBackup( false ), m_ReplyQueue ( "" ),
			m_MessageLength( 0 ),  m_Feedback( 0 ), m_ReplyUsrData( "" ), m_QueueOpenRefCount ( 0 ), m_ApplicationName( "" ),
			m_SSLKeyRepository( "" ), m_SSLCypherSpec( "" ), m_SSLPeerName( "" ), m_MessagePutDate( "" ), m_MessagePutTime( "" ),
			m_UsePassedMessageId( false ), m_UsePassedCorrelId( false ), m_UsePassedGroupId( false ), m_UsePassedAppName ( false ), m_AutoAbandon( -1 )
{}

TransportHelper* TransportHelper::CreateHelper( const TransportHelper::TRANSPORT_HELPER_TYPE& helperType )
{
	DEBUG( "Create MQ factory" );
	switch( helperType )
	{
#ifdef HAVE_WMQ
		case WMQ :
			return new WMqHelper();
#endif
#ifdef HAVE_AMQ
		case AMQ :
			return new AmqHelper();
#endif
	
	/*	
		 case TransportHelperProvider::AQ :
		 return new AqHelperFactory();
		*/ 

		default :
			stringstream errorMessage;
			errorMessage << "Message queue provider not set [None]";
			throw logic_error( errorMessage.str() );
	};
}

void TransportHelper::setMessageId( const string& passedMessageId )
{
	string messageId = Base64::decode( passedMessageId );
	unsigned int messageIdLength = messageId.length();
	if ( messageIdLength >= m_MessageIdLength )
		m_MessageId = messageId.substr( 0, m_MessageIdLength );
	else
		m_MessageId = messageId + string( m_MessageIdLength - messageIdLength, 0 );

	m_UsePassedMessageId = true;
}

void TransportHelper::setCorrelationId( const string& passedCorrelId )
{
	string correlId = Base64::decode( passedCorrelId );
	unsigned int correlIdLength = correlId.length();
	if ( correlIdLength >= m_MessageIdLength )
		m_CorrelationId = correlId.substr( 0, m_MessageIdLength );
	else
		m_CorrelationId = correlId + string( m_MessageIdLength - correlIdLength, 0 );

	m_UsePassedCorrelId = true;
}

void TransportHelper::setGroupId( const string& passedGroupId )
{
	string groupId = Base64::decode( passedGroupId );
	unsigned int groupIdLength = groupId.length();
	if ( groupIdLength >= m_MessageIdLength )
		m_GroupId = groupId.substr( 0, m_MessageIdLength );
	else
		m_GroupId = groupId + string( m_MessageIdLength - groupIdLength, 0 );

	m_UsePassedGroupId = true;
}

time_t TransportHelper::getMessagePutTime()
{
	time_t putTime;
	if ( m_MessagePutTime.length() > 0 && m_MessagePutDate.length() > 0 )
	{
		struct tm structPutTime;
		structPutTime.tm_isdst = -1;
		structPutTime.tm_year = StringUtil::ParseInt( m_MessagePutDate.substr( 0, 4) ) - 1900;
		structPutTime.tm_mon = StringUtil::ParseInt( m_MessagePutDate.substr( 4, 2 ) ) - 1 ;
		structPutTime.tm_mday = StringUtil::ParseInt( m_MessagePutDate.substr( 6, 2 ) );
		structPutTime.tm_hour = StringUtil::ParseInt( m_MessagePutTime.substr( 0, 2 ) );
		structPutTime.tm_min = StringUtil::ParseInt( m_MessagePutTime.substr( 2, 2 ) );
		structPutTime.tm_sec = StringUtil::ParseInt( m_MessagePutTime.substr( 4, 2 ) );
		putTime = mktime( &structPutTime );

		return putTime;
	}

	return putTime = time( NULL );

}

void TransportHelper::setBackupQueue( const string& queueName )
{
	m_BackupQueueName = queueName;
	m_SaveBackup = true;
}

void TransportHelper::setAutoAbandon( const int retries )
{
	m_AutoAbandon = retries;
}

TransportHelper::TRANSPORT_HELPER_TYPE TransportHelper::parseTransportType( const string& transportType )
{
		if( transportType == "WMQ" )
			return TransportHelper::WMQ;
		if( transportType == "AMQ" )
			return TransportHelper::AMQ;
		if( transportType == "AQ" )
			return TransportHelper::AQ;

		stringstream errorMessage;
		errorMessage << "Transport Helper tpe [" << transportType << "] not found.";
		throw invalid_argument( errorMessage.str() );
}


 AbstractLogPublisher* TransportHelper::createMqLogPublisher( const NameValueCollection& propSettings, bool& isDefault )
 {
	if ( propSettings.getCount() > 0 )
	{
		for( unsigned int i=0; i<propSettings.getCount(); i++ )
			if ( propSettings[ i ].first == "Log.PublisherToMQ" )
			{
				AbstractLogPublisher* logPublisher = NULL;
				// check required info
				if ( !propSettings.ContainsKey( "Log.PublisherMQ.Type" ) )
				{
					DEBUG_GLOBAL( "No Mq Publisher type specified" );
					AppException ex( "Missing Log.PublisherMQ.Type for MQ publisher in config file. Default value" );
					throw ex;
				}
				/**
					* TODO Use of new Log.PublisherMQ.URI as connection string by ne new ctor.MQLogPublisher( const string& m_ConnectionString, const string& helperType )
					*/
				if ( !propSettings.ContainsKey( "Log.PublisherMQ.QM" ) || !propSettings.ContainsKey( "Log.PublisherMQ.Q" ) )
				{
					DEBUG_GLOBAL( "No QM or Q" );
					AppException ex( "Missing Log.PublisherMQ.Q(M) for MQ publisher in config file." );
					LogManager::Publish( ex );
					continue;
				}
					
				if ( propSettings.ContainsKey( "Log.PublisherMQ.URI" ) )
					logPublisher = new MQLogPublisher( propSettings[ "Log.PublisherMQ.QM" ], propSettings[ "Log.PublisherMQ.Q" ], propSettings[ "Log.PublisherMQ.Type" ], propSettings[ "Log.PublisherMQ.URI" ] );
				else
					logPublisher = new MQLogPublisher( propSettings[ "Log.PublisherMQ.QM" ], propSettings[ "Log.PublisherMQ.Q" ] );

				if ( propSettings.ContainsKey( "Log.PublisherMQ.EventFilter" ) )
				{
					int eventFilter = StringUtil::ParseInt( propSettings[ "Log.PublisherMQ.EventFilter" ] );
					DEBUG_GLOBAL( "Setting mq publisher event filter to [" << eventFilter << "]" );
					logPublisher->setEventFilter( eventFilter );
				}
				else
				{
					DEBUG_GLOBAL( "Setting mq publisher event filter to default [" << logPublisher->eventFilter() << "]" );
				}

				if ( propSettings[ i ].second == "true" )
				{
					DEBUG_GLOBAL( "Added MQ publisher QM=["  << propSettings[ "Log.PublisherMQ.QM" ] << "] Q=[" << propSettings[ "Log.PublisherMQ.Q" ] << "]" );
					isDefault = false;
					return logPublisher;
				}
				else if ( propSettings[ i ].second == "default" )
				{	
					isDefault = true;
					return logPublisher;
				}
				else
				{
					// not activated and not default... why is it even there ?
					if ( logPublisher != NULL )
					{
						delete logPublisher;
						logPublisher = NULL;
					}
				}
			}
	 }
	isDefault = false;
	return NULL;
 }

void TransportHelper::putSAAmessage( const TransportReplyOptions& replyOptions, const string& replyQueue, ManagedBuffer* buffer, const string& batchId, long messageSequence, bool isLast )
{
	throw runtime_error( "SAA not supported for this helper type." );
}