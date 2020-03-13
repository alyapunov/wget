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
#include <SocketBase.hpp>

#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iterator>

#include <NetException.hpp>

namespace {

class AddrInfo
{
public:
    AddrInfo(const char* aAddress, const char* aPort)
    {
        struct addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        int rc = getaddrinfo(aAddress, aPort, &hints, &m_Info);
        if (rc != 0)
            throw NetException("getaddrinfo failed", gai_strerror(rc));
    }

    ~AddrInfo() noexcept
    {
        freeaddrinfo(m_Info);
    }

    class iterator : std::iterator<std::input_iterator_tag, const struct addrinfo>
    {
    private:
        using T = const struct addrinfo;
    public:
        T& operator*() const { return *m_Info; }
        T* operator->() const { return m_Info; }
        bool operator==(const iterator& aItr) { return m_Info == aItr.m_Info; }
        bool operator!=(const iterator& aItr) { return m_Info != aItr.m_Info; }
        iterator& operator++() { m_Info = m_Info->ai_next; return *this; }
        iterator operator++(int) { iterator sTmp = *this; m_Info = m_Info->ai_next; return sTmp; }

    private:
        friend class AddrInfo;
        explicit iterator(const struct addrinfo* aInfo) : m_Info(aInfo) {}
        T *m_Info;
    };

    iterator begin() const { return iterator(m_Info); }
    iterator end() const { return iterator(nullptr); }

private:
    struct addrinfo* m_Info;
};

enum fail_state_t
{
    SOCKET_FAILED,
    CONNECT_FAILED,
    SNDTIMEO_FAILED,
    RCVTIMEO_FAILED,
    WELL_DONE
};

const char* fail_msg[WELL_DONE] =
    {
        "socket failed",
        "connect failed",
        "setsockopt SO_SNDTIMEO failed",
        "setsockopt RCVTIMEO_FAILED failed"
    };

} // anonymous namespace

SocketBase::SocketBase(const char* aAddress, const char* aPort, size_t aUsecTimeout)
{
    size_t sCount = 0;
    fail_state_t fail_state{};
    for (auto sInfo : AddrInfo(aAddress, aPort))
    {
        ++sCount;
        m_Fd = socket(sInfo.ai_family, sInfo.ai_socktype, sInfo.ai_protocol);
        if (m_Fd < 0)
        {
            fail_state = SOCKET_FAILED;
            continue;
        }
        if (0 != aUsecTimeout)
        {
            struct timeval tv;
            tv.tv_sec = aUsecTimeout / 1000000;
            tv.tv_usec = aUsecTimeout % 1000000;
            int rc = setsockopt(m_Fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
            if (0 != rc)
            {
                close(m_Fd);
                fail_state = SNDTIMEO_FAILED;
                continue;
            }
            rc = setsockopt(m_Fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            if (0 != rc)
            {
                close(m_Fd);
                fail_state = RCVTIMEO_FAILED;
                continue;
            }
        }
        if (0 != connect(m_Fd,sInfo.ai_addr, sInfo.ai_addrlen))
        {
            close(m_Fd);
            fail_state = CONNECT_FAILED;
            continue;
        }
        return;
    }
    // We return on success above, only errors below.
    // Actually report the last error happend.
    if (0 == sCount)
        throw NetException("getaddrinfo", "unxpected empty result");
    else
        throw NetException(fail_msg[fail_state], errno);
}

SocketBase::~SocketBase() noexcept
{
    close(m_Fd);
}

void SocketBase::swap(SocketBase& a) noexcept
{
    std::swap(m_Fd, a.m_Fd);
}
