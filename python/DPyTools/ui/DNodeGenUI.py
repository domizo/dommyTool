# -*- coding: utf-8 -*-
"""Read-only catalogue of the procedural node ideas explored by DPyTools.

The original window presented a large number of enabled buttons without any
commands.  Keeping that UI in the default Maya menu made unfinished features
look operational.  This compact catalogue preserves the historical design
record while making its non-functional status explicit.
"""

import pymel.core as pm

from DPyTools.ui import windows as ui_windows


NODE_CATALOG = (
    ('DPoint', (
        ('Generator', ('Add Point', 'Circle', 'Plane', 'From Curve',
                       'From Surface', 'Volume', 'From Mesh', 'From Fluid',
                       'Scatter')),
        ('Attribute', ('Create', 'Delete', 'Transfer', 'Smooth', 'Blend',
                       'SeExpr')),
        ('Group', ('Create', 'Delete')),
        ('Deform', ('Vector Deform', 'Convert Weight')),
        ('Solver', ('DPSolver',)),
        ('UV', ('UV Gen',)),
        ('Mesh', ('Duplicator', 'Voronoi', 'DPToMesh', 'Foam')),
        ('Animation', ('Anim Matrix', 'Motion Create', 'Rotation')),
        ('Render', ('Light Gen', 'Read Texture')),
        ('Utility', ('Transform', 'Delete', 'Merge', 'Fill Space', 'Compare',
                     'Distance', 'Direction', 'Color', 'Sort', 'Time Shift',
                     'Ray', 'Blend', 'Noise', 'DFNoise', 'Smooth',
                     'Adjust Space', 'Affect', 'Pyro', 'Magnet',
                     'Self Collision', 'Goal', 'Duplicator', 'Cache', 'Null')),
        ('Dynamics', ('Particle Attr', 'To NParticle')),
        ('Display', ('DPDisplay',)),
    )),
    ('DCurve', (
        ('Generator', ('Fur', 'From PFX', 'Aho Gen', 'From DP', 'Link Curve',
                       'From Curve', 'From Surface', 'From Mesh', 'Cloth Gen',
                       'From MCurve', 'To MCurve')),
        ('Solver', ('DCSolver',)),
        ('Deform', ('Vector Deform',)),
        ('Utility', ('Reform', 'Divide', 'Character', 'Copy', 'Fit Copy',
                     'UV Map', 'Delete', 'Color', 'Motion Path', 'Noise',
                     'Resample', 'Weight Map', 'Normal', 'Replicator',
                     'Mesh Copy')),
        ('Display', ('DCDisplay',)),
    )),
    ('DMesh', (
        ('Generator', ('From MMesh', 'To MMesh')),
        ('Attribute', ('Transfer', 'Copy')),
        ('Group', ('Create', 'Delete')),
        ('Solver', ('DMSolver',)),
        ('Dynamics', ('N-Rigid Gen',)),
        ('Deform', ('Vector Deform',)),
        ('Utility', ('Color', 'Remesh', 'Slicer', 'Boolean', 'Character',
                     'Explode View', 'Face In/Out', 'Delete', 'Transform',
                     'Noise', 'Subdivide', 'Copy UV', 'Fast Noise', 'Normal',
                     'Merge')),
        ('Display', ('DMDisplay',)),
    )),
    ('DVDB', (
        ('Generate', ('From Mesh', 'From Fluid', 'From Particle', 'Platonic',
                      'From DP', 'Sampling DP', 'Scatter DP')),
        ('Edit', ('Filter', 'Combine', 'Filter LS', 'Resample', 'Noise',
                  'Fracture', 'Transform', 'Copy', 'Clip', 'Morph LS',
                  'Vector Merge', 'Vector Split', 'Advect', 'Advect Point',
                  'Analysis', 'Remap')),
        ('Convert', ('To Mesh', 'Convert', 'To Sphere')),
        ('I/O', ('Read VDB', 'Write VDB')),
        ('Display', ('DVDBDisplay',)),
    )),
    ('DData', (
        ('Data', ('Double', 'Vector', 'String', 'Matrix')),
        ('Modify', ('Array Size', 'Point to Float', 'Edit Double')),
    )),
    ('DMath', (
        ('Math', ('Add', 'Subtract', 'Multiply', 'Divide', 'Modulo', 'Power',
                  'Min', 'Max', 'Negative', 'Square Root', 'Floor', 'Round',
                  'Ceil', 'Exponential', 'Absolute')),
        ('Vector Math', ('Dot', 'Angle', 'Length', 'Min Component',
                         'Max Component', 'Average Component', 'Normalize',
                         'Cross Product')),
        ('Other', ('Remap', 'Fit Range')),
    )),
)


class DNodeCatalogWindow(object):
    """Display the archived node catalogue without implying functionality."""

    WINDOW_NAME = 'DPyToolsDNodeCatalogWindow'

    def __init__(self):
        ui_windows.delete_window(self.WINDOW_NAME)

        self.window = pm.window(
            self.WINDOW_NAME, title='DPyTools Node Ideas (Archive)',
            sizeable=True, widthHeight=(620, 520))
        with pm.columnLayout(adjustableColumn=True, rowSpacing=6):
            pm.text(
                label='Historical design catalogue — controls are intentionally disabled.',
                align='left')
            tabs = pm.tabLayout(innerMarginWidth=6, innerMarginHeight=6)
            tab_children = []
            for tab_name, sections in NODE_CATALOG:
                child = self._create_tab(sections)
                tab_children.append((child, tab_name))
            pm.tabLayout(tabs, edit=True, tabLabel=tab_children)
        pm.showWindow(self.window)

    @staticmethod
    def _create_tab(sections):
        scroll = pm.scrollLayout(childResizable=True)
        with pm.columnLayout(adjustableColumn=True, rowSpacing=4):
            for section_name, labels in sections:
                pm.text(label=section_name, align='left', font='boldLabelFont')
                columns = min(4, max(1, len(labels)))
                with pm.gridLayout(numberOfColumns=columns, cellWidthHeight=(140, 24)):
                    for label in labels:
                        pm.button(
                            label=label, enable=False,
                            annotation='Archived concept; no command is implemented.')
        pm.setParent('..')
        return scroll


# Backward-compatible entry point for users who imported this module directly.
openWin = DNodeCatalogWindow
