# -*- coding: utf-8 -*-
"""Host-independent SMTP delivery used by the optional Maya UI."""

import math
import numbers
import os
import smtplib
import socket
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText
try:
    from collections.abc import Mapping
except ImportError:
    from collections import Mapping

from DPyTools.core.mail_address import plain_address


DEFAULT_HOST = 'smtp.gmail.com'
DEFAULT_PORT = 465
DEFAULT_TIMEOUT = 10.0
MAX_RECIPIENTS = 10000
MAX_TITLE_LENGTH = 1024
MAX_MESSAGE_LENGTH = 1024 * 1024
MAX_CONFIG_LENGTH = 4096

try:
    STRING_TYPES = (basestring,)
except NameError:
    STRING_TYPES = (str,)


class MailError(RuntimeError):
    """Base class for expected notification failures."""


class MailConfigurationError(MailError):
    """Raised when SMTP configuration is missing or invalid."""


class MailDeliveryError(MailError):
    """Raised when the SMTP service cannot deliver a message."""

    def __init__(self, message, refused_recipients=None):
        MailError.__init__(self, message)
        self.refused_recipients = tuple(refused_recipients or ())


def _verified_tls_context():
    # Lazy import keeps an unavailable SSL runtime from breaking disabled mail UIs.
    try:
        import ssl
    except ImportError:
        raise MailConfigurationError('Verified TLS support is unavailable')
    create_context = getattr(ssl, 'create_default_context', None)
    if not callable(create_context):
        raise MailConfigurationError('Verified TLS requires SSLContext support')
    context = create_context()
    if (getattr(context, 'verify_mode', None) != ssl.CERT_REQUIRED
            or not getattr(context, 'check_hostname', False)):
        raise MailConfigurationError('TLS certificate and hostname verification are required')
    return context


class _VerifiedSMTP(smtplib.SMTP):
    """SMTP over a verified socket, including Python 2's context-less SMTP_SSL."""

    default_port = DEFAULT_PORT

    def __init__(self, host='', port=0, timeout=DEFAULT_TIMEOUT):
        self.sock = None
        self.file = None
        self._tls_context = _verified_tls_context()
        try:
            smtplib.SMTP.__init__(self, host, port, timeout=timeout)
        except BaseException:
            try:
                self.close()
            except Exception:
                pass
            raise

    def _get_socket(self, host, port, timeout):
        raw_socket = socket.create_connection((host, port), timeout)
        try:
            return self._tls_context.wrap_socket(raw_socket, server_hostname=host)
        except BaseException:
            try:
                raw_socket.close()
            except Exception:
                pass
            raise


def _validated_address(address, field_name):
    try:
        return plain_address(address, field_name, strip=True)
    except (TypeError, ValueError) as error:
        raise MailConfigurationError(str(error))


def _validated_text(value, field_name, limit, body=False):
    if not isinstance(value, STRING_TYPES):
        raise MailConfigurationError('{0} must be a string'.format(field_name))
    if len(value) > limit:
        raise MailConfigurationError('{0} is too long'.format(field_name))
    allowed_controls = ('\r', '\n', '\t') if body else ()
    if any((ord(character) < 32 and character not in allowed_controls)
           or ord(character) == 127 for character in value):
        raise MailConfigurationError('{0} contains a control character'.format(field_name))
    return value


def _smtp_settings(environment):
    if not isinstance(environment, Mapping):
        raise MailConfigurationError('SMTP environment must be a mapping')
    sender = _validated_address(
        environment.get('DOMMYTOOL_SMTP_USER'), 'DOMMYTOOL_SMTP_USER')
    password = environment.get('DOMMYTOOL_SMTP_PASSWORD')
    if not isinstance(password, STRING_TYPES) or len(password) > MAX_CONFIG_LENGTH:
        raise MailConfigurationError('DOMMYTOOL_SMTP_PASSWORD must be bounded text')
    if not password:
        raise MailConfigurationError('DOMMYTOOL_SMTP_PASSWORD must be configured')
    host = _validated_text(environment.get('DOMMYTOOL_SMTP_HOST', DEFAULT_HOST),
                           'DOMMYTOOL_SMTP_HOST', MAX_TITLE_LENGTH).strip()
    if not host:
        raise MailConfigurationError('DOMMYTOOL_SMTP_HOST is invalid')
    value = environment.get('DOMMYTOOL_SMTP_PORT', DEFAULT_PORT)
    if (isinstance(value, bool)
            or not isinstance(value, STRING_TYPES + (numbers.Integral,))
            or (isinstance(value, STRING_TYPES) and len(value) > 16)):
        raise MailConfigurationError('DOMMYTOOL_SMTP_PORT must be a bounded integer')
    try:
        port = int(value)
    except (TypeError, ValueError):
        raise MailConfigurationError('DOMMYTOOL_SMTP_PORT must be an integer')
    if not 1 <= port <= 65535:
        raise MailConfigurationError('DOMMYTOOL_SMTP_PORT is outside the valid range')
    return sender, password, host, port


def send_notification(title='title', message='', address_list=None,
                      timeout=DEFAULT_TIMEOUT, environment=None,
                      smtp_factory=None):
    """Send a plain-text notification through SMTP over SSL.

    Configuration is read only when this explicitly invoked function runs.
    ``smtp_factory`` exists so delivery can be verified without network access.
    """
    title = _validated_text(title, 'title', MAX_TITLE_LENGTH)
    message = _validated_text(message, 'message', MAX_MESSAGE_LENGTH, body=True)
    if (not isinstance(address_list, (list, tuple))
            or not 1 <= len(address_list) <= MAX_RECIPIENTS):
        raise MailConfigurationError('address_list must be a bounded nonempty list or tuple')
    if (isinstance(timeout, bool)
            or (isinstance(timeout, STRING_TYPES) and len(timeout) > 256)):
        raise MailConfigurationError('timeout must be a finite positive number')
    try:
        timeout = float(timeout)
    except (TypeError, ValueError, OverflowError):
        raise MailConfigurationError('timeout must be a finite positive number')
    if math.isnan(timeout) or math.isinf(timeout) or timeout <= 0:
        raise MailConfigurationError('timeout must be a finite positive number')

    environment = os.environ if environment is None else environment
    sender, password, host, port = _smtp_settings(environment)
    recipients = [
        _validated_address(address, 'recipient') for address in address_list
    ]

    msg = MIMEMultipart('alternative')
    msg['Subject'] = title
    msg['From'] = sender
    msg['To'] = ', '.join(recipients)
    if message:
        msg.attach(MIMEText(message, 'plain', 'utf-8'))

    # The injectable factory is a trusted test seam, not a TLS bypass setting.
    smtp_factory = _VerifiedSMTP if smtp_factory is None else smtp_factory
    smtp_object = None
    try:
        smtp_object = smtp_factory(host, port, timeout=timeout)
        smtp_object.ehlo()
        smtp_object.login(sender, password)
        refused = smtp_object.sendmail(sender, recipients, msg.as_string())
        if refused:
            raise MailDeliveryError(
                'SMTP notification was partially delivered: {0} recipient(s) refused. '
                'Do not automatically resend to all recipients.'.format(len(refused)),
                refused_recipients=sorted(refused))
    except (smtplib.SMTPException, socket.error, OSError) as error:
        raise MailDeliveryError('SMTP notification failed: {0}'.format(error))
    finally:
        if smtp_object is not None:
            try:
                smtp_object.quit()
            except (smtplib.SMTPException, socket.error, OSError):
                try:
                    smtp_object.close()
                except (smtplib.SMTPException, socket.error, OSError):
                    pass
