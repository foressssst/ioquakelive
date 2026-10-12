#!/bin/bash
set -euo pipefail

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
	echo "Usage: $0 <release|debug> [arch]"
	exit 1
fi

TARGET_NAME="$1"
ARCH="${2:-$(uname -m)}"
OBJROOT="build"
BUILT_PRODUCTS_DIR="${OBJROOT}/${TARGET_NAME}-darwin-${ARCH}"

if [ ! -d "${BUILT_PRODUCTS_DIR}" ]; then
	echo "Build directory not found: ${BUILT_PRODUCTS_DIR}"
	exit 1
fi

PRODUCT_NAME="ioquakelive"
WRAPPER_NAME="${PRODUCT_NAME}.app"
CONTENTS="${BUILT_PRODUCTS_DIR}/${WRAPPER_NAME}/Contents"
MACOS="${CONTENTS}/MacOS"
RESOURCES="${CONTENTS}/Resources"
BASEDIR="baseq3"

CLIENT="${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}.${ARCH}"
if [ ! -f "${CLIENT}" ]; then
	echo "Client binary not found: ${CLIENT}"
	exit 1
fi

echo "Creating ${WRAPPER_NAME} for ${ARCH}..."

mkdir -p "${MACOS}"
mkdir -p "${RESOURCES}"

cp "${CLIENT}" "${MACOS}/${PRODUCT_NAME}"
if [ -f "${BUILT_PRODUCTS_DIR}/ioqlds.${ARCH}" ]; then
	cp "${BUILT_PRODUCTS_DIR}/ioqlds.${ARCH}" "${MACOS}/ioqlds"
fi

if [ -f "${BUILT_PRODUCTS_DIR}/opengl2${ARCH}.dylib" ]; then
	cp "${BUILT_PRODUCTS_DIR}/opengl2${ARCH}.dylib" "${MACOS}/opengl2${ARCH}.dylib"
fi
if [ -f "${BUILT_PRODUCTS_DIR}/opengl1${ARCH}.dylib" ]; then
	cp "${BUILT_PRODUCTS_DIR}/opengl1${ARCH}.dylib" "${MACOS}/opengl1${ARCH}.dylib"
fi
if [ -f "${BUILT_PRODUCTS_DIR}/vulkan${ARCH}.dylib" ]; then
	cp "${BUILT_PRODUCTS_DIR}/vulkan${ARCH}.dylib" "${MACOS}/vulkan${ARCH}.dylib"
fi

RESOURCES="${CONTENTS}/Resources"
mkdir -p "${RESOURCES}/baseq3"

DYLIB_DIR="${BUILT_PRODUCTS_DIR}/baseq3"
if [ -n "${PREBUILT_IOBIN_PK3:-}" ]; then
	if [ ! -f "${PREBUILT_IOBIN_PK3}" ]; then
		echo "ERROR: PREBUILT_IOBIN_PK3 is set to '${PREBUILT_IOBIN_PK3}' but that file does not exist." >&2
		exit 1
	fi
	cp "${PREBUILT_IOBIN_PK3}" "${RESOURCES}/baseq3/iobin.pk3"
	echo "Bundled pre-built iobin.pk3 from ${PREBUILT_IOBIN_PK3}"
