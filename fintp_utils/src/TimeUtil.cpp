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

#include "StringUtil.h"
#include "TimeUtil.h"

using namespace FinTP;

const boost::gregorian::date TimeUtil::m_EpochDate(1970, 1, 1);
const boost::posix_time::ptime TimeUtil::m_Epoch(m_EpochDate);

string TimeUtil::Get( const string& timeFormat, const unsigned int bufferSize )
{
	return ToString(boost::posix_time::microsec_clock::local_time(), timeFormat);
}

string TimeUtil::Get( const string& timeFormat, const unsigned int bufferSize, const time_t* acttime )
{
	struct tm timeptr;

#ifdef CRT_SECURE
	int localtimeResult = localtime_s( &timeptr, acttime );
	if ( 0 != localtimeResult )
		cerr << "Unable to aquire local time [" << localtimeResult << "]";
#else
	timeptr = *localtime( acttime );
#endif

	char* formattedTime = new char[ bufferSize + 1 ];

	strftime( formattedTime, bufferSize + 1, timeFormat.c_str(), &timeptr );
	string returnValue( formattedTime, bufferSize );

	if ( formattedTime != NULL )
		delete[] formattedTime;

	return returnValue;
}

boost::posix_time::time_duration TimeUtil::GetTimeSinceEpoch()
{
	return boost::posix_time::microsec_clock::local_time() - m_Epoch;
}

string TimeUtil::ToString( boost::posix_time::ptime time, const string& timeFormat )
{
	stringstream timeStream;
	timeStream.imbue(locale(timeStream.getloc(), new boost::posix_time::time_facet(timeFormat.c_str())));
	timeStream << time;

	return timeStream.str();
}

boost::posix_time::ptime TimeUtil::ToPtime( const boost::posix_time::time_duration sinceEpoch )
{
	return boost::posix_time::ptime(m_EpochDate, sinceEpoch);
}

double TimeUtil::Compare( const string& time1, const string& time2, const string& format )
{
	if( time1.length() != time2.length() )
		return false;

	struct tm timeptr1;
	struct tm timeptr2;

	timeptr1.tm_isdst = 0;
	timeptr2.tm_isdst = 0;

	unsigned int i=0, j=0;
	while( j < time1.length() )
	{
		if( format[i] == '%' )
		{
			if( format[ i+1 ] == 'Y' )
			{
				timeptr1.tm_year = StringUtil::ParseInt( time1.substr( j, 4 ) ) - 1900;
				timeptr2.tm_year = StringUtil::ParseInt( time2.substr( j, 4 ) ) - 1900;
				j+=4;
			}
			else if( format[ i+1 ] == 'm' )
			{
				timeptr1.tm_mon = StringUtil::ParseInt( time1.substr( j, 2 ) ) - 1;
				timeptr2.tm_mon = StringUtil::ParseInt( time2.substr( j, 2 ) ) - 1;
				j+=2;
			}
			else if( format[ i+1 ] == 'd' )
			{
				timeptr1.tm_mday = StringUtil::ParseInt( time1.substr( j, 2 ) );
				timeptr2.tm_mday = StringUtil::ParseInt( time2.substr( j, 2 ) );
				j+=2;
			}
			else if( format[ i+1 ] == 'H' )
			{
				timeptr1.tm_hour = StringUtil::ParseInt( time1.substr( j, 2 ) );
				timeptr2.tm_hour = StringUtil::ParseInt( time2.substr( j, 2 ) );
				j+=2;
			}
			else if( format[ i+1 ] == 'M' )
			{
				timeptr1.tm_min = StringUtil::ParseInt( time1.substr( j, 2 ) );
				timeptr2.tm_min = StringUtil::ParseInt( time2.substr( j, 2 ) );
				j+=2;
			}
			else if( format[ i+1 ] == 'S' )
			{
				timeptr1.tm_sec = StringUtil::ParseInt( time1.substr( j, 2 ) );
				timeptr2.tm_sec = StringUtil::ParseInt( time2.substr( j, 2 ) );
				j+=2;
			}
			i+=2;
		}
		else
		{
			i++;
			j++;
		}
	}

	time_t time1t = mktime( &timeptr1 );
	time_t time2t = mktime( &timeptr2 );

	return difftime( time2t, time1t );
}

