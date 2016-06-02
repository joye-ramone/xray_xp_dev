/*
   LZ4 - Fast LZ compression algorithm
   Header File
   Copyright (C) 2011-2014, Yann Collet.
   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
   - LZ4 source repository : http://code.google.com/p/lz4/
   - LZ4 public forum : https://groups.google.com/forum/#!forum/lz4c
*/
#pragma once

#define LZ4_API	 __declspec(dllimport)

#define LZ4_MAX_INPUT_SIZE        0x7E000000   /* 2 113 929 216 bytes */
#define LZ4_COMPRESSBOUND(isize)  ((unsigned int)(isize) > (unsigned int)LZ4_MAX_INPUT_SIZE ? 0 : (isize) + ((isize)/255) + 16)

#pragma comment( lib, "LZ4.lib"	)
#if defined (__cplusplus)
extern "C" {
#endif


	LZ4_API int		LZ4_versionNumber(void);

	/*
	LZ4_compress() :
	Compresses 'sourceSize' bytes from 'source' into 'dest'.
	Destination buffer must be already allocated,
	and must be sized to handle worst cases situations (input data not compressible)
	Worst case size evaluation is provided by function LZ4_compressBound()
	inputSize : Max supported value is LZ4_MAX_INPUT_SIZE
	return : the number of bytes written in buffer dest
	or 0 if the compression fails

	LZ4_decompress_safe() :
	compressedSize : is obviously the source size
	maxDecompressedSize : is the size of the destination buffer, which must be already allocated.
	return : the number of bytes decompressed into the destination buffer (necessarily <= maxDecompressedSize)
	If the destination buffer is not large enough, decoding will stop and output an error code (<0).
	If the source stream is detected malformed, the function will stop decoding and return a negative result.
	This function is protected against buffer overflow exploits,
	and never writes outside of output buffer, nor reads outside of input buffer.
	It is also protected against malicious data packets.
	*/

	LZ4_API int		LZ4_compress(const char* source, char* dest, int sourceSize);
	LZ4_API int		LZ4_decompress_safe(const char* source, char* dest, int compressedSize, int maxDecompressedSize);



	/*
	LZ4_compressBound() :
	Provides the maximum size that LZ4 compression may output in a "worst case" scenario (input data not compressible)
	This function is primarily useful for memory allocation purposes (output buffer size).
	Macro LZ4_COMPRESSBOUND() is also provided for compilation-time evaluation (stack memory allocation for example).

	isize  : is the input size. Max supported value is LZ4_MAX_INPUT_SIZE
	return : maximum output size in a "worst case" scenario
	or 0, if input size is too large ( > LZ4_MAX_INPUT_SIZE)
	*/
	LZ4_API int LZ4_compressBound(int isize);


	/*
	LZ4_compress_limitedOutput() :
	Compress 'sourceSize' bytes from 'source' into an output buffer 'dest' of maximum size 'maxOutputSize'.
	If it cannot achieve it, compression will stop, and result of the function will be zero.
	This saves time and memory on detecting non-compressible (or barely compressible) data.
	This function never writes outside of provided output buffer.

	sourceSize  : Max supported value is LZ4_MAX_INPUT_VALUE
	maxOutputSize : is the size of the destination buffer (which must be already allocated)
	return : the number of bytes written in buffer 'dest'
	or 0 if compression fails
	*/
	LZ4_API int LZ4_compress_limitedOutput(const char* source, char* dest, int sourceSize, int maxOutputSize);

	/*
	LZ4_compress_withState() :
	Same compression functions, but using an externally allocated memory space to store compression state.
	Use LZ4_sizeofState() to know how much memory must be allocated,
	and then, provide it as 'void* state' to compression functions.
	*/
	LZ4_API int LZ4_sizeofState(void);
	LZ4_API int LZ4_compress_withState(void* state, const char* source, char* dest, int inputSize);
	LZ4_API int LZ4_compress_limitedOutput_withState(void* state, const char* source, char* dest, int inputSize, int maxOutputSize);


	/*
	LZ4_decompress_fast() :
	originalSize : is the original and therefore uncompressed size
	return : the number of bytes read from the source buffer (in other words, the compressed size)
	If the source stream is detected malformed, the function will stop decoding and return a negative result.
	Destination buffer must be already allocated. Its size must be a minimum of 'originalSize' bytes.
	note : This function fully respect memory boundaries for properly formed compressed data.
	It is a bit faster than LZ4_decompress_safe().
	However, it does not provide any protection against intentionally modified data stream (malicious input).
	Use this function in trusted environment only (data to decode comes from a trusted source).
	*/
	LZ4_API int LZ4_decompress_fast(const char* source, char* dest, int originalSize);


	/*
	LZ4_decompress_safe_partial() :
	This function decompress a compressed block of size 'compressedSize' at position 'source'
	into destination buffer 'dest' of size 'maxDecompressedSize'.
	The function tries to stop decompressing operation as soon as 'targetOutputSize' has been reached,
	reducing decompression time.
	return : the number of bytes decoded in the destination buffer (necessarily <= maxDecompressedSize)
	Note : this number can be < 'targetOutputSize' should the compressed block to decode be smaller.
	Always control how many bytes were decoded.
	If the source stream is detected malformed, the function will stop decoding and return a negative result.
	This function never writes outside of output buffer, and never reads outside of input buffer. It is therefore protected against malicious data packets
	*/
	LZ4_API int LZ4_decompress_safe_partial(const char* source, char* dest, int compressedSize, int targetOutputSize, int maxDecompressedSize);

#if defined (__cplusplus)
}
#endif
