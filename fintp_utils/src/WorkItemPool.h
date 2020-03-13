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

#ifndef WORKITEMPOOL_H
#define WORKITEMPOOL_H

#ifdef LINUX
//TODO check ETIMEDOUT
#include </usr/include/errno.h>
#endif

//allow 250 MB messages
#define MAX_MESSAGE_LEN (250 * 1024 * 1024 + 1) 

#include "DllMainUtils.h"

#include <typeinfo>
#include <pthread.h>
#include <deque>
#include <exception>
#include <stdexcept>
#include <map>

#include "Log.h"

using namespace std;

#include "ThreadingUtils.h"

// scenario :
// class MessagePoolHolder
//{
//		public :
//			static WorkItemPool< RoutingMessage > MessagePool( 2 );
// };
//
// thread 1 ( writer ) :
// MessagePoolHolder::MessagePool.reservePoolSize( 10 );
// loop
// {
//		RoutingMessage *theMessage = NULL;
//		// do work... get theMessage
// 
//		WorkItem< RoutingMessage > item( theMessage );
//		m_MessagePool.addPoolItem( item ); -> waits until the pool is unlocked ( writer barrier ), adds the item
//
//		// don't destroy the message. it will be destroyed by the WorkItem
// }
// 
// thread 2 ( reader ) :
// loop
// {
//		WorkItem< RoutingMessage > item = MessagePoolHolder::MessagePool.getPoolItem()
// }

namespace FinTP
{
	template< class T >
	class WorkItem
	{
		class CounterTypeWrapper
		{
			private :
				UIntType::base_type m_Value;
				pthread_mutex_t m_Mutex;
						
			public :
				
				~CounterTypeWrapper()
				{
					int mutexDestroyResult = pthread_mutex_destroy( &m_Mutex );
					if ( 0 != mutexDestroyResult )
					{
						int errCode = errno;
						stringstream errorMessage;
#ifdef CRT_SECURE
						char errBuffer[ 95 ];
						strerror_s( errBuffer, sizeof( errBuffer ), errCode );
 						TRACE_LOG( "Unable to destroy mutex WorkItem::CounterTypeWrapper::m_Mutex [" << mutexDestroyResult << "] Error code : " << errCode << " [" << errBuffer << "]" );
#else
						TRACE_LOG( "Unable to destroy mutex WorkItem::CounterTypeWrapper::m_Mutex [" << mutexDestroyResult << "] Error code : " << errCode << " [" << strerror( errCode ) << "]" );
#endif	
					}
				}

				explicit CounterTypeWrapper( UIntType::base_type value ) : m_Value( const_cast< UIntType::base_type& >( value ) ) 
				{
					int mutexInitResult = pthread_mutex_init( &m_Mutex, NULL );
					if ( 0 != mutexInitResult )
					{
						TRACE_LOG( "Unable to init mutex WorkItem::CounterTypeWrapper::m_Mutex [" << mutexInitResult << "]" );
					}
				}

				CounterTypeWrapper( const CounterTypeWrapper& source ) : m_Value( source.m_Value ) 
				{
					int mutexInitResult = pthread_mutex_init( &m_Mutex, NULL );
					if ( 0 != mutexInitResult )
					{
						TRACE_LOG( "Unable to init mutex WorkItem::CounterTypeWrapper::m_Mutex [" << mutexInitResult << "]" );
					}
				}

				CounterTypeWrapper& operator=( const CounterTypeWrapper& source ) 
				{
					if ( this == &source )
						return *this;
					m_Value = source.m_Value;
					return *this;
				}

				UIntType::base_type decrement() { return UIntType::decrement( &m_Value ); }
				UIntType::base_type increment() { return UIntType::increment( &m_Value ); }

				UIntType::base_type get() const { return m_Value; }
				void set( const UIntType::base_type value ) { m_Value = value; }

				pthread_mutex_t& getLock() { return m_Mutex; }
					
			private :
			
				CounterTypeWrapper();
		};

		//add volatile
		typedef volatile CounterTypeWrapper CounterType;

		private :
			
			volatile T* m_ItemRef;
			pthread_t m_OwnerThread;
			CounterType * m_RefCount;
			
		public :
			
			inline WorkItem() : m_ItemRef( NULL ), m_RefCount( NULL )
			{
				m_OwnerThread = pthread_self();
			}
			
