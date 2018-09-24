// This file is a part of the IncludeOS unikernel - www.includeos.org
//
// Copyright 2017-2018 IncludeOS AS, Oslo, Norway
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Autogenerated by NaCl

#include <iostream>
#include <net/inet>
#include <net/super_stack.hpp>
#include <net/ip4/cidr.hpp>
#include <net/router.hpp>
#include <net/vlan_manager.hpp>
#include <hw/devices.hpp>
#include <syslogd>

using namespace net;

namespace nacl {
  class Filter {
  public:
    virtual Filter_verdict<IP4> operator()(IP4::IP_packet_ptr pckt, Inet& stack, Conntrack::Entry_ptr ct_entry) = 0;
    virtual ~Filter() {}
  };
}

std::unique_ptr<Router<IP4>> nacl_router_obj;
std::shared_ptr<Conntrack> nacl_ct_obj;

namespace custom_made_classes_from_nacl {

class Another_Filter : public nacl::Filter {
public:
	Filter_verdict<IP4> operator()(IP4::IP_packet_ptr pckt, Inet& stack, Conntrack::Entry_ptr ct_entry) {
		if (not ct_entry) {
return {nullptr, Filter_verdict_type::DROP};
}
return {std::move(pckt), Filter_verdict_type::ACCEPT};

	}
};

class Eth0_Filter : public nacl::Filter {
public:
	Filter_verdict<IP4> operator()(IP4::IP_packet_ptr pckt, Inet& stack, Conntrack::Entry_ptr ct_entry) {
		if (not ct_entry) {
return {nullptr, Filter_verdict_type::DROP};
}
return {nullptr, Filter_verdict_type::DROP};

	}
};

} //< namespace custom_made_classes_from_nacl

void register_plugin_nacl() {
	INFO("NaCl", "Registering NaCl plugin");

	// vlan vlan1
	Super_stack::inet().create(VLAN_manager::get(0).add(hw::Devices::nic(0), 13), 0, 13);
	auto& vlan1 = Super_stack::get(0, 13);
	vlan1.network_config(IP4::addr{20,20,20,10}, IP4::addr{255,255,255,0}, 0);
	auto& eth1 = Super_stack::get(1);
	eth1.negotiate_dhcp(10.0, [&eth1] (bool timedout) {
		if (timedout) {
			INFO("NaCl plugin interface eth1", "DHCP request timed out. Nothing to do.");
			return;
		}
		INFO("NaCl plugin interface eth1", "IP address updated: %s", eth1.ip_addr().str().c_str());
	});
	auto& eth0 = Super_stack::get(0);
	eth0.network_config(IP4::addr{10,0,0,45}, IP4::addr{255,255,255,0}, IP4::addr{10,0,0,1}, IP4::addr{8,8,8,8});

	custom_made_classes_from_nacl::Another_Filter another_filter;
	custom_made_classes_from_nacl::Eth0_Filter eth0_filter;

	eth1.ip_obj().input_chain().chain.push_back(another_filter);

	eth0.ip_obj().prerouting_chain().chain.push_back(eth0_filter);

	// Router

	INFO("NaCl", "Setup routing");
	Router<IP4>::Routing_table routing_table {
		{ IP4::addr{10,0,0,0}, IP4::addr{255,255,255,0}, 0, eth0, 100 },
		{ IP4::addr{10,20,30,0}, IP4::addr{255,255,255,0}, 0, eth1, 50 }
	};
	nacl_router_obj = std::make_unique<Router<IP4>>(routing_table);
	// Set ip forwarding on every iface mentioned in routing_table
	eth0.set_forward_delg(nacl_router_obj->forward_delg());
	eth1.set_forward_delg(nacl_router_obj->forward_delg());

	// Ct

	nacl_ct_obj = std::make_shared<Conntrack>();
	
	nacl_ct_obj->reserve(10000);

	INFO("NaCl", "Enabling Conntrack on eth0");
	eth0.enable_conntrack(nacl_ct_obj);

	INFO("NaCl", "Enabling Conntrack on eth1");
	eth1.enable_conntrack(nacl_ct_obj);
}
