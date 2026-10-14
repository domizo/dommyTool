# -*- coding: utf-8 -*-
u"""Compatibility import for the historical user manager path."""

from DPyTools.pipeline.assets import users


DAddUserWin = users.DAddUserWin
DUserManager = users.DUserManager
DUserSelectWin = users.DUserSelectWin
addUser = users.addUser
addUserData = users.addUserData
deleteUserFromId = users.deleteUserFromId
deleteUserFromName = users.deleteUserFromName
getUserList = users.getUserList
userIdKey = users.userIdKey
userMailKey = users.userMailKey
userNameKey = users.userNameKey
user_list_path = users.user_list_path
__all__ = [
	'DAddUserWin',
	'DUserManager',
	'DUserSelectWin',
	'addUser',
	'addUserData',
	'deleteUserFromId',
	'deleteUserFromName',
	'getUserList',
	'userIdKey',
	'userMailKey',
	'userNameKey',
	'user_list_path',
]
