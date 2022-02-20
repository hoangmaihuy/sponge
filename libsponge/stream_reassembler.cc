#include "stream_reassembler.hh"

#include <cassert>
#include <iostream>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

bool operator<(const Substring &s1, const Substring &s2) {
    return s1.index < s2.index;
}

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), first_index(0), queue(multiset<Substring>()) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t last_index = index + data.size();
    string _data = data;
    bool _eof = eof;
    size_t remaining = _output.remaining_capacity();
    // Silently discard overflow
    if (last_index > first_index + remaining) {
        size_t len = data.size() - (last_index - first_index - remaining);
        _data = _data.substr(0, len);
        _eof = false;
    }
    if (index + _data.size() < first_index)
        return;
    queue.insert(Substring{index, _data, _eof});
    reassemble();
}

void StreamReassembler::reassemble() {
    while (!queue.empty()) {
        assert(!_output.input_ended());
        auto s = *queue.begin();
        if (s.index <= first_index) {
            if (s.index + s.data.length() < first_index) {
                queue.erase(queue.begin());
            } else {
                size_t len = s.data.length() - first_index + s.index;
                auto data = s.data.substr(first_index - s.index, len);
                _output.write(data);
                first_index += len;
                if (s.eof)
                    _output.end_input();
                queue.erase(queue.begin());
            }
        } else {
            break;
        }
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t last_index = first_index;
    size_t count = 0;
    for (const auto &s : queue) {
        size_t len = s.data.length();
        if (s.index + len > last_index) {
            if (s.index > last_index)
                count += len;
            else
                count += s.index + len - last_index;
            last_index = s.index + len;
        }
    }
    return count;
}

bool StreamReassembler::empty() const { return queue.empty() && _output.buffer_empty(); }
