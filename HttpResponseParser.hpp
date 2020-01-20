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

#include <array>
#include <climits>
#include <string_view>

// RFC7230 status line and header parser of an HTTP response.
// The parser is implemented for the fastest parsing of incoming byte stream, so
// it's implemented as a state machine with no allocations and branchless state transitions.
// The parser stores a fixed number of tag pairs. Each tag is an offset (byte number)
// in the input stream; a pair of tags thus represents a fragment in the input stream in standard
// way: offset of the first byte and offset of the next after the last byte of the fragment.

class HttpResponseParser
{
public:
    // Standard fragments that will be stored.
    enum special_t
    {
        MAJOR_VERSION = 0,
        MINOR_VERSION,
        STATUS_CODE,
        REASON_PHRASE,
        SPECIAL_MAX
    };

    // Header-value fragments to store.
    // If you need any other header to store, just add into that enum
    //  any desired header name and fix corresponding m_HeaderNames in cpp file.
    enum header_t
    {
        CONTENT_TYPE = SPECIAL_MAX,
        CONTENT_LENGTH,
        TRANSFER_ENCODING,
        LOCATION,
        HEADER_MAX
    };

    // Type of ID of fragment. One of header_t or special_t.
    using fragment_t = uint16_t;
    // Type of a tag ID - beginning or end position of a fragment.
    using tag_t = fragment_t;
    // Type of ID of a parsing state.
    using state_t = uint32_t;
    // Type of result of parsing one byte of the stream.
    using status_t = uint16_t;
    enum status_value_t
    {
        IN_PROGRESS = 0,
        SUCCESS,
        ERROR_NOT_HTTP,
        ERROR_NOT_A_DIGIT_MAJOR_VERSION,
        ERROR_NOT_A_DIGIT_MINOR_VERSION,
        ERROR_NOT_A_DIGIT_STATUS_CODE,
        ERROR_WRONG_LENGTH_OF_STATUS_CODE,
        STATUS_END,
    };

    // Feed the parser another character.
    // If it is the last character in the header - return SUCCESS.
    // If the character sequence is invalid - return appropriate error.
    // Otherwise - return 0. Only in this case further feeding is allowed.
    inline status_t feed(char c);

    // Return number of eaten characters.
    inline size_t count() const;

    // Reset parsing state to the initial.
    inline void reset();

    // Get a description of error status. Actually it's a null-terminating string.
    static const std::string_view getErrorStr(status_t s);

    // Check that a fragment (special_t or header_t) was found in the input stream.
    inline bool isFragmentFound(fragment_t aFragment) const;
    // Get begin and end offsets of a fragment (return (0, 0) if not found after a successful parsing).
    // isFragmentFound must be checked before using that if the parsing was not finished successfully!
    inline std::pair<size_t, size_t> getFragment(fragment_t aFragment) const;
    // Wrapper that extracts fragment substring from whole stream (empty if not found).
    inline std::string_view getFragmentStr(std::string_view sInput, fragment_t aFragment);


    //////////////////////////////////// PRIVATE BELOW ////////////////////////////////////
    //////////////////////////////////// PRIVATE BELOW ////////////////////////////////////
    //////////////////////////////////// PRIVATE BELOW ////////////////////////////////////
    // All staff below must be actually a private part of a class.
    // But that would significantly complicate constexpr building of the state machine in cpp file.
    // I think that public internals is lesser evil than huge header file or extern friend functions/classes.
//private:

    // Total number of tags.
    static constexpr size_t NUM_TAGS = HEADER_MAX * 2 + 1;
    // Storage offset for the beginning of the fragment.
    static constexpr tag_t tagBegin(fragment_t frag) { return (frag * 2) + 1; }
    // Storage offset for the end of the fragment.
    static constexpr tag_t tagEnd  (fragment_t frag) { return (frag + 1) * 2; }
    // Special offseat in a storage that stores useless thing; private part of implementation.
    static constexpr tag_t DUMMY_TAG = 0;

    // State machine!

    // Transition from one state to another.
    struct Transition
    {
        // This transition leads to that state.
        state_t m_State = 0;
        // In that tag the current position must be saved.
        tag_t m_Tag = DUMMY_TAG;
        // Status after this transition: zero if more bytes are needed,
        // nonzero if this is a final transition (SUCCESS or some ERROR..).
        status_t m_Status = 0;
    };

    // A set of transitions by each input byte.
    struct Conditions
    {
        // Transitions by each input byte.
        static constexpr size_t NUM_TRANSITIONS = 1 << CHAR_BIT;
        std::array<Transition, NUM_TRANSITIONS> m_Transitions;

        const Transition& operator[](unsigned char c) const { return m_Transitions[c]; }
    };

    // Conditions of each state. The start in state 0.
    struct StateMachine
    {
        static constexpr state_t MAX_NUM_CONDITIONS = 128;
        state_t m_NumConditions = 0;
        std::array<Conditions, MAX_NUM_CONDITIONS> m_Conditions = {};

        const Conditions& operator[](state_t s) const { return m_Conditions[s]; }
    };

private:

    // The instance of the state machine.
    // It could be made as static constexpr instance but that would make us
    //  to include all the construction to the header.
    // Let it be extern reference to a constexpr instance.
    static const StateMachine& TheStateMachine;

    // Variables of parsing state.
    // State in state machine.
    state_t m_CurrentState = 0;
    // Number of characters that was consumed.
    state_t m_CurrentPos = 0;
    // Saved tag positions.
    std::array<size_t, NUM_TAGS> m_SavedTagOffsets = {};
};

//////////////////////////////////// IMPLEMENTATION ////////////////////////////////////
HttpResponseParser::status_t HttpResponseParser::feed(char c)
{
    const Transition& t = TheStateMachine[m_CurrentState][c];
    m_CurrentState = t.m_State;
    m_SavedTagOffsets[t.m_Tag] = m_CurrentPos++;
    return t.m_Status;
}

size_t HttpResponseParser::count() const
{
    return m_CurrentPos;
}

void HttpResponseParser::reset()
{
    m_CurrentState = 0;
    m_CurrentPos = 0;
    m_SavedTagOffsets.fill(0);
}

bool HttpResponseParser::isFragmentFound(fragment_t aFragment) const
{
    // Due to HTTP prefix no fragment can start with zero offset.
    // It is guaranteed that if the end of a fragment is set then the beginning is also set.
    return m_SavedTagOffsets[tagEnd(aFragment)] != 0;
}

std::pair<size_t, size_t> HttpResponseParser::getFragment(fragment_t aFragment) const
{
    size_t b =  m_SavedTagOffsets[tagBegin(aFragment)];
    size_t e = m_SavedTagOffsets[tagEnd(aFragment)];
    return {b, e};
}

std::string_view HttpResponseParser::getFragmentStr(std::string_view sInput, fragment_t aFragment)
{
    size_t b =  m_SavedTagOffsets[tagBegin(aFragment)];
    size_t e = m_SavedTagOffsets[tagEnd(aFragment)];
    return sInput.substr(b, e - b);
}
