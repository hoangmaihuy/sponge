#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <cassert>
#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    auto addr_entry = _arp_table.find(next_hop_ip);
    if (addr_entry == _arp_table.end() || addr_entry->second.second < _timer) {
        auto it = _dgram_queue_map.find(next_hop_ip);
        if (it == _dgram_queue_map.end())
            _dgram_queue_map[next_hop_ip] = queue<pair<InternetDatagram, size_t>>();
        _dgram_queue_map[next_hop_ip].push(make_pair(dgram, _timer));
        _arp_request(next_hop_ip);
        return;
    }
    EthernetAddress eth_addr = addr_entry->second.first;
    _send_datagram(dgram, eth_addr);
}

void NetworkInterface::_send_datagram(const InternetDatagram &dgram, const EthernetAddress &dst) {
    cerr << "send datagram\n";
    EthernetHeader header{dst, _ethernet_address, EthernetHeader::TYPE_IPv4};
    EthernetFrame frame;
    frame.header() = header;
    frame.payload() = dgram.serialize();
    _frames_out.push(frame);
}

void NetworkInterface::_send_queued_datagram(const EthernetAddress &eth_addr, uint32_t ip_addr) {
    auto it = _dgram_queue_map.find(ip_addr);
    if (it == _dgram_queue_map.end())
        return;
    auto &dgram_queue = it->second;
    while (!dgram_queue.empty()) {
        const pair<InternetDatagram, size_t> &dgram = dgram_queue.front();
//        if (dgram.second + dgram.first.header().ttl <= _timer)
        _send_datagram(dgram.first, eth_addr);
        dgram_queue.pop();
    }
    _dgram_queue_map.erase(it);
}

void NetworkInterface::_arp_request(uint32_t ip_addr) {
    //    cerr << "arg request\n";
    auto arp_entry = _arp_sent_at.find(ip_addr);
    if (arp_entry != _arp_sent_at.end() && _timer - arp_entry->second <= ARP_REQUEST_TIMEOUT)
        return;
    _arp_sent_at[ip_addr] = _timer;
    EthernetHeader header{ETHERNET_BROADCAST, _ethernet_address, EthernetHeader::TYPE_ARP};
    EthernetFrame frame;
    frame.header() = header;
    ARPMessage arp_msg;
    arp_msg.sender_ethernet_address = _ethernet_address;
    arp_msg.sender_ip_address = _ip_address.ipv4_numeric();
    arp_msg.target_ethernet_address = EthernetAddress{};
    arp_msg.target_ip_address = ip_addr;
    arp_msg.opcode = ARPMessage::OPCODE_REQUEST;
    frame.payload() = arp_msg.serialize();
    _frames_out.push(frame);
}

void NetworkInterface::_arp_reply(const EthernetAddress &dst_eth_addr, uint32_t dst_ip_addr) {
    //    cerr << "arp reply\n";
    EthernetHeader header{dst_eth_addr, _ethernet_address, EthernetHeader::TYPE_ARP};
    EthernetFrame frame;
    frame.header() = header;
    ARPMessage arp_msg;
    arp_msg.sender_ethernet_address = _ethernet_address;
    arp_msg.sender_ip_address = _ip_address.ipv4_numeric();
    arp_msg.target_ethernet_address = dst_eth_addr;
    arp_msg.target_ip_address = dst_ip_addr;
    arp_msg.opcode = ARPMessage::OPCODE_REPLY;
    frame.payload() = arp_msg.serialize();
    _frames_out.push(frame);
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    //    cerr << "recv_frame\n";
    EthernetHeader header = frame.header();
    bool is_target_address = header.dst == _ethernet_address || header.dst == ETHERNET_BROADCAST;
    if (!is_target_address)
        return {};
    if (header.type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp_msg;
        ParseResult parse_result = arp_msg.parse(frame.payload());
        if (parse_result != ParseResult::NoError)
            return {};
        auto src_ip_addr = arp_msg.sender_ip_address;
        auto src_eth_addr = arp_msg.sender_ethernet_address;
        auto dst_ip_addr = arp_msg.target_ip_address;
        _arp_table.insert(make_pair(src_ip_addr, make_pair(src_eth_addr, _timer + ARP_ENTRY_TIMEOUT)));
        _arp_sent_at.erase(src_ip_addr);
        if (arp_msg.opcode == ARPMessage::OPCODE_REQUEST && dst_ip_addr == _ip_address.ipv4_numeric()) {
            _arp_reply(src_eth_addr, src_ip_addr);
        } else if (arp_msg.opcode == ARPMessage::OPCODE_REPLY) {
            _send_queued_datagram(src_eth_addr, src_ip_addr);
        }
        return {};
    }

    if (header.type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram dgram;
        ParseResult parse_result = dgram.parse(frame.payload());
        if (parse_result != ParseResult::NoError)
            return {};
        return dgram;
    }

    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { _timer += ms_since_last_tick; }
