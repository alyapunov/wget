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
#include "NetException.hpp"

#include <string.h>

NetException::NetException(const char* aWhat)
    : m_What(aWhat), m_Reason(nullptr), m_ErrNo(0)
{
}

NetException::NetException(const char* aWhat, const char* aReason)
    : m_What(aWhat), m_Reason(aReason), m_ErrNo(0)
{
}

NetException::NetException(const char* aWhat, int aErrNo)
    : m_What(aWhat), m_Reason(nullptr), m_ErrNo(aErrNo)
{
}

const char* NetException::how() const
{
    if (m_Reason != nullptr)
    {
        return m_Reason;
    }
    else if (m_ErrNo != 0)
    {
        thread_local char buf[1024];
        // There are different versions of strerror_r function.
        // In any case nonzero (or non-nullptr) indicates a error,
        //  perhaps the buffer size in insufficient.
        if (strerror_r(m_ErrNo, buf, sizeof(buf)))
            return "Could not explain..";
        return buf;
    }
    else
    {
        return "";
    }
}
