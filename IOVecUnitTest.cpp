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
#include <IOVec.hpp>

#include <cassert>
#include <iostream>
#include <stdexcept>

void check(bool aExpession, const char* aMessage)
{
    if (!aExpession)
    {
        assert(false);
        throw std::runtime_error(aMessage);
    }
}

void test_IVec_basic()
{
    {
        std::string s(10, '\0');
        std::vector<char> v(11);
        char c[12];
        std::array<char, 13> a;
        char buf[50];
        struct iovec io;
        io.iov_base = buf + 30;
        io.iov_len = 15;
        auto sVec = makeIVec(s, v, c, a, buf, 14, io);
        check(sVec.size() == 6, "Wrong ivec size");
        check(sVec[0].iov_base == s.data(), "Wrong ivec string addr");
        check(sVec[0].iov_len == s.size(), "Wrong ivec string size");
        check(sVec[1].iov_base == v.data(), "Wrong ivec vector addr");
        check(sVec[1].iov_len == v.size(), "Wrong ivec vector size");
        check(sVec[2].iov_base == c, "Wrong ivec c array addr");
        check(sVec[2].iov_len == sizeof(c), "Wrong ivec c array size");
        check(sVec[3].iov_base == a.data(), "Wrong ivec c++ array addr");
        check(sVec[3].iov_len == a.size(), "Wrong ivec c++ array size");
        check(sVec[4].iov_base == buf, "Wrong ivec buf addr");
        check(sVec[4].iov_len == 14, "Wrong ivec buf size");
        check(sVec[5].iov_base == io.iov_base, "Wrong ivec iov addr");
        check(sVec[5].iov_len == io.iov_len, "Wrong ivec iov size");
    }
}

void test_OVec_basic()
{
    {
        std::string s(10, '\0');
        std::vector<char> v(11);
        const char* c = "012345678901";
        const char* buf = "012345678901234567890123456789012345678901234567890123456789";
        std::string_view sv = std::string_view(buf).substr(20, 13);
        struct iovec io;
        //I'm surely not modifying the data
        io.iov_base = const_cast<char*>(buf + 40);
        io.iov_len = 15;
        auto sVec = makeOVec(s, v, c, buf, 13, sv, io);
        check(sVec.size() == 6, "Wrong ovec size");
        check(sVec[0].iov_base == s.data(), "Wrong ovec string addr");
        check(sVec[0].iov_len == s.size(), "Wrong ovec string size");
        check(sVec[1].iov_base == v.data(), "Wrong ovec vector addr");
        check(sVec[1].iov_len == v.size(), "Wrong ovec vector size");
        check(sVec[2].iov_base == c, "Wrong ovec literal addr");
        check(sVec[2].iov_len == strlen(c), "Wrong ovec literal size");
        check(sVec[3].iov_base == buf, "Wrong ovec buf addr");
        check(sVec[3].iov_len == 13, "Wrong ovec buf size");
        check(sVec[4].iov_base == sv.data(), "Wrong ovec sview addr");
        check(sVec[4].iov_len == sv.size(), "Wrong ovec sview size");
        check(sVec[5].iov_base == io.iov_base, "Wrong ovec iov addr");
        check(sVec[5].iov_len == io.iov_len, "Wrong ovec iov size");
    }
    {
        std::string s(0, '\0');
        std::vector<char> v(0);
        const char* c = "012345678901";
        const char* buf = "012345678901234567890123456789012345678901234567890123456789";
        std::string_view sv = std::string_view(buf).substr(20, 13);
        struct iovec io;
        //I'm surely not modifying the data
        io.iov_base = const_cast<char*>(buf + 40);
        io.iov_len = 15;
        auto sVec = makeOVec(std::string(0, '\0'), std::vector<char>(0), "012345678901",
                             "012345678901234567890123456789012345678901234567890123456789", 13,
                             std::string_view(buf).substr(20, 13),
                             iovec{const_cast<char*>(buf + 40), 15});
        check(sVec.size() == 6, "Wrong ovec size");
        check(sVec[0].iov_len == 0, "Wrong ovec string size");
        check(sVec[1].iov_len == 0, "Wrong ovec vector size");
        check(0 == memcmp(sVec[2].iov_base, c, strlen(c)), "Wrong ovec literal data");
        check(sVec[2].iov_len == strlen(c), "Wrong ovec literal size");
        check(0 == memcmp(sVec[3].iov_base, buf, 13), "Wrong ovec buf data");
        check(sVec[3].iov_len == 13, "Wrong ovec buf size");
        check(sVec[4].iov_base == sv.data(), "Wrong ovec sview addr");
        check(sVec[4].iov_len == sv.size(), "Wrong ovec sview size");
        check(sVec[5].iov_base == io.iov_base, "Wrong ovec iov addr");
        check(sVec[5].iov_len == io.iov_len, "Wrong ovec iov size");
    }
}

int main()
{
    try
    {
        test_IVec_basic();
        test_OVec_basic();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Well done!" << std::endl;

}
