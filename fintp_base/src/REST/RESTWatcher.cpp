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

#ifdef WIN32
	#define __MSXML_LIBRARY_DEFINED__
	#include <windows.h>
	#define sleep(x) Sleep( (x)*1000 )
#else
	#include <unistd.h>
#endif

#include <string>

#include "Trace.h"
#include "TimeUtil.h"
#include "RESTWatcher.h"
#include "../REST/RESTHelper.h"
#include "AppExceptions.h"

using namespace std;
using namespace FinTP;

RESTWatcher::RESTWatcher( NotificationPool* notificationPool ): InstrumentedObject(), 
	AbstractWatcher( notificationPool ), m_WatchOptions( RESTWatcher::NoOption ), m_Throttling( 5 ), m_IPEnabled( true )
{
	INIT_COUNTER( REQUEST_ATTEMPTS );
}
 
RESTWatcher::~RESTWatcher()
{
	try
	{
		DESTROY_COUNTER( REQUEST_ATTEMPTS );
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while destroying INPUT_Q_DEPTH counter" );
		} catch( ... ){}
	}
}

void RESTWatcher::internalScan()
{
	if ( m_NotificationPool == NULL )
	{
		TRACE( "Message pool not created." )
		throw logic_error( "Message pool not created." );
	}

	IPSHelper restHelper;
	restHelper.setClientId( m_ClientAppId );
	
	bool force = true;
	unsigned long requestCount = 0;

	ManagedBuffer* docMessage = new ManagedBuffer();
		
	while( m_Enabled && m_NotificationPool->IsRunning() )
	{
		bool succeeded = true;
		try
		{
			DEBUG_GLOBAL( TimeUtil::Get( "%d/%m/%Y %H:%M:%S", 19 ) << " - watcher getting new messages from [" << m_HttpChannel << m_ResourcePath << "] ... " );
			
			// TODO remove connect/disconnect from pool
			restHelper.connect( m_HttpChannel, m_CertificateFile, m_CertificatePhrase );
				
			DEBUG_GLOBAL( "Connected to resource host" );
			int result = 0;
			
			while( !result && ( succeeded ) && m_Enabled )
			{
				//TODO: Check message destructor call on test
				vector<unsigned char> message;
				result = restHelper.getOne( m_ResourcePath, message );

				string requestSequence = restHelper.getLastRequestSequence();
				string messageType = restHelper.getLastMessageType();
				unsigned long messageSize = message.size();
				float retrievingTime = restHelper.getLastRetrievingTime();

				//TODO: Rework for generic request status
				string requestFeedback = restHelper.getHeaderField( "X-MONTRAN-RTP-ReqSts" );
				bool IPOKStatus = m_IPEnabled ? ( requestFeedback != "EMPTY" )  : true ;
				
				DEBUG_GLOBAL( "New message retreived [" << requestSequence << "], in [" << retrievingTime << "] seconds ! " << ( m_IPEnabled ? ( "[" + requestFeedback + "]" ) : "" ) );
				 
				//TODO: Content-Type helper property to notify plain sau xml
				if( ( messageSize > 0 ) && IPOKStatus )
				{
					if( !m_ConfirmationPath.empty() && isConfirmationRequired( messageType ) )
					{
						restHelper.postOneConfirmation( m_ConfirmationPath, requestSequence );
						retrievingTime = restHelper.getLastRetrievingTime();
				
						DEBUG_GLOBAL( "Confirmation admitted for [" << requestSequence << "], in [" << retrievingTime << "] seconds !" );
					}

					//string msg = XmlUtil::SerializeToString( XmlUtil::DeserializeFromFile( "test-pacs008.xml" ) );
					//docMessage->copyFrom( msg.c_str(), msg.size() );
					docMessage->copyFrom( &message[0], message.size() );
					WorkItem< NotificationObject > notification( new NotificationObject( requestSequence, docMessage, "", messageSize ) );
				
					// enqueue notification
					DEBUG_GLOBAL( "Waiting on notification pool to allow inserts - thread [" << m_ScanThreadId << "]." );
				
					m_NotificationPool->addPoolItem( requestSequence, notification );

					DEBUG_GLOBAL( "Inserted notification in pool [" << notification.get()->getObjectId() << "]" );
					
				}

				ASSIGN_COUNTER( REQUEST_ATTEMPTS, requestCount++ );

			}
		}
		catch( const WorkPoolShutdown& shutdownError )
		{
			TRACE_GLOBAL( shutdownError.what() );
			if( docMessage != NULL )
			{
				delete docMessage;
				docMessage = NULL;
			}
			break;
		}
		catch( const AppException& ex )
		{
			string exceptionType = typeid( ex ) .name();
			string errorMessage = ex.getMessage();
			
			TRACE_GLOBAL( exceptionType << " encountered when request message : " << errorMessage );
			if( docMessage != NULL )
			{
				delete docMessage;
				docMessage = NULL;
			}
			sleep( m_Throttling );
		}
		catch( const std::exception& ex )
		{
			string exceptionType = typeid( ex ) .name();
			string errorMessage = ex.what();
			
			TRACE_GLOBAL( exceptionType << " encountered when request message: " << errorMessage );
			if( docMessage != NULL )
			{
				delete docMessage;
				docMessage = NULL;
			}
			sleep( m_Throttling );
		}
		catch( ... )
		{
			TRACE_GLOBAL( "Unhandled exception encountered when request resource. " );
			if( docMessage != NULL )
			{
				delete docMessage;
				docMessage = NULL;
			}
			sleep( m_Throttling );
		}
		
		if ( !succeeded )
		{
			TRACE_GLOBAL( "Last request attempt received an error. The next attempt will be made in " << m_Throttling << "seconds" );
			try
			{
				restHelper.disconnect();
			}
			catch( ... ){}
			sleep( m_Throttling );
		}
		else
			ASSIGN_COUNTER( REQUEST_ATTEMPTS, 0 );
		
		DEBUG_GLOBAL( "Wake and try again" );
	}

	if ( docMessage != NULL )
	{
		delete docMessage;
		docMessage = NULL;
	}

	TRACE_SERVICE( "REST watcher terminated." );
}



void RESTWatcher::setHttpChannel( const string& resourcesRoot )
{
	bool shouldReenable = m_Enabled;

	setEnableRaisingEvents( false );
	m_HttpChannel = resourcesRoot;
	setEnableRaisingEvents( shouldReenable );
}

void RESTWatcher::setResourcePath( const string& path )
{
	bool shouldReenable = m_Enabled;

	setEnableRaisingEvents( false );
	m_ResourcePath = path;
	setEnableRaisingEvents( shouldReenable );
}

void RESTWatcher::setConfirmationPath( const string& path )
{
	bool shouldReenable = m_Enabled;

	setEnableRaisingEvents( false );
	m_ConfirmationPath = path;
	setEnableRaisingEvents( shouldReenable );
}

bool RESTWatcher::isConfirmationRequired( string& messageType  ) const
{
	//TODO Extend this using some kind of evaluators
	return ( messageType != "pacs.008" ) ;
}

