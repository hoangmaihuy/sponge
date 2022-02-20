#include "tcp_receiver.hh"

#include <cassert>
#include <iostream>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    bool eof = false;

    if (seg.header().syn) {
        assert(!syn_received);
        syn_received = true;
        isn = seg.header().seqno;
        checkpoint = 0;
    }

    if (!syn_received)
        return;

    if (seg.header().fin) {
        assert(syn_received);
        eof = true;
    }
    size_t index = unwrap(seg.header().seqno, isn, checkpoint);
    checkpoint = index;
    _reassembler.push_substring(seg.payload().copy(), index - !seg.header().syn, eof);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!syn_received)
        return {};
    size_t index = _reassembler.first_unassembled_index() + 1 + stream_out().input_ended();
    return wrap(index, isn);
}

size_t TCPReceiver::window_size() const { return stream_out().remaining_capacity(); }
