#include "router.hh"

#include <cassert>
#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    RouteDef route = make_pair(route_prefix, prefix_length);
    auto it = _route_table.find(route);
    if (it != _route_table.end()) {
        cerr << "DEBUG: route already existed\n";
        return;
    }
    _route_table[route] = make_pair(next_hop, interface_num);
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    if (dgram.header().ttl <= 1)
        return;
    uint32_t addr = dgram.header().dst;
    uint32_t prefix_mask = UINT32_MAX;
    for (int prefix_length = 32; prefix_length >= 0; prefix_length--) {
        uint32_t addr_prefix = addr & prefix_mask;
        prefix_mask <<= 1;
        auto route = _route_table.find(make_pair(addr_prefix, prefix_length));
        if (route == _route_table.end())
            continue;
        auto [next_hop_opt, interface_num] = route->second;
        dgram.header().ttl--;
        auto next_hop = next_hop_opt.value_or(Address::from_ipv4_numeric(dgram.header().dst));
        _interfaces[interface_num].send_datagram(dgram, next_hop);
        return;
    }
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
