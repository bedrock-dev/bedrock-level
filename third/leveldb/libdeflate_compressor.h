#ifndef LEVELDB_MCPE_INCLUDE_LIBDEFLATE_COMPRESSOR_H_
#define LEVELDB_MCPE_INCLUDE_LIBDEFLATE_COMPRESSOR_H_

#include "leveldb/compressor.h"

namespace leveldb {

	// DEFLATE implemented by libdeflate instead of zlib.  The streams it reads
	// and writes are plain deflate/zlib, byte for byte the same format the
	// ZlibCompressor classes use, so the two are interchangeable: the serializer
	// ids are shared and existing files stay readable.  Only the codec differs,
	// and libdeflate is several times faster at both directions.
	//
	// Requires HAVE_LIBDEFLATE (see the HAVE_LIBDEFLATE check in CMakeLists.txt);
	// the class is unavailable when the library was not detected.
	class LEVELDB_EXPORT LibdeflateCompressorBase : public Compressor
	{
	public:
		// zlib-like scale with a higher ceiling: 1 = fastest, 6 = libdeflate's
		// default, 12 = slowest.  0 emits stored blocks only.
		const int compressionLevel;
		const bool raw;

		virtual ~LibdeflateCompressorBase() = default;

		LibdeflateCompressorBase(char uniqueCompressionID, int compressionLevel, bool raw) :
			Compressor(uniqueCompressionID),
			compressionLevel(compressionLevel),
			raw(raw)
		{
			assert(compressionLevel >= 0 && compressionLevel <= 12);
		}

		virtual void compressImpl(const char* input, size_t length, ::std::string& output) const override;

		virtual bool decompress(const char* input, size_t length, ::std::string &output) const override;
	};

	class LEVELDB_EXPORT LibdeflateCompressor : public LibdeflateCompressorBase {
	public:
		static const int SERIALIZE_ID = 2;  // Same stream as ZlibCompressor

		LibdeflateCompressor(int compressionLevel = 6) :
			LibdeflateCompressorBase(SERIALIZE_ID, compressionLevel, false) {
		}
	};

	class LEVELDB_EXPORT LibdeflateCompressorRaw : public LibdeflateCompressorBase {
	public:
		static const int SERIALIZE_ID = 4;  // Same stream as ZlibCompressorRaw

		LibdeflateCompressorRaw(int compressionLevel = 6) :
			LibdeflateCompressorBase(SERIALIZE_ID, compressionLevel, true) {
		}
	};
}

#endif
