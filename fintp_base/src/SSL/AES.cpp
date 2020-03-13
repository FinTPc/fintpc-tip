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

#include <stdexcept>
#include "AES.h"

namespace FinTP
{

AES::AES( const unsigned char* key, size_t keySize )
{
	unsigned char derivedKey[32], initializationVector[32];

	int result = EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha1(), NULL, key, keySize, 1, derivedKey, initializationVector );
	if ( result != 32 )
		throw std::runtime_error("AES initialization failed");

	EVP_CIPHER_CTX_init( &m_Encrypt);
	EVP_EncryptInit_ex( &m_Encrypt, EVP_aes_256_cbc(), NULL, derivedKey, initializationVector );
	EVP_CIPHER_CTX_init( &m_Decrypt);
	EVP_DecryptInit_ex( &m_Decrypt, EVP_aes_256_cbc(), NULL, derivedKey, initializationVector );
}

AES::~AES()
{
	EVP_CIPHER_CTX_cleanup( &m_Encrypt );
	EVP_CIPHER_CTX_cleanup( &m_Decrypt );
}

std::vector<unsigned char> AES::AES_decrypt( const unsigned char* encryptedData, size_t encryptedDataSize )
{
	std::vector<unsigned char> decryptedData;
	decryptedData.resize( encryptedDataSize + AES_BLOCK_SIZE );
	int outLength, finalLength;

	EVP_DecryptInit_ex( &m_Decrypt, NULL, NULL, NULL, NULL );
	EVP_DecryptUpdate( &m_Decrypt, &decryptedData[0], &outLength, encryptedData, encryptedDataSize );
	EVP_DecryptFinal_ex( &m_Decrypt, &decryptedData[outLength], &finalLength );

	decryptedData.resize( outLength + finalLength );

	return decryptedData;
}

std::vector<unsigned char> AES::AES_encrypt( const unsigned char* plainData, size_t plainDatasize )
{
	std::vector<unsigned char> encryptedData;
	encryptedData.resize( plainDatasize + AES_BLOCK_SIZE );
	int outLength, finalLength;

	EVP_EncryptInit_ex( &m_Encrypt, NULL, NULL, NULL, NULL );
	EVP_EncryptUpdate( &m_Encrypt, &encryptedData[0], &outLength, plainData, plainDatasize );
	EVP_EncryptFinal_ex( &m_Encrypt, &encryptedData[outLength], &finalLength );

	encryptedData.resize( outLength + finalLength );

	return encryptedData;
}

}
