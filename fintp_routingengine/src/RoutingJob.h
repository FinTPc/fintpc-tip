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

#ifndef ROUTINGJOB_H
#define ROUTINGJOB_H

#include "StringUtil.h"
#include "BatchManager/BatchManager.h"

#include "RoutingEngineMain.h"
#include "RoutingDbOp.h"

#define ROUTINGJOB_MAX_BACKOUT 3

class ExportedTestObject RoutingJob
{
	private:
	
		long m_BackoutCount;
		long m_DeferedQueue;
		
		string m_JobId, m_RoutingPoint, m_Function, m_BatchId, m_BatchType;
		int m_UserId;

		bool m_IsBatch, m_IsParallel;
		BatchManagerBase::BATCH_STATUS m_BatchStatus;

		int m_HasUnhold, m_IsMove, m_IsRoute, m_IsComplete, m_IsReply;
				
		map< std::string, std::string > m_Params;
		vector< string > m_RoutingActions;
		string m_Destination;
		
	public:

		static const string PARAM_ROUTINGPOINT;
		static const string PARAM_ROUTINGKEY;
		static const string PARAM_FUNCTION;
		static const string PARAM_USERID ;
		static const string PARAM_DESTINATION;
		static const string PARAM_FEEDBACK;
		static const string PARAM_GROUPORDER;
		static const string PARAM_GROUPCOUNT;
		static const string PARAM_BATCHID;
		static const string PARAM_BATCHREF;
		static const string PARAM_GROUPAMOUNT;
		static const string PARAM_USERCOMMENT;

		RoutingJob();		
		RoutingJob( const string& table, const string& jobId, const string& function, const int userId );		
		RoutingJob( const string& jobId );

		~RoutingJob();
		
		string getMessageId() const { return m_JobId; }
		string getJobId() const { return m_JobId; }
		
		string getFunction() const { return m_Function; }
		void setFunction( const string& function );

		bool isParallel() const { return m_IsParallel; }
		void setParallel( const bool value = true ) { m_IsParallel = value; }

		int getUserId() const { return m_UserId; }

		bool isBatch() const { return m_IsBatch; }
		string getBatchId() const { return m_BatchId; }
		void setBatchId( const string& batchId, const BatchManagerBase::BATCH_STATUS batchStatus )
		{
			m_BatchId = batchId; 
			m_IsBatch = true;
			m_BatchStatus = batchStatus;
		}

		string getBatchType() const { return m_BatchType; }
		void setBatchType( const string& batchType )
		{
			m_BatchType = batchType;
		}
		
		BatchManagerBase::BATCH_STATUS getBatchStatus() const { return m_BatchStatus; }
		
		string getJobTable() const { return m_RoutingPoint; }
		void setJobTable( const string& table ) { m_RoutingPoint = table; }
		
		long getBackoutCount() const { return m_BackoutCount; }
		
		void ReadNextJob( const string& jobId );
		
		void Commit() const
		{
			try
			{
				RoutingDbOp::CommitJob( m_JobId ); 
			}
			catch( const std::exception& ex )
			{
				TRACE( "Unable to commit job [" << ex.what() << "]" );
			}
			catch( ... )
			{
				TRACE( "Unable to commit job." );
			}
		}

		void Rollback() const
		{
			try
			{
				RoutingDbOp::RollbackJob( m_JobId ); 
			}
			catch( const std::exception& ex )
			{
				TRACE( "Unable to rollback job [" << ex.what() << "]" );
			}
			catch( ... )
			{
				TRACE( "Unable to rollback job." );
			}
		}

		void Abort() const
		{
			try
			{
				RoutingDbOp::AbortJob( m_JobId ); 
			}
			catch( const std::exception& ex )
			{
				TRACE( "Unable to abort job [" << ex.what() << "]" );
			}
			catch( ... )
			{
				TRACE( "Unable to abort job." );
			}
		}

		BatchManagerBase::BATCH_STATUS Batch( const string& batchId, const string& correlId, const string& feedback, const string& xformItem, const string& amountBDP, const string& amountADP )
		{
			BatchManagerBase::BATCH_STATUS batchStatus = BatchManagerBase::BATCH_FAILED;
			try
			{
				m_IsBatch = true;
				m_BatchId = batchId;
				batchStatus = RoutingDbOp::BatchJob( m_JobId, getFunctionParam( PARAM_GROUPORDER ), batchId, correlId, feedback, xformItem, amountBDP, amountADP );

				// HACK - simulate defer
				//m_DeferedQueue = -2;
			}
			catch( const std::exception& ex )
			{
				TRACE( "Unable to batch job [" << ex.what() << "]" );
			}
			catch( ... )
			{
				TRACE( "Unable to batch job." );
			}

			return batchStatus;
		}

		void Defer( const long queueId, const bool dbCommit = true );
		static void Resume( const long queueId ) { RoutingDbOp::ResumeJobs( queueId ); }
		
		// misc
		bool isDefered() const { return ( m_DeferedQueue != 0 ); }
		long getDeferedQueue() const { return m_DeferedQueue; }
		
		bool hasUnhold() const;
		bool isMove();
		bool isRoute();
		bool isComplete();
		bool isReply();
		
		bool hasFunctionParam( const string& paramName );
		string getFunctionParam( const string& paramName );

		void addAction( const string& action )
		{
			m_RoutingActions.push_back( action );
		}

		const vector< string >& getActions() const
		{
			return m_RoutingActions;
		}

		void copyActions( const RoutingJob& job )
		{
			// copy actions from the subjob
			const vector< string >& redirectActions = job.getActions();
			for( unsigned int i=0; i<redirectActions.size(); i++ )
			{
				m_RoutingActions.push_back( redirectActions[ i ] );
			}
			m_Destination = job.getDestination();
		}

		void populateAddInfo( AppException& ex ) const;

		string getDestination() const { return m_Destination; }
		void setDestination( const string& destination ) { m_Destination = destination; }
};

#endif
