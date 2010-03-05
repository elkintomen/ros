#!/usr/bin/env python
# Software License Agreement (BSD License)
#
# Copyright (c) 2009, Willow Garage, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials provided
#    with the distribution.
#  * Neither the name of Willow Garage, Inc. nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
import roslib; roslib.load_manifest('test_roslib')

import os
import sys
import unittest

import roslib.masterapi
import rostest

_ID = '/caller_id'

class MasterApiOnlineTest(unittest.TestCase):
  
    def setUp(self):
        self.m = roslib.masterapi.Master(_ID)

    def test_getPid(self):
        val = self.m.getPid()
        val = int(val)

    def test_lookupService(self):
        uri = 'http://localhost:897'
        rpcuri = 'rosrpc://localhost:9812'
        self.m.registerService('/bar/service', rpcuri, uri)
        self.assertEquals(rpcuri, self.m.lookupService('/bar/service'))
        try:
            self.assertEquals(uri, self.m.lookupService('/fake/service'))
            self.fail("should have thrown")
        except roslib.masterapi.Error:
            pass

    def test_registerService(self):
        self.m.registerService('/bar/service', 'rosrpc://localhost:9812', 'http://localhost:893')

    def test_unregisterService(self):
        self.m.registerService('/unreg_service/service', 'rosrpc://localhost:9812', 'http://localhost:893')
        val = self.m.registerService('/unreg_service/service', 'rosrpc://localhost:9812', 'http://localhost:893')
        self.assertEquals(1, val)
        
    def test_registerSubscriber(self):
        val = self.m.registerSubscriber('/reg_sub/node', 'std_msgs/String', 'http://localhost:9812')
        self.assertEquals([], val)

    def test_unregisterSubscriber(self):
        self.m.registerSubscriber('/reg_unsub/node', 'std_msgs/String', 'http://localhost:9812')
        val = self.m.unregisterSubscriber('/reg_unsub/node', 'http://localhost:9812')
        self.assertEquals(1, val)

    def test_registerPublisher(self):
        val = self.m.registerPublisher('/reg_pub/topic', 'std_msgs/String', 'http://localhost:9812')

    def test_unregisterPublisher(self):
        uri = 'http://localhost:9812'
        self.m.registerPublisher('/unreg_pub/fake_topic', 'std_msgs/String', uri)
        self.m.unregisterPublisher('/unreg_pub/fake_topic', uri)

    def test_lookupNode(self):
        # register and lookup self
        uri = 'http://localhost:12345'
        self.m.registerPublisher('fake_topic', 'std_msgs/String', uri)
        self.assertEquals(uri, self.m.lookupNode(_ID))
        
        try:
            self.m.lookupNode('/non/existent')
            self.fail("should have thrown")
        except roslib.masterapi.Error:
            pass
        
    def test_getPublishedTopics(self):
        topics = self.m.getPublishedTopics('/')
        
    def test_getSystemState(self):
        pub, sub, srvs = self.m.getSystemState()

if __name__ == '__main__':
    rostest.rosrun('test_roslib', 'test_roslib_masterapi_online', MasterApiOnlineTest)
