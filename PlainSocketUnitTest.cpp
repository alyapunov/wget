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
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

#include "NetException.hpp"

void check(bool aExpession, const char* aMessage)
{
    if (!aExpession)
    {
        assert(false);
        throw std::runtime_error(aMessage);
    }
}

// Simplest stupid echo server.
constexpr unsigned short PORT = 30000;
const char* SPORT = "30000";
std::atomic<bool> stop{false};
std::atomic<size_t> count{0};

struct Closer
{
    Closer(int s) : m_S(s) { }
    ~Closer() { close(m_S);-- count; }
    int m_S;
};

void stupidEchoConn(int s)
{
    try
    {
        Closer c(s);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        check(setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == 0, "setsockopt(sndtimeo)");
        check(setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0, "setsockopt(rcvtimeo)");
        const size_t BUF_SIZE = 16 * 1024;
        char buf[BUF_SIZE];
        while (!stop)
        {
            ssize_t size = recv(s, buf, BUF_SIZE, MSG_NOSIGNAL);
            if (size == 0)
                return;
            if (size < 0)
            {
                if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
                    continue;
                break;
            }
            bool sClose = std::any_of(buf, buf + size, [](char c){ return c == '!'; });
            const char* end = buf + size;
            do
            {
                ssize_t sent = send(s, end - size, size, 0);
                if (sent < 0)
                {
                    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        sClose = true;
                        continue;
                    }
                    break;
                }
                size -= sent;
            } while (size != 0);
            if (sClose)
                break;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error in echo connection: " << e.what() << std::endl;
    }
}

void stupidEchoServer()
{
    try
    {
        int s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        check(s >= 0, "socket");
        ++count;
        Closer c(s);
        int enable = 1;
        check(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == 0, "setsockopt()reuseraddr");
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        check(setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == 0, "setsockopt(sndtimeo)");
        check(setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0, "setsockopt(rcvtimeo)");
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(PORT);
        check(bind(s, (sockaddr*)&addr, sizeof(addr)) == 0, "bind");
        check(listen(s, 1024) == 0, "bind");
        while (!stop)
        {
            int a = accept(s, nullptr, 0);
            if (a > 0)
            {
                ++count;
                std::thread(stupidEchoConn, a).detach();
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error in echo server: " << e.what() << std::endl;
    }
}

void checkSimpleHttp(const char* aAddress, const char* aPort)
{
    char sBuf[65536];
    PlainSocket s(sBuf, aAddress, aPort, 500000);
    s.sendOrDie("GET / HTTP/1.1\r\n", "Host: ", std::string(aAddress), "\r\n\r\n");
    char sReply[8];
    s.recvOrDie(sReply);
    check(std::string_view(sReply, sizeof(sReply)) == "HTTP/1.1", "Wrong reply");
}

template <size_t BUF_SUZE, size_t MSG_SIZE>
void checkBufSize()
{
    char sBuf[BUF_SUZE];
    PlainSocket s(sBuf, sizeof(sBuf), "localhost", SPORT, 1000000);
    std::array<char, MSG_SIZE> sOut;
    for (size_t i = 0; i < MSG_SIZE; i++)
        sOut[i] = i;
    sOut[MSG_SIZE - 1] = '!';
    s.sendOrDie(sOut.data(), sOut.size());
    std::array<char, MSG_SIZE> sIn;
    s.recvOrDie(sIn);
    check(sOut == sIn, "Wrong result");
    bool sShutDownError = false;
    check(s.recvSome(1, sShutDownError, sIn) == 0, "Expected shutdown");
    check(sShutDownError, "Expected shutdown (2)");
}

int main(int, char**)
{
    std::thread srv(stupidEchoServer);
    try
    {
        checkSimpleHttp("mail.ru", "80");
        checkSimpleHttp("yandex.ru", "http");
        checkBufSize<3, 1000>();
        checkBufSize<1000, 3>();
        checkBufSize<16, 1024>();
        checkBufSize<1024, 16>();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch (const NetException& e)
    {
        std::cerr << e.what() << ": " << e.how() << std::endl;
        return EXIT_FAILURE;
    }

    stop = true;
    srv.join();
    while (count != 0)
        usleep(100000);

    std::cout << "Well done!" << std::endl;
}