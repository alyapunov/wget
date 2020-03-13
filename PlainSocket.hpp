/*
 * Copyright (c) 2020, Aleksandr Lyapunov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#pragma once

#include <IOVec.hpp>
#include <SocketBase.hpp>

// Read buffered TPC socket.
// Uses external (provided upon construction) buffer for buffering.
// That buffer is called 'cache' to distinguish it from 'buffer' arguments
// in recv* methods.
// There's no send buffering, instead a group of strings can be sent at once.
class PlainSocket : private SocketBase
{
public:
    template <size_t N>
    PlainSocket(char (&aCache)[N],
                const char* aAddress, const char* aPort, unsigned long aUsecTimeout = 0);
    PlainSocket(char* aCache, size_t aCacheSize,
                const char* aAddress, const char* aPort, unsigned long aUsecTimeout = 0);

    // Send expects strings, vectors, pointers followed by size etc (see OVec ctor).
    // Send all or throw.
    template <class ...ARGS>
    size_t sendOrDie(ARGS&&... aArgs);
    size_t sendOrDie(OVec* aOVec, size_t aCount);

    // Receive exactly one byte.
    char recvOrDie();
    // Recv expects vectors, arrays, pointers followed by size etc (see IVec ctor).
    // Receive all or throw.
    template <class ...ARGS>
    size_t recvOrDie(ARGS&&... aArgs);

    // Recv at least aMinSize bytes. One can specify aMinSize larger that
    // size of all given buffers, that means that extra data must be cached.
    // Args list may be empty, that means read only to cache.
    // There is a guarantee the cache buffer will be filled continuously (in
    // terms of cycle buffer) and from the beginning if it was empty.
    // If aShutDownError is true then peer shutdown causes error,
    // otherwise peer shutdown causes setting aShutDownError ti true.
    template <class ...ARGS>
    size_t recvSome(size_t aMinSize, bool& aShutDownError, ARGS&&... aArgs);

    // Begin and end end position in the internal buffer that was
    // used as a cache in previous recv call.
    size_t cachedBegPos() const { return m_CachedBegPos; }
    size_t cachedEndPos() const { return m_CachedEndPos; }
    // Discard some data from the internal buffer.
    void dropCache(size_t aSize);

    // Reset buffer and prepare to read from another socket.
    void exchange(SocketBase&& a);

private:
    // Inline part that tries to read from cache and calls recvImplSys if necessary.
    // The last two OVec must be the cache! Even if the cache in not splitted into
    // two parts, you have to pass one zero-size part!
    size_t recvImpl(IVec* aOVec, size_t aCount,
                    ssize_t sMinCount, ssize_t sMinSize, bool& aShutDownError);
    // Extern part that reads from socket.
    size_t recvImplSys(IVec* aOVec, ssize_t aCount,
                       ssize_t sMinCount, ssize_t sMinSize, bool& aShutDownError);

    char* const m_Cache;
    const size_t m_CacheSize;
    size_t m_CachedBegPos = 0; // Always less than m_CacheSize. Zero if nothing cached.
    size_t m_CachedEndPos = 0; // Can be less than m_CachedBegPos, cache is cycled.
};

template <size_t N>
inline PlainSocket::PlainSocket(char (&aCache)[N], const char* aAddress,
                                const char* aPort, unsigned long aUsecTimeout)
: PlainSocket(aCache, N, aAddress, aPort, aUsecTimeout)
{
}

template <class ...ARGS>
inline size_t PlainSocket::sendOrDie(ARGS&&... aArgs)
{
    auto sOVecs = makeOVec(std::forward<ARGS>(aArgs)...);
    return sendOrDie(sOVecs.data(), sOVecs.size());
}

inline char PlainSocket::recvOrDie()
{
    if (m_CachedBegPos != m_CachedEndPos)
    {
        char c = m_Cache[m_CachedBegPos++];
        if (m_CachedBegPos == m_CachedEndPos)
            m_CachedBegPos = m_CachedEndPos = 0;
        else if (m_CachedBegPos == m_CacheSize)
            m_CachedBegPos = 0;
        return c;
    }
    char c;
    auto sIVecs = makeIVec(&c, 1, m_Cache, m_CacheSize, m_Cache, 0);
    bool sShutDownError = true;
    recvImpl(sIVecs.data(), sIVecs.size(), 0, 1, sShutDownError);
    return c;
}

template <class ...ARGS>
inline size_t PlainSocket::recvOrDie(ARGS&&...aArgs)
{
    auto sIVecs = makeIVec(std::forward<ARGS>(aArgs)..., m_Cache, m_CacheSize, m_Cache, 0);
    bool sShutDownError = true;
    return recvImpl(sIVecs.data(), sIVecs.size(), sIVecs.size() - 2, 0, sShutDownError);
}

template <class ...ARGS>
inline size_t PlainSocket::recvSome(size_t aMinSize, bool& aShutDownError, ARGS&&... aArgs)
{
    auto sIVecs = makeIVec(std::forward<ARGS>(aArgs)..., m_Cache, m_CacheSize, m_Cache, 0);
    return recvImpl(sIVecs.data(), sIVecs.size(), 0, aMinSize, aShutDownError);
}

inline size_t PlainSocket::recvImpl(IVec* aIVec, size_t aCount,
                                    ssize_t sMinCount, ssize_t sMinSize, bool& aShutDownError)
{
    ssize_t sTotalRecvdSize = 0;
    ssize_t sCurIVec = 0;
    // The last two OVecs are reserved for cache vectors.
    ssize_t sCacheIVec = aCount - 2;

    auto sTakeCache = [&](size_t& aFrom, size_t aTo) -> bool
    {
        while (sCurIVec != sCacheIVec)
        {
            sTotalRecvdSize += aIVec[sCurIVec].take(m_Cache, aFrom, aTo);
            if (!aIVec[sCurIVec].empty())
                break;
            ++sCurIVec;
        }
        return aFrom == aTo;
    };

    // if m_CachedBegPos > m_CachedEndPos then the cache buffer is cycled.
    if (m_CachedBegPos > m_CachedEndPos && sTakeCache(m_CachedBegPos, m_CacheSize))
        m_CachedBegPos = 0;
    if (m_CachedBegPos < m_CachedEndPos && sTakeCache(m_CachedBegPos, m_CachedEndPos))
        m_CachedBegPos = m_CachedEndPos = 0;

    if (sTotalRecvdSize >0 && sCurIVec >= sMinCount && sTotalRecvdSize >= sMinSize)
        return sTotalRecvdSize;

    return sTotalRecvdSize +
        recvImplSys(aIVec + sCurIVec, aCount - sCurIVec,
                    sMinCount - sCurIVec, sMinSize - sTotalRecvdSize, aShutDownError);
}