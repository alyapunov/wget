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

#include <assert.h>

#include <iostream>

void check(bool aExpession, const char* aMessage)
{
    if (!aExpession)
    {
        assert(false);
        throw std::runtime_error(aMessage);
    }
}

void check_err(HttpResponseParser::status_t status, std::string_view str)
{
    check(HttpResponseParser::getErrorStr(status) == str, "unexpected error message");
}

void test_fail(const char* data, HttpResponseParser::status_t expected)
{
    HttpResponseParser p;
    HttpResponseParser::status_t res = 0;

    for (const char* c = data; *c != 0 && res == 0; c++)
        res = p.feed(*c);

    check(res == expected, "wrong result");

    res = 0;
    p.reset();

    for (const char* c = data; *c != 0 && res == 0; c++)
        res = p.feed(*c);

    check(res == expected, "wrong result after reset");
}

void test_pass(std::string_view resp,
               std::string_view major, std::string_view minor, std::string_view status, std::string_view reason,
               std::string_view length, std::string_view type, std::string_view location, std::string_view trenc)
{
    HttpResponseParser p;
    HttpResponseParser::status_t res = 0;
    auto itr = resp.begin();

    for (; itr != resp.end() && res == 0; ++itr)
        res = p.feed(*itr);

    check(itr == resp.end(), "Not all data was fed");
    check(res == HttpResponseParser::SUCCESS, "Not success");
    check(p.getFragmentStr(resp, HttpResponseParser::MAJOR_VERSION) == major, "Wrong major version");
    check(p.getFragmentStr(resp, HttpResponseParser::MINOR_VERSION) == minor, "Wrong minor version");
    check(p.getFragmentStr(resp, HttpResponseParser::STATUS_CODE) == status, "Wrong status");
    check(p.getFragmentStr(resp, HttpResponseParser::REASON_PHRASE) == reason, "Wrong reason phrase");
    check(p.getFragmentStr(resp, HttpResponseParser::CONTENT_LENGTH) == length, "Wrong content length");
    check(p.getFragmentStr(resp, HttpResponseParser::CONTENT_TYPE) == type, "Wrong content type");
    check(p.getFragmentStr(resp, HttpResponseParser::LOCATION) == location, "Wrong location");
    check(p.getFragmentStr(resp, HttpResponseParser::TRANSFER_ENCODING) == trenc, "Wrong trasfer encoding");

    p.reset();
    res = 0;
    itr = resp.begin();

    for (; itr != resp.end() && res == 0; ++itr)
        res = p.feed(resp[p.count()]);

    check(itr == resp.end(), "Not all data was fed after restart");
    check(res == HttpResponseParser::SUCCESS, "Not success after restart");
    check(p.getFragmentStr(resp, HttpResponseParser::MAJOR_VERSION) == major, "Wrong major version after restart");
    check(p.getFragmentStr(resp, HttpResponseParser::MINOR_VERSION) == minor, "Wrong minor version after restart");
    check(p.getFragmentStr(resp, HttpResponseParser::STATUS_CODE) == status, "Wrong status after restart");
    check(p.getFragmentStr(resp, HttpResponseParser::REASON_PHRASE) == reason, "Wrong reason phrase after restart");
    check(p.getFragmentStr(resp, HttpResponseParser::CONTENT_LENGTH) == length, "Wrong content length after restart");
    check(p.getFragmentStr(resp, HttpResponseParser::CONTENT_TYPE) == type, "Wrong content type after restart");
    check(p.getFragmentStr(resp, HttpResponseParser::LOCATION) == location, "Wrong location after restart");
    check(p.getFragmentStr(resp, HttpResponseParser::TRANSFER_ENCODING) == trenc, "Wrong trasfer encoding after restart");
}