			explicit inline WorkItem( volatile T* item ) : m_ItemRef( item )
			{
				m_OwnerThread = pthread_self();
				m_RefCount = new CounterTypeWrapper( 1 );
			}

			inline WorkItem( const WorkItem< T >& source ) : m_ItemRef( source.m_ItemRef ), m_OwnerThread( source.m_OwnerThread ), m_RefCount( source.m_RefCount )
			{
				Clone();
			}

			inline ~WorkItem() 
			{ 
				RemoveReference();
			}

			inline const WorkItem< T >& Clone() 
			{
				LockingPtr< CounterTypeWrapper > lpRefCount( *m_RefCount, const_cast< CounterTypeWrapper * >( m_RefCount )->getLock() );
							
				// increment reference count
				lpRefCount->increment();

				return *this;
			}

			inline void RemoveReference() volatile
			{
				// this may happen if delete is called on a WorkItem* ( should't be use like that anyway )
				if ( m_RefCount == NULL )
					return;

				// decrement reference count.. still, the race condition exists
				// delete the referenced item if reference count == 0 ( only one reference, now being deleted )
				bool deletable = false;
				
				{
					LockingPtr< CounterTypeWrapper > lpRefCount( *m_RefCount, const_cast< CounterTypeWrapper * >( m_RefCount )->getLock() );
					deletable = ( lpRefCount->decrement() == 0 );

					if ( deletable ) 
					{
						try
						{
							if ( m_ItemRef != NULL )
								delete m_ItemRef;

	#ifdef NO_CPPUNIT
							DEBUG_LOG( "Unreferenced item destroyed" );
	#endif
						}
						catch( ... )
						{
							TRACE_LOG( "Item already deleted" );
							// already deleted ref
						}
						m_ItemRef = NULL;
					}
				}

				if ( deletable )
				{
					delete m_RefCount;
					m_RefCount = NULL;
				}
			}
			
			inline WorkItem& operator=( const WorkItem< T >& source )
			{
				// if this item is valid, give it up
				if ( m_ItemRef != NULL )
					RemoveReference();
					
				// get pointers and ref count address
				m_ItemRef = source.m_ItemRef;
				m_RefCount = source.m_RefCount;
				m_OwnerThread = source.m_OwnerThread;
				
				Clone();
				return *this;
			}
			
			inline T* get() const 
			{
				// this may occur for an unitialized work item
				if( m_RefCount == NULL )
					throw runtime_error( "Item not created." );
					
				LockingPtr< CounterTypeWrapper > lpRefCount( *m_RefCount, const_cast< CounterTypeWrapper * >( m_RefCount )->getLock() );
				if ( lpRefCount->get() == 0 )
					throw runtime_error( "Item already destroyed." );
					
				return const_cast< T* >( m_ItemRef );
			}

			inline unsigned int getRefCount() volatile
			{
				if ( m_RefCount != NULL )
				{
					LockingPtr< CounterTypeWrapper > lpRefCount( *m_RefCount, const_cast< CounterTypeWrapper * >( m_RefCount )->getLock() );
					return lpRefCount->get();
				}
				throw runtime_error( "Item reference count was destroyed" );
			}

			inline pthread_t getOwnerThread()
			{
				return m_OwnerThread;
			}
	};

	class WorkPoolEmpty : public runtime_error
	{
		public :
			WorkPoolEmpty() : runtime_error( "Work pool empty" ) {};
			~WorkPoolEmpty() throw() {};
	};

	class WorkItemNotFound : public runtime_error
	{
		public :
			explicit WorkItemNotFound( const string& message = "Work item not found" ) : runtime_error( message ) {};
			~WorkItemNotFound() throw() {};
	};

	class WorkPoolShutdown : public runtime_error
	{
		public :
			WorkPoolShutdown() : runtime_error( "Work pool shutting down" ) {};
			~WorkPoolShutdown() throw() {};
	};

	template< class T >
	class WorkItemPool
	{
		private :
		
			typedef pair< string, WorkItem< T > > WorkItemPool_QueuedItemType;
			typedef deque< WorkItemPool_QueuedItemType > WorkItemPool_QueueType;
			typedef map< pthread_t, unsigned int > WorkItemPool_CounterType;

