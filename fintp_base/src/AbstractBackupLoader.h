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

#ifndef ABSTRACTBACKUPLOADER_H
#define ABSTRACTBACKUPLOADER_H

#include "DllMain.h"
#include "WorkItemPool.h"
#include "AbstractWatcher.h"

namespace FinTP
{
	class ExportedObject AbstractBackupLoader
	{
		
		public :

			typedef WorkItemPool< AbstractWatcher::DeepNotificationObject > DeepNotificationPool;
			void setNotificationPool( DeepNotificationPool* notificationPool ){ m_NotificationPool = notificationPool; }

			virtual ~AbstractBackupLoader();		

			void setEnableRaisingEvents( bool val );
			void setNotificationType( const AbstractWatcher::NotificationObject::NotificationType notifType ){	m_NotificationType = notifType;	}
			pthread_t getThreadId() { return m_BackupThreadId; }

			void waitForExit();

		protected : 

			explicit AbstractBackupLoader( DeepNotificationPool* notificationPool ) : m_BackupThreadId( 0 ), m_Enabled( false ), 
				m_NotificationType( AbstractWatcher::NotificationObject::TYPE_XMLDOM ), m_NotificationPool( notificationPool )
				{}

			virtual void internalLoad() = 0;
			
			pthread_t m_BackupThreadId;
			bool m_Enabled;
			AbstractWatcher::NotificationObject::NotificationType m_NotificationType;
			DeepNotificationPool* m_NotificationPool;

		private:

			static void* BackupInNewThread( void *pThis );

	};
}

#endif //ABSTRACTBACKUPLOADER_H
