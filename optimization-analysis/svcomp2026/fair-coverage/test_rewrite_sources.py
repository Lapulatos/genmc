#!/usr/bin/env python3
"""Focused fail-closed tests for the fair-coverage source rewriter."""

from __future__ import annotations

import unittest

from rewrite_sources import rewrite_text


class ReachErrorNormalizationTest(unittest.TestCase):
    def test_normalizes_source_assert_endpoint_and_removes_fallback_abort(self) -> None:
        source = r'''
extern void abort(void);
#include <assert.h>
void reach_error() { assert(0); }
void check(int value) { if (!value) { reach_error(); abort(); } }
'''
        result = rewrite_text(source, add_runtime_include=False)
        self.assertIn("direct-assert-reach-error", result.rules)
        self.assertIn("terminal-reach-error-abort", result.rules)
        self.assertIn("__VERIFIER_assert_fail", result.text)
        self.assertNotIn("reach_error(); abort();", result.text)

    def test_removes_only_abort_immediately_after_normalized_reach_error(self) -> None:
        source = r'''
extern void abort(void);
void reach_error() { __assert_fail("0", "case.c", 7, "reach_error"); }
void check(int value) { if (!value) { reach_error(); abort(); } }
'''
        result = rewrite_text(source, add_runtime_include=False)
        self.assertIn("direct-reach-error", result.rules)
        self.assertIn("terminal-reach-error-abort", result.rules)
        self.assertIn("reach_error();", result.text)
        self.assertNotIn("reach_error(); abort();", result.text)
        self.assertIn("extern void abort(void);", result.text)

    def test_preserves_abort_when_reach_error_definition_is_not_normalized(self) -> None:
        source = r'''
extern void abort(void);
void reach_error() { custom_error(); }
void check(int value) { if (!value) { reach_error(); abort(); } }
'''
        result = rewrite_text(source, add_runtime_include=False)
        self.assertNotIn("direct-reach-error", result.rules)
        self.assertNotIn("terminal-reach-error-abort", result.rules)
        self.assertIn("reach_error(); abort();", result.text)

    def test_preserves_unrelated_abort(self) -> None:
        source = r'''
extern void abort(void);
void reach_error() { __assert_fail("0", "case.c", 7, "reach_error"); }
void check(int value) { if (!value) reach_error(); else abort(); }
'''
        result = rewrite_text(source, add_runtime_include=False)
        self.assertIn("direct-reach-error", result.rules)
        self.assertNotIn("terminal-reach-error-abort", result.rules)
        self.assertIn("else abort();", result.text)


if __name__ == "__main__":
    unittest.main()
