# -*- coding: utf-8 -*-
u"""Serializable data models used by the historical face-rig UI."""

import math


try:
	STRING_TYPES = (basestring,)
	INTEGER_TYPES = (int, long)
except NameError:
	STRING_TYPES = (str,)
	INTEGER_TYPES = (int,)


def _preserve_text(value):
	"""Keep Python 2 unicode values intact instead of ASCII-coercing them."""
	if isinstance(value, STRING_TYPES):
		return value
	return str(value)


def _is_finite_scalar(value):
	if isinstance(value, bool):
		return True
	if not isinstance(value, INTEGER_TYPES + (float,)):
		return False
	try:
		number = float(value)
	except (OverflowError, TypeError, ValueError):
		return False
	return not math.isnan(number) and not math.isinf(number)


class Value(object):
	def __init__(self, name='', value=0.0, minValue=0.0, maxValue=10.0):
		self.m_name = name
		self.m_value = value
		self.m_min = minValue
		self.m_max = maxValue


class ControllerInfo(object):
	def __init__(self, name='', ctype=''):
		self.m_cntlr_name = _preserve_text(name)
		self.m_cntlr_type = _preserve_text(ctype)
		self.m_attr_list = []

	def add_attribute_value(self, attribute_name='', attribute_value=0):
		self.m_attr_list.append(Value(attribute_name, attribute_value))

	def addAttrValue(self, attr_name='', attr_value=0):
		self.add_attribute_value(attr_name, attr_value)

	def getAttrCount(self):
		return len(self.m_attr_list)

	def to_dict(self):
		attributes = {}
		for attribute in self.m_attr_list:
			if attribute.m_name in attributes:
				raise ValueError('Duplicate controller attribute name')
			attributes[attribute.m_name] = attribute.m_value
		return {
			'cntlr_name': self.m_cntlr_name,
			'cntlr_type': self.m_cntlr_type,
			'attr_list': attributes,
		}

	def genJsonData(self):
		return self.to_dict()

	def set_json_data(self, controller_data=None):
		if not isinstance(controller_data, dict):
			return False
		required_keys = ('cntlr_name', 'cntlr_type', 'attr_list')
		if not all(key in controller_data for key in required_keys):
			return False
		attributes = controller_data['attr_list']
		if not isinstance(attributes, dict):
			return False
		if (not isinstance(controller_data['cntlr_name'], STRING_TYPES)
				or not controller_data['cntlr_name']):
			return False
		if not isinstance(controller_data['cntlr_type'], STRING_TYPES):
			return False

		values = []
		for name, value in attributes.items():
			if (not isinstance(name, STRING_TYPES) or not name
					or not _is_finite_scalar(value)):
				return False
			values.append(Value(name, value))
		self.m_cntlr_name = controller_data['cntlr_name']
		self.m_cntlr_type = controller_data['cntlr_type']
		self.m_attr_list = values
		return True

	def setJsonData(self, cntlrDic=None):
		return self.set_json_data(cntlrDic)


class FaceExpression(object):
	def __init__(self, name=''):
		self.m_name = _preserve_text(name)
		self.m_controller_list = []

	def add_controller_info(self, controller_info=None):
		if controller_info is not None:
			self.m_controller_list.append(controller_info)

	def addControllerInfo(self, con_data=None):
		self.add_controller_info(con_data)

	def getControllerCount(self):
		return len(self.m_controller_list)

	def to_dict(self):
		return {
			'name': self.m_name,
			'cntlr_list': [
				controller.to_dict()
				for controller in self.m_controller_list
			],
		}

	def genJsonData(self):
		return self.to_dict()

	def set_json_data(self, expression_data=None):
		if not isinstance(expression_data, dict):
			return False
		if 'name' not in expression_data or 'cntlr_list' not in expression_data:
			return False
		controller_data = expression_data['cntlr_list']
		if not isinstance(controller_data, list):
			return False
		if (not isinstance(expression_data['name'], STRING_TYPES)
				or not expression_data['name']):
			return False

		controllers = []
		for item in controller_data:
			controller = ControllerInfo()
			if not controller.set_json_data(item):
				return False
			controllers.append(controller)
		self.m_name = expression_data['name']
		self.m_controller_list = controllers
		return True

	def setJsonData(self, faceDic=None):
		return self.set_json_data(faceDic)


def expressions_from_collection(collection_data):
	"""Validate and build every expression before returning any result."""
	if not isinstance(collection_data, dict):
		raise ValueError('Expression collection must be a dictionary')
	expression_data = collection_data.get('expr_list')
	if not isinstance(expression_data, list):
		raise ValueError('Expression collection must contain an expr_list array')

	expressions = []
	for index, item in enumerate(expression_data):
		expression = FaceExpression()
		if not expression.set_json_data(item):
			raise ValueError(
				'Invalid expression data at index {0}'.format(index))
		expressions.append(expression)
	return expressions


def collection_from_expressions(expressions):
	"""Serialize and fully revalidate model objects before persistence."""
	if not isinstance(expressions, (list, tuple)):
		raise ValueError('Expressions must be provided as a list or tuple')

	serialized = []
	for index, expression in enumerate(expressions):
		if not isinstance(expression, FaceExpression):
			raise ValueError(
				'Invalid expression object at index {0}'.format(index))
		try:
			serialized.append(expression.to_dict())
		except (AttributeError, TypeError):
			raise ValueError(
				'Invalid expression object at index {0}'.format(index))

	collection = {'expr_list': serialized}
	# Use the same complete validation path as loading. This also rejects
	# mutations made directly to the public historical model attributes.
	expressions_from_collection(collection)
	return collection


# Historical names remain import-compatible for archived Maya scripts.
DValue = Value
DControllerInfo = ControllerInfo
DFaceExpr = FaceExpression
