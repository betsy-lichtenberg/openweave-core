#
#    Copyright (c) 2015-2017 Nest Labs, Inc.
#    All rights reserved.
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

import time
import os
import sys
import time
import WeaveDeviceMgr
import WeaveBleMgr
from WeaveBleMgr import FAKE_CONN_OBJ_VALUE
from datetime import datetime

class WeaveBleHelper:
    def __init__(self):
        self.devMgr = WeaveDeviceMgr.WeaveDeviceManager()
        self.bleMgr = WeaveBleMgr.BleManager(self.devMgr)
        self.hush_pass_count = 0
        self.hush_fail_count = 0
        self.latency_stats = []

        def HandleBleCloseCB(connObj):
            # Don't close the connection asynchronously
            return True

        self.devMgr.SetBleCloseCB(HandleBleCloseCB)

    def _getCurrentTimeStr(self):
        return "[" + datetime.now().isoformat()[11:-3] + "] "

    def bgScanStart(self, name):
        self.bleMgr.bgScanStart(name)

    def bgScanStop(self):
        self.bleMgr.bgScanStop()

    def bleHush(self, name, scan_timeout=60, ble_connect_retries=1):
        # Scan
        if self.bleMgr.bg_peripheral_name is None:
            self.bleMgr.scan("-t " + str(scan_timeout) + " " + name)
        elif len(self.bleMgr.peripheral_list) == 0:
            self.bleMgr.runLoopUntil(("scan", time.time(), scan_timeout, name))
        identifier = None
        for p in self.bleMgr.peripheral_list:
            if p.name() == name:
                identifier = p.identifier().UUIDString()
                break
        if identifier is None:
            print "Did not find advertisement"
            return False

        print self._getCurrentTimeStr() + 'Hush sequence started'
        connected_ble = False
        connected_weave = False
        hushed = False
        disconnected_weave = False
        disconnected_ble = False

        proximityVerificationCode = 0xdeadbeef
        challenge = 0xABCD0123
        keyId = 0x0002
        key = bytearray([0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00])

        # BLE connection
        connect_attempts = 0
        ble_connection_start_time = time.time()
        while self.bleMgr.connect_state == False and connect_attempts < ble_connect_retries + 1:
            self.bleMgr.connect(identifier)
            connect_attempts += 1
        if self.bleMgr.connect_state == False:
            print "Was not able to connect"
        else:
            connected_ble = True
            ble_connection_end_time = time.time()
            print self._getCurrentTimeStr() + "BLE connection complete: %f seconds" % (ble_connection_end_time - ble_connection_start_time)

        # Weave connection
        if connected_ble:
            weave_connection_start_time = time.time()
            try:
                self.devMgr.ConnectBle(bleConnection=FAKE_CONN_OBJ_VALUE,
                                       pairingCode=None,
                                       accessToken=None)
                connected_weave = True
                weave_connection_end_time = time.time()
                print self._getCurrentTimeStr() + "WEAVE connection complete: %f seconds" % (weave_connection_end_time - weave_connection_start_time)
            except WeaveDeviceMgr.DeviceManagerException, ex:
                print str(ex)

        # Hush
        if connected_weave:
            hush_start_time = time.time()
            try:
                hush_response = self.devMgr.Hush(proximityVerificationCode, challenge, keyId, key)
                if hush_response.hushResult == 0:
                    hushed = True
                hush_end_time = time.time()
                print self._getCurrentTimeStr() + "HUSH(%d) complete: %f seconds" % (hush_response.hushResult, hush_end_time - hush_start_time)
            except WeaveDeviceMgr.DeviceManagerException, ex:
                print str(ex)

        # Weave disconnection
        if connected_weave:
            close_start_time = time.time()
            try:
                self.devMgr.Close()
                self.devMgr.CloseEndpoints()
                self.bleMgr.runLoopUntil(("unsubscribe", time.time(), 5.0))
                disconnected_weave = True
                close_end_time = time.time()
                print self._getCurrentTimeStr() + "CLOSE complete: %f seconds" % (close_end_time - close_start_time)
            except WeaveDeviceMgr.DeviceManagerException, ex:
                print str(ex)

        # BLE disconnection
        if connected_ble:
            disconnect_start_time = time.time()
            self.bleMgr.disconnect()
            if self.bleMgr.connect_state == True:
                print "Was not able to disconnect"
            else:
                disconnected_ble = True
                disconnect_end_time = time.time()
                print self._getCurrentTimeStr() + "DISCONNECT complete: %f seconds" % (disconnect_end_time - disconnect_start_time)

        if connected_ble and connected_weave and hushed and disconnected_weave and disconnected_ble:
            self.hush_pass_count += 1
            stats = []
            stats.append(ble_connection_end_time - ble_connection_start_time)
            stats.append(weave_connection_end_time - weave_connection_start_time)
            stats.append(hush_end_time - hush_start_time)
            stats.append(close_end_time - close_start_time)
            stats.append(disconnect_end_time - disconnect_start_time)
            self.latency_stats.append(stats)
        else:
            self.hush_fail_count += 1

        return hushed

    def plotStats(self):
        import matplotlib.pyplot as plt
        fig, ax = plt.subplots()
        for stats in self.latency_stats:
            x = range(len(stats)+1)
            y = [0] + [sum(stats[:i+1]) for i in xrange(len(stats))]
            ax.plot(x,y)
        ax.set_xticks(range(len(stats)+1))
        ax.set_xticklabels(('', 'BLE connected', 'Weave connected', 'Hush responded', 'Weave closed', 'BLE disconnected'))
        ax.yaxis.set_label_text("seconds")
        ax.autoscale_view()
        plt.show()

