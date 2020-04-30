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
#include "RSASign.h"
#include "Base64.h"

using namespace std;
using namespace FinTP;

string RSASign::SignMessage( string pemKeyFile, string plainText ) 
{
	DEBUG( "Signing message using RSA Algorithm..." );

	EVP_MD_CTX* RSASignCtx = EVP_MD_CTX_create( );
	EVP_PKEY* privateKey = NULL;

	unsigned char *signedText = NULL;
	string encodedSingnedText = "";
	size_t signedTextLength;

	try 
	{
		//Create read private key
		if( privateKey == NULL )
		{
			privateKey = EVP_PKEY_new();

			if( pemKeyFile.length( ) < 1 )
				throw runtime_error( "Empty PEM file provided!" );

			BIO *bio = BIO_new( BIO_s_mem() ); 

			int bio_write_ret = BIO_write( bio, static_cast<const char*>( pemKeyFile.c_str() ), pemKeyFile.size() );

			if( bio_write_ret <= 0 )
				throw runtime_error( "Write PEM Key to variable fail. Can't create Private Key" );

			if( !PEM_read_bio_PrivateKey( bio, &privateKey, NULL, NULL ) )
			{
				BIO_free(bio);
				throw runtime_error( "Create RSA Private Key failed ");
			}

			if( privateKey == NULL )
				throw runtime_error( "Load Private Key to sign message failed" );
		}
		//Calculate Signature Value
		if( EVP_DigestSignInit( RSASignCtx, NULL, EVP_sha256(), NULL, privateKey ) <= 0 )
			throw runtime_error( "Digest sign initialization failed" );
		
		if( EVP_DigestSignUpdate( RSASignCtx, ( unsigned char* )( plainText.c_str() ), plainText.length() ) <= 0 )
			throw runtime_error( "Digest sign update failed" );
			
		if( EVP_DigestSignFinal( RSASignCtx, NULL, &signedTextLength ) <= 0 )
			throw runtime_error( "Digest sign final failed" );
		
		signedText = ( unsigned char* )malloc( signedTextLength );
		if( signedText == NULL )
			throw runtime_error( "Failed to allocate memmory for signed message!" );

		if( EVP_DigestSignFinal( RSASignCtx, signedText, &signedTextLength ) <= 0 )
			throw runtime_error( "Signing message failed!" );

		encodedSingnedText = Base64::encode( signedText, signedTextLength );
		
		if( privateKey != NULL )
		{
			EVP_PKEY_free( privateKey );
			privateKey = NULL;
		}

		EVP_MD_CTX_cleanup( RSASignCtx );
		free( signedText );
	}
	catch( runtime_error &re )
	{
		EVP_MD_CTX_cleanup( RSASignCtx );

		if( signedText != NULL )
			free( signedText );

		if( privateKey != NULL )
		{
			EVP_PKEY_free( privateKey );
			privateKey = NULL;
		}

		TRACE( "Error while attempting to sign the message: " << re.what() );
		throw re;
	}
	catch( ... )
	{
		EVP_MD_CTX_cleanup( RSASignCtx );

		if( signedText != NULL )
			free( signedText );

		if( privateKey != NULL )
		{
			EVP_PKEY_free( privateKey );
			privateKey = NULL;
		}

		TRACE( "Unhandled exception while attempting to sign message" ); 
		throw;
	}
	DEBUG( "Signature value was calculated..." );
	return encodedSingnedText;
}

