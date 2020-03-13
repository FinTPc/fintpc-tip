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

#ifndef LOCKINGPTR_H
#define LOCKINGPTH_H

#ifdef WIN32
	#define __MSXML_LIBRARY_DEFINED__
	#include <windows.h>
	#include <string.h>
#endif

#include <sstream>
#include <cstring>
#include <cerrno>

namespace FinTP
{
	template <typename T>
	class LockingPtr
	{
		public:
		
			// Constructors/destructors
			LockingPtr( volatile T& obj, pthread_mutex_t& mtx ) : pObj_( const_cast< T* >( &obj ) ), pMtx_( &mtx )
			{
				int errCode = pthread_mutex_lock( pMtx_ );
				if ( 0 != errCode )
				{
					stringstream errorMessage;
#ifdef CRT_SECURE
					char errBuffer[ 95 ];
					strerror_s( errBuffer, sizeof( errBuffer ), errCode );
 					errorMessage << "Unable to lock mutex. Error code : " << errCode << " [" << errBuffer << "]";
#else
					errorMessage << "Unable to lock mutex. Error code : " << errCode << " [" << strerror( errCode ) << "]";
#endif	
					throw runtime_error( errorMessage.str() );
				}
			}

			LockingPtr( volatile T& obj, volatile pthread_mutex_t& mtx ) : pObj_( const_cast< T* >( &obj ) ), 
				pMtx_( const_cast< pthread_mutex_t* >( &mtx ) )
			{
				int errCode = pthread_mutex_lock( pMtx_ );
				if ( 0 != errCode )
				{
					stringstream errorMessage;
#ifdef CRT_SECURE
					char errBuffer[ 95 ];
					strerror_s( errBuffer, sizeof( errBuffer ), errCode );
 					errorMessage << "Unable to lock mutex. Error code : " << errCode << " [" << errBuffer << "]";
#else
					errorMessage << "Unable to lock mutex. Error code : " << errCode << " [" << strerror( errCode ) << "]";
#endif	
					throw runtime_error( errorMessage.str() );
				}
			}
		
			~LockingPtr()
			{
				int errCode = pthread_mutex_unlock( pMtx_ );
				if ( 0 != errCode )
				{
					stringstream errorMessage;
#ifdef CRT_SECURE
					char errBuffer[ 95 ];
					strerror_s( errBuffer, sizeof( errBuffer ), errCode );
 					errorMessage << "Unable to unlock mutex. Error code : " << errCode << " [" << errBuffer << "]";
#else
					errorMessage << "Unable to unlock mutex. Error code : " << errCode << " [" << strerror( errCode ) << "]";
#endif	
					throw runtime_error( errorMessage.str() );
				}
			}
			
			// Pointer behavior
			T& operator*() { return *pObj_; }
			T* operator->() { return pObj_; }

		private :
		
			T* pObj_;
			pthread_mutex_t* pMtx_;
			
			// disable copying
			LockingPtr( const LockingPtr& );
			LockingPtr& operator=( const LockingPtr& );
	};

	class UIntType
	{
		public :

			#if defined( AIX ) && defined( DIE_HARD )
			typedef register UINT32 base_type;
			#elif defined( WIN32 ) //&& defined( DIE_HARD )
			typedef volatile LONG base_type;
			#else
			typedef volatile unsigned int base_type;
			#endif

			typedef base_type* base_type_ptr;

			inline static UIntType::base_type increment( UIntType::base_type_ptr value )
			{
				#if defined( AIX ) && defined( DIE_HARD )
				register unsigned int zeroOffset = 0;
    			register unsigned int temp;
				asm
    			{    
					retry:
						lwarx temp, zeroOffset, value		// Read integer from RAM into r4, placing reservation.
						addi temp, temp, 1					// Add 1 to r4.
						stwcx. temp, zeroOffset, value		// Attempt to store incremented value back to RAM.
						bne- retry							// If the store failed (unlikely), retry.
				}
				return temp;
				#elif defined( WIN32 ) //&& defined( DIE_HARD )
				return InterlockedIncrement( value );
				#else
				return ++( *value );
				#endif
			}
			
			inline static UIntType::base_type decrement( UIntType::base_type_ptr value )
			{
				#if defined( AIX ) && defined( DIE_HARD )
				register unsigned int zeroOffset = 0;
				register unsigned int temp;
			
				asm
				{
					again:
						lwarx temp, zeroOffset, value
						subi temp, temp, 1
						stwcx. temp, zeroOffset, value
						bne- again
				}
				return temp;
				#elif defined( WIN32 ) //&& defined( DIE_HARD )
				return InterlockedDecrement( value );
				#else
				return --( *value );
				#endif
			}
	};
}

#endif // LOCKINGPTR_H
