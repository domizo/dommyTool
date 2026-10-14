# -*- coding: utf-8 -*-
"""Backward-compatible Maya entry point for the archived DPyTools package."""

DPToolsMenuName = 'DPToolsMenu'


def _bootstrap():
	import DPyTools.bootstrap as bootstrap
	return bootstrap


def createDPyToolsMenu():
	return _bootstrap().createDPyToolsMenu()


def menuImport():
	return _bootstrap().menuImport()


def main():
	return _bootstrap().main()


def unload():
	return _bootstrap().unload()
