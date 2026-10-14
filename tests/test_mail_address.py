#!/usr/bin/env python3
"""Shared mailbox admission without SMTP connections or Maya imports."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'python'))
from DPyTools.core import mail_address
from DPyTools.pipeline.assets import user_data
from DPyTools.integrations.smtp import client


class MailAddressTests(unittest.TestCase):
    def test_plain_address_preserves_text_and_rejects_display_names(self):
        self.assertEqual(mail_address.plain_address('artist@example.com'), 'artist@example.com')
        for value in ('Artist <artist@example.com>', 'artist', '', 'a@example.com,b@example.com'):
            with self.assertRaises(ValueError):
                mail_address.plain_address(value)

    def test_all_ascii_controls_are_rejected_before_whitespace_normalization(self):
        for code in tuple(range(32)) + (127,):
            for value in ('a' + chr(code) + 'b@example.com', chr(code) + 'a@example.com'):
                with self.subTest(code=code), self.assertRaises(ValueError):
                    mail_address.plain_address(value, strip=True)

    def test_whitespace_policy_preserves_registry_and_smtp_contracts(self):
        address = ' artist@example.com '
        with self.assertRaises(ValueError):
            user_data.validate_user_record({'id': 'artist', 'name': 'Artist', 'mail': address})
        self.assertEqual(client._validated_address(address, 'recipient'), 'artist@example.com')

    def test_length_boundary_is_shared(self):
        address = 'a' * (mail_address.MAX_ADDRESS_LENGTH - len('@example.com')) + '@example.com'
        self.assertEqual(mail_address.plain_address(address), address)
        for admit in (lambda: mail_address.plain_address('a' + address),
                      lambda: client._validated_address('a' + address, 'recipient'),
                      lambda: user_data.validate_user_record({'id': 'a', 'name': 'A', 'mail': 'a' + address})):
            with self.assertRaises((ValueError, client.MailConfigurationError)):
                admit()

    def test_types_and_whitespace_policy_are_explicit(self):
        for value in (None, True, 1, [], {}):
            with self.assertRaises(TypeError):
                mail_address.plain_address(value)
        for value in (None, 0, 1, 'true'):
            with self.assertRaises(TypeError):
                mail_address.plain_address('a@example.com', strip=value)

    def test_registry_and_smtp_use_the_same_admission_function(self):
        self.assertIs(user_data.plain_address, mail_address.plain_address)
        self.assertIs(client.plain_address, mail_address.plain_address)


if __name__ == '__main__':
    unittest.main()