void test_misc()
{
    const char *data, *c;
    data = "HTTP/1.0 200 OK\r\nCoNtenT-leNGth:123\r\nLOCATION: 321\r\ncontent-type: 222 \r\n\r\n";

    HttpResponseParser p;
    HttpResponseParser::status_t res = 0;

    for (c = data; *c != 0 && res == 0; c++)
        res = p.feed(*c);

    check(*c == 0, "Not all data was fed");
    check(res == HttpResponseParser::SUCCESS, "Not success");
    check(p.isFragmentFound(HttpResponseParser::CONTENT_LENGTH), "Content-length was not found");
    check(p.getFragmentStr(std::string_view(data), HttpResponseParser::CONTENT_LENGTH) == "123", "Wrong Content-length");
    check(p.isFragmentFound(HttpResponseParser::CONTENT_TYPE), "Content-type was not found");
    check(p.getFragmentStr(std::string_view(data), HttpResponseParser::CONTENT_TYPE) == "222", "Wrong Content-type");
    check(p.isFragmentFound(HttpResponseParser::LOCATION), "Location was not found");
    check(p.getFragmentStr(std::string_view(data), HttpResponseParser::LOCATION) == "321", "Wrong Location");
    check(!p.isFragmentFound(HttpResponseParser::TRANSFER_ENCODING), "Encoding was found");

    data = "HTTP/1.0 200 OK\r\nContent-length:123\r\nContent-length: 321\r\nLocation: 222 \r\n\r\n";
    p.reset();
    res = 0;
    for (c = data; *c != 0 && res == 0; c++)
        res = p.feed(data[p.count()]);

    check(*c == 0, "Not all data was fed");
    check(res == HttpResponseParser::SUCCESS, "Not success");
    check(p.isFragmentFound(HttpResponseParser::CONTENT_LENGTH), "Content-length was not found");
    check(p.getFragmentStr(std::string_view(data), HttpResponseParser::CONTENT_LENGTH) == "321", "Wrong Content-length");
    check(!p.isFragmentFound(HttpResponseParser::CONTENT_TYPE), "Content type was found");
    check(p.isFragmentFound(HttpResponseParser::LOCATION), "Location was not found");
    check(p.getFragmentStr(std::string_view(data), HttpResponseParser::LOCATION) == "222", "Wrong Location");
    check(!p.isFragmentFound(HttpResponseParser::TRANSFER_ENCODING), "Encoding was found");
}

void test_massive()
{
    const size_t COUNT = 64 * 1024;
    std::string req;
    constexpr size_t NUM_FRAGS = HttpResponseParser::HEADER_MAX;
    std::string NAMES[] =
    { "Majorver", "Minorer", "Code", "Reason",
      "Content-type",
      "Content-length",
      "Transfer-encoding",
      "location"
    };
    static_assert(NUM_FRAGS == sizeof(NAMES) / sizeof(NAMES[0]), "Let's test all headers!");
    std::string frags[NUM_FRAGS];
    constexpr  size_t NUM_VARS = 8;
    std::string vars[NUM_VARS] = {"", "a", "bb", "a c", "a b\tc", "\377a\377", "\377", "0"};
    constexpr  size_t NUM_PADS = 8;
    std::string pads[] = {"", " ", "\t ", " \t", "\r", "\n\r", "\r ", "\t\n"};

    auto gen_spaces = [&]() -> std::string
    {
        return pads[rand() % NUM_PADS];
    };

    auto gen_value = [&](std::string& real, bool broken_key = false) -> std::string
    {
        std::string pref = gen_spaces();
        std::string post = gen_spaces();
        std::string val = vars[rand() % NUM_VARS];
        if (!broken_key && !val.empty())
            real = val;
        return pref + val + post;
    };

    for (size_t i = 0; i < COUNT; i++)
    {
        int r = rand();
        frags[0] = (r & 1) ? "1" : "10";
        frags[1] = (r & 2) ? "0" : "012";
        frags[2] = (r & 4) ? "200" : "555";
        r >>= 3;
        for (size_t j = 3; j < NUM_FRAGS; j++)
            frags[j] = "";

        req = std::string("HTTP/") + frags[0] + "." + frags[1] + " " + frags[2] + " " + gen_value(frags[3]) + "\r\n";

        while (true) {
            r = rand();
            if ((r & 3) == 0)
                break;
            r >>= 3;

            if ((r & 31) == 0)
            {
                req += "Content-content: nothing\r\n";
            }
            r >>= 4;
            if ((r & 31) == 0)
            {
                req += "\377Content-content: nothing\r\n";
            }
            r >>= 4;
            if ((r & 31) == 0)
            {
                req += " Broken: nothing\r\n";
            }
            r >>= 4;

            r = rand();
            int f = 4 + r % (NUM_FRAGS - 4);
            r /= (NUM_FRAGS - 4);
            bool break_name = (r & 7) == 0;
            r >>= 3;
            if (break_name)
            {
                if (r & 1)
                    req += " " + NAMES[f] + ":";
                else
                    req += NAMES[f] + " :";
            }
            else
            {
                req += NAMES[f] + ":";
            }
            req += gen_value(frags[f], break_name) + "\r\n";
        }

        req += "\r\n";

        HttpResponseParser p;
        HttpResponseParser::status_t res = 0;
        const char* c;

        for (c = req.c_str(); *c != 0 && res == 0; c++)
            res = p.feed(*c);

        check(*c == 0, "Not all data was fed");

        for (size_t j = 0; j < NUM_FRAGS; j++)
            check(frags[j] == p.getFragmentStr(req, j), "wrong fragment");
    }
}

