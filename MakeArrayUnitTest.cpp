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
#include <MakeArray.hpp>

#include <array>
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

#define DECL_CLASS(Name) \
struct Name \
{ \
    int i = -1; \
    Name() = default; \
    Name(int ai) : i(ai) {} \
    bool operator==(const Name& a) const { return i == a.i; } \
};

DECL_CLASS(A)
DECL_CLASS(B)
DECL_CLASS(C)

struct Counters
{
    size_t ctor1 = 0;
    size_t ctor2 = 0;
    size_t ctor3 = 0;
    size_t dtor  = 0;
    size_t copy_ctor = 0;
    size_t copy_asgn = 0;
    size_t move_ctor = 0;
    size_t move_asgn = 0;
    void reset() { *this = Counters{}; }
} counters;

struct D
{
    A a;
    B b;
    C c;
    D(A aa) : a(aa) { counters.ctor1++; }
    D(A aa, const B& ab) : a(aa), b(ab) { counters.ctor2++; }
    D(A aa, const B& ab, C&& ac) : a(aa), b(ab), c(ac) { counters.ctor3++; }
    ~D() { counters.dtor++; }

    D(const D& d) : a(d.a), b(d.b), c(d.c) { counters.copy_ctor++; }
    D& operator=(const D& d) { a = d.a; b = d.b; c = d.c; counters.copy_asgn++; return *this; }
    D(D&& d) : a(d.a), b(d.b), c(d.c) { counters.move_ctor++; }
    D& operator=(D&& d) { a = d.a; b = d.b; c = d.c; counters.move_asgn++; return *this; }

    bool operator==(const D& d) const { return a == d.a && b == d.b && c == d.c; }
};

void test_basic()
{
    {
        A a2{2}, a4{4};
        B b4{4};
        auto test = makeArray<D>(A{1}, a2, B{2}, A{3}, a4, b4);
        std::array<D, 4> ref = {D{A{1}}, D{A{2}, B{2}}, D{A{3}}, D{A{4}, B{4}}};
        check(test == ref, "Wrong result");
        test[0].a.i = 42;
        check(!(test == ref), "Check const");
        ref[0].a.i = 42;
        check(test == ref, "Check const");
    }
    {
        A a2{2}, a4{4};
        B b4{4};
        auto test = makeArray<D>(A{1}, a2, B{2}, A{3}, a4, b4, A{5});
        std::array<D, 5> ref = {D{A{1}}, D{A{2}, B{2}}, D{A{3}}, D{A{4}, B{4}}, D{A{5}}};
        check(test == ref, "Wrong result");
    }
    {
        auto test = makeArray<D>(A{1}, A{2}, B{2}, A{3}, B{3}, C{3});
        std::array<D, 3> ref = {D{A{1}}, D{A{2}, B{2}}, D{A{3}, B{3}, C{3}}};
        check(test == ref, "Wrong result");
    }
    {
        auto test = makeArray<D>(A{1}, A{2}, B{2}, A{3}, B{3}, C{3}, A{4});
        std::array<D, 4> ref = {D{A{1}}, D{A{2}, B{2}}, D{A{3}, B{3}, C{3}}, D{A{4}}};
        check(test == ref, "Wrong result");
    }
}

void test_forwarding()
{
    counters.reset();
    {
        auto test = makeArray<D>(A{1}, A{2}, B{2}, A{3}, B{3}, C{3});
    }
    check(1 == counters.ctor1, "how come");
    check(1 == counters.ctor2, "how come");
    check(1 == counters.ctor3, "how come");
    check(3 == counters.dtor, "how come");
    check(0 == counters.copy_ctor, "how come");
    check(0 == counters.copy_asgn, "how come");
    check(0 == counters.move_ctor, "how come");
    check(0 == counters.move_asgn, "how come");

    counters.reset();
    {
        auto test = makeArray<D>(A{1}, A{2}, B{2}, A{3}, B{3}, C{3}, A{4});
    }
    check(2 == counters.ctor1, "how come");
    check(1 == counters.ctor2, "how come");
    check(1 == counters.ctor3, "how come");
    check(4 == counters.dtor, "how come");
    check(0 == counters.copy_ctor, "how come");
    check(0 == counters.copy_asgn, "how come");
    check(0 == counters.move_ctor, "how come");
    check(0 == counters.move_asgn, "how come");

    counters.reset();
    {
        D d{A{2}};
        auto test = makeArray<D>(A{1}, d, D{A{3}}, A{4});
    }
    check(4 == counters.ctor1, "how come");
    check(0 == counters.ctor2, "how come");
    check(0 == counters.ctor3, "how come");
    check(6 == counters.dtor, "how come");
    check(1 == counters.copy_ctor, "how come");
    check(0 == counters.copy_asgn, "how come");
    check(1 == counters.move_ctor, "how come");
    check(0 == counters.move_asgn, "how come");

    counters.reset();
    {
        D d{A{1}};
        auto test = makeArray<D>(d, A{2}, D{A{3}});
    }
    check(3 == counters.ctor1, "how come");
    check(0 == counters.ctor2, "how come");
    check(0 == counters.ctor3, "how come");
    check(5 == counters.dtor, "how come");
    check(1 == counters.copy_ctor, "how come");
    check(0 == counters.copy_asgn, "how come");
    check(1 == counters.move_ctor, "how come");
    check(0 == counters.move_asgn, "how come");

    counters.reset();
    {
        D d{A{3}};
        auto test = makeArray<D>(D{A{1}}, A{2}, d);
    }
    check(3 == counters.ctor1, "how come");
    check(0 == counters.ctor2, "how come");
    check(0 == counters.ctor3, "how come");
    check(5 == counters.dtor, "how come");
    check(1 == counters.copy_ctor, "how come");
    check(0 == counters.copy_asgn, "how come");
    check(1 == counters.move_ctor, "how come");
    check(0 == counters.move_asgn, "how come");
}

struct Special
{
    template <size_t N>
    Special(std::array<char, N>& aArr) : m_Data(aArr.data()), m_Size(N) {}
    template <size_t N>
    Special(char (&aArr)[N]) : m_Data(aArr), m_Size(N) {}

    char* m_Data;
    size_t m_Size;
};

void test_special()
{
    std::array<char, 10> arr1;
    char arr2[12];
    auto group = makeArray<Special>(arr1, arr2);
    check(2 == group.size(), "wrong size");
    check(group[0].m_Data == arr1.data(), "wrong ptr(1)");
    check(group[0].m_Size == arr1.size(), "wrong size(1)");
    check(group[1].m_Data == arr2, "wrong ptr(2)");
    check(group[1].m_Size == sizeof(arr2), "wrong size(2)");
}

int main()
{
    try
    {
        test_basic();
        test_forwarding();
        test_special();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Well done!" << std::endl;

}
