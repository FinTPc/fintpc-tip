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

#include "ECDSASign.h"

using namespace std;
using namespace FinTP;

EC_KEY* ECDSASign::m_PrivateECKey = NULL;
EC_KEY* ECDSASign::m_PublicECKey = NULL;

void ECDSASign::ReadPrivateEC_KEY( string pemKeyFile )
{
	EVP_PKEY *pKey = EVP_PKEY_new( );
	BIO* bio = BIO_new( BIO_s_mem( ) );

	try
	{
		int bio_write_ret = BIO_write( bio, static_cast<const char*>( pemKeyFile.c_str( ) ), pemKeyFile.size( ) );

		if ( bio_write_ret <= 0 )
			throw runtime_error( "Write PEM Key to variable fail. Can't create Private Key" );

		if ( !PEM_read_bio_PrivateKey( bio, &pKey, NULL, NULL ) )
			throw runtime_error( "Read ECDSA Private Key failed" );

		EC_KEY* eckey_local = EVP_PKEY_get1_EC_KEY( pKey );
		if ( !eckey_local )
			throw runtime_error( "Create ECDSA Private Key failed" );
		else 
		{
			m_PrivateECKey = eckey_local;
			EC_KEY_set_asn1_flag( m_PrivateECKey, OPENSSL_EC_NAMED_CURVE );
		}
	}
	catch( runtime_error &re )
	{
		BIO_free( bio );
		EVP_PKEY_free( pKey );

		TRACE( re.what() );
		throw re;
	}
	catch( ... )
	{
		BIO_free( bio );
		EVP_PKEY_free( pKey );
		TRACE( "Unknown error while read ECDSA Private Key" );
		throw;
	}

	BIO_free( bio );
	EVP_PKEY_free( pKey );
}

void ECDSASign::ReadPublicEC_KEY( string pemKeyFile )
{
	BIO* bio = BIO_new( BIO_s_mem( ) );
	try
	{
		int bio_write_ret = BIO_write( bio, static_cast<const char*>( pemKeyFile.c_str( ) ), pemKeyFile.size( ) );

		if ( bio_write_ret <= 0 )
			throw runtime_error( "Write PEM Key to variable fail. Can't create Public Key" );

		if ( !PEM_read_bio_EC_PUBKEY( bio, &m_PublicECKey, NULL, NULL ) )
			throw runtime_error( "Create ECDSA Public Key failed" );
	}
	catch( runtime_error &re )
	{
		BIO_free( bio );

		TRACE( re.what() );
		throw re;
	}
	catch( ... )
	{
		BIO_free( bio );
		TRACE( "Unknown error while read ECDSA Private Key" );
		throw;
	}

	BIO_free( bio );
}

string ECDSASign::GetBase64Signature( BIGNUM* r_sign, BIGNUM* s_sign )
{
	unsigned char sign_r[32], sign_s[32], sign_rs[64];
	string encodedSignature = "";
	try
	{
		if ( r_sign == NULL || s_sign == NULL )
			throw runtime_error( "A point from  signature null" );

		/*Transforming the r and s points into binary*/
		BN_bn2bin( r_sign, sign_r );
		BN_bn2bin( s_sign, sign_s );

		/*Copy the r and s points into single variable to encode in base64 */
		for ( int i = 0; i < 32; i++ )
		{
			sign_rs[i] = sign_r[i];
			sign_rs[i + 32] = sign_s[i];
		}

		encodedSignature = Base64::encode( sign_rs, 64 );
	}
	catch( runtime_error &ex )
	{
		TRACE( ex.what( ) );
		throw ex;
	}
	catch( ... )
	{
		TRACE( "Error while attempting to encode the signature in base64" );
		throw;
	}

	return encodedSignature;
}

string ECDSASign::SignMessage( string pemKeyFile, string plainText ) 
{
	DEBUG( "Signing message using ECDSA Algorithm..." );
	/**
	 * ECDSA_SIG contains a pair of integars ( r, s ) that together represent the signature
	 *  r and as are stored as *BIGNUM
	 *  If signing fails, returns empty		
	 */
	ECDSA_SIG* signatureECDSA = NULL; 

	//hash_c is filled with SHA256( PlainText )
	unsigned char hash_c[32];
	string signedText = plainText;
	
	try 
	{
		//Verify if Private Key has already been initiated
		// TODO: thread safe initialization
		if ( m_PrivateECKey == NULL ) 
		{
			if ( pemKeyFile.length( ) < 1 )
				throw runtime_error( "Empty Private Key provided!" );

			ReadPrivateEC_KEY( pemKeyFile );

			if ( m_PrivateECKey == NULL )
				throw runtime_error( "Load Private Key to sign message failed" );
		}
		
		if ( plainText.length( ) < 1 )
			throw runtime_error( "Empty text providet for signing!" );

		string hash_s = HMAC::Sha256( plainText, HMAC::BASE64 );
		string hashBuffer = Base64::decode( hash_s ).c_str( );

		for ( int i = 0; i < 32; i++ )
			hash_c[i] = hashBuffer.c_str()[i];

		signatureECDSA = ECDSA_do_sign( hash_c, sizeof( hash_c ), m_PrivateECKey );

		if ( signatureECDSA == NULL )
			throw runtime_error( "ECDSA signing failed!" );

		signedText = GetBase64Signature( signatureECDSA->r, signatureECDSA->s );
		if ( signatureECDSA != NULL )
			ECDSA_SIG_free( signatureECDSA );

	}
	catch( exception &ex ) 
	{
		if ( signatureECDSA != NULL )
			ECDSA_SIG_free( signatureECDSA );
		TRACE( "Error while attempting to sign the message: " << ex.what() );
		throw ex;
	}
	catch( ... )
	{
		if ( signatureECDSA != NULL )
			ECDSA_SIG_free( signatureECDSA );
		TRACE( "Unhandled exception while attempting to sign message" ); 
		throw;
	}

	return signedText;
}

