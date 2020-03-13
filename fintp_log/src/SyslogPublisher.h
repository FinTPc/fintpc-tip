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

#ifndef SYSLOGPUBLISHER_H
#define SYSLOGPUBLISHER_H

#include <boost/asio.hpp>

#include "AbstractLogPublisher.h"

namespace FinTP
{
	/// publishes events to a file
	class ExportedLogObject SyslogPublisher : public AbstractLogPublisher
	{
		private :
				boost::asio::io_service m_Io_service;
				boost::asio::ip::udp::socket m_Socket;
				string m_ProgramName;

				inline int priority( unsigned int severity, unsigned int facility = 1  ) // 1 - user-level messages
				{
					return facility * 8 + severity;
				}

		public :

			const static string CONFIG_NAME;
			const static string CONFIG_HOST;
			const static string CONFIG_PORT;

			explicit SyslogPublisher( const string& host, const string& port = "514", const string& programName = "FinTP" );
			~SyslogPublisher() {};

			// override of base class method
			void Publish( const AppException& except );
			string formatRF3164( const string& theMessage, unsigned int severity );
	};
}

#endif //SYSLOGPUBLISHER_H