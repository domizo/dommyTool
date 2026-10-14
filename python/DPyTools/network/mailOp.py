# -*- coding: utf-8 -*-
"""Compatibility imports for the historical SMTP module path."""

from DPyTools.integrations.smtp import mail as _implementation

DMailingListManager = _implementation.DMailingListManager
send_gmail_notif = _implementation.send_gmail_notif

__all__ = ('DMailingListManager', 'send_gmail_notif')