int main()
{
    try
    {
        check_err(HttpResponseParser::SUCCESS, "Success");
        check_err(HttpResponseParser::ERROR_NOT_HTTP, "Wrong character, not an HTTP");
        check_err(HttpResponseParser::ERROR_NOT_A_DIGIT_MAJOR_VERSION, "Not a digit in major version");
        check_err(HttpResponseParser::ERROR_NOT_A_DIGIT_MINOR_VERSION, "Not a digit in minor version");
        check_err(HttpResponseParser::ERROR_NOT_A_DIGIT_STATUS_CODE, "Not a digit in status code");
        check_err(HttpResponseParser::ERROR_WRONG_LENGTH_OF_STATUS_CODE, "Wrong length of status code");

        test_fail("HTTP\r\n\r\n", HttpResponseParser::ERROR_NOT_HTTP);
        test_fail("http/1.1 200 OK\r\n\r\n", HttpResponseParser::ERROR_NOT_HTTP);
        test_fail("HTTTP/1.1 200 OK\r\n\r\n", HttpResponseParser::ERROR_NOT_HTTP);
        test_fail("HTTP/a1.1 200 OK\r\n\r\n", HttpResponseParser::ERROR_NOT_A_DIGIT_MAJOR_VERSION);
        test_fail("HTTP/1a.1 200 OK\r\n\r\n", HttpResponseParser::ERROR_NOT_A_DIGIT_MAJOR_VERSION);
        test_fail("HTTP/a.1 200 OK\r\n\r\n", HttpResponseParser::ERROR_NOT_A_DIGIT_MAJOR_VERSION);
        test_fail("HTTP/1.b1 200 OK\r\n\r\n", HttpResponseParser::ERROR_NOT_A_DIGIT_MINOR_VERSION);
        test_fail("HTTP/1.1b 200 OK\r\n\r\n", HttpResponseParser::ERROR_NOT_A_DIGIT_MINOR_VERSION);
        test_fail("HTTP/1.b 200 OK\r\n\r\n", HttpResponseParser::ERROR_NOT_A_DIGIT_MINOR_VERSION);
        test_fail("HTTP/1.1 a00 OK\r\n\r\n", HttpResponseParser::ERROR_NOT_A_DIGIT_STATUS_CODE);
        test_fail("HTTP/1.1 2b0 OK\r\n\r\n", HttpResponseParser::ERROR_NOT_A_DIGIT_STATUS_CODE);
        test_fail("HTTP/1.1 20c OK\r\n\r\n", HttpResponseParser::ERROR_NOT_A_DIGIT_STATUS_CODE);
        test_fail("HTTP/1.1 OK\r\n\r\n", HttpResponseParser::ERROR_NOT_A_DIGIT_STATUS_CODE);
        test_fail("HTTP/1.1 2 OK\r\n\r\n", HttpResponseParser::ERROR_WRONG_LENGTH_OF_STATUS_CODE);
        test_fail("HTTP/1.1 20 OK\r\n\r\n", HttpResponseParser::ERROR_WRONG_LENGTH_OF_STATUS_CODE);
        test_fail("HTTP/1.1 2000 OK\r\n\r\n", HttpResponseParser::ERROR_WRONG_LENGTH_OF_STATUS_CODE);
        test_fail("HTTP/1.1 20000 OK\r\n\r\n", HttpResponseParser::ERROR_WRONG_LENGTH_OF_STATUS_CODE);
        test_fail("HTTP/1.1 200OK OK\r\n\r\n", HttpResponseParser::ERROR_WRONG_LENGTH_OF_STATUS_CODE);

        test_fail("\377TTP/1.1 200\r\n\r\n", HttpResponseParser::ERROR_NOT_HTTP);
        test_fail("H\377TTP/1.1 200\r\n\r\n", HttpResponseParser::ERROR_NOT_HTTP);
        test_fail("HTTP/\377.1 200\r\n\r\n", HttpResponseParser::ERROR_NOT_A_DIGIT_MAJOR_VERSION);
        test_fail("HTTP/1\377.1 200\r\n\r\n", HttpResponseParser::ERROR_NOT_A_DIGIT_MAJOR_VERSION);
        test_fail("HTTP/1.\377 200\r\n\r\n", HttpResponseParser::ERROR_NOT_A_DIGIT_MINOR_VERSION);
        test_fail("HTTP/1.1\377 200\r\n\r\n", HttpResponseParser::ERROR_NOT_A_DIGIT_MINOR_VERSION);
        test_fail("HTTP/1.1 \37700\r\n\r\n", HttpResponseParser::ERROR_NOT_A_DIGIT_STATUS_CODE);
        test_fail("HTTP/1.1 2\3770\r\n\r\n", HttpResponseParser::ERROR_NOT_A_DIGIT_STATUS_CODE);
        test_fail("HTTP/1.1 20\377\r\n\r\n", HttpResponseParser::ERROR_NOT_A_DIGIT_STATUS_CODE);
        test_fail("HTTP/1.1 200\377\r\n\r\n", HttpResponseParser::ERROR_WRONG_LENGTH_OF_STATUS_CODE);

        test_pass("HTTP/1.0 200 OK\r\n\r\n", "1", "0", "200", "OK", "", "", "", "");
        test_pass("HTTP/12.34 333 \r\n\r\n", "12", "34", "333", "", "", "", "", "");
        test_pass("HTTP/1.0 200 OK\r\nContent-length:a b c\r\n\r\n", "1", "0", "200", "OK", "a b c", "", "", "");
        test_pass("HTTP/1.0 200 \rO K\n\r\nContent-length:a b\rc\td\ne\r\n\r\n", "1", "0", "200", "O K", "a b\rc\td\ne", "", "", "");
        test_pass("HTTP/1.0 200 OK\r\nContent-length :123\r\n\r\n", "1", "0", "200", "OK", "", "", "", "");
        test_pass("HTTP/1.0 200 OK\r\n Content-length:123\r\n\r\n", "1", "0", "200", "OK", "", "", "", "");
        test_pass("HTTP/1.0 200 OK\r\nContent-type:222\r\nContent-length:111\r\n\r\n", "1", "0", "200", "OK", "111", "222", "", "");
        test_pass("HTTP/1.0 200 OK\r\nContent-type:    222    \r\nContent-LENGTH:\r\r\r111\n\n\n\r\n\r\n", "1", "0", "200", "OK", "111", "222", "", "");
        test_pass("HTTP/1.0 200 OK\r\nContent-type:222\r\nContent-length:111\r\n\r\n", "1", "0", "200", "OK", "111", "222", "", "");
        test_pass("HTTP/1.0 200 OK\r\nContent-type:222\r\nContent-length:111\r\nLocation:xxx\r\nTransfer-encoding:yyy\r\n\r\n",
                  "1", "0", "200", "OK", "111", "222", "xxx", "yyy");
        test_pass("HTTP/1.0 200 OK\r\nContent-type:222\r\nContent-length:111\r\nLocation:xxx\r\nTransfer-encoding:yyy\r\nLocation:aaa\r\nTransfer-encoding:bbb\r\n\r\n",
                  "1", "0", "200", "OK", "111", "222", "aaa", "bbb");
        test_pass("HTTP/1.0 200 OK\r\nContent-type:222\r\nContent-length:111\r\nLocation:xxx\r\nTransfer-encoding:yyy\r\nLocation1:aaa\r\nUnknonwn:bbb\r\n\r\n",
                  "1", "0", "200", "OK", "111", "222", "xxx", "yyy");
        test_pass("HTTP/1.0 200 OK\r\n\377Content-type:222\r\nContent\377-length:111\r\nLocation:\377 xxx\r\nTransfer-encoding:yyy\377\r\n\r\n",
                  "1", "0", "200", "OK", "", "", "\377 xxx", "yyy\377");

        test_misc();

        test_massive();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Well done!" << std::endl;

}
