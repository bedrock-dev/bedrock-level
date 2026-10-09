#ifndef LEVELDB_MCPE_INCLUDE_ZLIB_COMPRESSOR_H_
#define LEVELDB_MCPE_INCLUDE_ZLIB_COMPRESSOR_H_

#include "leveldb/compressor.h"

namespace leveldb {

	class LEVELDB_EXPORT ZlibCompressorBase : public Compressor 
	{
	public:
		int inflate(const char* input, size_t length, ::std::string &output) const;

		const int compressionLevel;
		const bool raw;

		// Run compression and decompression through libdeflate instead of zlib.
		// Both libraries read and write the same deflate/zlib streams, so this
		// only picks who does the work: the block format, the serializer ids and
		// the on-disk data are the same either way. Set it to false to force the
		// zlib path. Ignored when the build has no libdeflate, in which case zlib
		// is always used. Intended to be set before the database is opened.
		bool useLibdeflate = true;
        
        virtual ~ZlibCompressorBase() = default;

		ZlibCompressorBase(char uniqueCompressionID, int compressionLevel, bool raw) :
			Compressor(uniqueCompressionID),
			compressionLevel(compressionLevel),
			raw(raw)
		{
			assert(compressionLevel >= -1 && compressionLevel <= 9);
		}

		virtual void compressImpl(const char* input, size_t length, ::std::string& output) const override;

		virtual bool decompress(const char* input, size_t length, ::std::string &output) const override;

	private:

		int _window() const;

	};

	class LEVELDB_EXPORT ZlibCompressor : public ZlibCompressorBase {
	public:
		static const int SERIALIZE_ID = 2;

		ZlibCompressor(int compressionLevel = -1) :
			ZlibCompressorBase(SERIALIZE_ID, compressionLevel, false) {

		}
	};

	class LEVELDB_EXPORT ZlibCompressorRaw : public ZlibCompressorBase {
	public:
		static const int SERIALIZE_ID = 4;

		ZlibCompressorRaw(int compressionLevel = -1) :
			ZlibCompressorBase(SERIALIZE_ID, compressionLevel, true) {

		}
	};
}

#endif
