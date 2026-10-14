# -*- coding: utf-8 -*-
u"""Compatibility exports for the historical asset operations path."""

from DPyTools.pipeline.assets import operations as _implementation


__all__ = (
	'mayaGeoDic', 'mayaConstDic', 'typeTagList', 'sideTagList',
	'sideTagCheckList', 'transTagList', 'classList', 'assetClassDic',
	'stateList', 'rootProjectPath', 'vfxFolder', 'mailingList_file',
	'user_asset_file', 'proj_asset_file', 'root_path_key', 'username_key',
	'asset_type_key', 'current_project_key', 'notif_send_email_key',
	'deleteUnUsedNodes', 'createProjectFolderStructure',
	'getAssetUserFileName', 'getAssetProjectFileName',
	'createDefaultAssetUserData', 'isExistUserAssetFile',
	'readUserAssetData', 'updateUserAssetData', 'getAssetUserDataDic',
	'getNotifState', 'setAssetUserDataDic', 'getAssetProjectRootPath',
	'getAssetCurrentProjectPath', 'getAssetTypeList', 'getAssetFolderPath',
	'getMaillingListFile', 'getMaillingList', 'addUserMailingList', 'addUsersMailingList',
	'deleteUserMailningList', 'getMailAddressFromML', 'getShotFolderPath',
	'getAssetTypeProjectList', 'getAssetTypeListInProject',
	'createNewAssetFolder', 'getAssetList', 'getAssetPath',
	'getAssetVersionList', 'generateAssetFileName', 'getExportPath',
	'DMakeProject', 'DMakeAssetClass', 'DProjectTextScroll',
	'DAssetTypeTextScroll', 'DAssetTextScroll', 'getSimplePorjectCode',
	'getUserName', 'getObjectTypeCode', 'getAssetTypeNodeFromHierarchy',
	'isHaveShape', 'getAssetTransformNoShapeFromHierarchy',
	'getAssetTransformFromHierarchy', 'getAssetGeometryFromHierarchy',
	'checkShapeType', 'getAssetShapeParentFromHierarchy',
	'assetAutoRenameGen',
)

for _name in __all__:
	globals()[_name] = getattr(_implementation, _name)
del _name
