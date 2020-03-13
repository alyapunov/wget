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
#include <PlainSocket.hpp>

#include <assert.h>
#include <limits.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <algorithm>

#include <NetException.hpp>

PlainSocket::PlainSocket(char* aCache, size_t aCacheSize,
                         const char* aAddress, const char* aPort, unsigned long aUsecTimeout)
: SocketBase(aAddress, aPort, aUsecTimeout), m_Cache(aCache), m_CacheSize(aCacheSize)
{
}

size_t PlainSocket::sendOrDie(OVec* aOVec, size_t aCount)
{
    size_t sTotalSentSize = 0;
    while (true)
    {
        // Skip empty vectors if they was given.
        while (aOVec->iov_len == 0 && aCount > 0)
        {
            ++aOVec;
            --aCount;
        }
        if (aCount == 0)
            break;

        // Prepare sendmsg arguments.
        struct msghdr hdr{};
        hdr.msg_iov = aOVec;
        int flags = MSG_NOSIGNAL;
        if (aCount <= IOV_MAX)
        {
            hdr.msg_iovlen = aCount;
        }
        else
        {
            hdr.msg_iovlen = IOV_MAX;
            flags |= MSG_MORE;
        }

        // sendmsg.
        ssize_t r;
        do
        {
            r = sendmsg(m_Fd, &hdr, flags);
        } while (r < 0 && errno == EINTR);
        if (r <= 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                throw NetException("send failed", "timeout exceeded");
            else
                throw NetException("send failed", errno);
        }

        // remove sent bytes from vec.
        size_t sSent = r;
        sTotalSentSize += sSent;
        while (true)
        {
            if (!aOVec->skip(sSent))
                break;
            ++aOVec;
            --aCount;
            if (sSent > 0 && aCount == 0)
                throw NetException("can't be", "'send' returned more than was asked to send");
        }
    }
    return sTotalSentSize;
}

size_t PlainSocket::recvImplSys(IVec* aIVec, ssize_t aCount,
                                ssize_t aMinCount, ssize_t aMinSize, bool& aShutDownError)
{
    ssize_t sTotalRecvdSize = 0;
    ssize_t sTotalRecvdAndCachedSize = 0;
    ssize_t sCurIVec = 0;
    // The last two OVecs are reserved for cache vectors.
    assert(aCount >= 2);
    ssize_t sCacheIVec = aCount - 2;
    aCount -= 2;
    // Set up cache iovecs. If the cache is not empty (there is cached data):
    // 1) that means that there are no non-full user provided buffers.
    // 2) cache empty space can consist of two iovecs.
    assert(m_CachedBegPos < m_CacheSize);
    assert(m_CachedBegPos != m_CachedEndPos || m_CachedBegPos == 0);
    if (m_CachedEndPos != m_CacheSize)
    {
        aIVec[aCount  ].iov_base = m_Cache + m_CachedEndPos;
        aIVec[aCount++].iov_len = m_CacheSize - m_CachedEndPos;
    }
    if (m_CachedBegPos != 0)
    {
        aIVec[aCount  ].iov_base = m_Cache;
        aIVec[aCount++].iov_len = m_CachedBegPos;
    }

    while (true)
    {
        // Skip empty vectors if they was given.
        while (aIVec[sCurIVec].iov_len == 0 && sCurIVec < sCacheIVec)
            ++sCurIVec;
        if (sCurIVec == aCount)
            break;

        // Prepare recvmsg arguments.
        struct msghdr hdr{};
        hdr.msg_iov = aIVec + sCurIVec;
        hdr.msg_iovlen = std::min(size_t(aCount - sCurIVec), size_t(IOV_MAX));
        int flags = 0;

        // recvmsg.
        ssize_t r;
        do
        {
            r = recvmsg(m_Fd, &hdr, flags);
        } while (r < 0 && errno == EINTR);
        if (r <= 0)
        {
            if (r == 0)
            {
                if (aShutDownError)
                {
                    throw NetException("recv failed", "peer was closed");
                }
                else
                {
                    aShutDownError = true;
                    break;
                }
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                throw NetException("recv failed", "timeout exceeded");
            else
                throw NetException("recv failed", errno);
        }

        // remove sent bytes from vec.
        size_t sRecvd = r;
        if (sCurIVec < sCacheIVec) // Don't take into account reads to cache
            sTotalRecvdSize += sRecvd;
        sTotalRecvdAndCachedSize += sRecvd;
        do
        {
            if (!aIVec[sCurIVec].skip(sRecvd))
                break;
            ++sCurIVec;
            if (sCurIVec == sCacheIVec)
                sTotalRecvdSize -= sRecvd; // Don't take into account reads to cache
        } while (sRecvd > 0);
        if (sRecvd > 0 && sCurIVec == aCount)
            throw NetException("recv failed", "not enough cache for requested operation");
        if (sCurIVec >= aMinCount && sTotalRecvdAndCachedSize >= aMinSize)
            break;
    }
    return sTotalRecvdSize;
}