			pthread_key_t ReserveKey;

			static void DeleteReserves( void* data )
			{
				unsigned int* threadReserve = ( unsigned int* )data;
				if ( threadReserve != NULL )
					delete threadReserve;
				
				//pthread_setspecific( ReservesKey, NULL );
			}

			// disable copying
			WorkItemPool( const WorkItemPool& );
			WorkItemPool& operator=( const WorkItemPool& );
			
		public :

			WorkItemPool() : m_Shutdown( false )
			{
				int mutexInitResult = pthread_mutex_init( &PoolSyncMutex, NULL );
				if ( 0 != mutexInitResult )
				{
					TRACE_LOG( "Unable to init mutex WorkItemPool::PoolSyncMutex [" << mutexInitResult << "]" );
				}
				
				mutexInitResult = pthread_mutex_init( &ReserveSyncMutex, NULL );
				if ( 0 != mutexInitResult )
				{
					TRACE_LOG( "Unable to init mutex WorkItemPool::ReserveSyncMutex [" << mutexInitResult << "]" );
				}

				int condInitResult = pthread_cond_init( &PoolReaderBarrier, NULL );
				if ( 0 != condInitResult )
				{
					TRACE_LOG( "Unable to init condition WorkItemPool::PoolReaderBarrier [" << condInitResult << "]" );
				}

				condInitResult = pthread_cond_init( &PoolWriterBarrier, NULL );
				if ( 0 != condInitResult )
				{
					TRACE_LOG( "Unable to init condition WorkItemPool::PoolWriterBarrier [" << condInitResult << "]" );
				}
							
				int keyCreateResult = pthread_key_create( &ReserveKey, &WorkItemPool::DeleteReserves );
				if ( 0 != keyCreateResult )
				{
					TRACE_LOG( "Unable to create thread key WorkItemPool::ReserveKey [" << keyCreateResult << "]" );
				}
				DEBUG_LOG( "Created pool reserve key [" << ReserveKey << "]" );
			}

			~WorkItemPool()
			{
				m_Shutdown = true;

				int mutexDestroyResult = pthread_mutex_destroy( &ReserveSyncMutex );
				if ( 0 != mutexDestroyResult )
				{
					TRACE_LOG( "Unable to destroy mutex WorkItemPool::ReserveSyncMutex [" << mutexDestroyResult << "]" );
				}
				
				mutexDestroyResult = pthread_mutex_destroy( &PoolSyncMutex );
				if ( 0 != mutexDestroyResult )
				{
					TRACE_LOG( "Unable to destroy mutex WorkItemPool::PoolSyncMutex [" << mutexDestroyResult << "]" );
				}

				int condDestroyResult = pthread_cond_destroy( &PoolReaderBarrier );
				if ( 0 != condDestroyResult )
				{
					TRACE_LOG( "Unable to destroy condition WorkItemPool::PoolReaderBarrier [" << condDestroyResult << "]" );
				}
				
				condDestroyResult = pthread_cond_destroy( &PoolWriterBarrier );
				if ( 0 != condDestroyResult )
				{
					TRACE_LOG( "Unable to destroy condition WorkItemPool::PoolWriterBarrier [" << condDestroyResult << "]" );
				}
			}
			
			void reservePoolSize( const unsigned int reservedPoolSize ) volatile
			{
				unsigned int* threadReserve = ( unsigned int* )pthread_getspecific( ReserveKey );
				if ( threadReserve == NULL )
					threadReserve = new unsigned int;
					
				*threadReserve = reservedPoolSize;
				int setSpecificResult = pthread_setspecific( ReserveKey, threadReserve );
				if ( 0 != setSpecificResult )
				{
					TRACE_LOG( "Signal poolreaderbarrier failed [" << setSpecificResult << "]" );
				}
			}
			
