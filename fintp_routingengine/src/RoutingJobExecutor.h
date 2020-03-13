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

#ifndef ROUTINGJOBEXECUTOR_H
#define ROUTINGJOBEXECUTOR_H

#include "RoutingEngineMain.h"
#include "RoutingJob.h"

class RoutingJobExecutor
{
	public:
	
		RoutingJobExecutor();
		~RoutingJobExecutor();

		// thread mock-up
		static void* JobExecutor( void *data );
		
		// signal waiting thread to either continue and execute, or 
		void Signal( bool shouldContinue );
		void Lock();
		string GetSignature() const { return m_Signature; }

		// accessors
		string getJobId() const { return m_RoutingJob.getJobId(); }
		pthread_t getThreadId() { return m_ExecThreadId; }

		bool Startable() const { return m_JobRead; }

	private :
		
		void Execute();
		void CheckExecute();

		RoutingJob m_RoutingJob;
		string m_Signature;
		bool m_ShouldContinue;
		bool m_ReadyToExecute;
		bool m_JobRead;
		pthread_t m_ExecThreadId;
		pthread_cond_t m_ExecuteCondition;
		pthread_mutex_t m_ExecuteMutex;
};

#endif // ROUTINGJOBEXECUTOR_H
