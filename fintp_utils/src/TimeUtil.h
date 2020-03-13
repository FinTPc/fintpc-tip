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

#ifndef TIMEUTIL_H
#define TIMEUTIL_H

#include <sys/timeb.h>
#include <ctime>
#include <iostream>
#include <string>

#include "DllMainUtils.h"

#include <boost/date_time/posix_time/posix_time.hpp>


namespace FinTP
{
	//Class to convert time to string and to compute difference between two moments
	class ExportedUtilsObject TimeUtil
	{
		public :
			class TimeMarker
			{
				public :
					
					inline TimeMarker()
					{
						ftime( &m_Marker );
					}
					explicit inline TimeMarker( const struct timeb& timeMarker ) : m_Marker( timeMarker ) {}

					//return difference in miliseconds between parameter and member
					inline double operator-( const TimeUtil::TimeMarker& startMarker )
					{
						return difftime( m_Marker.time, startMarker.m_Marker.time )*1000 + m_Marker.millitm - startMarker.m_Marker.millitm;
					}

				private :
		
					struct timeb m_Marker;
			};
			
		private :
			TimeUtil(){};


			static const boost::gregorian::date m_EpochDate;
			static const boost::posix_time::ptime m_Epoch;

		public :

			//Convert current time to string in "timeFormat" form
			static string Get( const string& timeFormat, const unsigned int bufferSize );
			//Convert "acttime" to string in "timeFormat" form
			static string Get( const string& timeFormat, const unsigned int bufferSize, const time_t* acttime );

			//Convert from boost::posix_time::time_duration since epoch to boost::posix_time::ptime
			static boost::posix_time::ptime ToPtime( const boost::posix_time::time_duration sinceEpoch );
			static boost::posix_time::time_duration GetTimeSinceEpoch();
			static string ToString( const boost::posix_time::ptime time, const string& timeFormat );

			// compares diference beetween time2 and time1 with interval , all in string format
			static double Compare( const string& time1, const string& time2, const string& format );
	};
}

#endif // TIMEUTIL_H
