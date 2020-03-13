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

#ifndef ROUTINGSTRUCTURES_H
#define ROUTINGSTRUCTURES_H

#include "TransportHelper.h"
#include "BatchManager/BatchManager.h"
#include "BatchManager/Storages/BatchMQStorage.h"

#include "RoutingEngineMain.h"
#include "RoutingMessage.h"

class RoutingState
{
	public:
		
		RoutingState(){};
		virtual ~RoutingState(){};
	
		virtual string ProcessMessage( RoutingMessage* message, const int userId, bool bulk, const string& batchType ) = 0;
		string ProcessMessage( RoutingMessage* message, const int userId, bool bulk )
			{  return ProcessMessage( message, userId, bulk, "" ); }
		string ProcessMessage( RoutingMessage* message, const int userId )
			{  return ProcessMessage( message, userId, false, "" ); }
};

class ExportedTestObject RoutingExitpoint : public RoutingState
{
#if defined( TESTDLL_EXPORT ) || defined ( TESTDLL_IMPORT )
	friend class RoutingStructuresTest;
#endif

	private :
	
		string m_Queue, m_QueueManager, m_TransportURI, m_ReplyToQueue;
		TransportHelper::TRANSPORT_HELPER_TYPE m_HelperType;
		string m_BatchConfig;

		TransportReplyOptions m_ReplyOptions;
		int m_MessageOptions;

		long m_ServiceId;
		string m_ServiceName;

		string m_MessageTransform, m_MessageTrailer, m_ValidationSchema, m_ValidationSchemaNamespace;
		string m_Definition;

		int m_MessageSkipHeaderLength;		
		
		TransportHelper* m_TransportHelper;
		XSLTFilter* m_TransformFilter;
		BatchManager<BatchMQStorage>* m_BatchManager;
		
	public :

		RoutingExitpoint( const string& serviceName, long serviceId, const string& exitpointDefinition );
		~RoutingExitpoint();
		
		string ProcessMessage( RoutingMessage* message, const int userId, bool bulk, const string& batchType );
		
		void Commit( const string& batchId );
		void Rollback( const bool isBatch );

		bool isValid() const { return ( m_Queue.size() > 0 ); }
		
		string getServiceName() const { return m_ServiceName; }
		string getQueueName() const { return m_Queue; }
		long getServiceId() const { return m_ServiceId; }

		int getMessageOptions() const { return m_MessageOptions; };

		string getValidationSchema() const { return m_ValidationSchema; }
		string getValidationSchemaNamespace() const { return m_ValidationSchemaNamespace; }

		string getDefinition() const { return m_Definition; }

	private : 

		void parseExitpointDefinition( const string& exitpointDefinition );
};

class RoutingPlan;

class ExportedTestObject RoutingQueue : public RoutingState
{
#if defined( TESTDLL_EXPORT ) || defined ( TESTDLL_IMPORT )
	friend class RoutingStructuresTest;
#endif

	private :
	
		RoutingExitpoint m_Exitpoint;
		
		long m_QueueId;
		
		string m_QueueName;
		bool m_HoldStatus;
		bool m_BulkOperationInProgress;
		
	public:

		RoutingQueue();
		RoutingQueue( const long queueId, const string& queueName, const string& serviceName, const long serviceId, const string& queueEP, const long holdStatus );
		~RoutingQueue(){};
		
		string ProcessMessage( RoutingMessage* message, const int userId, bool bulk, const string& batchType );
		
		// accessors 
		bool getHeld() const { return m_HoldStatus; }
		void setHeld( const bool holdStatus = true ){ m_HoldStatus = holdStatus; }
		
		long getId() const { return m_QueueId; }
		string getName() const { return m_QueueName; }		
		long getServiceId() const { return m_Exitpoint.getServiceId(); }
		int getMessageOptions() const { return m_Exitpoint.getMessageOptions(); };

		const RoutingExitpoint& getExitpoint() const { return m_Exitpoint; }
};

#endif // ROUTINGSTRUCTURES_H
