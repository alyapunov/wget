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
#include <HttpResponseParser.hpp>

#include <algorithm>
#include <chrono>
#include <iostream>

const size_t N = 4 * 1024 * 1024;
const char req1[] = "HTTP/1.0 200 OK\r\nContent-Length:111\r\n\r\n";
const size_t M1 = sizeof(req1) - 1;
const char req2[] = "HTTP/1.0 200 OK\r\nContent-Type:222\r\nContent-Length:111\r\nLocation:here\r\nSomething:more\r\n\r\n";
const size_t M2 = sizeof(req2) - 1;
char reqs[N * std::max(M1, M2)];

static void checkpoint(const char* aText = "", size_t aOpCount = 0, size_t aDataSize = 0)
{
    using namespace std::chrono;
    high_resolution_clock::time_point now = high_resolution_clock::now();
    static high_resolution_clock::time_point was;
    duration<double> time_span = duration_cast<duration<double>>(now - was);
    if (0 != aOpCount)
    {
        double Mrps = aOpCount / 1000000. / time_span.count();
        std::cout << aText << ": " << Mrps << " Mrps" << std::endl;
        Mrps = aDataSize / 1000000. / time_span.count();
        std::cout << aText << ": " << Mrps << " MB/sec" << std::endl;
    }
    was = now;
}

static size_t test(std::string_view data) __attribute__((noinline));
static size_t test(std::string_view data)
{
    size_t count = 0;

    HttpResponseParser p;
    for (char c : data)
    {
        if (0 != p.feed(c))
        {
            count++;
            p.reset();
        }
    }
    return count;
}

static bool starts_with(std::string_view str, std::string_view prefix)
{
    return str.size() >= prefix.size() && str.substr(0, prefix.size()) == prefix;
}

static bool starts_with_ci(std::string_view str, std::string_view prefix)
{
    return str.size() >= prefix.size() &&
        std::equal(str.begin(), str.begin() + prefix.size(),
                   prefix.begin(), prefix.end(),
                   [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); }
        );
}

static bool only_digits(std::string_view str)
{
    for (unsigned char c : str)
        if (!std::isdigit(c))
            return false;
    return true;
}

constexpr std::string_view naive_headers[4] = {"Content-type:", "Content-length:", "Transfer-encoding:", "Location:"};
constexpr size_t NAIVE_HEADERS_COUNT = sizeof(naive_headers) / sizeof(naive_headers[0]);

static bool naive_one(std::string_view data)
{
    size_t pos = data.find(std::string_view("\r\n"));
    if (pos == data.npos)
        return false;

    std::string_view line = data.substr(0, pos);
    data = data.substr(pos + 2);
    if (!starts_with(line, "HTTP/"))
        return false;
    size_t prev = 5; // strlen("HTTP/");

    pos = line.find('.', prev);
    if (pos == line.npos)
        return false;
    if (!only_digits(line.substr(prev, pos - prev)))
        return false;
    prev = pos + 1;

    pos = line.find(' ', prev);
    if (pos == line.npos)
        return false;
    if (!only_digits(line.substr(prev, pos - prev)))
        return false;
    prev = pos + 1;

    pos = line.find(' ', prev);
    if (pos == line.npos)
        return false;
    if (pos != prev + 3)
        return false;
    if (!only_digits(line.substr(prev, pos - prev)))
        return false;

    size_t known_headers = 0;
    while (!data.empty())
    {
        pos = data.find(std::string_view("\r\n"));
        if (pos == data.npos)
            return false;
        line = data.substr(0, pos);
        data = data.substr(pos + 2);
        for (size_t i = 0; i < NAIVE_HEADERS_COUNT; i++)
        {
            if (starts_with_ci(line, naive_headers[i]))
            {
                known_headers++;
                break;
            }
        }
    }

    return known_headers > 0;
}

static size_t naive(std::string_view data) __attribute__((noinline));
static size_t naive(std::string_view data)
{
    size_t res = 0;
    while (!data.empty())
    {
        size_t pos = data.find(std::string_view("\r\n\r\n"));
        if (pos == data.npos)
            break;
        res += naive_one(data.substr(0, pos + 2));
        data = data.substr(pos + 4);
    }
    return res;
}

int main()
{
    srand(time(nullptr));
    size_t s = 0;

    for (size_t i = 0; i < N; i++)
        std::copy(req1, req1 + M1, reqs + i * M1);
    checkpoint();
    s += test(std::string_view(reqs, N * M1));
    checkpoint("Result simple ", N, N * M1);
    checkpoint();
    s += naive(std::string_view(reqs, N * M1));
    checkpoint("Naive simple  ", N, N * M1);

    for (size_t i = 0; i < N; i++)
        std::copy(req2, req2 + M2, reqs + i * M2);
    checkpoint();
    s += test(std::string_view(reqs, N * M2));
    checkpoint("Result complex", N, N * M2);
    checkpoint();
    s += naive(std::string_view(reqs, N * M2));
    checkpoint("Naive complex ", N, N * M2);

    std::cout << "Side effect: " << s << std::endl;
}