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

//AppExceptions.h 
//contains AppException class - Holds Application Exception
//def enum EventType
#ifndef APPEXCEPTIONS_H
#define APPEXCEPTIONS_H

#include "DllMainLog.h"
#include "Collections.h"
#include <string>
#include <iostream>

using namespace std;

namespace FinTP
{
	class ExportedLogObject EventType
	{
		public : 
			enum EventTypeEnum { Error = 1, Warning = 2, Info = 4, Management = 8};
			
			// .ctor
			explicit EventType( const EventTypeEnum eventType = EventType::Error ) { m_EventType = eventType; }

			// convert to string
			string ToString() const;
			static string ToString( const EventTypeEnum type );

			static EventTypeEnum Parse( const string& value );
			
			EventTypeEnum getType() const { return m_EventType; }
			
		private : 
			EventTypeEnum m_EventType;	
	};

	class ExportedLogObject ClassType
	{
		public : 
			enum ClassTypeEnum { TransactionFetch = 1, TransactionPublish = 2, NoClass = 9 };
			
			// .ctor
			explicit ClassType( const ClassTypeEnum classType = ClassType::NoClass ) { m_ClassType = classType; }

			// convert to string
			string ToString() const;
			static string ToString( const ClassTypeEnum type );

			static ClassTypeEnum Parse( const string& value );
			
			ClassTypeEnum getType() const { return m_ClassType; }
			
		private : 
			ClassTypeEnum m_ClassType;
	};

	class ExportedLogObject EventSeverity
	{
		public : 
			enum EventSeverityEnum { Fatal, Transient, Info };
			
			// .ctor
			explicit EventSeverity( const EventSeverityEnum severity = EventSeverity::Transient ) { m_Severity = severity; }
			EventSeverity& operator=( const EventSeverityEnum severity ) 
			{ 
				m_Severity = severity;
				return *this;
			}

			// convert to string
			string ToString() const;
			static EventSeverityEnum Parse( const string& value );
			
			EventSeverityEnum getSeverity() const { return m_Severity; }
			
		private : 
			EventSeverityEnum m_Severity;	
	};

	class ExportedLogObject AppException : public std::exception
	{
		protected :

			string m_MachineName;			// name of the machine generating this event
			static string m_CrtMachineName;
			string m_CreatedDateTime;		// date/time when this message was generated
			string m_Pid;					// process id that generates this event

			EventType m_EventType;			// type of event
			ClassType m_ClassType;			// event class
			EventSeverity m_EventSeverity;	// severity of event
			string m_Message;				// message 

			string m_InnerException;
			NameValueCollection* m_AdditionalInfo;	// other context information
			string m_CorrelationId;

		public :
			// .ctors & destructor
			explicit AppException( const string& message, const EventType::EventTypeEnum eventType = EventType::Error, const NameValueCollection* additionalInfo = NULL );		
			AppException( const string& message, const std::exception& innerException, const EventType::EventTypeEnum eventType= EventType::Error, const NameValueCollection* additionalInfo = NULL );
			AppException( const AppException & );
			AppException(); // used by deserializer
			
			virtual ~AppException() throw();	
			
			// methods
			const char *what() const throw() 
			{ 
				try
				{
					return m_Message.c_str(); 
				} catch( ... ){};
				return "unable to throw";
			}
			
			// member accessors		
			EventType getEventType( void ) const { return m_EventType; }	
			void setEventType( const EventType::EventTypeEnum eventType ) { m_EventType = EventType( eventType ); }
			
			ClassType getClassType( void ) const { return m_ClassType; }
			void setClassType( const ClassType::ClassTypeEnum classType ) { m_ClassType = ClassType( classType ); }

			EventSeverity getSeverity( void ) const { return m_EventSeverity; }	
			void setSeverity( const EventSeverity::EventSeverityEnum eventSeverity ) { m_EventSeverity = EventSeverity( eventSeverity ); }

			string getMessage( void ) const { return m_Message; }
			void setMessage( const string& message ) { m_Message = message; }
			
			string getException( void ) const { return m_InnerException; }
			//exception* getExceptionPtr() const { return m_Exception; }
			void setException( const std::exception& innerException );
			void setException( const string& innerException ){ m_InnerException = innerException; }
					
			string getPid() const { return m_Pid; }
			void setPid( const string& value ) { m_Pid = value; }
			
			string getCreatedDateTime() const { return m_CreatedDateTime; }
			void setCreatedDateTime( const string& datetime ){ m_CreatedDateTime = datetime; }
			
			string getCorrelationId() const { return m_CorrelationId; }
			void setCorrelationId( const string& correlid ){ m_CorrelationId = correlid; }
			
			string getMachineName() const { return m_MachineName; }
			void setMachineName( const string& machine ){ m_MachineName = machine; }
			
			const NameValueCollection* getAdditionalInfo( void ) const;
			string getAdditionalInfo( const string& name ) const { return ( *m_AdditionalInfo )[ name ]; }
			void setAdditionalInfo( const NameValueCollection* additionalInfo );
			void setAdditionalInfo( const string& name, const string& value );
			void addAdditionalInfo( const string& name, const string& value );
			
			void InitDefaultValues();
			
			AppException& operator=( const AppException& ex );

			friend ostream& operator << ( ostream& os, const FinTP::AppException& except );

		protected:		
			
			// methods
			// set basic information about this exception.
			void setBasicExceptionInfo( );
			void addBasicExceptionInfo();
	};
	
	ostream& operator << ( ostream& os, const FinTP::AppException& except );
}

#endif // CACHEMANAGER_H