			void waitForPoolEmpty( const unsigned int secWait = 0 ) volatile
			{
				LockingPtr< WorkItemPool_QueueType > lpPool( m_Pool, PoolSyncMutex );
				
				for(;;)
				{
					int condWait = 0;
					if ( !lpPool->empty() )
					{
						if ( secWait == 0 )
						{
							condWait = pthread_cond_wait( const_cast< pthread_cond_t* >( &PoolWriterBarrier ), const_cast< pthread_mutex_t* >( &PoolSyncMutex ) );
						}
						else
						{
							struct timespec wakePerf;

							wakePerf.tv_sec = time( NULL ) + secWait;
							wakePerf.tv_nsec = 0;
							condWait = pthread_cond_timedwait( const_cast< pthread_cond_t* >( &PoolWriterBarrier ), const_cast< pthread_mutex_t* >( &PoolSyncMutex ), &wakePerf );
						}
					}
					// if the pool is empty, or a timeout was specified, return
					if ( lpPool->empty() || ( ( secWait > 0 ) && ( condWait == ETIMEDOUT ) ) )
						break;
				}
			}

			void waitToWrite( pthread_t selfId ) volatile
			{
				// default 10 for reserve and 0 for requests pending
				unsigned int countActual = 0;
				bool reserveCreated = false;

				unsigned int* countReserve = ( unsigned int* )pthread_getspecific( ReserveKey );
				if ( countReserve == NULL )
				{
					DEBUG_LOG( "Creating initial pool reserve for thread [" << selfId << "]" );
					reservePoolSize( 10 );
					reserveCreated = true;
					
					countReserve = ( unsigned int* )pthread_getspecific( ReserveKey );
				}
				
				// find how many items we've written / how many we are allowed to write
				LockingPtr< WorkItemPool_CounterType > lpWriterItems( m_WriterItems, ReserveSyncMutex );

				if ( reserveCreated )
				{
					lpWriterItems->insert( pair< pthread_t, unsigned int >( selfId, countActual ) );
				}
				else
				{
					// loop and wait for the thread to be allowed to write
					do
					{
						countActual = ( *lpWriterItems )[ selfId ];
						DEBUG_LOG( "Work items in pool for thread [" << selfId << "] is now [" << countActual << "/" << *countReserve << "]" );

						int condWait = 0;
						if ( countActual >= *countReserve )
							condWait = pthread_cond_wait( const_cast< pthread_cond_t* >( &PoolWriterBarrier ), const_cast< pthread_mutex_t* >( &ReserveSyncMutex ) );

						if ( m_Shutdown )
							throw WorkPoolShutdown();
					}
					while( countActual >= *countReserve );
				}

				// HACK increment here ( assume a write will follow );
				( ( *lpWriterItems )[ selfId ] )++;
			}

			void addPoolItem( const string& id, const WorkItem< T >& item ) volatile
			{
				// check for write access
				pthread_t selfId = pthread_self();							

				// wait until we can write
				waitToWrite( selfId );
				LockingPtr< WorkItemPool_QueueType > lpPool( m_Pool, PoolSyncMutex );

				// create a temp object, store a reference on the queue;
				//lpPool->push_back( WorkItemPool::QueuedItemType( id, const_cast< WorkItem< T >* >( &item )->Clone() ) );
				lpPool->push_back( WorkItemPool_QueuedItemType( id, item ) );

	#ifdef NO_CPPUNIT
				Dump();
	#endif
				// signal readers 
				int condSignalResult = pthread_cond_signal( const_cast< pthread_cond_t* >( &PoolReaderBarrier ) );
				if ( 0 != condSignalResult )
				{
					TRACE_LOG( "Signal PoolReaderBarrier failed [" << condSignalResult << "]" );
				}
			}

			void addUniquePoolItem( const string id, const WorkItem< T >& item ) volatile
			{
				// check for write access
				pthread_t selfId = pthread_self();							

				// lock the pool here to ensure iterator stability
				{
					LockingPtr< WorkItemPool_QueueType > lpPool( m_Pool, PoolSyncMutex );
					for( typename WorkItemPool_QueueType::const_iterator poolItemFinder = lpPool->begin(); poolItemFinder != lpPool->end(); poolItemFinder++ )
					{
						if ( poolItemFinder->first == id )
						{
							DEBUG_LOG( "Item [" << id << "] not added to pool. Unique constraint failed." );
							return;
						}
					}
				}

				// wait until we can write ( if wait returns false, we don't write ( unique violated ) )
				waitToWrite( selfId );

				// reaquire the lock, so that we can safely add messages
				LockingPtr< WorkItemPool_QueueType > lpPool( m_Pool, PoolSyncMutex );

				//lpPool->push_back( WorkItemPool::QueuedItemType( id, const_cast< WorkItem< T >* >( &item )->Clone() ) );
				lpPool->push_back( WorkItemPool_QueuedItemType( id, item ) );

				// signal readers 
				int condSignalResult = pthread_cond_signal( const_cast< pthread_cond_t* >( &PoolReaderBarrier ) );
				if ( 0 != condSignalResult )
				{
					TRACE_LOG( "Signal PoolReaderBarrier failed [" << condSignalResult << "]" );
				}
			}
			
