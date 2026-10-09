// ----------------------------------------------------------------------------
// MIT License
//
// Copyright (c) 2026 HTMonkeyG
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// ----------------------------------------------------------------------------
//
// Imported from https://github.com/HTMonkeyG/leveldb-mcne, which adds support
// for the obfuscated NetEase ("mcne") Minecraft world databases.
//
// The obfuscation is a repeating-key XOR, not encryption. Its purpose is to be
// byte compatible with the game, not to protect anything: the default key is a
// published constant and the scheme is trivially reversible.

#ifndef STORAGE_LEVELDB_INCLUDE_MCNE_H_
#define STORAGE_LEVELDB_INCLUDE_MCNE_H_

#include <string>

#include "leveldb/env.h"
#include "leveldb/export.h"
#include "leveldb/options.h"

namespace leveldb {

    // XOR encrypted file.
    class LEVELDB_EXPORT McneSequentialFile : public SequentialFile {
       public:
        explicit McneSequentialFile(SequentialFile* pFile, const Slice& key);

        virtual ~McneSequentialFile() override { delete pFile; }

        virtual Status Read(size_t n, Slice* result, char* scratch) override;

        virtual Status Skip(uint64_t n) override;

        SequentialFile* target() const { return pFile; }

       private:
        // No copy allowed.
        McneSequentialFile(const McneSequentialFile&);
        void operator=(const McneSequentialFile&);

        SequentialFile* pFile;
        // Owned copy of the key. Holding a Slice instead would dangle: Slice does not
        // own its bytes, and the caller's are gone by the time a read happens.
        const std::string key;
        size_t offset;
        bool isEncrypted;
    };

    // XOR encrypted file.
    class LEVELDB_EXPORT McneRandomAccessFile : public RandomAccessFile {
       public:
        explicit McneRandomAccessFile(RandomAccessFile* pFile, const Slice& key);

        virtual ~McneRandomAccessFile() override { delete pFile; }

        virtual Status Read(uint64_t offset, size_t n, Slice* result, char* scratch) const override;

        RandomAccessFile* target() const { return pFile; }

       private:
        // No copy allowed.
        McneRandomAccessFile(const McneRandomAccessFile&);
        void operator=(const McneRandomAccessFile&);

        RandomAccessFile* pFile;
        const std::string key;
        bool isEncrypted;
    };

    // XOR encrypted file.
    class LEVELDB_EXPORT McneWritableFile : public WritableFile {
       public:
        explicit McneWritableFile(WritableFile* pFile, const Slice& key);

        virtual ~McneWritableFile() override { delete pFile; }

        virtual Status Append(const Slice& data) override;
        virtual Status Close() override { return pFile->Close(); }
        virtual Status Flush() override { return pFile->Flush(); }
        virtual Status Sync() override { return pFile->Sync(); }

        WritableFile* target() const { return pFile; }

       private:
        // No copy allowed.
        McneWritableFile(const McneWritableFile&);
        void operator=(const McneWritableFile&);

        WritableFile* pFile;
        const std::string key;
        size_t offset;
        bool isEncrypted;
        // Reused so an append does not allocate a copy of the caller's data.
        std::string buffer;
    };

    // XOR encrypted database.
    //
    // The wrapped Env is NOT owned: EnvWrapper does not take ownership of its
    // target either, and the documented use is to wrap Env::Default(), which must
    // never be deleted. The caller keeps ownership of whatever it passes in.
    class LEVELDB_EXPORT McneWrapper : public EnvWrapper {
       public:
        enum {
            // 80 1D 30 01, in big endian.
            // We don't accept 90 1D 30 01 (for AES-128).
            kMagicNum = 0x01301D80,
            kMagicNumSize = 4
        };

        // Create a McneWrapper Env. If a zero-length Slice is passed, the database
        // will be considered as unencrypted.
        // The key is copied, so it only has to be valid for the duration of this call.
        explicit McneWrapper(Env* pEnv, const Slice& key = "88329851");

        // Create a McneWrapper with default Env.
        explicit McneWrapper(const Slice& key = "88329851");

        ~McneWrapper() override = default;

        Status NewSequentialFile(const std::string& fname, SequentialFile** result) override;

        Status NewRandomAccessFile(const std::string& fname, RandomAccessFile** result) override;

        Status NewWritableFile(const std::string& fname, WritableFile** result) override;

        Status NewAppendableFile(const std::string& fname, WritableFile** result) override;

       private:
        // No copy allowed.
        McneWrapper(const McneWrapper&);
        void operator=(const McneWrapper&);

        const std::string key;
    };

    // Automatically infer the encryption key from the database. The result is
    // stored in *key. Original contents of *key are dropped. This function won't
    // change the files.
    //
    // If the database is unencrypted, a zero-lengthed string is stored. If the
    // database is corrupted, a Status::Corrupted is returned.
    //
    // Env in options cannot be McneWrapper, so that the function can obtain the
    // original data.
    //
    // NOT IMPLEMENTED YET: upstream declares this but does not define it, so
    // calling it fails to link. See leveldb_mcne.cc in the upstream repository.
    extern LEVELDB_EXPORT Status InferDB(const std::string& dbname, const Options& options, std::string* key);

    // Encrypt (or decrypt, decided on the database) the database with the given
    // key. Every files of the database except log files will be circularly XOR
    // encrypted with the given key.
    //
    // NOT IMPLEMENTED YET, as above.
    extern LEVELDB_EXPORT Status EncryptDB(const std::string& dbname, const Options& options, const Slice& key);

    // namespace leveldb
}  // namespace leveldb

#endif
