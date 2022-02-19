#include <cassert>
#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : buffer(vector<char>(capacity)), is_eof(false), write_cnt(0), read_cnt(0), head(0), tail(0), remaining(capacity) {}

size_t ByteStream::next(size_t i, size_t step = 1) const { return (i + step) % buffer.size(); }

size_t ByteStream::write(const string &data) {
    size_t old_write_cnt = write_cnt;
    for (auto x : data) {
        if (remaining > 0) {
            remaining--;
            write_cnt++;
            buffer.at(tail) = x;
            tail = next(tail);
        } else {
            break;
        }
    }
    return write_cnt - old_write_cnt;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    assert(len <= buffer_size());
    string data;
    for (size_t i = head; data.size() < len; i = next(i)) {
        data.push_back(buffer.at(i));
    }
    return data;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    assert(len <= buffer_size());
    head = next(head, len);
    remaining += len;
    read_cnt += len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    assert(len <= buffer_size());
    string data = peek_output(len);
    pop_output(len);
    return data;
}

void ByteStream::end_input() { is_eof = true; }

bool ByteStream::input_ended() const { return is_eof; }

size_t ByteStream::buffer_size() const { return buffer.size() - remaining; }

bool ByteStream::buffer_empty() const { return remaining == buffer.size(); }

bool ByteStream::eof() const { return is_eof && buffer_empty(); }

size_t ByteStream::bytes_written() const { return write_cnt; }

size_t ByteStream::bytes_read() const { return read_cnt; }

size_t ByteStream::remaining_capacity() const { return remaining; }
