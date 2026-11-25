#!/usr/bin/env bash
# Rebuild an external, disposable beyond2lte firmware tree with the XNU app.
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
: "${WORKSPACE:?Set WORKSPACE to the disposable prepared Mu-Silicium tree}"
"$repo_root/scripts/build-efi.sh"
loader="$WORKSPACE/Build/B2LXnu/RELEASE_CLANGPDB/AARCH64/XnuLoader.efi"
board="${B2L_BOARD:-beyond2lte}"
export B2L_BOARD="$board"
prepare_options=()
if [[ "${B2L_ENABLE_TOUCH:-0}" == 1 ]]; then prepare_options+=(--enable-touch); fi
# Dual boot: Windows Boot Manager needs ACPI. Compile the S5E9820 tables the beyond2lte-uefi overlay
# put into Silicium-ACPI (only its four .dsl; PPTT.aml ships compiled) and keep ACPI in the FV.
if [[ "${B2L_WINDOWS_ACPI:-0}" == 1 ]]; then
  acpi_dir="$WORKSPACE/Silicium-ACPI/SoCs/Samsung/S5E9820"
  command -v iasl >/dev/null || { echo "iasl missing (brew install acpica)" >&2; exit 1; }
  # Touchscreen devices (I2C0 + TSP0) for the exynos9810-woa S10e drivers.
  "${PYTHON_COMMAND:-python3}" "$repo_root/scripts/windows-acpi-devices.py" "$acpi_dir/DSDT_minimal.dsl"
  # The overlay left only CPU0 enabled; Windows gets CPU0..CPU(N-1) (BCD numproc still caps what it starts).
  "${PYTHON_COMMAND:-python3}" "$repo_root/scripts/windows-acpi-cpus.py" \
    "$acpi_dir/APIC.dsl" "$acpi_dir/DSDT_minimal.dsl" --cpus "${B2L_WINDOWS_CPUS:-8}"
  for table in APIC FACP GTDT DSDT_minimal; do
    (cd "$acpi_dir" && iasl -ve "$table.dsl")
    test -s "$acpi_dir/$table.aml"
  done
  prepare_options+=(--windows-acpi)
fi
# Expand safely when the array is empty under bash 3.2's set -u.
"${PYTHON_COMMAND:-python3}" "$repo_root/scripts/prepare-boot-firmware.py" \
  --mu "$WORKSPACE" --loader "$loader" \
  --loader-guid 92742D9E-1A4F-42F8-9DC9-53895297B2F4 ${prepare_options[@]+"${prepare_options[@]}"}
export PACKAGES_PATH="$WORKSPACE/Platforms/Samsung:$WORKSPACE/Common/Mu:$WORKSPACE/Common/Mu_OEM_Sample:$WORKSPACE/Silicon/Samsung:$WORKSPACE/Silicon/Silicium${PACKAGES_PATH:+:$PACKAGES_PATH}"
# DSC build-time ACPI declarations remain external. Their firmware-volume
# inclusion is disabled by the XNU profile; XNU uses an Apple device tree.
export PACKAGES_PATH="$PACKAGES_PATH:$WORKSPACE/Silicium-ACPI:$WORKSPACE/Silicium-ACPI/Platforms/Samsung:$WORKSPACE/Silicium-ACPI/SoCs/Samsung"
export DSC_PATH="${board}Pkg/${board}.dsc"
exec "$repo_root/scripts/build-efi.sh" \
  -D FD_BASE=0xC9000000 -D FD_SIZE=0x400000 -D FD_BLOCKS=0x400 \
  -D ENABLE_SECUREBOOT=0 -D DEVICE_MODEL=0 "$@"
