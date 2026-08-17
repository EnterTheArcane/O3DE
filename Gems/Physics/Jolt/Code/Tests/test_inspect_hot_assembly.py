#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

import unittest

import inspect_hot_assembly


class InspectHotAssemblyTests(unittest.TestCase):
    def test_counts_stack_calls_branches_conversions_vectors_and_copies(self):
        result = inspect_hot_assembly.analyze_function(
            "Jolt::World::StepDetailed",
            [
                "  1000: sub rsp, 0x80",
                "  1004: call 0x2000 <Jolt::World::Update(float)>",
                "  1008: jne 0x1010",
                "  100c: vcvtsi2ss xmm0, xmm1, eax",
                "  1010: rep movsq qword ptr [rdi], qword ptr [rsi]",
            ],
        )

        self.assertEqual(result["stack_frame_bytes"], 0x80)
        self.assertEqual(result["call_count"], 1)
        self.assertEqual(result["branch_count"], 1)
        self.assertEqual(result["conversion_count"], 1)
        self.assertEqual(result["vector_instruction_count"], 1)
        self.assertEqual(result["copy_candidate_count"], 1)

    def test_counts_att_stack_allocation_and_saved_registers(self):
        result = inspect_hot_assembly.analyze_function(
            "Jolt::World::GetPerformanceStatistics",
            [
                "  1000: pushq %r15",
                "  1002: pushq %r14",
                "  1004: subq $0x3c8, %rsp",
            ],
        )

        self.assertEqual(result["stack_frame_bytes"], 0x3C8 + 16)


if __name__ == "__main__":
    unittest.main()
