#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    auto header = seg.header();
    _time_since_last_segment_received = 0;
    if (header.rst) {
//        cerr << "received reset connection\n";
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        return;
    }
    _receiver.segment_received(seg);
    if (header.ack) {
        _sender.ack_received(header.ackno, header.win);
    }
//    cerr << "seg received: " << _receiver.stream_out().input_ended() << " " << _sender.is_fin_sent() << "\n";
    if (_receiver.stream_out().input_ended() && !_sender.is_fin_sent()) {
//        cerr << "No linger after finish\n";
        _linger_after_streams_finish = false;
    }
    if (_receiver.ackno().has_value() &&
        (seg.length_in_sequence_space() || header.seqno == _receiver.ackno().value() - 1)) {
        _sender.send_empty_segment();
        send_segments();
    }
}

bool TCPConnection::active() const {
    if (_receiver.stream_out().error() || _sender.stream_in().error())
        return false;
    auto is_inbound_ended = _receiver.stream_out().eof() && !_receiver.unassembled_bytes();
    auto is_outbound_ended = _sender.stream_in().eof() && !_sender.bytes_in_flight();
    return !is_inbound_ended || !is_outbound_ended || !_sender.is_fin_sent() || _linger_after_streams_finish;
}

size_t TCPConnection::write(const string &data) {
    auto n = _sender.stream_in().write(data);
    _sender.fill_window();
    send_segments();
    return n;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
//    cerr << "tick " << ms_since_last_tick << "\n";
    _sender.tick(ms_since_last_tick);
    _time_since_last_segment_received += ms_since_last_tick;
    if (_sender.next_seqno_absolute() && active())
        _sender.fill_window();
    send_segments();
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
//        cerr << "abort connection...\n";
        reset_connection();
        return;
    }
    if (!_receiver.stream_out().eof() || unassembled_bytes() || !_sender.stream_in().eof() || bytes_in_flight() ||
        !_linger_after_streams_finish)
        return;

    if (_time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
        _linger_after_streams_finish = false;
        return;
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_segments();
}

void TCPConnection::connect() {
    _sender.fill_window();
    send_segments();
}

void TCPConnection::send_segments() {
    while (!_sender.segments_out().empty()) {
        auto seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (!seg.header().rst) {
            seg.header().ack = _receiver.ackno().has_value();
            if (seg.header().ack)
                seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        _segments_out.push(seg);
//        cerr << "sent segment with header:\n"
//             << seg.header().to_string() << "Payload size: " << seg.payload().size() << "\n";
    }
}

void TCPConnection::reset_connection() {
//    cerr << "reset connection" << "\n";
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _sender.send_empty_segment(true);
    send_segments();
}

TCPConnection::~TCPConnection() {
    try {
//        cerr << "close connection\n";
        if (active()) {
//            cerr << "unclean shutdown " << active() << " " << _linger_after_streams_finish << "\n";
            reset_connection();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
