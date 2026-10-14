import pymel.core as pm

import os

import DPyTools.system.DFileOp as dfile
import DPyTools.system.DPathOp as dpath


'''
to run:
	>>> import DPyTools.system.DWorkspace as dws

'''







def getProjectPath():

	#projectpath example : /path/to/project/human/3d/ <-last char is /(slash)

	return os.path.normpath(pm.workspace(q=True, rootDirectory=True))


def getCurrentFileName():
	filepath = pm.system.sceneName()
	return dpath.lastComponent(filepath)
	#return mel.eval("file -q -sn -shn;")



def getProjDataPath(datafolder=''):

	#get project path
	#projPath = getProjectPath()

	#append datafolder
	#datapath = (projPath+datafolder)
	#return datapath
	return dpath.append(getProjectPath(), datafolder)



def isExistDataDir(datafolder=''):
	datapth = getProjDataPath(datafolder=datafolder)
	return dfile.isExist(datapth)











