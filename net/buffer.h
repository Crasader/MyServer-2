#pragma once

#include <algorithm>
#include <vector>
#include <string>
#include <assert.h>
#include <string.h>

#include "endian.h"

namespace net{
	class Buffer
	{
	public:
		static const size_t kCheapPrepend = 8;
		static const size_t kInitialSize = 1024;
		explicit Buffer(size_t initialSize = kInitialSize)   //为buffer默认分配1032个字节的位置
			:buffer_(kCheapPrepend + initialSize),
			readerIndex_(kCheapPrepend),
			writerIndex_(kCheapPrepend)
		{
			assert(readableBytes() == 0);
			assert(writableBytes() == initialSize);
			assert(prependableBytes() == kCheapPrepend);
		}
		//~Buffer();

		void swap(Buffer& rhs)
		{
			buffer_.swap(rhs.buffer_);
			std::swap(readerIndex_, rhs.readerIndex_);
			std::swap(writerIndex_, rhs.writerIndex_);
		}
		size_t readableBytes() const
		{
			return writerIndex_ - readerIndex_;
		}

		size_t writableBytes() const
		{
			return buffer_.size() - writerIndex_;
		}

		size_t prependableBytes() const
		{
			return readerIndex_;
		}

		const char* peek() const
		{
			//返回指向readindex_位置的指针
			return begin() + readerIndex_;
		}

		const char* findCRLF() const
		{
			// FIXME: replace with memmem()?
			const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF + 2);
			return crlf == beginWrite() ? NULL : crlf;
		}

		const char* findCRLF(const char* start) const
		{
			assert(peek() <= start);
			assert(start <= beginWrite());
			// FIXME: replace with memmem()?
			const char* crlf = std::search(start, beginWrite(), kCRLF, kCRLF + 2);
			return crlf == beginWrite() ? NULL : crlf;
		}

		const char* findEOL() const
		{
			const void* eol = memchr(peek(), '\n', readableBytes());
			return static_cast<const char*>(eol);
		}

		const char* findEOL(const char* start) const
		{
			assert(peek() <= start);
			assert(start <= beginWrite());
			const void* eol = memchr(start, '\n', beginWrite() - start);
			return static_cast<const char*>(eol);
		}
		
		//取回
		void retrieve(size_t len)
		{
			assert(len <= readableBytes());
			if (len < readableBytes())
			{
				readerIndex_ += len;
			}
			else
			{
				retrieveAll();
			}
		}

		void retrieveUntil(const char* end)
		{
			assert(peek() <= end);
			assert(end <= beginWrite());
			retrieve(end - peek());
		}

		void retrieveInt64()
		{
			retrieve(sizeof(int64_t));
		}

		void retrieveInt32()
		{
			retrieve(sizeof(int32_t));
		}

		void retrieveInt16()
		{
			retrieve(sizeof(int16_t));
		}

		void retrieveInt8()
		{
			retrieve(sizeof(int8_t));
		}

		void retrieveAll()
		{
			readerIndex_ = kCheapPrepend;
			writerIndex_ = kCheapPrepend;
		}
		//以字符串形式取回所有数据
		std::string retrieveAllAsString()
		{
			return retrieveAsString(readableBytes());;
		}

		std::string retrieveAsString(size_t len)
		{
			assert(len <= readableBytes());
			std::string result(peek(), len);
			retrieve(len);
			return result;
		}

		std::string toStringPiece() const
		{
			return std::string(peek(), static_cast<int>(readableBytes()));
		}

		void append(const std::string& str)
		{
			append(str.c_str(), str.size());
		}

		void append(const char* /*restrict*/ data, size_t len)
		{
			//其实相当于把已有数据往前挪动
			ensureWritableBytes(len);
			std::copy(data, data + len, beginWrite());
			hasWritten(len);
		}


		void append(const void* /*restrict*/ data, size_t len)
		{
			append(static_cast<const char*>(data), len);
		}

		void ensureWritableBytes(size_t len)
		{
			//剩下的可写空间如果小于需要的空间len，则增加len长度个空间
			if (writableBytes() < len)
			{
				makeSpace(len);
			}
			assert(writableBytes() >= len);
		}
		char* beginWrite()
		{
			return begin() + writerIndex_;
		}

		const char* beginWrite() const
		{
			return begin() + writerIndex_;
		}

		void hasWritten(size_t len)
		{
			assert(len <= writableBytes());
			writerIndex_ += len;
		}

		void unwrite(size_t len)
		{
			assert(len <= readableBytes());
			writerIndex_ -= len;
		}

		///
		/// Append int64_t using network endian
		///
		void appendInt64(int64_t x)
		{
			int64_t be64 = sockets::hostToNetwork64(x);
			append(&be64, sizeof be64);
		}

