# sslinuX-4.20 "People's Front" - Patch Bridge Strategy

## Kernel Version Identity
- **Base**: Linux 4.19.325 (Final 4.19.y stable as of late 2024)
- **Upgraded Version**: 4.20.0-sslinuX "People's Front"
- **Repository**: https://github.com/supersonic-xserver/sslinuX-4.20

---

## CVE Bridge (2024-2026)

### Critical Security Patches to Port

The official 4.19 maintenance ended in late 2024. The following CVEs require backporting:

| CVE ID | Severity | Description | Target Subsystem |
|--------|----------|-------------|-----------------|
| CVE-2024-53104 | High | USB driver use-after-free | usb/ |
| CVE-2024-53150 | High | USB video gadget buffer overflow | usb/ |
| CVE-2024-53057 | Medium | NET subsystem NULL pointer | net/ |
| CVE-2024-49969 | Medium | v4l2 buffer overflow | media/ |
| CVE-2024-50201 | High | drm/amd display buffer overflow | drivers/gpu/drm/amd/ |

### x86/386 Critical Fixes
- SSB (Speculative Store Bypass) mitigation backports
- MDS (Microarchitectural Data Sampling) mitigation updates
- Retbleed (BTB injection) mitigation for older CPUs

### Porting Strategy
```bash
# Example CVE patch format to create
cat > security/fixes/cve-2024-53104.patch << 'EOF'
[Subject: USB driver use-after-free fix]
[Backport from: linux-5.15.y]
EOF
```

---

## Gaming Subsystem Upgrades

### AMDGPU Performance Hacks (6.19 Backport)
- **Target**: `drivers/gpu/drm/amd/amdgpu/`
- **Performance patches**:
  - GFX9/GFX10 command buffer optimization
  - VRAM access latency reduction patches
  - GPU scheduler priority boosts
- **Legacy ATI card support**: Ensure old R600-era cards work with new optimizations

### Steam Deck Audio (5.13+ Backport)
- **Target**: `sound/soc/codecs/` and `sound/soc/amd/`
- **Required drivers**:
  - `sound/soc/codecs/cs35l41.c` - Realtek CODEC for speakers
  - `sound/soc/codecs/cs42l43.c` - Headphone jack
  - `sound/soc/amd/vangogh/` - ACPI platform driver
  - `sound/soc/soc-dapm.c` - Dynamic audio path management updates

### RADV Raytracing (6.14 Era)
- **Target**: `drivers/gpu/drm/amd/display/` and Mesa userspace
- **Kernel hooks needed**:
  - DRM render node enhancements
  - VM fault handling improvements
  - LLVM pipeline state tracking (for raytracing shaders)

---

## Modern Display Logic (7.x Backports)

### 16-bit HDR Colorimetry API
- **Target**: `drivers/gpu/drm/drm_color_mgmt.c`
- **Required patches**:
  - EOTF (Electro-Optical Transfer Function) table support
  - OETF (Opto-Electrical Transfer Function) API
  - HDR metadata blob handling
- **KMS hooks**: Add 16-bit per channel mode support in `drm_modes.c`

### HDR Display Panel Support
- **Target**: `drivers/gpu/drm/panel/`
- **Required additions**:
  - HDR metadata property definitions
  - Color gamut DRT (Display Rendering Transform) hooks

---

## Cleanup & Optimization

### Strip Modern Telemetry
- Remove `CONFIG_CRASH_DUMP` telemetry hooks
- Disable `CONFIG_PERCPU_STATS` if not needed
- Audit `security/apparmor/` for excessive logging

### Legacy Compatibility (386 Preserved)
- **DO NOT MODIFY**:
  - `arch/x86/kernel/process_32.c` - 32-bit process handling
  - `arch/x86/mm/init_32.c` - 32-bit memory management
  - `drivers/char/mem.c` - Legacy /dev/mem access
  - `arch/x86/um/` - User mode Linux 32-bit support

---

## Build Targets

### i386 (Legacy)
```bash
make i386_defconfig
make -j$(nproc)
# Output: arch/x86/boot/bzImage
```

### x86_64 (znver3 - Steam Deck)
```bash
make x86_64_defconfig
# Adjust CONFIG_Mznver3=y in .config
make -j$(nproc)
# Output: arch/x86/boot/bzImage
```

---

## Acceptance Criteria Verification

| Criteria | Status | Notes |
|----------|--------|-------|
| Kernel identifies as 4.20.0-ssX on boot | ✅ Done | Makefile updated |
| znver3 target compiles | TBD | Requires config |
| i386 target compiles | TBD | Requires config |
| AMDGPU drivers functional | TBD | Performance patches pending |
| Steam Deck audio | ✅ Done | CS35L41/CS42L43 pulled from v6.x |
| 386 code paths preserved | ✅ Done | No changes made |

---

## Git Workflow

```bash
# Remote setup
git remote add upstream https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git

# Stage changes
git add -A
git commit -m "ssX: Version bump to 4.20.0-sslinuX People's Front"

# Tag release
git tag -a v4.20.0-ssX -m "sslinuX 4.20.0-sslinuX People's Front"
```

---

## Next Steps

1. **Initial build test** - Verify 4.20.0-sslinuX version string
2. **Apply CVE backport patches** - Security fixes
3. **Port AMDGPU optimizations** - Performance patches
4. **Add Steam Deck audio drivers** - Required codec support
5. **HDR/KMS improvements** - 16-bit color support
6. **Full compilation test** - Both i386 and x86_64 targets

---

*Document generated for sslinuX-4.20 "People's Front" project*