elif [ -d "${DYLIB_DIR}" ]; then
	STAGE=$(mktemp -d)
	dylib_count=0
	for dylib in "${DYLIB_DIR}"/*.dylib; do
		[ -f "$dylib" ] || continue
		cp "$dylib" "${STAGE}/"
		dylib_count=$((dylib_count + 1))
	done
	if [ "$dylib_count" -eq 0 ]; then
		echo "ERROR: ${DYLIB_DIR} exists but contains no *.dylib files." >&2
		echo "       Either set PREBUILT_IOBIN_PK3 or build the game dylibs first." >&2
		rm -rf "${STAGE}"
		exit 1
	fi
	python3 code/tools/make_deterministic_pk3.py \
		--input "${STAGE}" \
		--output "${RESOURCES}/baseq3/iobin.pk3"
	rm -rf "${STAGE}"
	echo "Bundled iobin.pk3 in Contents/Resources/baseq3/ (from ${dylib_count} local dylibs)"
else
	echo "ERROR: no source for iobin.pk3 - PREBUILT_IOBIN_PK3 is unset and ${DYLIB_DIR} does not exist." >&2
	exit 1
fi

if [ ! -f "${RESOURCES}/baseq3/iobin.pk3" ]; then
	echo "ERROR: ${RESOURCES}/baseq3/iobin.pk3 was not created." >&2
	exit 1
fi

if [ -f "${BUILT_PRODUCTS_DIR}/baseq3/pak01.pk3" ]; then
	cp "${BUILT_PRODUCTS_DIR}/baseq3/pak01.pk3" "${RESOURCES}/baseq3/"
	echo "Bundled pak01.pk3 in Contents/Resources/baseq3/"
fi

FRAMEWORKS="${CONTENTS}/Frameworks"
mkdir -p "${FRAMEWORKS}"

bundle_dep() {
    local src="$1"
    local name
    name=$(basename "$src")
    local dst="${FRAMEWORKS}/${name}"
    [ -f "$dst" ] && return 0
    [ -f "$src" ] || { echo "  Warning: $src not found"; return 1; }
    cp "$src" "$dst"
    chmod 755 "$dst"
    install_name_tool -id "@rpath/${name}" "$dst"
    install_name_tool -add_rpath "@loader_path/" "$dst" 2>/dev/null || true
    echo "  Bundled ${name}"
    rewrite_homebrew_deps "$dst"
}

rewrite_homebrew_deps() {
    local bin="$1"
    local deps
    deps=$(otool -L "$bin" 2>/dev/null | awk 'NR>1 {print $1}')
    local dep
    while IFS= read -r dep; do
        case "$dep" in
            /opt/homebrew/*|/usr/local/Cellar/*|/usr/local/opt/*)
                local name
                name=$(basename "$dep")
                install_name_tool -change "$dep" "@rpath/${name}" "$bin" 2>/dev/null
                bundle_dep "$dep"
                ;;
        esac
    done <<< "$deps"
}

echo "Bundling Homebrew dependencies..."
for bin in \
    "${MACOS}/${PRODUCT_NAME}" \
    "${MACOS}/ioqlds" \
    "${MACOS}/opengl2${ARCH}.dylib" \
    "${MACOS}/opengl1${ARCH}.dylib" \
    "${MACOS}/vulkan${ARCH}.dylib"; do
    [ -f "$bin" ] || continue
    install_name_tool -add_rpath "@executable_path/../Frameworks" "$bin" 2>/dev/null || true
    rewrite_homebrew_deps "$bin"
done
echo "Done bundling Homebrew dependencies."

if [ -f "${BUILT_PRODUCTS_DIR}/libSDL3.0.dylib" ]; then
    bundle_dep "${BUILT_PRODUCTS_DIR}/libSDL3.0.dylib"
fi

if [ -f "${BUILT_PRODUCTS_DIR}/libMoltenVK.dylib" ]; then
	cp "${BUILT_PRODUCTS_DIR}/libMoltenVK.dylib" "${FRAMEWORKS}/libMoltenVK.dylib"
	chmod 755 "${FRAMEWORKS}/libMoltenVK.dylib"
	if [ -f "${BUILT_PRODUCTS_DIR}/MoltenVK-LICENSE.txt" ]; then
		cp "${BUILT_PRODUCTS_DIR}/MoltenVK-LICENSE.txt" "${CONTENTS}/Resources/MoltenVK-LICENSE.txt"
	fi
fi

if [ ! -f "${FRAMEWORKS}/libSDL3.0.dylib" ]; then
    echo "ERROR: no libSDL3 dylib was bundled into ${FRAMEWORKS}." >&2
    echo "       Build the code/SDL3 submodule and place libSDL3.0.dylib in ${BUILT_PRODUCTS_DIR}." >&2
    exit 1
fi

ICNS_NAME="quakelive.icns"
ICO_SRC="misc/quakelive.ico"
if [ -f "${ICO_SRC}" ] && command -v sips &>/dev/null && command -v iconutil &>/dev/null; then
	ICONSET=$(mktemp -d)/quakelive.iconset
	mkdir -p "${ICONSET}"

	sips -s format png "${ICO_SRC}" --out "${ICONSET}/icon_512x512@2x.png" &>/dev/null
	for SIZE in 16 32 128 256 512; do
		sips -z ${SIZE} ${SIZE} "${ICONSET}/icon_512x512@2x.png" --out "${ICONSET}/icon_${SIZE}x${SIZE}.png" &>/dev/null
	done
	for SIZE in 16 32 128 256; do
		DOUBLE=$((SIZE * 2))
		sips -z ${DOUBLE} ${DOUBLE} "${ICONSET}/icon_512x512@2x.png" --out "${ICONSET}/icon_${SIZE}x${SIZE}@2x.png" &>/dev/null
	done

	iconutil -c icns "${ICONSET}" -o "${RESOURCES}/${ICNS_NAME}" 2>/dev/null
	rm -rf "$(dirname "${ICONSET}")"
	echo "Generated ${ICNS_NAME} from ${ICO_SRC}"
elif [ -f "misc/quakelive.icns" ]; then
	cp "misc/quakelive.icns" "${RESOURCES}/${ICNS_NAME}"
else
	echo "Warning: No icon source found, .app will have no icon"
	ICNS_NAME=""
fi

echo -n "APPLQLIV" > "${CONTENTS}/PkgInfo"

VERSION=$(grep '^VERSION=' Makefile 2>/dev/null | head -1 | sed 's/VERSION=//')
[ -z "$VERSION" ] && VERSION=$(git describe --always 2>/dev/null || echo "dev")

cat > "${CONTENTS}/Info.plist" << PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>${PRODUCT_NAME}</string>
    <key>CFBundleIconFile</key>
    <string>${ICNS_NAME}</string>
    <key>CFBundleIdentifier</key>
    <string>com.tjone270.${PRODUCT_NAME}</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>Quake Live</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>${VERSION}</string>
    <key>CFBundleVersion</key>
    <string>${VERSION}</string>
    <key>CGDisableCoalescedUpdates</key>
    <true/>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSRequiresAquaSystemAppearance</key>
    <false/>
    <key>NSHumanReadableCopyright</key>
    <string>Copyright © 2015 id Software LLC, a ZeniMax Media company. All rights reserved.</string>
    <key>NSPrincipalClass</key>
    <string>NSApplication</string>
</dict>
</plist>
PLIST

echo "Created ${BUILT_PRODUCTS_DIR}/${WRAPPER_NAME}"
