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

//AppExceptions.cpp - Application Exception implementation

#include <sstream>
//#include <fstream>
#include <string>
#include <ctime>
//#include <iomanip>

#include <iostream>
#include "AppExceptions.h"
#include "LogManager.h"
#include "Trace.h"
#include "TimeUtil.h"
#include "PlatformDeps.h"
#include "Collaboration.h"
#include "StringUtil.h"

using namespace std;
using namespace FinTP;

/// EventType implementation
string EventType::ToString() const
{
	return ToString( m_EventType );
}

string EventType::ToString( const EventTypeEnum type )
{
	switch( type )
	{
		case EventType::Error :
			return "Error";
			
		case EventType::Warning :
			return "Warning";
			
		case EventType::Info :
			return "Info";
			
		case EventType::Management : 
			return "Management";	
			
		default:
			return "Event type out of range";
	}
}

EventType::EventTypeEnum EventType::Parse( const string& value )
{
	if( value == "Error" )
		return EventType::Error;
		
	if( value == "Warning" )
		return EventType::Warning;
		
	if( value == "Info"	)
		return EventType::Info;
		
	if( value == "Management" )
		return EventType::Management;
		
	throw invalid_argument( "Event type out of range" );
}

///Event class implementation
string ClassType::ToString() const
{
	return ToString( m_ClassType) ;
}

string ClassType::ToString( const ClassTypeEnum classType)
{
	switch ( classType )
	{
	case ClassType::TransactionFetch:
		return "Transaction.Fetch";

	case ClassType::TransactionPublish:
		return "Transaction.Publish";

	case ClassType::NoClass:
		return "no class";

	default:
		return "Class type out of range";
	}
}

ClassType::ClassTypeEnum ClassType::Parse( const string& value )
{
	if ( value == "Transaction.Fetch" )
		return ClassType::TransactionFetch;

	if ( value == "Transaction.Publish" )
		return ClassType::TransactionPublish;

	if ( value == "no class" )
		return ClassType::NoClass;

	throw invalid_argument( "Class type out of range" );
}

/// EventSeverity implementation
string EventSeverity::ToString() const
{
	switch( m_Severity )
	{
		case EventSeverity::Fatal :
			return "Fatal";
			
		case EventSeverity::Transient :
			return "Transient";
			
		case EventSeverity::Info :
			return "Info";
			
		default:
			return " Event severity out of range";
	}
}

EventSeverity::EventSeverityEnum EventSeverity::Parse( const string& value )
{
	if( value == "Fatal" )
		return EventSeverity::Fatal;
		
	if( value == "Transient" )
		return EventSeverity::Transient;
		
	if( value == "Info"	)
		return EventSeverity::Info;
		
	throw invalid_argument( "Event severity out of range" );
}
 
/// AppException implementation

string AppException::m_CrtMachineName = "localhost";

AppException::AppException() : m_EventSeverity( EventSeverity::Transient )
{
	// alloc pointer members
	//m_AppExceptionInfo = new NameValueCollection(); 
	m_AdditionalInfo = new NameValueCollection();
	m_CorrelationId = Collaboration::EmptyGuid();
}

AppException::AppException( const AppException& ex ) : std::exception( ex )
{
	DEBUG2( "Copy .ctor for " << ex.getMessage() );
	
	m_AdditionalInfo = NULL;	
	setAdditionalInfo( ex.getAdditionalInfo() );

	DEBUG2( "Adding basic info" );
	m_CreatedDateTime = ex.getCreatedDateTime();
	m_Pid = ex.getPid();
	m_EventType = ex.getEventType();
	m_ClassType = ex.getClassType();
	m_EventSeverity = ex.getSeverity();
	m_Message = ex.getMessage();
	m_MachineName = ex.getMachineName();
	m_CorrelationId = ex.getCorrelationId();
	
	DEBUG2( "Adding exception" );
	m_InnerException = ex.getException();
		
	DEBUG2( "Copy .ctor done" );
}

//-- Default .ctor - should not be used - no information is not worth anything
AppException::AppException( const string& message, const EventType::EventTypeEnum eventType, const NameValueCollection* additionalInfo )
{
	// alloc pointer members
	//m_AppExceptionInfo = new NameValueCollection();
	m_AdditionalInfo = new NameValueCollection();
	//m_Exception = NULL;
	
	//DEBUG( "CONSTRUCTOR" );	
	setBasicExceptionInfo();
	
	m_EventType = EventType( eventType );
	if ( eventType == EventType::Info )
		m_EventSeverity = EventSeverity::Info;
	else
		m_EventSeverity = EventSeverity::Transient;
	m_Message = message;

}

AppException::AppException( const string& message, const std::exception& innerException, const EventType::EventTypeEnum eventType, const NameValueCollection* additionalInfo )
{
	DEBUG2( "CONSTRUCTOR" );
	
	//Additional info will be created at setAdditionalInfo
	m_AdditionalInfo = NULL; //new NameValueCollection();
	setAdditionalInfo( additionalInfo );			
	
	setBasicExceptionInfo();
	
	m_EventType = EventType( eventType );
	if ( eventType == EventType::Info )
		m_EventSeverity = EventSeverity( EventSeverity::Info );
	else
		m_EventSeverity = EventSeverity( EventSeverity::Transient );
	
	// if a message was supplied, use it as the message, otherwise use the inner exception
	if ( message.length() == 0 )
		m_Message = innerException.what();
	else
		m_Message = message;

	setException( innerException );
}

