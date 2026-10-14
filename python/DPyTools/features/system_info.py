# -*- coding: utf-8 -*-
"""Read-only Maya diagnostic reports with independent query failure handling."""

import numbers
import platform
import sys

import pymel.core as pm

from DPyTools.core import strings


MAX_REPORT_ITEMS = 10000
_QUERY_ERRORS = (RuntimeError, TypeError, ValueError, AttributeError)
_HARDWARE_FIELDS = (('Cpu', 'cpuType'), ('Board', 'brdType'),
	('Graphic Card', 'graphicsType'), ('processor Number', 'numProcessors'), ('MHz', 'megaHertz'))
_ABOUT_SECTIONS = (
	('Maya Info', (('product', 'product'), ('application', 'application'),
		('apiVersion', 'apiVersion'), ('installedVersion', 'installedVersion'),
		('buildDirectory', 'buildDirectory'), ('codeset', 'codeset'),
		('compositingManager', 'compositingManager'), ('Internet connected', 'connected'),
		('batch mode', 'batch'), ('evalVersion', 'evalVersion'), ('Maya LT', 'ltVersion'),
		('version', 'version'), ('file version', 'file'), ('fontInfo', 'fontInfo'),
		('liveUpdate', 'liveUpdate'))),
	('Qt Info', (('qtVersion', 'qtVersion'),)),
	('System File', (('environmentFile', 'environmentFile'), ('preferences', 'preferences'),
		('helpDataDirectory', 'helpDataDirectory'))),
	('Date Info', (('ctime', 'ctime'), ('currentDate', 'currentDate'),
		('currentTime', 'currentTime'), ('cutIdentifier', 'cutIdentifier'), ('date', 'date'))),
	('OS Info', (('operatingSystem', 'operatingSystem'),
		('operatingSystemVersion', 'operatingSystemVersion'), ('is64', 'is64'),
		('irix', 'irix'), ('linux', 'linux'), ('linux64', 'linux64'), ('macOS', 'macOS'),
		('macOSppc', 'macOSppc'), ('macOSx86', 'macOSx86'), ('win64', 'win64'),
		('windowManager', 'windowManager'), ('windows', 'windows'), ('ntOS', 'ntOS'))),
	('Language', (('localizedResourceLocation', 'localizedResourceLocation'),
		('languageResources', 'languageResources'), ('uiLanguage', 'uiLanguage'),
		('uiLanguageForStartup', 'uiLanguageForStartup'),
		('uiLanguageIsLocalized', 'uiLanguageIsLocalized'), ('uiLocaleLanguage', 'uiLocaleLanguage'))),
)


def _text(value):
	"""Display text only: invalid UTF-8 bytes use replacement, never node identity."""
	if isinstance(value, strings.TEXT_TYPE):
		return value
	if isinstance(value, bytes):
		return value.decode('utf-8', 'replace')
	try:
		return strings.TEXT_TYPE(value)
	except UnicodeError:
		value = str(value)
		return value.decode('utf-8', 'replace') if isinstance(value, bytes) else value


def _value_text(value):
	if value is None:
		return u'<unavailable>'
	if isinstance(value, (list, tuple)):
		if len(value) > MAX_REPORT_ITEMS:
			raise ValueError('Diagnostic value exceeds the item limit')
		value = u', '.join(_text(item) for item in value)
	else:
		value = _text(value)
	return value if value else u'<unavailable>'


def _sequence_text(values):
	if values is None:
		return ()
	if isinstance(values, strings.STRING_TYPES) or isinstance(values, bytes):
		values = (values,)
	if not isinstance(values, (list, tuple)) or len(values) > MAX_REPORT_ITEMS:
		raise ValueError('Diagnostic rows must be a bounded sequence')
	return tuple(_value_text(value) for value in values)


def _plugin_names(values):
	values = () if values is None else values
	if (not isinstance(values, (list, tuple)) or len(values) > MAX_REPORT_ITEMS
			or any(not isinstance(value, strings.STRING_TYPES) or not value for value in values)):
		raise ValueError('Plug-in inventory must contain bounded nonempty names')
	return tuple(values)


def _extensions(value):
	if value is None or value == '':
		return ()
	if not isinstance(value, strings.STRING_TYPES) and not isinstance(value, bytes):
		raise ValueError('OpenGL extensions must be text')
	return _sequence_text(_text(value).split())


