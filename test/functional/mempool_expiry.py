#!/usr/bin/env python3
# Copyright (c) 2020-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Tests that the deprecated -mempoolexpiry option emits a warning and has no effect."""

from test_framework.test_framework import BitcoinTestFramework

EXPIRY_WARNING = "Option '-mempoolexpiry' is set but transaction expiry from the mempool has been removed. This option has no effect."


class MempoolExpiryTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def run_test(self):
        self.log.info("Test that -mempoolexpiry emits a deprecation warning")
        self.stop_node(0)
        with self.nodes[0].assert_debug_log([EXPIRY_WARNING]):
            self.start_node(0, extra_args=["-mempoolexpiry=10"])
        self.stop_node(0, expected_stderr=f"Warning: {EXPIRY_WARNING}")

        self.log.info("Test that omitting -mempoolexpiry emits no warning")
        with self.nodes[0].assert_debug_log([], unexpected_msgs=[EXPIRY_WARNING]):
            self.start_node(0)


if __name__ == '__main__':
    MempoolExpiryTest(__file__).main()