bool ECDSASign::VerifySignature( const string pemKeyFile, const string plainText, const string signatureBase64 )
{
	DEBUG( "Verifying message using ECDSA Algorithm..." );
	unsigned char sign_r[32], sign_s[32], hash_c[32];
	ECDSA_SIG *signature = NULL;
	int verifySignatureReturn = 0;

	try 
	{
		if ( m_PublicECKey == NULL )
		{
			if ( pemKeyFile.length( ) < 1 )
				throw runtime_error( "Empty Private Key provided!" );

			ReadPublicEC_KEY( pemKeyFile );

			if ( m_PublicECKey == NULL )
				throw runtime_error( "Load Public key to validate message signature failed" );
		}
		
		if ( plainText.length( ) < 1 )
			throw runtime_error( "Empty text provided to validate signature!" );

		string hmac = HMAC::Sha256( plainText, HMAC::BASE64 );
		string hashBufferStr = Base64::decode( hmac );

		for ( int i = 0; i < 32; i++ )
			hash_c[i] = hashBufferStr.c_str()[i];

		string decodedSgntr = Base64::decode( signatureBase64 );
		for ( int i = 0; i < 32; i++ )
		{
			sign_r[i] = decodedSgntr.c_str()[i];
			sign_s[i] = decodedSgntr.c_str()[i + 32];
		}

		signature = ECDSA_SIG_new( );
		if( signature == NULL )
			throw runtime_error( "Error while trying to instantiate new ECDSA signature" );
		signature->r = BN_new( );
		signature->s = BN_new( );
		BN_bin2bn( sign_r, 32, signature->r );
		BN_bin2bn( sign_s, 32, signature->s );
		//printf("(sig->r, sig->s): (%s,%s)\n", BN_bn2hex(signature->r), BN_bn2hex(signature->s));
		
		verifySignatureReturn = ECDSA_do_verify( hash_c, 32, signature, m_PublicECKey );

		// 1->Checked, 0->Failed, (>1)->Error
		if( ( verifySignatureReturn != 1 ) && ( verifySignatureReturn != 0 ) )
			throw runtime_error( "Invalid signature form" );
	}
	catch ( runtime_error &re )
	{
		if ( signature != NULL )
			ECDSA_SIG_free( signature );

		TRACE( "Error while attempting to validate signature:" << re.what() );
		throw re;
	}
	catch ( ... )
	{
		if ( signature != NULL )
			ECDSA_SIG_free( signature );
		TRACE( "Unhandled exception while attempting to validate signature" ); 
		throw;
	}

	if ( signature != NULL )
		ECDSA_SIG_free( signature );
	return ( verifySignatureReturn == 1 ) ;
}

/*
void ECDSASign::CreateKeyPairEC_KEY( string pemPrivateKey )
{
	m_PrivateECKey = EC_KEY_new();
	m_PublicECKey = EC_KEY_new();
	EC_GROUP* ecgroup = EC_GROUP_new_by_curve_name( NID_X9_62_prime256v1 );
	EC_KEY_set_group( m_PrivateECKey, ecgroup );
	EC_KEY_set_asn1_flag( m_PrivateECKey, OPENSSL_EC_NAMED_CURVE );

	// the private key value 
	const char *p_str = pemPrivateKey.c_str( );
	BIGNUM* prv = BN_new( );
	BN_hex2bn( &prv, p_str );
	EC_POINT* pub = EC_POINT_new( ecgroup );

	//calculate the public key
	EC_POINT_mul( ecgroup, pub, prv, NULL, NULL, NULL );

	// add the private & public key to the EC_KEY structure
	EC_KEY_set_private_key( m_PrivateECKey, prv );
	EC_KEY_set_public_key( m_PrivateECKey, pub );
}
*/
