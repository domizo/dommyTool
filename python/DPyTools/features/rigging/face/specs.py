# -*- coding: utf-8 -*-
u"""Immutable naming specifications shared by the archived face-rig tools."""


EYE_LOOK_LEFT = (
	'eye_lookUp_l', 'eye_lookDown1_l', 'eye_lookDown2_l',
	'eye_lookLeft_l', 'eye_lookRight_l',
)
EYE_LOOK_RIGHT = (
	'eye_lookUp_r', 'eye_lookDown1_r', 'eye_lookDown2_r',
	'eye_lookLeft_r', 'eye_lookRight_r',
)
EYE_LOOK_ALL = tuple(
	name for pair in zip(EYE_LOOK_LEFT, EYE_LOOK_RIGHT) for name in pair)

EYE_LOOK_GROUP = 'eye_look_bs'
EYE_GROUP = 'eye_bs'
BROW_GROUP = 'brow_bs'
NOSE_GROUP = 'nose_bs'
CHEEK_GROUP = 'cheek_bs'
MOUTH_GROUP = 'mouth_bs'
LIP_GROUP = 'lip_bs'
JAW_GROUP = 'jaw_bs'
NECK_GROUP = 'neck_bs'
EAR_GROUP = 'ear_bs'

FACE_BLEND_SHAPE_GROUPS = (
	EYE_LOOK_GROUP, EYE_GROUP, BROW_GROUP, NOSE_GROUP, CHEEK_GROUP,
	MOUTH_GROUP, LIP_GROUP, JAW_GROUP, NECK_GROUP, EAR_GROUP,
)

BROW_BLEND_SHAPES = (
	'brow_lower_l', 'brow_lower_r',
	'brow_raise_c_l', 'brow_raise_c_r',
	'brow_raise_l', 'brow_raise_r',
	'brow_frown_l', 'brow_frown_r',
)

EYE_BLEND_SHAPES = (
	'eye_blink1_l', 'eye_blink1_r',
	'eye_blink2_l', 'eye_blink2_r',
	'eye_blink_max_l', 'eye_blink_max_r',
	'eye_lidTight_l', 'eye_lidTight_r',
	'eye_upperLidRaise_l', 'eye_upperLidRaise_r',
	'eye_lidTight_u_l', 'eye_lidTight_u_r',
	'eye_lidTight_d_l', 'eye_lidTight_d_r',
	'eye_lid_smile_u_l', 'eye_lid_smile_u_r',
	'eye_lid_smile_d_l', 'eye_lid_smile_d_r',
	'eye_lid_dn_in_up_l', 'eye_lid_dn_in_up_r',
	'eye_lid_dn_out_up_l', 'eye_lid_dn_out_up_r',
	'eye_lid_dn_smile_l', 'eye_lid_dn_smile_r',
)

NOSE_BLEND_SHAPES = (
	'nose_wrinkle_l', 'nose_wrinkle_r',
	'nose_nostrils_out_l', 'nose_nostrils_out_r',
	'nose_nostrils_compressed_l', 'nose_nostrils_compressed_r',
	'nose_nostrils_delated_l', 'nose_nostrils_delated_r',
	'nose_nostrils_delated_exr_l', 'nose_nostrils_delated_exr_r',
	'nose_nostrils_delated_max_l', 'nose_nostrils_delated_max_r',
	'nose_nostrils_lower_l', 'nose_nostrils_lower_r',
)

CHEEK_BLEND_SHAPES = (
	'cheek_puff_l', 'cheek_puff_r',
	'cheek_raise_l', 'cheek_raise_r',
	'cheek_suck_l', 'cheek_suck_r',
	'cheek_raised_l', 'cheek_raised_r',
	'cheek_contracted_l', 'cheek_contracted_r',
	'cheek_pop_l', 'cheek_pop_r',
	'cheek_inhaled_l', 'cheek_inhaled_r',
	'cheek_inhaled_no_mouth_l', 'cheek_inhaled_no_mouth_r',
)

MOUTH_BLEND_SHAPES = (
	'mouth_chew_c',
	'mouth_chinRaise_u', 'mouth_chinRaise_d',
	'mouth_dimple_l', 'mouth_dimple_r',
	'mouth_oo_c', 'mouth_oh_c',
	'mouth_press_l', 'mouth_press_r',
	'mouth_pucker_l', 'mouth_pucker_r',
	'mouth_scream_c',
	'mouth_sideways_l', 'mouth_sideways_r',
	'mouth_stretch_c',
	'mouth_suck_u_l', 'mouth_suck_u_r',
	'mouth_suck_d_l', 'mouth_suck_d_r',
	'mouth_little_opened', 'mouth_opened', 'mouth_larged_opened',
	'mouth_exlarged_opened1', 'mouth_wide_opened',
	'mouth_sideways_l2', 'mouth_sideways_r2',
	'mouth_closed_contracted_jaw', 'mouth_closed_chin_up',
	'mouth_opened_closed_teeth',
	'mouth_closed_corner_raised_l', 'mouth_closed_corner_raised_r',
	'mouth_closed_corner_lower_l', 'mouth_closed_corner_lower_r',
	'mouth_opend_l', 'mouth_opend_r',
	'mouth_opened_closed_teeth_l', 'mouth_opened_closed_teeth_r',
	'mouth_opened_pinched_corner_l', 'mouth_opened_pinched_corner_r',
)

