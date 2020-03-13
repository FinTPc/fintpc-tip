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

#include "MqLogPublisher.h"
#include "LogPublisher.h"
#include "AppExceptions.h"
#ifdef HAVE_WMQ
#include "WebsphereMq/WMqHelper.h"
#endif

namespace FinTP {

//MQLogPublisher implementation
MQLogPublisher::MQLogPublisher( const string& queueManager, const string& queue, const string& helperType, const string& transportURI ) :
	AbstractLogPublisher(), m_QueueManager( queueManager ), m_Queue( queue ), m_TransportURI( transportURI ),
	m_Helper( NULL ), m_QueueOpen( false )
{
	try
	{
		m_Helper = TransportHelper::CreateHelper( TransportHelper::parseTransportType( helperType ) );
#ifdef HAVE_WMQ
		if ( TransportHelper::parseTransportType( helperType ) == TransportHelper::WMQ )
			static_cast<WMqHelper*>(m_Helper)->setConnectOptions( MQCNO_HANDLE_SHARE_BLOCK );
#endif
		
		// don't connect in this thread... will connect in the publisher's thread
		//m_Helper->connect( m_QueueManager, m_TransportURI );
	}
	catch( const std::exception& ex )
	{
		if ( m_Helper != NULL ) 
		{
			delete m_Helper;
			m_Helper = NULL;
		}
		AppException aex( "Unable to create MQ log publisher", ex );
		aex.addAdditionalInfo( "Queue manager", m_QueueManager );
		throw aex;
	}
	catch( ... )
	{
		if ( m_Helper != NULL )
		{
			delete m_Helper;
			m_Helper = NULL;
		}
		AppException aex( "Unable to publish to MQ" );
		aex.addAdditionalInfo( "Queue manager", m_QueueManager );
		throw aex;
	}
}

MQLogPublisher::~MQLogPublisher()
{
	if( m_Helper != NULL )
	{
		//DEBUG( "DESTRUCTOR" );
		try
		{
			m_Helper->closeQueue();
			m_Helper->disconnect();
		}
		catch( ... )
		{
			try
			{
				TRACE( "An error occured when disconnecting from MQ" );
			}catch( ... ){}
		}

		try
		{
			delete m_Helper;
			m_Helper = NULL;
		}
		catch( ... )
		{
			try
			{
				TRACE( "An error occured when deleting the MQ helper" );
			}catch( ... ){}
		}
	}
}

void MQLogPublisher::Publish( const AppException& except )
{
//	DEBUG( "Publish to MQ BEGIN" );

	string formattedException;
	
	try
	{
		formattedException = FormatException( except );
	}
	catch( const std::exception& ex )
	{
		TRACE( "Unable to format exception. " << ex.what() );
		if ( !m_Default )
		{
			try
			{
				string strExcept = SerializeToXmlStr( except );
				TRACE( "Exception serialized to string [" << strExcept << "]" );
			}
			catch( ... ){}
			throw AppException( "Unable to format exception", ex );
		}
		else
			return;
	}
	catch( ... )
	{
		TRACE( "Unable to format exception. [unknown error]" );
		if ( !m_Default )
		{
			try
			{
				string strExcept = SerializeToXmlStr( except );
				TRACE( "Exception serialized to string [" << strExcept << "]" );
			}
			catch( ... ){}
			throw AppException( "Unable to format exception. [unknown error]" );
		}
		else
			return;
	}
	
	try
	{
		//open QM, Q 
		// check if QM and Q exist
		//........
		if ( !m_QueueOpen )
		{
			m_Helper->connect( m_QueueManager, m_TransportURI ); 
			m_Helper->openQueue( m_Queue );
			m_QueueOpen = true;
		}

		//append xml object to QM	
		m_Helper->putOne( ( unsigned char* )formattedException.data(), formattedException.size(), false );
	}
	catch( const std::exception& ex )
	{
		m_QueueOpen = false;
		
		if ( !m_Default )
		{
			AppException aex( "Unable to publish to MQ", ex );
			aex.addAdditionalInfo( "Queue manager", m_QueueManager );
			aex.addAdditionalInfo( "Queue", m_Queue );
			
			throw aex;
		}
	}
	catch( ... )
	{
		m_QueueOpen = false;
		
		if ( !m_Default )
		{
			// can't open QM, Q.. .can't do anything
			AppException aex( "Unable to publish to MQ" );
			aex.addAdditionalInfo( "Queue manager", m_QueueManager );
			aex.addAdditionalInfo( "Queue", m_Queue );
			
			throw aex;
		}
	}

//	DEBUG( "Publish to MQ END" );
}
}
