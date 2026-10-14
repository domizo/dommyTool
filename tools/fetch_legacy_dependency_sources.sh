#!/usr/bin/env bash
set -euo pipefail

destination="${1:-${TMPDIR:-/tmp}/dommytool-legacy-dependencies}"
mkdir -p "$destination"

fetch_and_extract() {
  local archive_name="$1"
  local source_url="$2"
  local expected_sha256="$3"
  local extracted_directory="$4"
  local archive_path="$destination/$archive_name"

  if [[ ! -f "$archive_path" ]]; then
    curl --fail --silent --show-error --location \
      --output "$archive_path" "$source_url"
  fi

  local actual_sha256
  actual_sha256="$(shasum -a 256 "$archive_path" | awk '{print $1}')"
  if [[ "$actual_sha256" != "$expected_sha256" ]]; then
    echo "Checksum mismatch: $archive_path" >&2
    echo "expected: $expected_sha256" >&2
    echo "actual:   $actual_sha256" >&2
    exit 1
  fi

  if [[ ! -d "$destination/$extracted_directory" ]]; then
    tar -xzf "$archive_path" -C "$destination"
  fi
}

# These commits are the official release-tag commits closest to the original
# Maya 2019 development period. They are audit candidates, not a claim that
# the original binary was built from these exact revisions.
fetch_and_extract \
  "bullet-2.88-126b6762.tar.gz" \
  "https://github.com/bulletphysics/bullet3/archive/126b6762200b1a9ec8df39b8fd53cb2ad62fb146.tar.gz" \
  "b35c399be436165d8076a0c5e895b418f990137bf9f0bc9495881b54650cab7e" \
  "bullet3-126b6762200b1a9ec8df39b8fd53cb2ad62fb146"

fetch_and_extract \
  "openvdb-5.2.0-a79df2f3.tar.gz" \
  "https://github.com/AcademySoftwareFoundation/openvdb/archive/a79df2f32b8ebb5d15fd73325c46cee845a81e49.tar.gz" \
  "5196511bb496c9d19d815c85127d378b65eef98431ba0ca03d57cf2472aaef3b" \
  "openvdb-a79df2f32b8ebb5d15fd73325c46cee845a81e49"

fetch_and_extract \
  "openexr-2.2.1-dcaf5fdb.tar.gz" \
  "https://codeload.github.com/AcademySoftwareFoundation/openexr/tar.gz/dcaf5fdb4d1244d8e60a58832cfe9c54734a2257" \
  "d6943bda5418353ff37ddb6dbf7473dcab99d91c3022f27e5da64eb4925791e7" \
  "openexr-dcaf5fdb4d1244d8e60a58832cfe9c54734a2257"

fetch_and_extract \
  "opensubdiv-3.3.3-3b5c3d99.tar.gz" \
  "https://github.com/PixarAnimationStudios/OpenSubdiv/archive/3b5c3d99d29ba078385013d15f8b2ebeac0daf45.tar.gz" \
  "76491d1dfbbf8154ecf819d7a6e5099a3724613f0dac32aea6d67843c21af303" \
  "OpenSubdiv-3b5c3d99d29ba078385013d15f8b2ebeac0daf45"

fetch_and_extract \
  "seexpr-2.11-a5f02bb0.tar.gz" \
  "https://github.com/wdas/SeExpr/archive/a5f02bb03199630759b0b94a64f37ce56c08675a.tar.gz" \
  "6afdb4d624781593e82b3ec9d18ea9a25e7cc1e1e7b7d616bc4ac4ceaf8a4d85" \
  "SeExpr-a5f02bb03199630759b0b94a64f37ce56c08675a"

fetch_and_extract \
  "nanoflann-1.2.3-f2efec01.tar.gz" \
  "https://github.com/jlblancoc/nanoflann/archive/f2efec0110eb68edf8cae90fce394a5044bf3db8.tar.gz" \
  "fc0b3c38b6af7ff59079ac03eb1db76972ed75a0a8ce5a7faa38b740569168d2" \
  "nanoflann-f2efec0110eb68edf8cae90fce394a5044bf3db8"

echo "Legacy dependency sources are ready in: $destination"
