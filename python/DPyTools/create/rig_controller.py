# -*- coding: utf-8 -*-
"""Rig-controller creation tool exposed from the canonical module name."""

import pymel.core as pm
import numbers
import sys


from DPyTools.scene import ownership, parameters
from DPyTools.ui import windows as ui_windows


'''
to run:
	>>> import DPyTools.create.rig_controller as rigCon

	open window
	rigCon.openWin()



'''


def _build_controller(owned, attributes, name):
	kwargs = {'name': 'drigCtrl'}
	if name is not None:
		name = parameters.control_name(name)
		if pm.objExists(name):
			raise ValueError('Requested controller name already exists')
		kwargs['name'] = name
	parameters.value_items(attributes)
	generator = owned.create('DRigController')
	parameters.set_values(generator, attributes)
	transform = owned.create('transform', **kwargs)
	if name is not None and transform.name().lstrip(':') != name.lstrip(':'):
		raise ValueError('Native controller naming did not preserve the requested name')
	shape = owned.create('nurbsCurve', parent=transform)
	pm.connectAttr(generator.attr('outputCurve'), shape.attr('create'))
	return transform, shape, generator


def createRigCtrlBase(name='drigCtrl1'):
	owned = ownership.SceneNodes()
	completed = False
	try:
		result = _build_controller(owned, {}, name)
		completed = True
		return result
	finally:
		if not completed:
			exception_type = sys.exc_info()[0]
			owned.cleanup(active_error=exception_type is not None)




def createRigCtrl(attributes=None, side=None):
	if side is not None and (isinstance(side, bool) or not isinstance(side, numbers.Integral) or side not in (0, 1, 2)):
		raise ValueError('Controller side must be zero, one or two')
	parameters.value_items(attributes)
	owned = ownership.SceneNodes()
	completed = False
	try:
		result = _build_controller(owned, attributes, None)
		if side is not None:
			parameters.set_values(result[1], {'overrideEnabled': True, 'overrideColor': (13, 17, 6)[side]})
		completed = True
	finally:
		if not completed:
			exception_type = sys.exc_info()[0]
			owned.cleanup(active_error=exception_type is not None)
	try:
		pm.select(result[0], replace=True)
	except (RuntimeError, TypeError, ValueError) as error:
		pm.warning('Controller was created, but selecting it failed: {0}'.format(error))
	return result




class openWin(object):

	def __init__(self):
		uiname = "DPTCreateRigContorllerWin"
		ui_windows.delete_window(uiname)

		with pm.window(uiname,widthHeight=(480, 640)):
			with pm.columnLayout(adjustableColumn=True):
				self.rigTypeMenu = pm.optionMenuGrp( l='Controller Type')
				pm.menuItem( label='Square' )
				pm.menuItem( label='Circle' )
				pm.menuItem( label='Cube' )
				pm.menuItem( label='Sphere' )
				pm.menuItem( label='Pyramid' )
				pm.menuItem( label='Arrow' )
				pm.menuItem( label='Slide Arrow' )
				pm.menuItem( label='Cross Arrow' )
				pm.menuItem( label='Pin' )

				self.scaleSlider = pm.floatSliderGrp( label='Scale', field=True, fieldMinValue=0.0, fieldMaxValue=100.0, value=10)
				self.divSlider = pm.intSliderGrp( field=True, label='Division', minValue=3, fieldMinValue=3, fieldMaxValue=100, value=8 )
				self.keepCheck = pm.checkBoxGrp(numberOfCheckBoxes=1, label='Keep Center', label1='', v1=False)
				self.ratioSlider = pm.floatSliderGrp( label='Ratio', field=True, fieldMinValue=0.0, fieldMaxValue=2.0, value=1.0)
				self.dirTypeMenu = pm.optionMenuGrp( l='Direction')
				pm.menuItem( label='+X' )
				pm.menuItem( label='+Y' )
				pm.menuItem( label='+Z' )
				pm.menuItem( label='-X' )
				pm.menuItem( label='-Y' )
				pm.menuItem( label='-Z' )
				pm.optionMenuGrp( self.dirTypeMenu, edit=True, select=2)

				self.taperAmtSlider = pm.floatSliderGrp( label='Taper Amount', field=True, fieldMinValue=-10.0, fieldMaxValue=10.0, value=0.0)
				self.taperAxisTypeMenu = pm.optionMenuGrp( l='Taper Axis' )
				pm.menuItem( label='X' )
				pm.menuItem( label='Y' )
				pm.menuItem( label='Z' )
				self.bendAmtSlider = pm.floatSliderGrp( label='Bend Amount', field=True, fieldMinValue=-10.0, fieldMaxValue=10.0, value=0.0)
				self.bendAxisTypeMenu = pm.optionMenuGrp( l='Bend Axis' )
				pm.menuItem( label='X' )
				pm.menuItem( label='Z' )
				self.arrowRevCheck = pm.checkBoxGrp(numberOfCheckBoxes=1, label='Arrow Reverse', label1='', v1=False)
				self.arrowRatioSlider = pm.floatSliderGrp( label='Arrow Ratio', field=True, fieldMinValue=0.0, fieldMaxValue=1.0, value=0.75)
				self.arrowWidthSlider= pm.floatSliderGrp( label='Arrow Width', field=True, fieldMinValue=0.0, fieldMaxValue=1.0, value=0.25)
				pm.separator()
				self.sideRadio = pm.radioButtonGrp( numberOfRadioButtons=3, label='Side', labelArray3=['Right(Red)', 'Center(Yellow)', 'Left(Blue)'], select=2 )
				pm.separator()
				pm.button( label='Create', c = lambda *args: self.createController())


	def createController(self):

		attrs={}
		attrs['shapeType'] = pm.optionMenuGrp(self.rigTypeMenu, q=True, select=True)-1
		attrs['localScale'] = self.scaleSlider.getValue()
		attrs['division'] = self.divSlider.getValue()
		attrs['ratio'] = self.ratioSlider.getValue()
		attrs['keepCenter'] = pm.checkBoxGrp(self.keepCheck, q=True, value1=True)
		attrs['dirAxis'] = pm.optionMenuGrp(self.dirTypeMenu, q=True, select=True)-1
		attrs['taperAmount'] = self.taperAmtSlider.getValue()
		attrs['taperAxis'] = pm.optionMenuGrp(self.taperAxisTypeMenu, q=True, select=True)-1
		attrs['bendAmount'] = self.bendAmtSlider.getValue()
		attrs['bendAxis'] = pm.optionMenuGrp(self.bendAxisTypeMenu, q=True, select=True)-1
		attrs['arrowReverse'] = pm.checkBoxGrp(self.arrowRevCheck, q=True, value1=True)
		attrs['arrowRatio'] = self.arrowRatioSlider.getValue()
		attrs['arrowWidth'] = self.arrowWidthSlider.getValue()

		sideVal = pm.radioButtonGrp(self.sideRadio, q=True, select=True)-1


		try:
			return createRigCtrl(attrs, sideVal)
		except (RuntimeError, TypeError, ValueError) as error:
			pm.warning('Controller was not created: {0}'.format(error))
			return False