			WorkItem< T > getPoolItem( const string id, const bool throwOnError = true ) volatile
			{
				LockingPtr< WorkItemPool_QueueType > lpPool( m_Pool, PoolSyncMutex );
				
				try
				{
					bool found = false;
					typename WorkItemPool_QueueType::const_iterator poolItemFinder = lpPool->begin();
					while( poolItemFinder != lpPool->end() )
					{
						if ( poolItemFinder->first == id )
						{
							found = true;
							break;
						}
						poolItemFinder++;
					}
					if ( m_Shutdown )
						throw WorkPoolShutdown();
					if ( !found )
						throw WorkItemNotFound( id );

					return poolItemFinder->second;
				}
				catch( ... )
				{
					if ( throwOnError )
						throw;
				}
				return WorkItem< T >();
			}

			WorkItem< T > getPoolItem( const bool lock = true ) volatile
			{
				LockingPtr< WorkItemPool_QueueType > lpPool( m_Pool, PoolSyncMutex );
						
				// while the pool is empty ( the condition variable may be signaled for different reasons )
				while( lpPool->empty() )
				{
					// if we lock on the pool, wait for items
					if ( lock )
					{
						int condWaitResult = pthread_cond_wait( const_cast< pthread_cond_t* >( &PoolReaderBarrier ), const_cast< pthread_mutex_t* >( &PoolSyncMutex ) );
						if ( 0 != condWaitResult )
						{
							TRACE_LOG( "Condition wait on PoolReaderBarrier failed [" << condWaitResult << "]" );
						}
					}
					else
						throw WorkPoolEmpty();

					if ( m_Shutdown )
						throw WorkPoolShutdown();
				}
				return lpPool->front().second;
			}
				
			WorkItem< T > removePoolItem( const bool lock = true ) volatile
			{
				WorkItem< T > item;
				try
				{
					LockingPtr< WorkItemPool_QueueType > lpPool( m_Pool, PoolSyncMutex );

					// while the pool is empty ( the condition variable may be signaled for different reasons )
					while( lpPool->empty() )
					{
						if ( m_Shutdown )
							throw WorkPoolShutdown();

						// if we lock on the pool, wait for items
						if ( lock )
						{
							int condWaitResult = pthread_cond_wait( const_cast< pthread_cond_t* >( &PoolReaderBarrier ), const_cast< pthread_mutex_t* >( &PoolSyncMutex ) );
							if ( 0 != condWaitResult )
							{
								TRACE_LOG( "Condition wait on PoolReaderBarrier failed [" << condWaitResult << "]" );
							}
						}
						else
							throw WorkPoolEmpty();

						if ( m_Shutdown )
							throw WorkPoolShutdown();
					}

	#ifdef NO_CPPUNIT
					Dump();
	#endif

					item = lpPool->front().second;
					lpPool->pop_front();
				}
				catch( const std::exception& ex )
				{
					TRACE_LOG( "A [" << typeid( ex ).name() << "] has occured [" << ex.what() << "] while removing item from pool" );
					throw;
				}
				catch( ... )
				{
					TRACE_LOG( "An [unknown exception] has occured while removing item from pool." );
					throw;
				}

				// signal writers
				{
					LockingPtr< WorkItemPool_CounterType > lpWriterItems( m_WriterItems, ReserveSyncMutex );

					// decrement thread item count
					( ( *lpWriterItems )[ item.getOwnerThread() ] )--;

					// signal writer thread ( allow it to write again )
					int condBroadcastResult = pthread_cond_broadcast( const_cast< pthread_cond_t * >( &PoolWriterBarrier ) );
					if ( 0 != condBroadcastResult )
					{
						TRACE_LOG( "Condition broadcast on PoolWriterBarrier failed [" << condBroadcastResult << "]" );
					}
				}

				return item;
			}

