// g++ -g3 -ggdb -O0 -DDEBUG -Wall -Wextra cryptopp-test.cpp -o cryptopp-test.exe -lcryptopp -lpthread
// g++ -g -O2 -DNDEBUG -Wall -Wextra cryptopp-test.cpp -o cryptopp-test.exe -lcryptopp -lpthread
#include "stdafx.h"
#include <Windows.h>
#include "Shlwapi.h"
#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

#include <string>
using std::string;

#include <stdexcept>
using std::runtime_error;

#include <cstdlib>
using std::exit;

#include "cryptopp/osrng.h"
using CryptoPP::AutoSeededRandomPool;

#include "cryptopp/eccrypto.h"
using CryptoPP::ECP;
using CryptoPP::ECDSA;

#include "cryptopp/sha.h"
using CryptoPP::SHA1;

#include "cryptopp/queue.h"
using CryptoPP::ByteQueue;

#include "cryptopp/oids.h"
using CryptoPP::OID;

// ASN1 is a namespace, not an object
#include "cryptopp/asn.h"
using namespace CryptoPP::ASN1;

#include "cryptopp/integer.h"
using CryptoPP::Integer;

#include "CryptoPP/fips140.h"

// simple 1KiB buffer
typedef struct _BUFFER {
	DWORD	   cbLength;
	BYTE	   data[1020];
} BUFFER;
	 


void store_buff(LPCSTR szFileName, const void *buff, size_t bytes)
{
	HANDLE hFile = CreateFile(szFileName, FILE_GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
	_ASSERT(hFile != INVALID_HANDLE_VALUE);
	DWORD wb = 0;
	WriteFile(hFile, buff, bytes, &wb, NULL);
	_ASSERT(wb == bytes);
	CloseHandle(hFile);
}

string *load_file(LPCSTR szFileName)
{
	if (NULL == szFileName || !PathFileExists(szFileName))
	{
		cout << "#ERROR: invalid file name specified in argument 2" << endl;
		return NULL;
	}
	HANDLE hFile = CreateFile(szFileName, FILE_GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
	_ASSERT (hFile != INVALID_HANDLE_VALUE);

	size_t size = GetFileSize(hFile, NULL);

	if (0 == size) return NULL;

	string *result = new string(size, 0x00);	
	
	DWORD rb = 0;
	ReadFile(hFile, (void*) result->data(), size, &rb, NULL);
	_ASSERT (rb == size); 
	return result;
}

// int CRYPTOPP_API main(int argc, char *argv[]){



_declspec(dllexport) 
bool __cdecl create_keys(BUFFER *priv_k, BUFFER *pub_k)
{
	AutoSeededRandomPool prng;
	ByteQueue privateKey, publicKey;

	// string message = "Do or do not. There is no try.";

	//////////////////////////////////////////////////////

	// Generate private key
	ECDSA<ECP, SHA1>::PrivateKey privKey;
	// Create public key
	ECDSA<ECP, SHA1>::PublicKey pubKey;

	privKey.Initialize(prng, secp160r1());
	privKey.Save(privateKey);
	priv_k->cbLength = (size_t)privateKey.MaxRetrievable();
	// cout << " private key raw size = " << max_ret << endl;

	privateKey.Peek(priv_k->data, priv_k->cbLength);
	privKey.MakePublicKey(pubKey);
	pubKey.Save(publicKey);

	// store_buff("private_key.bin", raw_pk, max_ret);		
	// saving public key
	pub_k->cbLength = (size_t)publicKey.MaxRetrievable();
	publicKey.Peek(pub_k->data, pub_k->cbLength);
	// store_buff("public_key.bin", raw_pk, max_ret);
	return (priv_k->cbLength && pub_k->cbLength);	
}

_declspec(dllexport) 
 bool __cdecl sign_data(BUFFER *data, BUFFER *priv_k, BUFFER *signature)
 {
	AutoSeededRandomPool prng;
	ByteQueue privateKey;		
	privateKey.Put( (const byte*)priv_k->data, priv_k->cbLength);
	//////////////////////////////////////////////////////    
	// Load private key (in ByteQueue, PKCS#8 format)
	ECDSA<ECP, SHA1>::Signer signer( privateKey );

	// Determine maximum size, allocate a string with that size
	size_t siglen = signer.MaxSignatureLength();
	// Sign, and trim signature to actual size
	siglen = signer.SignMessage (prng, (const byte*)data->data, data->cbLength, signature->data);
	signature->cbLength = siglen;
	return (siglen > 0);
}

_declspec(dllexport) 
bool __cdecl verify_signature(BUFFER *data, BUFFER *pub_k, BUFFER *signature)	
{
	//////////////////////////////////////////////////////    
	ByteQueue privateKey, publicKey;		
	publicKey.Put ( (const byte*) pub_k->data, pub_k->cbLength);	
	// Load public key (in ByteQueue, X509 format)
	ECDSA<ECP, SHA1>::Verifier verifier( publicKey );

	return verifier.VerifyMessage (data->data, data->cbLength, signature->data, signature->cbLength);	
}
