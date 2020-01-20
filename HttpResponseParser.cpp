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

namespace
{
    using Transition = HttpResponseParser::Transition;
    using Conditions = HttpResponseParser::Conditions;
    using StateMachine = HttpResponseParser::StateMachine;

    using fragment_t = HttpResponseParser::fragment_t;
    using tag_t = HttpResponseParser::tag_t;
    using state_t = HttpResponseParser::state_t;
    using status_t = HttpResponseParser::status_t;

    // Header names. Case insensitive, visible characters only.
    constexpr std::string_view m_HeaderNames[] = {
        "Content-Type",
        "Content-Length",
        "Transfer-Encoding",
        "Location"
    };

    static_assert(sizeof(m_HeaderNames) / sizeof(m_HeaderNames[0]) == HttpResponseParser::HEADER_MAX - HttpResponseParser::SPECIAL_MAX, "smth went wrong!");

    constexpr std::string_view m_StatusErrors[] = {
        "",
        "Success",
        "Wrong character, not an HTTP",
        "Not a digit in major version",
        "Not a digit in minor version",
        "Not a digit in status code",
        "Wrong length of status code"
    };

    static_assert(sizeof(m_StatusErrors) / sizeof(m_StatusErrors[0]) == HttpResponseParser::STATUS_END, "smth went wrong!");

    constexpr Transition final(status_t aStatus)
    {
        return Transition{0, HttpResponseParser::DUMMY_TAG, aStatus};
    }

    constexpr Transition normal(state_t aNextState, tag_t aTag = HttpResponseParser::DUMMY_TAG)
    {
        return Transition{aNextState, aTag};
    }

    constexpr Conditions buildConditions(Transition aDefault)
    {
        Conditions sRes{};
        // array::fill is not constexp..
        for (size_t i = 0; i < HttpResponseParser::Conditions::NUM_TRANSITIONS; i++)
            sRes.m_Transitions[i] = aDefault;
        return sRes;
    }

    constexpr Conditions buildConditions(Transition aDefault, unsigned char c, Transition t)
    {
        Conditions sRes{};
        // array::fill is not constexp..
        for (size_t i = 0; i < HttpResponseParser::Conditions::NUM_TRANSITIONS; i++)
            sRes.m_Transitions[i] = aDefault;
        sRes.m_Transitions[c] = t;
        return sRes;
    }

    constexpr Conditions buildConditions(Transition aDefault, unsigned char c1, Transition t1, unsigned char c2, Transition t2)
    {
        Conditions sRes{};
        // array::fill is not constexp..
        for (size_t i = 0; i < HttpResponseParser::Conditions::NUM_TRANSITIONS; i++)
            sRes.m_Transitions[i] = aDefault;
        sRes.m_Transitions[c1] = t1;
        sRes.m_Transitions[c2] = t2;
        return sRes;
    }

    // Primitive constexpr tolower and toupper: works only for 26 english letters.
    constexpr char simple_tolower(char c) { return c < 'A' || c > 'Z' ? c : c + 'a' - 'A'; }
    constexpr char simple_toupper(char c) { return c < 'a' || c > 'z' ? c : c + 'A' - 'a'; }