bool RSASign::VerifySignature( const string pemKeyFile, const string plainText, const string signatureBase64 )
{

	DEBUG( "Verify message using RSA Algorithm..." );
	EVP_PKEY* publicKey = NULL;
	EVP_MD_CTX* RSAVerifyCtx = EVP_MD_CTX_create();
	int verifySignatureReturn = 0;

	try 
	{
		//Verify if Private Key has already been initiated
		// TODO: thread safe initialization
		//Read Public Key
		if( publicKey == NULL )
		{
			publicKey = EVP_PKEY_new();

			if ( pemKeyFile.length( ) < 1 )
				throw runtime_error( "Empty Public Key provided!" );

			BIO *bio = BIO_new( BIO_s_mem() );

			int bio_write_ret = BIO_write( bio, static_cast<const char*>( pemKeyFile.c_str() ), pemKeyFile.size() );

			if ( bio_write_ret <= 0 )
				throw runtime_error( "Write PEM Key to variable fail. Can't create Public Key" );

			if ( !PEM_read_bio_PUBKEY( bio, &publicKey, NULL, NULL ) )
			{
				BIO_free( bio );
				throw runtime_error( "Create RSA Public Key failed" );
			}

			if ( publicKey == NULL )
				throw runtime_error( "Load Public Key to sign message failed" );
		}
		//Verify signature 
		string msgHashStr = Base64::decode( signatureBase64 );
		size_t msgHashLen = msgHashStr.size();

		if ( EVP_DigestVerifyInit( RSAVerifyCtx, NULL, EVP_sha256( ), NULL, publicKey ) <= 0 )
			throw runtime_error( "Verify digest initialization failed" );

		if ( EVP_DigestVerifyUpdate( RSAVerifyCtx, plainText.c_str(), plainText.length() ) <= 0 )
			throw runtime_error( " Verify digest update failed" );

		unsigned char* msgHash = ( unsigned char* )( msgHashStr.c_str() );

		verifySignatureReturn = EVP_DigestVerifyFinal( RSAVerifyCtx, msgHash, msgHashLen );
		
		// 1->Checked, 0->Failed, (>1)->Error
		if( ( verifySignatureReturn != 1 ) && ( verifySignatureReturn != 0 ) )
			throw runtime_error( "Invalid signature form" );

		if ( publicKey != NULL )
		{
			EVP_PKEY_free( publicKey );
			publicKey = NULL;
		}

	}
	catch( runtime_error &re )
	{
		if ( publicKey != NULL )
		{
			EVP_PKEY_free( publicKey );
			publicKey = NULL;
		}
		EVP_MD_CTX_cleanup( RSAVerifyCtx );
		TRACE( "Error while attempting to sign the message: " << re.what() );
		throw re;
	}
	catch( ... )
	{
		if ( publicKey != NULL )
		{
			EVP_PKEY_free( publicKey );
			publicKey = NULL;
		}
		EVP_MD_CTX_cleanup( RSAVerifyCtx );
		TRACE( "Unhandled exception while attempting to sign message" ); 
		throw;
	}

	EVP_MD_CTX_cleanup( RSAVerifyCtx );
	DEBUG( "Signature value was verified..." );
	return ( verifySignatureReturn == 1 );

}


/*
size_t RSASign::calcDecodeLength( const char* b64input ) 
{
	size_t len = strlen( b64input ), padding = 0;

	if ( b64input[len - 1] == '=' && b64input[len - 2] == '=' ) //last two chars are =
		padding = 2;
	else if ( b64input[len - 1] == '=' ) //last char is =
		padding = 1;
	return ( len * 3 ) / 4 - padding;
}

bool RSASign::VerifySignature( const char* privateKeyFilePath, std::string plainText, char* signatureBase64 ) 
{

	string line;
	string finalCertificate = "";
	ifstream myfile( privateKeyFilePath );
	if ( myfile.is_open( ) )
	{
		while ( getline( myfile, line ) )
		{
			finalCertificate += line + '\n';
		}
		myfile.close( );
	}
	else
		TRACE( "Unable to open Certificate File" );

	return VerifySignature( finalCertificate, plainText, signatureBase64 );
}

string RSASign::SignMessage( const char* privateKeyFilePath, std::string plainText ) 
{
	
	string line;
	string finalCertificate = "";
	ifstream myfile( privateKeyFilePath );
	if ( myfile.is_open( ) )
	{
		while ( getline( myfile, line ) )
		{
			finalCertificate += line + '\n';
		}
		myfile.close( );
	}
	else
		TRACE( "Unable to open Certificate File" );

	return SignMessage( finalCertificate,plainText );
}

void  RSASign::Base64Encode( const unsigned char* buffer, size_t length, char** base64Text ) 
{
	BIO *bio, *b64;
	BUF_MEM *bufferPtr;

	b64 = BIO_new( BIO_f_base64( ) );
	bio = BIO_new( BIO_s_mem( ) );
	bio = BIO_push( b64, bio );

	BIO_write( bio, buffer, length );
	BIO_flush( bio );
	BIO_get_mem_ptr( bio, &bufferPtr );
	BIO_set_close( bio, BIO_NOCLOSE );
	BIO_free_all( bio );

	*base64Text = ( *bufferPtr ).data;
}
void RSASign::Base64Decode( char* b64message, unsigned char** buffer, size_t* length ) 
{
	BIO *bio, *b64;

	int decodeLen = calcDecodeLength( b64message );
	*buffer = ( unsigned char* )malloc( decodeLen + 1 );
	( *buffer )[decodeLen] = '\0';

	bio = BIO_new_mem_buf( b64message, -1 );
	b64 = BIO_new( BIO_f_base64( ) );
	bio = BIO_push( b64, bio );

	*length = BIO_read( bio, *buffer, strlen( b64message ) );
	BIO_free_all( bio );
}
*/