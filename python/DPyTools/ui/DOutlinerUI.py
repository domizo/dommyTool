import pymel.core as pm

from DPyTools.ui import windows as ui_windows
from DPyTools.scene import outliner

'''
to run:
	>>> import DPyTools.ui.DOutlinerUI as DOLui
	>>> DOLui.openWin()

'''


#create window
class openWin(object):

	def __init__(self):
		uiname = "DOutlinear Editor"

		ui_windows.delete_window(uiname)

		self.window = pm.window(uiname,widthHeight=(640, 320))
		self.form = pm.formLayout(numberOfDivisions=100)
		self.colmun = pm.columnLayout( columnAttach=('both', 5), rowSpacing=10, adjustableColumn=True )

		pm.button(label='ON Color',c = lambda *args: self.activeVisCB(vis=True) )
		pm.button(label='OFF Color',c = lambda *args: self.activeVisCB(vis=False) )

		self.colSlider = pm.colorSliderGrp( label='Outliner Color', rgb=(1, 1, 1), cc = lambda *args: self.changeColorCB() )

		pm.formLayout( self.form, edit=True,
			attachForm=[(self.colmun, 'top', 5), (self.colmun, 'left', 5), (self.colmun, 'right', 5), (self.colmun, 'bottom', 5) ] )

		pm.showWindow( self.window )

	def activeVisCB(self, vis=True):
		try:
			return outliner.set_color_enabled(enabled=vis) > 0
		except (RuntimeError, TypeError, ValueError) as error:
			pm.warning('Outliner color enablement was not completed: {0}'.format(error))
			return False

	def changeColorCB(self):
		try:
			color = pm.colorSliderGrp(self.colSlider, query=True, rgb=True)
			return outliner.set_color(color=color) > 0
		except (RuntimeError, TypeError, ValueError) as error:
			pm.warning('Outliner color change was not completed: {0}'.format(error))
			return False




