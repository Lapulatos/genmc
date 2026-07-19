#!/usr/bin/env bash
set -euo pipefail

root="${GENMC_EXPERIMENT_ROOT:-${HOME}/svcomp2026-caat}"
downloads="${root}/downloads"
tools="${root}/tools"
mkdir -p "${downloads}" "${tools}"

verify_archive() {
	local archive="$1" expected_size="$2" expected_md5="$3"
	local actual_size actual_md5
	actual_size="$(stat -c %s "${archive}")"
	actual_md5="$(md5sum "${archive}" | cut -d' ' -f1)"
	[[ "${actual_size}" == "${expected_size}" ]] || {
		echo "size mismatch for ${archive}: ${actual_size} != ${expected_size}" >&2
		return 1
	}
	[[ "${actual_md5}" == "${expected_md5}" ]] || {
		echo "MD5 mismatch for ${archive}: ${actual_md5} != ${expected_md5}" >&2
		return 1
	}
	unzip -tq "${archive}" >/dev/null
}

install_archive() {
	local archive="$1" top="$2" expected_md5="$3" destination="${tools}/$2"
	if [[ -f "${destination}/.svcomp-archive-md5" ]] &&
	   [[ "$(<"${destination}/.svcomp-archive-md5")" == "${expected_md5}" ]]; then
		echo "already installed ${destination}"
		return
	fi
	[[ ! -e "${destination}" ]] || {
		echo "refusing to overwrite existing ${destination}" >&2
		return 1
	}
	local temporary
	temporary="$(mktemp -d "${tools}/.${top}.XXXXXX")"
	trap 'rm -rf "${temporary}"' RETURN
	unzip -q "${archive}" -d "${temporary}"
	[[ -d "${temporary}/${top}" ]] || {
		echo "archive ${archive} has no expected top-level ${top}/" >&2
		return 1
	}
	mv "${temporary}/${top}" "${destination}"
	printf '%s\n' "${expected_md5}" >"${destination}/.svcomp-archive-md5"
	rmdir "${temporary}"
	trap - RETURN
}

ensure_archive() {
	local archive="$1" expected_size="$2" expected_md5="$3" url="$4"
	if [[ -f "${archive}" ]] &&
	   [[ "$(stat -c %s "${archive}")" == "${expected_size}" ]] &&
	   [[ "$(md5sum "${archive}" | cut -d' ' -f1)" == "${expected_md5}" ]]; then
		verify_archive "${archive}" "${expected_size}" "${expected_md5}"
		return
	fi
	curl -L --fail --retry 5 -C - -o "${archive}" "${url}"
	verify_archive "${archive}" "${expected_size}" "${expected_md5}"
}

deagle_archive="${downloads}/deagle.zip"
cbmc_archive="${downloads}/cbmc.zip"

ensure_archive "${deagle_archive}" 6338434 ee6cc0f6f37e661b04945732af1233a8 \
	https://zenodo.org/api/records/17636587/files/deagle.zip/content
ensure_archive "${cbmc_archive}" 14615005 66d24472f543c9f157a4fb65804b039c \
	https://zenodo.org/api/records/10396159/files/cbmc.zip/content

install_archive "${deagle_archive}" deagle ee6cc0f6f37e661b04945732af1233a8
install_archive "${cbmc_archive}" cbmc 66d24472f543c9f157a4fb65804b039c
chmod +x "${tools}/deagle/deagle" "${tools}/deagle/deagle_exe"
find "${tools}/cbmc" -maxdepth 2 -type f -name 'cbmc*' -exec chmod +x {} +

echo "installed Deagle at ${tools}/deagle"
echo "installed CBMC at ${tools}/cbmc"
