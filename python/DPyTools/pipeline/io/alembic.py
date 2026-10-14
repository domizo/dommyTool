import pymel.core as pm
import maya.cmds as cmds

import os
import time

from DPyTools.core.components import component_index
from DPyTools.pipeline.io import alembic_job
from DPyTools.ui import windows as ui_windows


'''
to run:
	>>> import DPyTools.pipeline.io.alembic as abcOp
	>>> abcOp.ddata_export()

'''


def _quote_job_argument(value):
	"""Compatibility wrapper for the historical private helper."""
	return alembic_job.quote_argument(value)



#open  window
class ddata_export():


	def __init__(self):

		uiname = "DExportAbc"

		ui_windows.delete_window(uiname)

		self.window = pm.window(uiname,widthHeight=(1400, 640))
		self.form = pm.formLayout(numberOfDivisions=100)
		self.fileRowLayout = pm.rowLayout(numberOfColumns=3,
										columnWidth3=(80, 600, 35),
										columnAttach3=("right", "both", "both"),
										columnAlign3=("left", "left", "center"),
										rowAttach=[(1, "both", 0),(2, "both", 0),(3, "both", 0)]
										)
		pm.text(label='Export Path', annotation='Export alembic file name.')
		self.exportFileField = pm.textField(text='', insertionPosition=0, editable=True, annotation='Browse for alembic file.')
		self.openFileDialogBtn = pm.button(label='...', align="center", command= lambda *args: self.openFileBrowser())

		pm.setParent( '..' )

		self.frameRange = pm.intFieldGrp(numberOfFields=2, label="Frame Range", value1=1, value2=60)
		self.frameStep = pm.floatSliderGrp(label="Frame Step", field=True, minValue=0.0, fieldMinValue=0.0, fieldMaxValue=1.0, value=1.0)
		self.selectedCheck = pm.optionMenuGrp( label='Export Object', columnWidth=(2, 120) )
		pm.menuItem( label='All Objects' )
		pm.menuItem( label='Selected Object to Root' )
		pm.menuItem( label='Selected Object Only' )


		self.ogawaCheckBox = pm.checkBox( label='Ogawa', value=True )
		self.isVerbose = pm.checkBox( label='VERBOSE', value=False )
		self.exportBtn = pm.button(label='Export',c = lambda *args: self.exportRun() )


		pm.formLayout(self.form, edit=True,
			attachForm=[(self.fileRowLayout, 'top', 0), (self.fileRowLayout, 'left', 0), (self.fileRowLayout, 'right', 0),
				(self.frameRange, 'left', 0), (self.frameRange, 'right', 0),
				(self.frameStep, 'left', 0), (self.frameStep, 'right', 0),
				(self.selectedCheck, 'left', 0), (self.selectedCheck, 'right', 0),
				(self.ogawaCheckBox, 'left', 0), (self.ogawaCheckBox, 'right', 0),
				(self.isVerbose, 'left', 0), (self.isVerbose, 'right', 0),
				(self.exportBtn, 'left', 0), (self.exportBtn, 'right', 0), (self.exportBtn, 'bottom', 0)],
			#attachPosition=[(self.setBtn, 'right', 0, 10)],
			attachControl=[(self.frameRange, 'top', 5, self.fileRowLayout),
				(self.frameStep, 'top', 5, self.frameRange),
				(self.selectedCheck, 'top', 5, self.frameStep),
				(self.ogawaCheckBox, 'top', 5, self.selectedCheck),
				(self.isVerbose, 'top', 5, self.ogawaCheckBox),
				(self.exportBtn, 'top', 5, self.isVerbose)]
			)

		#set default timeslider range.
		starttime = pm.playbackOptions( q=True, minTime=True )
		endTime = pm.playbackOptions( q=True, maxTime=True )
		pm.intFieldGrp(self.frameRange, e=True, value1=starttime, value2=endTime )

		pm.showWindow( self.window )

	def openFileBrowser(self):
		currentPath = pm.textField( self.exportFileField, q=True, text=True )
		if(len(currentPath)==0):
			currentPath = pm.workspace(query=1, dir=1)


		basicFilter = "Alembic Files (*.abc)"
		paths = pm.fileDialog2(fileFilter=basicFilter, dialogStyle=2, caption='Alembic File', startingDirectory=currentPath, fileMode=0)

		if paths and len(paths)>0 :
			pm.textField( self.exportFileField, e=True, text=paths[0] )



	def exportRun(self):
		filepath = pm.textField( self.exportFileField, q=True, text=True )
		if not filepath:
			pm.warning('File path is empty!')
			return
		filepath = os.path.abspath(os.path.expanduser(filepath))
		parent_directory = os.path.dirname(filepath)
		if not os.path.isdir(parent_directory):
			pm.warning('Export directory does not exist: {0}'.format(parent_directory))
			return

		print ("\n\n\n\n\n\n\n\n\nExport Alembic Start------------------------------------\n\n")
		start = time.time()
		pm.waitCursor(state=True)
		try:
			roots = []
			selection_only = False
			selectedType = component_index(pm.optionMenuGrp(self.selectedCheck, q=True, select=True))
			if selectedType not in (1, 2, 3):
				raise ValueError('Export object mode must be one of the three supported choices.')
			if selectedType == 2:
				selections = pm.ls(sl=True, long=True)
				if not selections:
					raise ValueError('Select at least one root object.')
				roots = selections
			elif selectedType == 3:
				if not pm.ls(sl=True):
					raise ValueError('Select at least one object.')
				selection_only = True

			flags = []
			if pm.checkBox(self.isVerbose, q=True, value=True):
				flags.append('-verbose')

			startFrame = pm.intFieldGrp(self.frameRange, q=True, value1=True)
			endFrame = pm.intFieldGrp(self.frameRange, q=True, value2=True)
			stepFrame = pm.floatSliderGrp(self.frameStep, q=True, value=True)
			data_format = None
			if pm.checkBox(self.ogawaCheckBox, q=True, value=True):
				data_format = 'ogawa'
			job = alembic_job.build_export_job(
				startFrame, endFrame, stepFrame, filepath,
				roots=roots, selection=selection_only, flags=flags,
				data_format=data_format)
			print ('Alembic job: {0}'.format(job))
			cmds.DExportAbcCmd(j=job)
		except (TypeError, ValueError) as error:
			pm.warning(str(error))
			return
		finally:
			pm.waitCursor(state=False)

		print ("********* Finish *********\n\n")
		elapsed_time = time.time() - start
		print ("Process_time:{0}".format(elapsed_time) + "[sec]")
		print ("\n\nExport Alembic End------------------------------------\n\n\n\n\n\n\n\n\n")
