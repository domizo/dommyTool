# -*- coding: utf-8 -*-
"""Immutable joint-name specifications shared by the historical rig UI."""


LEFT_ANKLE = ('ballL', 'toeL', 'heelL')
RIGHT_ANKLE = ('ballR', 'toeR', 'heelR')
LEFT_FOOT = ('up_legL', 'kneeL', 'ankleL', 'ballL', 'toeL')
RIGHT_FOOT = ('up_legR', 'kneeR', 'ankleR', 'ballR', 'toeR')
LEFT_ARM = ('clavicleL', 'up_armL', 'elbowL', 'wristL')
RIGHT_ARM = ('clavicleR', 'up_armR', 'elbowR', 'wristR')
LEFT_HAND = ('handL',)
RIGHT_HAND = ('handR',)
LEFT_CLAVICLE = ('real_clavicleL', 'real_clavicle_tipL')
RIGHT_CLAVICLE = ('real_clavicleR', 'real_clavicle_tipR')

LEFT_THUMB = ('thumb_a_L', 'thumb_b_L', 'thumb_c_L', 'thumb_d_L')
RIGHT_THUMB = ('thumb_a_R', 'thumb_b_R', 'thumb_c_R', 'thumb_d_R')
LEFT_INDEX = ('index_a_L', 'index_b_L', 'index_c_L', 'index_d_L')
RIGHT_INDEX = ('index_a_R', 'index_b_R', 'index_c_R', 'index_d_R')
LEFT_MIDDLE = ('middle_a_L', 'middle_b_L', 'middle_c_L', 'middle_d_L')
RIGHT_MIDDLE = ('middle_a_R', 'middle_b_R', 'middle_c_R', 'middle_d_R')
LEFT_RING = ('ring_a_L', 'ring_b_L', 'ring_c_L', 'ring_d_L')
RIGHT_RING = ('ring_a_R', 'ring_b_R', 'ring_c_R', 'ring_d_R')
LEFT_PINKY = ('pinky_a_L', 'pinky_b_L', 'pinky_c_L', 'pinky_d_L')
RIGHT_PINKY = ('pinky_a_R', 'pinky_b_R', 'pinky_c_R', 'pinky_d_R')

MIRRORED_JOINT_LISTS = (
    (LEFT_ANKLE, RIGHT_ANKLE),
    (LEFT_FOOT, RIGHT_FOOT),
    (LEFT_ARM, RIGHT_ARM),
    (LEFT_HAND, RIGHT_HAND),
    (LEFT_CLAVICLE, RIGHT_CLAVICLE),
    (LEFT_THUMB, RIGHT_THUMB),
    (LEFT_INDEX, RIGHT_INDEX),
    (LEFT_MIDDLE, RIGHT_MIDDLE),
    (LEFT_RING, RIGHT_RING),
    (LEFT_PINKY, RIGHT_PINKY),
)


def mirrored_name(name):
    """Return the expected right-side name for an archived left-side name."""
    if name.endswith('_L'):
        return name[:-2] + '_R'
    if name.endswith('L'):
        return name[:-1] + 'R'
    raise ValueError('Left-side joint name has no recognised suffix: {0}'.format(name))


def validate_mirrored_joint_lists(pairs=MIRRORED_JOINT_LISTS):
    """Raise when any left/right joint specification is not symmetrical."""
    for left_names, right_names in pairs:
        expected = tuple(mirrored_name(name) for name in left_names)
        if expected != tuple(right_names):
            raise ValueError(
                'Joint specification mismatch: expected {0}, found {1}'.format(
                    expected, tuple(right_names)))
    return True


def mirrored_position_items(left_items):
    """Mirror named XYZ positions across Maya's YZ plane."""
    mirrored = []
    for name, position in left_items:
        if len(position) != 3:
            raise ValueError(
                'Joint position must contain three values: {0}'.format(name))
        mirrored.append((
            mirrored_name(name),
            (-position[0], position[1], position[2])))
    return tuple(mirrored)


def validate_mirrored_position_items(left_items, right_items):
    """Raise when named right-side positions are not exact YZ mirrors."""
    expected = mirrored_position_items(left_items)
    if expected != tuple(right_items):
        raise ValueError(
            'Joint position mismatch: expected {0}, found {1}'.format(
                expected, tuple(right_items)))
    return True
