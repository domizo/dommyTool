import pymel.core as pm

from DPyTools.ui import windows as ui_windows
from DPyTools.features import point_generation

'''
to run:
	>>> import DPyTools.dpoint.generate.DPointAddOp as DPointAddOp
	>>> DPointAddOp.openWin()

'''



class openWin(object):
	def __init__(self):
		uiname = "DPointAddWin"
		ui_windows.delete_window(uiname)

		self.window = pm.window(uiname, widthHeight=(480, 120))
		self.form = pm.formLayout(numberOfDivisions=100)

		self.connectCheckBox = pm.checkBox(label='Target Connect', value=False)
		self.createBtn = pm.button(
			label='Create', c=lambda *args: self.runCreateCB())

		pm.formLayout(
			self.form, edit=True,
			attachForm=[(self.connectCheckBox, 'top', 5),
					(self.connectCheckBox, 'left', 5),
					(self.connectCheckBox, 'right', 5),
					(self.createBtn, 'bottom', 5),
					(self.createBtn, 'left', 5),
					(self.createBtn, 'right', 5)],
			attachControl=[(self.createBtn, 'top', 5, self.connectCheckBox)])

		pm.showWindow(self.window)

	def runCreateCB(self):
		try:
			connect = bool(pm.checkBox(self.connectCheckBox, q=True, value=True))
			return point_generation.create_from_selection(connect)
		except (RuntimeError, TypeError, ValueError) as error:
			pm.warning('DPoint creation option could not be read: {0}'.format(error))
			return False