def _tablet_presence(value):
	if not isinstance(value, numbers.Integral) or value not in (0, 1):
		raise ValueError('Tablet presence was not a Boolean result')
	return bool(value)


def _query(operation, formatter, *args, **kwargs):
	"""Expected query/format failures are data; interruption remains interruption."""
	try:
		return True, formatter(operation(*args, **kwargs))
	except _QUERY_ERRORS as error:
		return False, u'<unavailable: {0}>'.format(_text(error))


def echoWarning(message=''):
	"""Print explicit diagnostic output; no automatic file or external upload."""
	print _value_text(message)


def _heading(label):
	echoWarning(u'\n\n------------{0}------------\n'.format(label))


def _report_query(label, operation, *args, **kwargs):
	unused, text = _query(operation, _value_text, *args, **kwargs)
	echoWarning(u'{0}:{1}'.format(label, text))


def _report_sequence(label, operation, formatter=_sequence_text, *args, **kwargs):
	ok, rows = _query(operation, formatter, *args, **kwargs)
	if not ok:
		echoWarning(u'{0}:{1}'.format(label, rows))
		return
	for text in rows or (u'<none>',):
		echoWarning(u'{0}:{1}'.format(label, text))


def displayInfo():
	"""Print every available section even when one native query is unsupported."""
	_heading('Memory')
	_report_sequence('Summary', lambda: pm.memory(summary=True))
	_heading('Hardware')
	for label, flag in _HARDWARE_FIELDS:
		_report_query(label, lambda: pm.hardware(**{flag: True}))
	for heading, fields in _ABOUT_SECTIONS:
		_heading(heading)
		for label, flag in fields:
			_report_query(label, lambda: pm.about(**{flag: True}))
		if heading == 'Maya Info':
			_heading('Python Info')
			_report_query('Python Version', lambda: platform.python_version())
			_heading('Python PATH')
			_report_sequence('Path', lambda: sys.path)
	_heading('Tablet')
	ok, present = _query(lambda: pm.about(tablet=True), _tablet_presence)
	if not ok:
		echoWarning(u'tablet:{0}'.format(present))
	elif present:
		echoWarning(u'Tablet is found.')
		_report_query('tabletMode', lambda: pm.about(tabletMode=True))
	else:
		echoWarning(u'Tablet is not found.')


def pluginInfo():
	"""Admit a complete plug-in list, then report each field independently."""
	ok, names = _query(lambda: pm.pluginInfo(query=True, listPlugins=True), _plugin_names)
	if not ok:
		echoWarning(names)
		return
	if not names:
		echoWarning(u'No plug-ins loaded.')
	for name in names:
		_heading(name)
		for label, flag in (('ver', 'version'), ('vendor', 'vendor'), ('path', 'path'), ('apiVer', 'apiVersion')):
			_report_query(label, lambda: pm.pluginInfo(name, query=True, **{flag: True}))


def currentFileInfo():
	"""Workspace failure does not hide available file metadata, or vice versa."""
	_heading('File Info')
	_report_query('Project root dir', lambda: pm.workspace(q=True, rootDirectory=True))
	_report_sequence('Metadata', lambda: list(pm.fileInfo.items()))


def displayOpenGLInfo():
	"""Report renderer fields/extensions without assuming every query exists."""
	for label, flag in (('Renderer', 'renderer'), ('Vendor', 'vendor'), ('Version', 'version')):
		_report_query(label, lambda: pm.openGLExtension(**{flag: True}))
	_heading('Supported Extension')
	_report_sequence('Extension', lambda: pm.openGLExtension(extension=''), formatter=_extensions)


def isLoadedPlugin(pluginName=''):
	return bool(pm.pluginInfo(pluginName, query=True, loaded=True))


def loadMayaPlugin(pluginName=''):
	try:
		pm.loadPlugin(pluginName)
	except RuntimeError as error:
		pm.warning('Failed to load {name}: {error}'.format(name=pluginName, error=error))
		return False
	if isLoadedPlugin(pluginName=pluginName):
		print '{name} loaded.'.format(name=pluginName)
		return True
	return False


def clearNodeCache():
	"""Historical explicit mutating helper; never invoked by diagnostic reports."""
	result = pm.clearCache(all=True)
	echoWarning('Clear Result:' + str(result))
