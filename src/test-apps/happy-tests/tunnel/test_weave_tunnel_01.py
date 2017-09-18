#!/usr/bin/env python
#
#       Copyright (c) 2015-2017  Nest Labs, Inc.
#       All rights reserved.
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

#
#    @file
#       Calls Weave Tunnel between nodes.
#

import itertools
import os
import sys
import unittest
import set_test_path
import time

from happy.Utils import *
import happy.HappyNodeList
import plugins.weave.WeaveStateLoad as WeaveStateLoad
import plugins.weave.WeaveStateUnload as WeaveStateUnload
import plugin.WeaveTunnelStart as WeaveTunnelStart
import plugin.WeaveTunnelStop as WeaveTunnelStop
import plugin.WeaveUtilities as WeaveUtilities

class test_weave_tunnel_01(unittest.TestCase):
    def setUp(self):
        self.tap = None

        if "WEAVE_SYSTEM_CONFIG_USE_LWIP" in os.environ.keys() and os.environ["WEAVE_SYSTEM_CONFIG_USE_LWIP"] == "1":
            self.topology_file = os.path.dirname(os.path.realpath(__file__)) + \
                "/../topologies/thread_wifi_on_tap_ap_service.json"
            self.tap = "wpan0"
        else:
            self.topology_file = os.path.dirname(os.path.realpath(__file__)) + \
                "/../topologies/thread_wifi_ap_service.json"

        self.show_strace = False

        # setting Mesh for thread test
        options = WeaveStateLoad.option()
        options["quiet"] = True
        options["json_file"] = self.topology_file

        setup_network = WeaveStateLoad.WeaveStateLoad(options)
        ret = setup_network.run()

        # Wait for a second to ensure that Weave ULA addresses passed dad
        # and are no longer tentative
        time.sleep(2)


    def tearDown(self):
        # cleaning up
        options = WeaveStateUnload.option()
        options["quiet"] = True
        options["json_file"] = self.topology_file

        teardown_network = WeaveStateUnload.WeaveStateUnload(options)
        teardown_network.run()


    def test_weave_tunnel(self):
        # TODO: Once LwIP bugs are fix, enable this test on LwIP
        if "WEAVE_SYSTEM_CONFIG_USE_LWIP" in os.environ.keys() and os.environ["WEAVE_SYSTEM_CONFIG_USE_LWIP"] == "1":
            return

        # topology has nodes: ThreadNode, BorderRouter, onhub and cloud
        # we run tunnel between BorderRouter and cloud

        # Start tunnel
        value, data = self.__start_tunnel_between("BorderRouter", "cloud")

        time.sleep(1)

        # Stop tunnel
        value, data = self.__stop_tunnel_between("BorderRouter", "cloud")


    def __start_tunnel_between(self, gateway, service):
        options = WeaveTunnelStart.option()
        options["quiet"] = False
        options["border_gateway"] = gateway
        options["service"] = service
        options["tap"] = self.tap

        weave_tunnel = WeaveTunnelStart.WeaveTunnelStart(options)
        ret = weave_tunnel.run()

        value = ret.Value()
        data = ret.Data()

        return value, data


    def __stop_tunnel_between(self, gateway, service):
        options = WeaveTunnelStop.option()
        options["quiet"] = False
        options["border_gateway"] = gateway
        options["service"] = service

        weave_tunnel = WeaveTunnelStop.WeaveTunnelStop(options)
        ret = weave_tunnel.run()

        value = ret.Value()
        data = ret.Data()

        return value, data


if __name__ == "__main__":
    WeaveUtilities.run_unittest()


