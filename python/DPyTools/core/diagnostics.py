# -*- coding: utf-8 -*-
u"""Opt-in package diagnostics that stay silent during normal Maya use."""

import logging


LOGGER_NAME = 'DPyTools'
_PACKAGE_LOGGER = logging.getLogger(LOGGER_NAME)
_PACKAGE_LOGGER.setLevel(logging.WARNING)
_PACKAGE_LOGGER.propagate = True
for _handler in list(_PACKAGE_LOGGER.handlers):
	if getattr(_handler, '_dpytools_debug_handler', False):
		_PACKAGE_LOGGER.removeHandler(_handler)
		_handler.close()
if not any(
		getattr(handler, '_dpytools_null_handler', False)
		for handler in _PACKAGE_LOGGER.handlers):
	_NULL_HANDLER = logging.NullHandler()
	_NULL_HANDLER._dpytools_null_handler = True
	_PACKAGE_LOGGER.addHandler(_NULL_HANDLER)
_DEBUG_HANDLER = None


def get_logger(module_name=''):
	u"""Return a child logger below the stable ``DPyTools`` namespace."""
	if not module_name or module_name == LOGGER_NAME:
		return _PACKAGE_LOGGER
	if module_name.startswith(LOGGER_NAME + '.'):
		return logging.getLogger(module_name)
	return logging.getLogger(LOGGER_NAME + '.' + module_name)


def configure_debug(enabled=False, stream=None):
	u"""Enable or disable explicit debug output for the complete package."""
	global _DEBUG_HANDLER
	if _DEBUG_HANDLER is not None:
		_PACKAGE_LOGGER.removeHandler(_DEBUG_HANDLER)
		_DEBUG_HANDLER.close()
		_DEBUG_HANDLER = None

	if enabled:
		_DEBUG_HANDLER = logging.StreamHandler(stream)
		_DEBUG_HANDLER._dpytools_debug_handler = True
		_DEBUG_HANDLER.setFormatter(logging.Formatter(
			'%(name)s %(levelname)s: %(message)s'))
		_PACKAGE_LOGGER.addHandler(_DEBUG_HANDLER)
		_PACKAGE_LOGGER.setLevel(logging.DEBUG)
		_PACKAGE_LOGGER.propagate = False
	else:
		_PACKAGE_LOGGER.setLevel(logging.WARNING)
		_PACKAGE_LOGGER.propagate = True
	return bool(enabled)


def is_debug_enabled():
	return _PACKAGE_LOGGER.isEnabledFor(logging.DEBUG)