			void erasePoolItem( string id, const bool throwOnError = true ) volatile
			{
				pthread_t itemOwnerThread;
				
				try
				{
					LockingPtr< WorkItemPool_QueueType > lpPool( m_Pool, PoolSyncMutex );

					bool found = false;
					typename WorkItemPool_QueueType::iterator poolItemFinder = lpPool->begin();
					while( poolItemFinder != lpPool->end() )
					{
						if ( poolItemFinder->first == id )
						{
							itemOwnerThread = poolItemFinder->second.getOwnerThread();
							found = true;
							break;
						}
						poolItemFinder++;
					}
					if ( m_Shutdown )
						throw WorkPoolShutdown();

					if ( !found )
						throw WorkItemNotFound( id );

					lpPool->erase( poolItemFinder );
				}
				catch( const std::exception& ex )
				{
					TRACE_LOG( "A [" << typeid( ex ).name() << "] has occured [" << ex.what() << "] while removing item from pool" );
					if ( throwOnError )
						throw;
				}
				catch( ... )
				{
					TRACE_LOG( "An [unknown exception] has occured while removing item from pool." );
					if ( throwOnError )
						throw;
				}

				// signal writers
				{
					LockingPtr< WorkItemPool_CounterType > lpWriterItems( m_WriterItems, ReserveSyncMutex );

					// decrement thread item count
					( ( *lpWriterItems )[ itemOwnerThread ] )--;

					// signal writer thread ( allow it to write again )
					int condBroadcastResult = pthread_cond_broadcast( const_cast< pthread_cond_t* >( &PoolWriterBarrier ) );
					if ( 0 != condBroadcastResult )
					{
						TRACE_LOG( "Condition broadcast on PoolWriterBarrier failed [" << condBroadcastResult << "]" );
					}
				}
			}
			
			WorkItem< T > removePoolItem( string id, const bool throwOnError = true ) volatile
			{
				WorkItem< T > item;
				
				try
				{
					LockingPtr< WorkItemPool_QueueType > lpPool( m_Pool, PoolSyncMutex );

					bool found = false;
					typename WorkItemPool_QueueType::iterator poolItemFinder = lpPool->begin();
					while( poolItemFinder != lpPool->end() )
					{
						if ( poolItemFinder->first == id )
						{
							found = true;
							break;
						}
						poolItemFinder++;
					}
					if ( m_Shutdown )
						throw WorkPoolShutdown();

					if ( !found )
						throw WorkItemNotFound( id );

					item = poolItemFinder->second;
					lpPool->erase( poolItemFinder );
				}
				catch( const std::exception& ex )
				{
					TRACE_LOG( "A [" << typeid( ex ).name() << "] has occured [" << ex.what() << "] while removing item from pool" );
					if ( throwOnError )
						throw;
					else
						return item;
				}
				catch( ... )
				{
					TRACE_LOG( "An [unknown exception] has occured while removing item from pool." );
					if ( throwOnError )
						throw;
					else
						return item;
				}

				// signal writers
				{
					LockingPtr< WorkItemPool_CounterType > lpWriterItems( m_WriterItems, ReserveSyncMutex );

					// decrement thread item count
					( ( *lpWriterItems )[ item.getOwnerThread() ] )--;

					// signal writer thread ( allow it to write again )
					int condBroadcastResult = pthread_cond_broadcast( const_cast< pthread_cond_t* >( &PoolWriterBarrier ) );
					if ( 0 != condBroadcastResult )
					{
						TRACE_LOG( "Condition broadcast on PoolWriterBarrier failed [" << condBroadcastResult << "]" );
					}
				}
				
				return item;
			}

			void Dump() volatile
			{
				WorkItemPool_QueueType* lpQueue = const_cast< WorkItemPool_QueueType* >( &m_Pool );
				typename WorkItemPool_QueueType::const_iterator poolItemFinder = lpQueue->begin();
				stringstream output;
				while( poolItemFinder != lpQueue->end() )
				{
					output << *( poolItemFinder->second.get() );
					poolItemFinder++;
				}

				DEBUG_LOG( output.str() );
			}
			
			unsigned int getSize() volatile
			{
				LockingPtr< WorkItemPool_QueueType > lpPool( m_Pool, PoolSyncMutex );
				return lpPool->size();
			}
			
