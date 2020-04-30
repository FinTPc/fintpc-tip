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

#ifndef RESTWATCHER_H
#define RESTWATCHER_H

#include <string>
#include <pthread.h>

#include "../DllMain.h"
#include "../AbstractWatcher.h"
#include "../InstrumentedObject.h"
#include "RESTHelper.h"

using namespace std;

namespace FinTP
{
	class ExportedObject RESTWatcher : public AbstractWatcher, public InstrumentedObject
	{
		public :

			enum WatchOptions
			{
				NoOption = 0,
				ConfirmRequest = 1,
			};
			
			explicit  RESTWatcher( NotificationPool* notificationPool );
			~RESTWatcher();
			
			void setHttpChannel( const string& httpChannel );
			void setResourcePath( const string& resourcePath );
			void setConfirmationPath( const string& confirmationPath );
			void setWatchOptions( WatchOptions options ) { m_WatchOptions = options; }
			void setRequestThrottling( unsigned int throttling ) { m_Throttling = throttling;  }
			
			void setCertifcatePhrase( const string& phrase ) { m_CertificatePhrase = phrase; }
			void setCertificateFileName( const string& certificatePath ) { m_CertificateFile = certificatePath; }
			void setClientApplicationId( const string& clientAppId ) { m_ClientAppId = clientAppId; };

		protected :
				
			void internalScan();

		private :

			unsigned int m_Throttling;
			string m_ClientAppId;
			int m_WatchOptions;
			int m_RetryInterval, m_Retry;
			string m_HttpChannel, m_ResourcePath, m_ConfirmationPath ;
			string m_CertificateFile, m_CertificatePhrase;

			bool m_IPEnabled;
			bool isConfirmationRequired( string& messageType ) const;
	};
}

#endif //MQWATCHER_H
