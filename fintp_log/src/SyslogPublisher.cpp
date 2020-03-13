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

#include <iomanip>
#include <iostream>
#include <string>

#include "SyslogPublisher.h"
#include "Trace.h"

using namespace FinTP;

const string SyslogPublisher::CONFIG_NAME = "Log.PublisherToSyslog";
const string SyslogPublisher::CONFIG_HOST = "Log.PublisherToSyslog.Host";
const string SyslogPublisher::CONFIG_PORT = "Log.PublisherToSyslog.Port";

string SyslogPublisher::formatRF3164( const string& theMessage, unsigned int severity )
{
#if __STDC_VERSION__ >= 199901L //c99 support is required for %e
	//the day should be left padded with space not zero(%d), Visual Studio 2010 for example doesn't support %e
	string time = TimeUtil::Get("%b %e %H:%M:%S", 15);
#else
	time_t t = time(NULL);
	tm ts;
    tm* time_stamp = boost::date_time::c_time::localtime( &t, &ts );

    static const char months[][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Mar", "Oct", "Nov", "Dec" };

	string month = months[time_stamp->tm_mon];
	int day = time_stamp->tm_mday;
	int hour = time_stamp->tm_hour;
	int minute = time_stamp->tm_min;
	int second = time_stamp->tm_sec;
	ostringstream timestream;
	timestream << month << " " \
	<< setfill(' ') << setw(2) << day << " " \
	<< setfill('0') << setw(2) << hour << ":" \
	<< setfill('0') << setw(2) << minute << ":" \
	<< setfill('0') << setw(2) << second << " ";
	string time = timestream.str();
#endif

	ostringstream syslogMessage;
	//PRI part
	syslogMessage << "<" << priority( severity ) << ">" \
	//The HEADER contains two fields called the TIMESTAMP and the HOSTNAME.
	//The TIMESTAMP field is the local time and is in the format of "Mmm dd hh:mm:ss"
	<< time << " " \
	//HOSTNAME
	<< boost::asio::ip::host_name() << " "	\
	//The MSG part has two fields known as the TAG field and the CONTENT field.
	//The value in the TAG field will be the name of the program or process that generated the message.
	<< m_ProgramName << " "
	//The CONTENT contains the details of the message.
	<< theMessage;

	return syslogMessage.str();
}

SyslogPublisher::SyslogPublisher( const string& host, const string& port, const string& programName ):m_Socket( m_Io_service ), m_ProgramName( programName )
{
	boost::asio::ip::udp::resolver resolver( m_Io_service );
	boost::asio::ip::udp::resolver::query query( host, port );
	boost::asio::ip::udp::resolver::iterator iterator = resolver.resolve( query );

	boost::asio::connect( m_Socket, iterator );
}

void SyslogPublisher::Publish( const AppException& except )
{
	string formattedException;
	try
	{
		formattedException = FormatException( except );

		string syslogMessage;
		EventType::EventTypeEnum eventType = except.getEventType().getType();
		switch ( eventType )
		{
			case EventType::Error:
				syslogMessage = formatRF3164( formattedException, 3 ); //	LOG_ERR
				break;
			case EventType::Warning:
				syslogMessage = formatRF3164( formattedException, 4 ); //	LOG_WARNING
				break;
			case EventType::Info:
				syslogMessage = formatRF3164( formattedException, 6 ); //	LOG_INFO
				break;
			case EventType::Management:
				syslogMessage = formatRF3164( formattedException, 7 ); //	LOG_DEBUG
				break;
		}

		m_Socket.send( boost::asio::buffer( syslogMessage.c_str(), syslogMessage.size() ) );
	}
	catch( const std::exception& ex )
	{
		TRACE( "Unable to format exception. " << except.what() );
		if ( !m_Default )
			throw AppException( "Unable to format exception", ex );
		else
			return;
	}
	catch( ... )
	{
		TRACE( "Unable to format exception. [unknown error]" );
		if ( !m_Default )
			throw AppException( "Unable to format exception. [unknown error]" );
		else
			return;
	}
}
