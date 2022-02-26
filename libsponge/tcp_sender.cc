#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <math.h>
#include <iostream>
#include <cassert>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

using namespace std;

bool operator<(const TransmittingSegment& a, const TransmittingSegment &b) {
    return a.seqno < b.seqno;
}

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _retransmission_timeout{retx_timeout}
    , _timer()
    , _stream(capacity) {}

size_t TCPSender::bytes_in_flight() const {
    size_t bytes_count = 0;
    for (const auto& segment : _segments_transmitting) {
        bytes_count += segment.tcp_segment.length_in_sequence_space();
    }
    return bytes_count;
}

void TCPSender::fill_window() {
//    cerr << "fill_window: " << _stream.eof() << " " << _stream.bytes_written() << " " << _next_seqno << " " << _window_size << "\n";
    // input ended, sent all
    size_t last_seqno = _stream.bytes_written() + _stream.input_ended() + 1;
    if (_next_seqno == last_seqno)
        return;
    uint16_t window_size = max(_window_size, uint16_t(1));
    size_t max_segment_size = _ackno + window_size - _next_seqno;
    if (!max_segment_size)
        return;
    TCPSegment segment;
    TCPHeader header;
    header.syn = !_next_seqno;
    size_t max_payload_size = min(max_segment_size - header.syn, TCPConfig::MAX_PAYLOAD_SIZE);
    Buffer payload = _stream.read(min(max_payload_size, _stream.buffer_size()));
    header.fin = _stream.eof() && header.syn + payload.size() < max_segment_size;
    header.seqno = wrap(_next_seqno, _isn);
    segment.header() = header;
    segment.payload() = payload;
    _segments_out.push(segment);
    if (_segments_transmitting.empty())
        _timer.reset(_retransmission_timeout);
    _segments_transmitting.insert(TransmittingSegment{_next_seqno, segment});
    _next_seqno += segment.length_in_sequence_space();
    fill_window();
}


//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    size_t raw_ackno = unwrap(ackno, _isn, _next_seqno);
    _window_size = window_size;
    if (raw_ackno > _next_seqno || raw_ackno <= _ackno) {
        assert(_segments_transmitting.empty() || _segments_transmitting.begin()->seqno < raw_ackno);
        return;
    }
    if (window_size)
        _retransmission_timeout = _initial_retransmission_timeout;
    _consecutive_retranmissions = 0;
    _ackno = raw_ackno;
    while (!_segments_transmitting.empty()) {
        auto segment = *_segments_transmitting.begin();
        auto last_seqno = segment.seqno + segment.tcp_segment.length_in_sequence_space();
        if (last_seqno <= raw_ackno) {
            _segments_transmitting.erase(_segments_transmitting.begin());
        } else {
            break;
        }
    }
    if (_segments_transmitting.empty())
        _timer.stop();
    else
        _timer.reset(_retransmission_timeout);
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_timer.is_running())
        return;
    _timer.passed(ms_since_last_tick);
    if (!_timer.is_timeout())
        return;
    _retransmit();
}

void TCPSender::_retransmit() {
    if (_segments_transmitting.empty())
        return;
//    cerr << "retransmitting...\n";
    if (_window_size) {
        _consecutive_retranmissions++;
        _retransmission_timeout *= 2;
    }
    auto segment = _segments_transmitting.begin()->tcp_segment;
    _segments_out.push(segment);
    _timer.reset(_retransmission_timeout);
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retranmissions; }

void TCPSender::send_empty_segment() {
    TCPHeader header;
    TCPSegment segment;
    header.seqno = wrap(_next_seqno, _isn);
    header.syn = !_next_seqno;
    header.fin = _stream.input_ended();
    segment.header() = header;
    segment.payload() = Buffer("");
    _segments_out.push(segment);
    _next_seqno += segment.length_in_sequence_space();
}
