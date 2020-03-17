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

#include <string.h>
#include <sys/uio.h>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

#include <MakeArray.hpp>

// struct iovec C++ wrapper

struct IOVec : iovec
{
    constexpr IOVec(iovec aData) : iovec(aData) {}
    void* data() const { return iov_base; }
    size_t size() const { return iov_len; }
    bool empty() const { return 0 == iov_len; }
    // return whether the iovec was depleted.
    bool skip(size_t& aSize)
    {
        if (iov_len <= aSize)
        {
            iov_base = static_cast<char*>(iov_base) + iov_len;
            aSize -= iov_len;
            iov_len = 0;
            return true;
        }
        else
        {
            iov_base = static_cast<char*>(iov_base) + aSize;
            iov_len -= aSize;
            aSize = 0;
            return false;
        }
    }
    // return number of bytes taken.
    size_t take(char* aData, size_t& aFrom, size_t aTo)
    {
        size_t sTaken = std::min(aTo - aFrom, iov_len);
        memcpy(iov_base, aData + aFrom, sTaken);
        iov_base = static_cast<char*>(iov_base) + sTaken;
        iov_len -= sTaken;
        aFrom += sTaken;
        return sTaken;
    }
};

struct IVec : IOVec
{
    using IOVec::IOVec;
    constexpr IVec(char* aData, size_t aSize) : IOVec(mk(aData, aSize)) {}
    template <size_t N>
    constexpr IVec(char (&aData)[N]) : IOVec(mk(aData, N)) {}
    template <size_t N>
    constexpr IVec(std::array<char, N>& aData) : IOVec(mk(aData.data(), aData.size())) {}
    IVec(std::string& aData) : IOVec(mk(aData.data(), aData.size())) {}
    IVec(std::vector<char>& aData) : IOVec(mk(aData.data(), aData.size())) {}

private:
    static constexpr IOVec mk(char* aData, size_t aSize)
    {
        // I hope the order of iovec's members is portable.
        return iovec{aData, aSize};
    }
};

// Make an array of IVec from something that is convertible to IVec.
// For example: makeIVec((std::vector<char>&)vector, (char *)buf, (size_t)buf_size);
template <class ...ARGS>
inline auto makeIVec(ARGS&&... aArgs)
{
    return makeArray<IVec>(std::forward<ARGS>(aArgs)...);
}

struct OVec : IOVec
{
    using IOVec::IOVec;
    constexpr OVec(const char* aData, size_t aSize) : IOVec(mk(aData, aSize)) {}
    constexpr OVec(const char* aData) : IOVec(mk(aData, strlen(aData))) {}
    OVec(const std::string& aData) : IOVec(mk(aData.data(), aData.size())) {}
    constexpr OVec(std::string_view aData) : IOVec(mk(aData.data(), aData.size())) {}
    OVec(const std::vector<char>& aData) : IOVec(mk(aData.data(), aData.size())) {}

private:
    static constexpr IOVec mk(const char* aData, size_t aSize)
    {
        // Linux API makes me mad. I hope sendmsg etc doesn't actually modify the data.
        return iovec{const_cast<char*>(aData), aSize};
    }
};

// Make an array of OVec from something that is convertible to OVec.
// For example: makeIVec("GET ", (const std::string&)aPath, " HTTP/1.1\r\n\r\n");
template <class ...ARGS>
inline auto makeOVec(ARGS&&... aArgs)
{
    return makeArray<OVec>(std::forward<ARGS>(aArgs)...);
}

static_assert(sizeof(IVec) == sizeof(struct iovec), "Can't be");
static_assert(sizeof(IVec[2]) == sizeof(struct iovec[2]), "Can't be");
static_assert(sizeof(OVec) == sizeof(struct iovec), "Can't be");
static_assert(sizeof(OVec[2]) == sizeof(struct iovec[2]), "Can't be");
