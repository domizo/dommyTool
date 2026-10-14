#!/usr/bin/env python3
"""Host-free regression checks for optional SMTP notification delivery."""

import smtplib
import socket
import ssl
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'python'))

from DPyTools.integrations.smtp import client


class FakeSMTP(object):
    instances = []

    def __init__(self, host, port, timeout=None):
        self.host = host
        self.port = port
        self.timeout = timeout
        self.login_args = None
        self.mail_args = None
        self.quit_called = False
        self.__class__.instances.append(self)

    def ehlo(self):
        pass

    def login(self, user, password):
        self.login_args = (user, password)

    def sendmail(self, sender, recipients, content):
        self.mail_args = (sender, recipients, content)
        return {}

    def quit(self):
        self.quit_called = True

    def close(self):
        pass


class SMTPClientTests(unittest.TestCase):
    def setUp(self):
        FakeSMTP.instances = []
        self.environment = {
            'DOMMYTOOL_SMTP_USER': 'sender@example.com',
            'DOMMYTOOL_SMTP_PASSWORD': 'secret-for-test',
            'DOMMYTOOL_SMTP_HOST': 'mail.example.com',
            'DOMMYTOOL_SMTP_PORT': '2465',
        }

    def test_delivery_uses_timeout_and_closes_connection(self):
        client.send_notification(
            title='Asset update', message='Done',
            address_list=['artist@example.com'], timeout=3,
            environment=self.environment, smtp_factory=FakeSMTP)
        smtp = FakeSMTP.instances[0]
        self.assertEqual((smtp.host, smtp.port, smtp.timeout),
                         ('mail.example.com', 2465, 3.0))
        self.assertEqual(smtp.login_args,
                         ('sender@example.com', 'secret-for-test'))
        self.assertEqual(smtp.mail_args[:2],
                         ('sender@example.com', ['artist@example.com']))
        self.assertTrue(smtp.quit_called)

    def test_missing_credentials_fail_before_network_access(self):
        with self.assertRaises(client.MailConfigurationError):
            client.send_notification(
                address_list=['artist@example.com'], environment={},
                smtp_factory=FakeSMTP)
        self.assertEqual(FakeSMTP.instances, [])

    def test_header_injection_is_rejected(self):
        with self.assertRaises(client.MailConfigurationError):
            client.send_notification(
                address_list=['artist@example.com\nBcc: bad@example.com'],
                environment=self.environment, smtp_factory=FakeSMTP)
        self.assertEqual(FakeSMTP.instances, [])

    def test_invalid_timeout_is_rejected_before_factory_access(self):
        for timeout in (float('nan'), float('inf'), -float('inf'),
                        0, -1, None, 'invalid', 10 ** 1000):
            with self.subTest(timeout_type=type(timeout).__name__):
                with self.assertRaises(client.MailConfigurationError):
                    client.send_notification(
                        address_list=['artist@example.com'], timeout=timeout,
                        environment=self.environment, smtp_factory=FakeSMTP)
                self.assertEqual(FakeSMTP.instances, [])

    def test_valid_timeout_values_keep_their_seconds(self):
        for timeout, expected in ((0.25, 0.25), ('2.5', 2.5), (10, 10.0)):
            client.send_notification(
                address_list=['artist@example.com'], timeout=timeout,
                environment=self.environment, smtp_factory=FakeSMTP)
            self.assertEqual(FakeSMTP.instances[-1].timeout, expected)
            self.assertTrue(FakeSMTP.instances[-1].quit_called)

    def test_default_transport_requires_certificate_and_hostname_verification(self):
        observed = []

        def initialize(instance, host, port, **kwargs):
            self.assertEqual(set(kwargs), {'timeout'})
            context = getattr(instance, '_tls_context', None)
            if context is None:
                context = getattr(instance, 'context', None)
            observed.append((host, port, kwargs['timeout'], context))
            instance.sock = None
            instance.file = None

        with patch.object(smtplib.SMTP, '__init__', initialize), \
                patch.object(smtplib.SMTP, 'ehlo'), \
                patch.object(smtplib.SMTP, 'login') as login, \
                patch.object(smtplib.SMTP, 'sendmail', return_value={}), \
                patch.object(smtplib.SMTP, 'quit'), \
                patch.object(socket, 'create_connection') as connect:
            client.send_notification(address_list=['artist@example.com'],
                                     environment=self.environment)
        self.assertEqual(observed[0][:3], ('mail.example.com', 2465, 10.0))
        self.assertEqual(observed[0][3].verify_mode, ssl.CERT_REQUIRED)
        self.assertTrue(observed[0][3].check_hostname)
        connect.assert_not_called()
        login.assert_called_once()

    def test_missing_verified_tls_capability_fails_before_network_access(self):
        with patch.object(ssl, 'create_default_context', None), \
                patch.object(smtplib.SMTP, '__init__') as initialize, \
                patch.object(socket, 'create_connection') as connect:
            with self.assertRaises(client.MailConfigurationError):
                client.send_notification(address_list=['artist@example.com'],
                                         environment=self.environment)
        initialize.assert_not_called()
        connect.assert_not_called()

    def test_missing_ssl_module_is_a_configuration_error_when_mail_is_invoked(self):
        with patch.dict(sys.modules, {'ssl': None}), \
                patch.object(smtplib.SMTP, '__init__') as initialize, \
                patch.object(socket, 'create_connection') as connect:
            with self.assertRaises(client.MailConfigurationError):
                client.send_notification(address_list=['artist@example.com'],
                                         environment=self.environment)
        initialize.assert_not_called()
        connect.assert_not_called()

    def test_unverified_context_is_rejected_before_network_access(self):
        for context in (None, SimpleNamespace(verify_mode=ssl.CERT_NONE, check_hostname=False),
                        SimpleNamespace(verify_mode=ssl.CERT_REQUIRED, check_hostname=False)):
            with self.subTest(context=context), \
                    patch.object(ssl, 'create_default_context', return_value=context), \
                    patch.object(smtplib.SMTP, '__init__') as initialize, \
                    patch.object(socket, 'create_connection') as connect:
                with self.assertRaises(client.MailConfigurationError):
                    client.send_notification(address_list=['artist@example.com'],
                                             environment=self.environment)
                initialize.assert_not_called()
                connect.assert_not_called()

    def test_verified_socket_uses_server_hostname(self):
        wrapped = object()
        raw = SimpleNamespace(close=lambda: self.fail('Successful raw socket was closed'))
        context = SimpleNamespace(wrap_socket=lambda sock, **kwargs: wrapped)
        transport = object.__new__(client._VerifiedSMTP)
        transport._tls_context = context
        with patch.object(socket, 'create_connection', return_value=raw) as connect, \
                patch.object(context, 'wrap_socket', return_value=wrapped) as wrap:
            self.assertIs(transport._get_socket('mail.example.com', 465, 3.0), wrapped)
        connect.assert_called_once_with(('mail.example.com', 465), 3.0)
        wrap.assert_called_once_with(raw, server_hostname='mail.example.com')

    def test_tls_failure_closes_raw_socket_and_keeps_original_error(self):
        primary = ssl.SSLError('certificate rejected')
        for cleanup_fails in (False, True):
            closed = []

            def close():
                closed.append(True)
                if cleanup_fails:
                    raise OSError('cleanup failed')

            raw = SimpleNamespace(close=close)
            context = SimpleNamespace(wrap_socket=lambda *args, **kwargs: None)
            transport = object.__new__(client._VerifiedSMTP)
            transport._tls_context = context
            with patch.object(socket, 'create_connection', return_value=raw), \
                    patch.object(context, 'wrap_socket', side_effect=primary):
                with self.assertRaises(ssl.SSLError) as caught:
                    transport._get_socket('mail.example.com', 465, 3.0)
            self.assertIs(caught.exception, primary)
            self.assertEqual(closed, [True])

    def test_constructor_failure_closes_adopted_socket(self):
        closed = []
        primary = OSError('greeting failed')

        def initialize(instance, *args, **kwargs):
            instance.sock = SimpleNamespace(close=lambda: closed.append(True))
            raise primary

        with patch.object(smtplib.SMTP, '__init__', initialize):
            with self.assertRaises(OSError) as caught:
                client._VerifiedSMTP('mail.example.com', 465, timeout=3)
        self.assertIs(caught.exception, primary)
        self.assertEqual(closed, [True])

    def test_partial_refusal_reports_failure_without_resending(self):
        class RefusingSMTP(FakeSMTP):
            def sendmail(self, sender, recipients, content):
                super().sendmail(sender, recipients, content)
                return {recipients[-1]: (550, b'disposable refusal')}

        with self.assertRaises(client.MailDeliveryError) as caught:
            client.send_notification(address_list=['one@example.com', 'two@example.com'],
                                     environment=self.environment, smtp_factory=RefusingSMTP)
        self.assertEqual(caught.exception.refused_recipients, ('two@example.com',))
        self.assertIn('partially delivered', str(caught.exception))
        self.assertEqual(len(FakeSMTP.instances), 1)
        self.assertTrue(FakeSMTP.instances[0].quit_called)

    def assert_configuration_rejected(self, **updates):
        options = dict(title='Asset update', message='Done', address_list=['a@example.com'],
                       environment=self.environment, smtp_factory=FakeSMTP)
        options.update(updates)
        with self.assertRaises(client.MailConfigurationError):
            client.send_notification(**options)
        self.assertEqual(FakeSMTP.instances, [])

    def test_controls_in_each_address_fail_before_transport_construction(self):
        for code in tuple(range(32)) + (127,):
            with self.subTest(code=code):
                self.assert_configuration_rejected(address_list=['a' + chr(code) + 'b@example.com'])
                self.assert_configuration_rejected(environment=dict(self.environment,
                    DOMMYTOOL_SMTP_USER='a' + chr(code) + 'b@example.com'))

    def test_recipient_containers_are_explicit_and_bounded(self):
        for value in (None, [], (), 'a@example.com', {'a@example.com'},
                      {'a@example.com': True}, iter(['a@example.com']), True,
                      ['a@example.com'] * (client.MAX_RECIPIENTS + 1)):
            self.assert_configuration_rejected(address_list=value)

    def test_recipient_limit_and_tuple_order_are_preserved(self):
        addresses = ('a@example.com', 'b@example.com')
        with patch.object(client, 'MAX_RECIPIENTS', 2):
            client.send_notification(address_list=addresses, environment=self.environment,
                                     smtp_factory=FakeSMTP)
        self.assertEqual(FakeSMTP.instances[0].mail_args[1], list(addresses))

    def test_title_and_message_types_and_limits_fail_before_transport(self):
        for key, limit in (('title', client.MAX_TITLE_LENGTH), ('message', client.MAX_MESSAGE_LENGTH)):
            for value in (None, True, 1, [], {}, 'x' * (limit + 1)):
                self.assert_configuration_rejected(**{key: value})

    def test_header_controls_and_body_nul_are_rejected(self):
        for code in tuple(range(32)) + (127,):
            self.assert_configuration_rejected(title='Subject' + chr(code))
            if chr(code) not in ('\r', '\n', '\t'):
                self.assert_configuration_rejected(message='Body' + chr(code))

    def test_valid_unicode_multiline_body_and_empty_text_remain_supported(self):
        for title, message in (('アセット更新', '一行目\r\n\t二行目'), ('', '')):
            client.send_notification(title=title, message=message, address_list=['a@example.com'],
                                     environment=self.environment, smtp_factory=FakeSMTP)
            self.assertTrue(FakeSMTP.instances[-1].quit_called)

    def test_invalid_config_types_are_configuration_errors_without_password_disclosure(self):
        for key, values in (('DOMMYTOOL_SMTP_HOST', (None, True, 1, [], 'mail\x00.example.com')),
                            ('DOMMYTOOL_SMTP_PASSWORD', (None, True, 1, [], 'x' * (client.MAX_CONFIG_LENGTH + 1))),
                            ('DOMMYTOOL_SMTP_PORT', (None, True, False, 465.5, [], {}, '1' * 17, '465.0', 65536))):
            for value in values:
                with self.subTest(key=key, type=type(value).__name__):
                    self.assert_configuration_rejected(environment=dict(self.environment, **{key: value}))
        for environment in ([], 'environment', True):
            self.assert_configuration_rejected(environment=environment)

    def test_integer_ports_and_legacy_numeric_timeout_strings_remain_supported(self):
        for port in (465, '465', ' 465 '):
            client.send_notification(address_list=['a@example.com'], timeout='2.5',
                environment=dict(self.environment, DOMMYTOOL_SMTP_PORT=port), smtp_factory=FakeSMTP)
            self.assertEqual((FakeSMTP.instances[-1].port, FakeSMTP.instances[-1].timeout), (465, 2.5))
        FakeSMTP.instances = []
        for value in (True, False, '1' * 257):
            self.assert_configuration_rejected(timeout=value)

    def test_last_invalid_recipient_cannot_send_a_valid_prefix(self):
        self.assert_configuration_rejected(address_list=['a@example.com', 'b\x00@example.com'])

    def test_text_budget_boundaries_are_admitted(self):
        with patch.object(client, 'MAX_TITLE_LENGTH', 4), patch.object(client, 'MAX_MESSAGE_LENGTH', 4):
            client.send_notification(title='1234', message='1234', address_list=['a@example.com'],
                                     environment=dict(self.environment, DOMMYTOOL_SMTP_HOST='host'),
                                     smtp_factory=FakeSMTP)
        self.assertTrue(FakeSMTP.instances[0].quit_called)


if __name__ == '__main__':
    unittest.main()
