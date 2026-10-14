# -*- coding: utf-8 -*-
"""Bounded image output plans and owned staging in trusted directories."""

import os

from DPyTools.core import components, paths, strings
from DPyTools.core.atomic_file import StagedFile, validate_destination


MAX_IMAGE_SIZE = 4096
MAX_CAPTURE_DIMENSION = 16384
MAX_CAPTURE_PIXELS = 16777216
MAX_ENCODED_BYTES = 64 * 1024 * 1024
FORMATS = {'png': 'PNG', 'jpg': 'JPEG', 'jpeg': 'JPEG'}
RESERVED_NAMES = paths.PORTABLE_RESERVED_NAMES  # Historical constant alias.


def image_size(value=None):
	value = 320 if value is None else components.component_index(value)
	if not 1 <= value <= MAX_IMAGE_SIZE:
		raise ValueError('Image size must be within 1..{0}'.format(MAX_IMAGE_SIZE))
	return value


def capture_byte_count(width, height):
	width, height = components.component_index(width), components.component_index(height)
	if (not 1 <= width <= MAX_CAPTURE_DIMENSION or not 1 <= height <= MAX_CAPTURE_DIMENSION
			or width * height > MAX_CAPTURE_PIXELS):
		raise ValueError('Viewport dimensions exceed the supported capture budget')
	return width * height * 4


def _destination(destination):
	return validate_destination(destination, error_type=ValueError)


def output_plan(directory, filename, ext='png', size=None):
	"""Validate every caller-supplied value before native image acquisition."""
	size = image_size(size)
	filename = paths.validate_portable_component(filename, 'image filename')
	if not isinstance(ext, strings.STRING_TYPES) or ext.lower() not in FORMATS:
		raise ValueError('Image format must be png, jpg or jpeg')
	ext = ext.lower()
	leaf = paths.validate_portable_component(filename + '.' + ext, 'image filename')
	if (not isinstance(directory, strings.STRING_TYPES) or not directory
			or any(ord(character) < 32 or ord(character) == 127 for character in directory)):
		raise ValueError('Image directory must be a nonempty path without control characters')
	directory = os.path.abspath(os.path.expanduser(directory))
	if not os.path.isdir(directory) or os.path.islink(directory):
		raise IOError('Image directory must exist and must not be a symbolic link')
	destination = os.path.join(directory, leaf)
	if not paths.is_within(destination, directory):
		raise ValueError('Image destination escapes its directory')
	return _destination(destination), FORMATS[ext], size


def publish_image(destination, image_format, encoder):
	"""Publish only an encoder-confirmed, bounded file; preserve an old image on failure.

	The encoder is a trusted native callback and must not replace the staging inode.
	Directory ancestry and concurrent hostile namespace changes are not authenticated.
	"""
	if image_format not in FORMATS.values() or not callable(encoder):
		raise ValueError('Image publication requires an admitted format and encoder')
	_destination(destination)
	with StagedFile(destination, mode='wb') as staged:
		saved = encoder(staged.temporary_name, image_format)
		if not isinstance(saved, bool) or not saved:
			raise IOError('Native image encoder did not confirm a successful save')
		staged.verify_owned()
		if not 0 < os.path.getsize(staged.temporary_name) <= MAX_ENCODED_BYTES:
			raise IOError('Encoded image is empty or exceeds the output byte budget')
		staged.seal()
		_destination(destination)
		staged.preserve_destination_mode(destination)
		staged.publish(destination)
	return destination
