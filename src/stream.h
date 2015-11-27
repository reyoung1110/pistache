/* stream.h
   Mathieu Stefani, 05 September 2015
   
   A set of classes to control input over a sequence of bytes
*/

#pragma once

#include <cstddef>
#include <stdexcept>
#include <cstring>
#include <streambuf>
#include <vector>
#include <limits>

static constexpr char CR = 0xD;
static constexpr char LF = 0xA;

template<typename CharT = char>
class StreamBuf : public std::basic_streambuf<CharT> {
public:
    typedef std::basic_streambuf<CharT> Base;
    typedef typename Base::traits_type traits_type;

    void setArea(char* begin, char *current, char *end) {
        this->setg(begin, current, end);
    }

    CharT *begptr() const {
        return this->eback();
    }

    CharT* curptr() const {
        return this->gptr();
    }

    CharT* endptr() const {
        return this->egptr();
    }

    size_t position() const {
        return this->gptr() - this->eback();
    }

    void reset() {
        this->setg(nullptr, nullptr, nullptr);
    }

    typename Base::int_type snext() const {
        if (this->gptr() == this->egptr()) {
            return traits_type::eof();
        }

        const CharT* gptr = this->gptr();
        return *(gptr + 1);
    }

};

template<typename CharT = char>
class RawStreamBuf : public StreamBuf<CharT> {
public:
    typedef StreamBuf<CharT> Base;

    RawStreamBuf(char* begin, char* end) {
        Base::setg(begin, begin, end);
    }
    RawStreamBuf(char* begin, size_t len) {
        Base::setg(begin, begin, begin + len);
    }

};

template<size_t N, typename CharT = char>
class ArrayStreamBuf : public StreamBuf<CharT> {
public:
    typedef StreamBuf<CharT> Base;

    ArrayStreamBuf()
      : size(0)
    {
        memset(bytes, 0, N);
        Base::setg(bytes, bytes, bytes);
    }

    template<size_t M>
    ArrayStreamBuf(char (&arr)[M]) {
        static_assert(M <= N, "Source array exceeds maximum capacity");
        memcpy(bytes, arr, M);
        size = M;
        Base::setg(bytes, bytes, bytes + M);
    }

    bool feed(const char* data, size_t len) {
        if (size + len >= N) {
            return false;
        }

        memcpy(bytes + size, data, len);
        Base::setg(bytes, bytes + size, bytes + size + len);
        size += len;
        return true;
    }

    void reset() {
        memset(bytes, 0, N);
        size = 0;
        Base::setg(bytes, bytes, bytes);
    }

private:
    char bytes[N];
    size_t size;
};

class NetworkStream : public StreamBuf<char> {
public:
    struct Buffer {
        Buffer(const void *data, size_t len)
            : data(data)
            , len(len)
        { }

        const void* data;
        const size_t len;
    };

    typedef StreamBuf<char> Base;
    typedef typename Base::traits_type traits_type;
    typedef typename Base::int_type int_type;

    NetworkStream(
            size_t size,
            size_t maxSize = std::numeric_limits<uint32_t>::max())
        : maxSize_(maxSize)
    {
        reserve(size);
    }

    NetworkStream(const NetworkStream& other) = delete;
    NetworkStream& operator=(const NetworkStream& other) = delete;

    NetworkStream(NetworkStream&& other)
       : maxSize_(other.maxSize_)
       , data_(std::move(other.data_)) {
           setp(other.pptr(), other.epptr());
           other.setp(nullptr, nullptr);
    }

    NetworkStream& operator=(NetworkStream&& other) {
        maxSize_ = other.maxSize_;
        data_ = std::move(other.data_);
        setp(other.pptr(), other.epptr());
        other.setp(nullptr, nullptr);
        return *this;
    }

    Buffer buffer() const {
        return Buffer(data_.data(), pptr() - &data_[0]);
    }

protected:
    int_type overflow(int_type ch);

private:
    void reserve(size_t size);

    size_t maxSize_;
    std::vector<char> data_;
};

class StreamCursor {
public:
    StreamCursor(StreamBuf<char>* buf, size_t initialPos = 0)
        : buf(buf)
    {
        advance(initialPos);
    }

    static constexpr int Eof = -1;

    struct Token {
        Token(StreamCursor& cursor)
            : cursor(cursor)
            , position(cursor.buf->position())
            , eback(cursor.buf->begptr())
            , gptr(cursor.buf->curptr())
            , egptr(cursor.buf->endptr())
        { }

        size_t start() const { return position; }

        size_t end() const {
            return cursor.buf->position();
        }

        size_t size() const {
            return end() - start();
        }

        std::string text() {
            return std::string(gptr, size());
        }

        const char* rawText() const {
            return gptr;
        }

    private:
        StreamCursor& cursor;
        size_t position;
        char *eback;
        char *gptr;
        char *egptr;
    };

    struct Revert {
        Revert(StreamCursor& cursor)
            : cursor(cursor)
            , eback(cursor.buf->begptr())
            , gptr(cursor.buf->curptr())
            , egptr(cursor.buf->endptr())
            , active(true)
        { }

        ~Revert() {
            if (active)
                revert();
        }

        void revert() {
            cursor.buf->setArea(eback, gptr, egptr);
        }

        void ignore() {
            active = false;
        }

    private:
        StreamCursor& cursor;
        char *eback;
        char *gptr;
        char *egptr;
        bool active;

    };

    bool advance(size_t count);
    operator size_t() const { return buf->position(); }

    bool eol() const;
    bool eof() const;
    int next() const;
    char current() const;

    const char* offset() const;
    const char* offset(size_t off) const;

    size_t diff(size_t other) const;
    size_t diff(const StreamCursor& other) const;

    size_t remaining() const;

    void reset();

public:
    StreamBuf<char>* buf;
};


enum class CaseSensitivity {
    Sensitive, Insensitive
};

bool match_raw(const void* buf, size_t len, StreamCursor& cursor);
bool match_literal(char c, StreamCursor& cursor, CaseSensitivity cs = CaseSensitivity::Insensitive);
bool match_until(char c, StreamCursor& cursor, CaseSensitivity cs = CaseSensitivity::Insensitive);
bool match_until(std::initializer_list<char> chars, StreamCursor& cursor, CaseSensitivity cs = CaseSensitivity::Insensitive);
bool match_double(double* val, StreamCursor& cursor);