    constexpr StateMachine buildStateMachine()
    {
        StateMachine sRes;
        state_t sState = 0;

        // Read prefix.
        {
            constexpr std::string_view sPrefix("HTTP/");
            for (char c : sPrefix)
            {
                Transition sFin = final(HttpResponseParser::ERROR_NOT_HTTP);
                sRes.m_Conditions[sState] = buildConditions(sFin, c, normal(sState + 1));
                sState++;
            }
        }

        // Read major and minor version.
        for (size_t i = 0; i < 2; i++)
        {
            fragment_t sSaveFragment = i == 0 ? HttpResponseParser::MAJOR_VERSION : HttpResponseParser::MINOR_VERSION;
            status_t sError = i == 0 ? HttpResponseParser::ERROR_NOT_A_DIGIT_MAJOR_VERSION : HttpResponseParser::ERROR_NOT_A_DIGIT_MINOR_VERSION;
            unsigned char sExitChar = i == 0 ? '.' : ' ';
            tag_t sTagBegin = HttpResponseParser::tagBegin(sSaveFragment);
            tag_t sTagEnd = HttpResponseParser::tagEnd(sSaveFragment);

            // First, required digit.
            sRes.m_Conditions[sState] = buildConditions(final(sError));
            for (unsigned char c = '0'; c <= '9'; c++)
                sRes.m_Conditions[sState].m_Transitions[c] = normal(sState + 1, sTagBegin);
            sState++;

            // More digits.
            sRes.m_Conditions[sState] = buildConditions(final(sError));
            for (unsigned char c = '0'; c <= '9'; c++)
                sRes.m_Conditions[sState].m_Transitions[c] = normal(sState);
            sRes.m_Conditions[sState].m_Transitions[sExitChar] = normal(sState + 1, sTagEnd);
            sState++;
        }

        // Read status code.
        {
            tag_t sTagBegin = HttpResponseParser::tagBegin(HttpResponseParser::STATUS_CODE);
            tag_t sTagEnd = HttpResponseParser::tagEnd(HttpResponseParser::STATUS_CODE);

            // Exactly 3 digits.
            for (size_t i = 0; i < 3; i++)
            {
                tag_t sTag = (0 == i) ? sTagBegin : HttpResponseParser::DUMMY_TAG;

                Transition sFin = final(HttpResponseParser::ERROR_NOT_A_DIGIT_STATUS_CODE);
                Transition sFin2 = final(HttpResponseParser::ERROR_WRONG_LENGTH_OF_STATUS_CODE);
                sRes.m_Conditions[sState] = buildConditions(sFin);
                for (unsigned char c = '0'; c <= '9'; c++)
                    sRes.m_Conditions[sState].m_Transitions[c] = normal(sState + 1, sTag);
                sRes.m_Conditions[sState].m_Transitions[' '] = sFin2;
                sState++;
            }

            // Must be a space.
            Transition sFin = final(HttpResponseParser::ERROR_WRONG_LENGTH_OF_STATUS_CODE);
            sRes.m_Conditions[sState] = buildConditions(sFin, ' ', normal(sState + 1, sTagEnd));
            sState++;
        }

        // Read reason phrase.
        {
            tag_t sTagBegin = HttpResponseParser::tagBegin(HttpResponseParser::REASON_PHRASE);
            tag_t sTagEnd = HttpResponseParser::tagEnd(HttpResponseParser::REASON_PHRASE);

            // Ignore leading and trailing whitespaces.
            // From RFC: optional whitespaces OWS = *( SP / HTAB ),
            // but we also ignore single '/r' and '/n'.

            // Below NS - non-space.
            state_t sWaitNS = sState++;
            state_t sWaitNSLF = sState++;
            state_t sFoundNS = sState++;
            state_t sTralingWS = sState++;
            state_t sTralingCR = sState++;
            state_t sNextLine = sState;

            // Skip all whitespaces, wait for first non-space char.
            sRes.m_Conditions[sWaitNS] = buildConditions(normal(sFoundNS, sTagBegin));
            sRes.m_Conditions[sWaitNS].m_Transitions[' '] = normal(sWaitNS);
            sRes.m_Conditions[sWaitNS].m_Transitions['\t'] = normal(sWaitNS);
            sRes.m_Conditions[sWaitNS].m_Transitions['\r'] = normal(sWaitNSLF);
            sRes.m_Conditions[sWaitNS].m_Transitions['\n'] = normal(sWaitNS);

            // Same as above, but previous char is '\r', so '\n' means empty string.
            sRes.m_Conditions[sWaitNSLF] = buildConditions(normal(sFoundNS, sTagBegin));
            sRes.m_Conditions[sWaitNSLF].m_Transitions[' '] = normal(sWaitNS);
            sRes.m_Conditions[sWaitNSLF].m_Transitions['\t'] = normal(sWaitNS);
            sRes.m_Conditions[sWaitNSLF].m_Transitions['\r'] = normal(sWaitNSLF);
            sRes.m_Conditions[sWaitNSLF].m_Transitions['\n'] = normal(sNextLine);

            // Non-whitespace have been found, now looking for whitespace.
            sRes.m_Conditions[sFoundNS] = buildConditions(normal(sFoundNS));
            sRes.m_Conditions[sFoundNS].m_Transitions[' '] = normal(sTralingWS, sTagEnd);
            sRes.m_Conditions[sFoundNS].m_Transitions['\t'] = normal(sTralingWS, sTagEnd);
            sRes.m_Conditions[sFoundNS].m_Transitions['\r'] = normal(sTralingCR, sTagEnd);
            sRes.m_Conditions[sFoundNS].m_Transitions['\n'] = normal(sTralingWS, sTagEnd);

            // Previous char is whitespace, look for non-space again or '\r'.
            sRes.m_Conditions[sTralingWS] = buildConditions(normal(sFoundNS));
            sRes.m_Conditions[sTralingWS].m_Transitions[' '] = normal(sTralingWS);
            sRes.m_Conditions[sTralingWS].m_Transitions['\t'] = normal(sTralingWS);
            sRes.m_Conditions[sTralingWS].m_Transitions['\r'] = normal(sTralingCR);
            sRes.m_Conditions[sTralingWS].m_Transitions['\n'] = normal(sTralingWS);

            // Previous char is '\r', same as above except exit on '\n'.
            sRes.m_Conditions[sTralingCR] = buildConditions(normal(sFoundNS));
            sRes.m_Conditions[sTralingCR].m_Transitions[' '] = normal(sTralingWS);
            sRes.m_Conditions[sTralingCR].m_Transitions['\t'] = normal(sTralingWS);
            sRes.m_Conditions[sTralingCR].m_Transitions['\r'] = normal(sTralingCR);
            sRes.m_Conditions[sTralingCR].m_Transitions['\n'] = normal(sNextLine);
        }

        // Read header values.
        // Following newest RFC ignore obsolete line folding.
        state_t sNewLine = sState++;
        state_t sSkipLine = sState++;
        state_t sSearchLF = sState++;
        state_t sSearchFinalLF = sState++;

        sRes.m_Conditions[sNewLine] = buildConditions(normal(sSkipLine), '\r', normal(sSearchFinalLF));
        sRes.m_Conditions[sSkipLine] = buildConditions(normal(sSkipLine), '\r', normal(sSearchLF));
        sRes.m_Conditions[sSearchLF] = buildConditions(normal(sSkipLine), '\r', normal(sSearchLF), '\n', normal(sNewLine));
        sRes.m_Conditions[sSearchFinalLF] = buildConditions(normal(sSkipLine), '\r', normal(sSearchLF), '\n', final(HttpResponseParser::SUCCESS));

        for (fragment_t sFrag = HttpResponseParser::SPECIAL_MAX; sFrag < HttpResponseParser::HEADER_MAX; sFrag++)
        {
            const std::string_view sName = m_HeaderNames[sFrag - HttpResponseParser::SPECIAL_MAX];
            state_t s = sNewLine;
            for (unsigned char c : sName)
            {
                if (sRes.m_Conditions[s].m_Transitions[c].m_State == sSkipLine)
                {
                    // Create new path.
                    state_t sNext = sState++;
                    sRes.m_Conditions[s].m_Transitions[simple_tolower(c)].m_State = sNext;
                    sRes.m_Conditions[s].m_Transitions[simple_toupper(c)].m_State = sNext;
                    sRes.m_Conditions[sNext] = buildConditions(normal(sSkipLine), '\r', normal(sSearchLF));
                }
                s = sRes.m_Conditions[s].m_Transitions[c].m_State;
            }
            sRes.m_Conditions[s].m_Transitions[':'].m_State = sState;

            // Read and store fragment until the end of line.
            tag_t sTagBegin = HttpResponseParser::tagBegin(sFrag);
            tag_t sTagEnd = HttpResponseParser::tagEnd(sFrag);

            // Ignore leading and trailing whitespaces.
            // From RFC: optional whitespaces OWS = *( SP / HTAB ),
            // but we also ignore single '/r' and '/n'.

            // Below NS - non-space.
            state_t sWaitNS = sState++;
            state_t sWaitNSLF = sState++;
            state_t sFoundNS = sState++;
            state_t sTralingWS = sState++;
            state_t sTralingCR = sState++;

            // Skip all whitespaces, wait for first non-space char.
            sRes.m_Conditions[sWaitNS] = buildConditions(normal(sFoundNS, sTagBegin));
            sRes.m_Conditions[sWaitNS].m_Transitions[' '] = normal(sWaitNS);
            sRes.m_Conditions[sWaitNS].m_Transitions['\t'] = normal(sWaitNS);
            sRes.m_Conditions[sWaitNS].m_Transitions['\r'] = normal(sWaitNSLF);
            sRes.m_Conditions[sWaitNS].m_Transitions['\n'] = normal(sWaitNS);

            // Same as above, but previous char is '\r', so '\n' means empty string.
            sRes.m_Conditions[sWaitNSLF] = buildConditions(normal(sFoundNS, sTagBegin));
            sRes.m_Conditions[sWaitNSLF].m_Transitions[' '] = normal(sWaitNS);
            sRes.m_Conditions[sWaitNSLF].m_Transitions['\t'] = normal(sWaitNS);
            sRes.m_Conditions[sWaitNSLF].m_Transitions['\r'] = normal(sWaitNSLF);
            sRes.m_Conditions[sWaitNSLF].m_Transitions['\n'] = normal(sNewLine);

            // Non-whitespace have been found, now looking for whitespace.
            sRes.m_Conditions[sFoundNS] = buildConditions(normal(sFoundNS));
            sRes.m_Conditions[sFoundNS].m_Transitions[' '] = normal(sTralingWS, sTagEnd);
            sRes.m_Conditions[sFoundNS].m_Transitions['\t'] = normal(sTralingWS, sTagEnd);
            sRes.m_Conditions[sFoundNS].m_Transitions['\r'] = normal(sTralingCR, sTagEnd);
            sRes.m_Conditions[sFoundNS].m_Transitions['\n'] = normal(sTralingWS, sTagEnd);

            // Previous char is whitespace, look for non-space again or '\r'.
            sRes.m_Conditions[sTralingWS] = buildConditions(normal(sFoundNS));
            sRes.m_Conditions[sTralingWS].m_Transitions[' '] = normal(sTralingWS);
            sRes.m_Conditions[sTralingWS].m_Transitions['\t'] = normal(sTralingWS);
            sRes.m_Conditions[sTralingWS].m_Transitions['\r'] = normal(sTralingCR);
            sRes.m_Conditions[sTralingWS].m_Transitions['\n'] = normal(sTralingWS);

            // Previous char is '\r', same as above except exit on '\n'.
            sRes.m_Conditions[sTralingCR] = buildConditions(normal(sFoundNS));
            sRes.m_Conditions[sTralingCR].m_Transitions[' '] = normal(sTralingWS);
            sRes.m_Conditions[sTralingCR].m_Transitions['\t'] = normal(sTralingWS);
            sRes.m_Conditions[sTralingCR].m_Transitions['\r'] = normal(sTralingCR);
            sRes.m_Conditions[sTralingCR].m_Transitions['\n'] = normal(sNewLine);
        }

        sRes.m_NumConditions = sState;
        return sRes;
    }

    constexpr StateMachine theStateMachine = buildStateMachine();

    static_assert(theStateMachine.m_NumConditions <= theStateMachine.MAX_NUM_CONDITIONS, "Overflow?");
    static_assert(theStateMachine.m_NumConditions > theStateMachine.MAX_NUM_CONDITIONS / 2, "Underflow?");
} // namespace {

const StateMachine& HttpResponseParser::TheStateMachine = theStateMachine;

const std::string_view HttpResponseParser::getErrorStr(status_t s) { return m_StatusErrors[s]; }