AppException& AppException::operator=( const AppException& ex )
{
	if ( this == &ex )
		return *this;

	DEBUG2( "Operator = " << ex.getMessage() );
	
	setAdditionalInfo( ex.getAdditionalInfo() );

	DEBUG2( "Adding basic info" );
	m_CreatedDateTime = ex.getCreatedDateTime();
	m_Pid = ex.getPid();
	m_EventType = ex.getEventType();
	m_ClassType = ex.getClassType();
	m_EventSeverity = ex.getSeverity();
	m_Message = ex.getMessage();
	m_MachineName = ex.getMachineName();
	m_CorrelationId = ex.getCorrelationId();
	
	DEBUG2( "Adding exception" );
	m_InnerException = ex.getException();
		
	DEBUG2( "Operator =  done" );
	return *this;
}

/// destructor (virtual)
AppException::~AppException() throw()
{
	DEBUG2( "DESTRUCTOR " )//; << m_AppExceptionInfo << endl;	
	//if ( m_AppExceptionInfo != NULL )
	//	delete m_AppExceptionInfo;
	try
	{
		if ( m_AdditionalInfo != NULL )
		{
			delete m_AdditionalInfo;
			m_AdditionalInfo = NULL;
		}
	} 
	catch( ... )
	{
		try
		{
			TRACE( "An error occured while deleting the addtional info" );
		} catch( ... ){}
	}
	DEBUG2( "DESTRUCTOR done" );	
}

// set m_MachineName, m_CreatedDateTime, m_Pid
void AppException::setBasicExceptionInfo() 
{
	// avoid regetting machine name every time
	if ( m_CrtMachineName == "localhost" )
		m_CrtMachineName = Platform::GetMachineName();
	
	m_MachineName = m_CrtMachineName;
	m_CorrelationId = Collaboration::EmptyGuid();
	m_CreatedDateTime = TimeUtil::Get( "%Y-%m-%d-%H.%M.%S", 19 );
}

void AppException::InitDefaultValues()
{
	setBasicExceptionInfo();
}

/*
NameValueCollection* AppException::getAdditionalInfo( void )
{
	if ( m_AdditionalInfo == NULL )
		m_AdditionalInfo = new NameValueCollection();  
	return m_AdditionalInfo;
}*/

void AppException::addAdditionalInfo( const string& name, const string& value )
{
	m_AdditionalInfo->Add( name, value );
}

void AppException::setAdditionalInfo( const string& name, const string& value )
{
	m_AdditionalInfo->ChangeValue( name, value );
}

const NameValueCollection* AppException::getAdditionalInfo( void ) const
{
	return m_AdditionalInfo;
}

void AppException::setAdditionalInfo( const NameValueCollection* additionalInfo )
{
	if ( m_AdditionalInfo != NULL )
		delete m_AdditionalInfo;
		
	m_AdditionalInfo = new NameValueCollection();
	if( additionalInfo == NULL )
		return;
		
	for( unsigned int i=0; i<additionalInfo->getCount(); i++ )
	{
		m_AdditionalInfo->Add( ( *additionalInfo )[ i ].first, ( *additionalInfo )[ i ].second );
	}
}

void AppException::setException( const std::exception& innerException ) 
{ 
	stringstream errorMessage;

	// if it's an AppException we can do more : serialize inner exception 
	
	if ( typeid( innerException ) == typeid( *this ) )
	{
		DEBUG( "Serialize inner AppException" );
		const AppException* innerAppException = dynamic_cast< const AppException* >( &innerException );
		if ( innerAppException != NULL )
			errorMessage << AbstractLogPublisher::FormatException( *innerAppException );
		else
		{
			errorMessage << typeid( innerException ).name() << " : " << innerException.what();
		}
	}			
	else
	{
		errorMessage << typeid( innerException ).name() << " : " << innerException.what();
	}

	m_InnerException = errorMessage.str();
	DEBUG( "Inner exception saved : " << errorMessage.str() );
}

ostream& FinTP::operator << ( ostream& os, const FinTP::AppException& except )
{
	os << endl << "Application exception" << endl << "--------------------------------------------" << endl;
	os << "\tMachineName [" << except.getMachineName() << "]" << endl;
	os << "\tCorrelationId [" << except.getCorrelationId() << "]" << endl;
	os << "\tCreatedDateTime [" << except.getCreatedDateTime() << "]" << endl;
	os << "\tPid [" << except.getPid() << "]" << endl;
	os << "\tEventType [" << except.getEventType().ToString() << "]" << endl;
	os << "\tEventType [" << except.getClassType().ToString() << "]" << endl;
	os << "\tMessage [" << except.getMessage() << "]" << endl;
	os << "\tInnerException [" << except.getException() << "]" << endl << "--------------------------------------------" << endl;
	
	if ( except.getAdditionalInfo() != NULL )
	{
		os << "Additional info [" << except.getAdditionalInfo()->getCount() << "]" << endl << "--------------------------------------------" << endl;
		
		const vector< DictionaryEntry >& addInfoCollection = except.getAdditionalInfo()->getData();
		vector< DictionaryEntry >::const_iterator addinfodata = addInfoCollection.begin();
		while( addinfodata != addInfoCollection.end() )
		{
			os << "\t[" << addinfodata->first << "] = [" << addinfodata->second << "]" << endl;
			addinfodata++;
		}
	}
	else
		os << "Additional info [NULL]" << endl << "--------------------------------------------" << endl;

	return os;
}