LIP_BLEND_SHAPES = (
	'mouth_lipCornerPullOpen_l', 'mouth_lipCornerPullOpen_r',
	'mouth_lipCornerPull_l', 'mouth_lipCornerPull_r',
	'mouth_lipStretchOpen_l', 'mouth_lipStretchOpen_r',
	'mouth_lipStretch_l', 'mouth_lipStretch_r',
	'mouth_lowerLipDepress_l', 'mouth_lowerLipDepress_r',
	'mouth_lowerLipProtude_c',
	'mouth_upperLipRaise_l', 'mouth_upperLipRaise_r',
	'lips_up_lower_c', 'lips_up_raised_c',
	'lips_up_in', 'lips_up_out', 'lips_dn_in', 'lips_dn_out',
	'lip_push_up', 'lip_push_down',
	'mouth_smile_lip_closed1', 'mouth_smile_lip_closed_l',
	'mouth_smile_lip_closed_r', 'mouth_smile_lip_opened1',
	'mouth_smile_lip_opened_l', 'mouth_smile_lip_opened_r',
	'lips_inner', 'lips_frown', 'lips_frown_l', 'lips_frown_r',
	'lips_frown_expr_l', 'lips_frown_expr_r',
	'lips_closed_blown', 'lips_kiss',
	'lips_protude', 'lips_protude_l', 'lips_protude_r',
	'lips_protude_d_l', 'lips_protude_d_r',
	'lips_stretch_l', 'lips_stretch_r',
	'lips_up_bite_u_l', 'lips_up_bite_u_r',
	'lips_up_bite_d_l', 'lips_up_bite_d_r',
	'lips_up_spread_u_l', 'lips_up_spread_u_r',
	'lips_up_spread_d_l', 'lips_up_spread_d_r',
	'lips_funnel_u_l', 'lips_funnel_u_r',
	'lips_funnel_d_l', 'lips_funnel_d_r',
	'lips_up_raised_l', 'lips_up_raised_r',
	'lips_up_tight_l', 'lips_up_tight_r',
	'lips_dn_raised_l', 'lips_dn_raised_r',
	'lips_dn_lower_l', 'lips_dn_lower_r',
	'lips_dn_raised_m_l', 'lips_dn_raised_m_r',
	'lips_dn_lower_m_l', 'lips_dn_lower_m_r',
)

JAW_BLEND_SHAPES = (
	'jaw_sideways_l', 'jaw_sideways_r', 'jaw_thrust_c',
	'jaw_move_out', 'jaw_move_lower',
)
NECK_BLEND_SHAPES = (
	'neck_contracted_l', 'neck_contracted_r',
	'neck_platysma_cntracted_l', 'neck_platysma_cntracted_r',
	'neck_platysma_l', 'neck_platysma_r',
	'neck_sternomastoid_contracted_l',
	'neck_sternomastoid_contracted_r',
)
EAR_BLEND_SHAPES = ('ear_up_l', 'ear_up_r')

FACE_BLEND_SHAPES_BY_GROUP = (
	(BROW_GROUP, BROW_BLEND_SHAPES),
	(EYE_GROUP, EYE_BLEND_SHAPES),
	(NOSE_GROUP, NOSE_BLEND_SHAPES),
	(CHEEK_GROUP, CHEEK_BLEND_SHAPES),
	(MOUTH_GROUP, MOUTH_BLEND_SHAPES),
	(LIP_GROUP, LIP_BLEND_SHAPES),
	(JAW_GROUP, JAW_BLEND_SHAPES),
	(NECK_GROUP, NECK_BLEND_SHAPES),
	(EAR_GROUP, EAR_BLEND_SHAPES),
)
FACE_BLEND_SHAPE_LISTS = tuple(
	shape_names for unused_group, shape_names in FACE_BLEND_SHAPES_BY_GROUP)
EYE_MOTION_BIASES = (-1.0, -0.66667, -0.3333, 0.0, 1.0)


def validate_specs():
	u"""Validate uniqueness and left/right eye pairing without importing Maya."""
	if len(FACE_BLEND_SHAPE_GROUPS) != len(set(FACE_BLEND_SHAPE_GROUPS)):
		raise ValueError('face blend-shape group names must be unique')
	if len(EYE_LOOK_LEFT) != len(EYE_LOOK_RIGHT):
		raise ValueError('left and right eye-look specifications must be paired')
	for left_name, right_name in zip(EYE_LOOK_LEFT, EYE_LOOK_RIGHT):
		if not left_name.endswith('_l') or not right_name.endswith('_r'):
			raise ValueError('eye-look names must use _l and _r suffixes')
		if left_name[:-2] != right_name[:-2]:
			raise ValueError('left and right eye-look names must match')

	all_names = []
	for group_name, shape_names in FACE_BLEND_SHAPES_BY_GROUP:
		if group_name == EYE_LOOK_GROUP or not shape_names:
			raise ValueError('invalid face blend-shape group specification')
		if len(shape_names) != len(set(shape_names)):
			raise ValueError(
				'duplicate blend-shape name in {0}'.format(group_name))
		all_names.extend(shape_names)
	if len(all_names) != len(set(all_names)):
		raise ValueError('blend-shape names must be unique across groups')
	return True