			void SignalAllReaders()
			{
				int condSignalResult = pthread_cond_broadcast( &PoolReaderBarrier );
				if ( 0 != condSignalResult )
				{
					TRACE_LOG( "Condition broadcast on PoolReaderBarrier failed [" << condSignalResult << "]" );
				}
			}

			void SignalReaders()
			{
				int condSignalResult = pthread_cond_signal( &PoolReaderBarrier );
				if ( 0 != condSignalResult )
				{
					TRACE_LOG( "Condition signal on PoolReaderBarrier failed [" << condSignalResult << "]" );
				}			
			}

			void SignalWriters()
			{
				int condSignalResult = pthread_cond_signal( &PoolWriterBarrier );
				if ( 0 != condSignalResult )
				{
					TRACE_LOG( "Condition signal on PoolWriterBarrier failed [" << condSignalResult << "]" );
				}	
			}

			void ShutdownPool()
			{
				// mark shutdown
				{
					LockingPtr< WorkItemPool_QueueType > lpPool( m_Pool, PoolSyncMutex );
					m_Shutdown = true;
				}
				SignalAllReaders();
				SignalWriters();
			}

			void ShutdownPoolWriters()
			{
				// mark shutdown
				{
					LockingPtr< WorkItemPool_QueueType > lpPool( m_Pool, PoolSyncMutex );
					m_Shutdown = true;
				}
				SignalWriters();
			}

			bool IsRunning() const
			{
				return !m_Shutdown;
			}

	#if defined( TESTDLL_EXPORT ) || defined ( TESTDLL_IMPORT )
			bool Start() const
			{
				m_Shutdown = false;
			}
	#endif

		private :
				
			bool m_Shutdown;
			volatile WorkItemPool_QueueType m_Pool;
			volatile WorkItemPool_CounterType m_WriterItems;

			pthread_mutex_t PoolSyncMutex;
			pthread_mutex_t ReserveSyncMutex;
			
			pthread_cond_t PoolReaderBarrier;
			pthread_cond_t PoolWriterBarrier;
	};

	class ExportedUtilsObject ManagedBuffer
	{
		public :  

			enum BufferType
			{
				// will adopt the buffer, destroy it upon exit
				Adopt,
				// will reference the buffer, will not destroy it on destructor
				Ref,
				// create a copy of the buffer, leave it to the caller to destroy original
				Copy
			};

		public :
			
			explicit ManagedBuffer( unsigned char* buffer = NULL, ManagedBuffer::BufferType bufferType = ManagedBuffer::Adopt, unsigned long size = 0, unsigned long maxBufferSize = MAX_MESSAGE_LEN );
			ManagedBuffer( const ManagedBuffer& source );
			ManagedBuffer& operator=( const ManagedBuffer& source );

			ManagedBuffer getRef();

			~ManagedBuffer();

			void allocate( unsigned long size );

			unsigned long size() const
			{
				return m_BufferSize;
			}

			unsigned long max_size() const
			{
				return m_MaxBufferSize;
			}

			unsigned char* buffer() const
			{
				return *m_BufferAddr;
			}

			ManagedBuffer::BufferType type() const
			{
				return m_BufferType;
			}

			string str() const;
			string str( const string::size_type size ) const;

			void copyFrom( const string& source, const unsigned long maxBufferSize = MAX_MESSAGE_LEN );
			void copyFrom( const unsigned char* source, unsigned long size, unsigned long maxBufferSize = MAX_MESSAGE_LEN );
			void copyFrom( const ManagedBuffer& source );
			void copyFrom( const ManagedBuffer* source );

			void truncate( const unsigned long index )
			{
				//TODO delete and realloc
				if ( index >= m_BufferSize )
					return;
				m_BufferSize = index;
			}

			// operators
			ManagedBuffer operator+( const unsigned long offset ) const;
			ManagedBuffer& operator+=( const unsigned long offset );
			long operator-( const ManagedBuffer& source ) const;

		private :
			
			ManagedBuffer::BufferType m_BufferType;
			
			unsigned char** m_BufferAddr;
			unsigned long m_MaxBufferSize;
			unsigned long m_BufferSize;
	};
}

#endif // WORKITEMPOOL_H