		///
		/// Append int32_t using network endian
		///
		void appendInt32(int32_t x)
		{
			int32_t be32 = sockets::hostToNetwork32(x);
			append(&be32, sizeof be32);
		}

		void appendInt16(int16_t x)
		{
			int16_t be16 = sockets::hostToNetwork16(x);
			append(&be16, sizeof be16);
		}

		void appendInt8(int8_t x)
		{
			append(&x, sizeof x);
		}

		///
		/// Read int64_t from network endian
		///
		/// Require: buf->readableBytes() >= sizeof(int32_t)
		int64_t readInt64()
		{
			int64_t result = peekInt64();
			retrieveInt64();
			return result;
		}

		///
		/// Read int32_t from network endian
		///
		/// Require: buf->readableBytes() >= sizeof(int32_t)
		int32_t readInt32()
		{
			int32_t result = peekInt32();
			retrieveInt32();
			return result;
		}

		int16_t readInt16()
		{
			int16_t result = peekInt16();
			retrieveInt16();
			return result;
		}

		int8_t readInt8()
		{
			int8_t result = peekInt8();
			retrieveInt8();
			return result;
		}

		///
		/// Peek int64_t from network endian
		///
		/// Require: buf->readableBytes() >= sizeof(int64_t)
		int64_t peekInt64() const
		{
			assert(readableBytes() >= sizeof(int64_t));
			int64_t be64 = 0;
			::memcpy(&be64, peek(), sizeof be64);
			return sockets::networkTohost64(be64);
		}

		///
		/// Peek int32_t from network endian
		///
		/// Require: buf->readableBytes() >= sizeof(int32_t)
		int32_t peekInt32() const
		{
			assert(readableBytes() >= sizeof(int32_t));
			int32_t be32 = 0;
			::memcpy(&be32, peek(), sizeof be32);
			return sockets::networkTohost32(be32);
		}

		int16_t peekInt16() const
		{
			assert(readableBytes() >= sizeof(int16_t));
			int16_t be16 = 0;
			::memcpy(&be16, peek(), sizeof be16);
			return sockets::networkTohost16(be16);
		}

		int8_t peekInt8() const
		{
			assert(readableBytes() >= sizeof(int8_t));
			int8_t x = *peek();
			return x;
		}

		///
		/// Prepend int64_t using network endian
		///
		void prependInt64(int64_t x)
		{
			int64_t be64 = sockets::hostToNetwork64(x);
			prepend(&be64, sizeof be64);
		}

		///
		/// Prepend int32_t using network endian
		///
		void prependInt32(int32_t x)
		{
			int32_t be32 = sockets::hostToNetwork32(x);
			prepend(&be32, sizeof be32);
		}

		void prependInt16(int16_t x)
		{
			int16_t be16 = sockets::hostToNetwork16(x);
			prepend(&be16, sizeof be16);
		}

		void prependInt8(int8_t x)
		{
			prepend(&x, sizeof x);
		}

		void prepend(const void* /*restrict*/ data, size_t len)
		{
			assert(len <= prependableBytes());
			readerIndex_ -= len;
			const char* d = static_cast<const char*>(data);
			std::copy(d, d + len, begin() + readerIndex_);
		}

		void shrink(size_t reserve)
		{
			// FIXME: use vector::shrink_to_fit() in C++ 11 if possible.
			Buffer other;
			other.ensureWritableBytes(readableBytes() + reserve);
			other.append(toStringPiece());
			swap(other);
		}

		size_t internalCapacity() const
		{
			return buffer_.capacity();
		}

		/// Read data directly into buffer.
		///
		/// It may implement with readv(2)
		/// @return result of read(2), @c errno is saved
		ssize_t readFd(int fd, int* savedErrno);


	private:
		//buffer_.begin()代表迭代器，加*号表示值，取地址是让指针指向这个位置向后推移
		char* begin()
		{
			return &*buffer_.begin();
		}

		const char* begin() const
		{
			return &*buffer_.begin();
		}

		void makeSpace(size_t len)
		{
			//kCheapPrepend为保留的空间
			if (writableBytes() + prependableBytes() < len + kCheapPrepend)
			{
				// FIXME: move readable data
				buffer_.resize(writerIndex_ + len);
			}
			else
			{
				// move readable data to the front, make space inside buffer
				assert(kCheapPrepend < readerIndex_);
				size_t readable = readableBytes();
				std::copy(begin() + readerIndex_,
					begin() + writerIndex_,
					begin() + kCheapPrepend);
				readerIndex_ = kCheapPrepend;
				writerIndex_ = readerIndex_ + readable;
				assert(readable == readableBytes());
			}
		}

	private:
		std::vector<char> buffer_;
		size_t readerIndex_;
		size_t writerIndex_;

		static const char kCRLF[];
	};

}