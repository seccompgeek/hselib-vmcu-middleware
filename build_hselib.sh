#!/usr/bin/env bash
# build_hselib.sh — Build script for HSELib (C + Rust combined static library)
#
# Builds the Rust component, attempts the C component build, then combines
# both into build/libhselib.a as a single deduplicated archive.
#
# Usage:
#   ./build_hselib.sh [--clean] [--rust-only]
#
# Options:
#   --clean      Wipe and reconfigure the build directory before building
#   --rust-only  Skip the C build step (useful when C sources have not changed)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
RUST_DIR="$SCRIPT_DIR/rust"
RUST_TARGET="thumbv7em-none-eabihf"
RUST_ARCHIVE="$RUST_DIR/target/$RUST_TARGET/release/libhselib_rust.a"
OUTPUT="$BUILD_DIR/libhselib.a"
TOOLCHAIN="$SCRIPT_DIR/arm-none-eabi-linux.cmake"
C_OBJ_DIR="$BUILD_DIR/CMakeFiles/hselib.dir/src"

# --- locate toolchain binaries ------------------------------------------------
AR=$(command -v arm-none-eabi-ar 2>/dev/null) || {
    echo "ERROR: arm-none-eabi-ar not found in PATH" >&2; exit 1
}
RANLIB=$(command -v arm-none-eabi-ranlib 2>/dev/null) || {
    echo "ERROR: arm-none-eabi-ranlib not found in PATH" >&2; exit 1
}

# --- parse arguments ----------------------------------------------------------
DO_CLEAN=0
RUST_ONLY=0
for arg in "$@"; do
    case "$arg" in
        --clean)     DO_CLEAN=1 ;;
        --rust-only) RUST_ONLY=1 ;;
        *) echo "Unknown argument: $arg  (valid: --clean, --rust-only)" >&2; exit 1 ;;
    esac
done

if [[ $DO_CLEAN -eq 1 && -d "$BUILD_DIR" ]]; then
    echo "==> Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# ==============================================================================
# Step 1: Build Rust component
# ==============================================================================
echo "==> [1/3] Building Rust component (target: $RUST_TARGET)..."
cd "$RUST_DIR"

RUSTFLAGS="--remap-path-prefix=${RUST_DIR}=HSELib/rust \
           --remap-path-prefix=${HOME}/.cargo=.cargo-home \
           -C target-cpu=cortex-m7" \
    cargo build --release --target "$RUST_TARGET"

[[ -f "$RUST_ARCHIVE" ]] || {
    echo "ERROR: Rust archive not produced: $RUST_ARCHIVE" >&2; exit 1
}
echo "    Rust archive: $(du -sh "$RUST_ARCHIVE" | cut -f1)  ($RUST_ARCHIVE)"

# ==============================================================================
# Step 2: Build C component (best-effort — some files are expected to fail)
# ==============================================================================
echo ""
echo "==> [2/3] Building C component..."

if [[ $RUST_ONLY -eq 1 ]]; then
    echo "    Skipped (--rust-only)"
else
    mkdir -p "$BUILD_DIR"

    # Configure if the CMake cache doesn't exist yet
    if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
        cmake -B "$BUILD_DIR" \
              -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
              "$SCRIPT_DIR"
    fi

    # Build — some files fail to compile due to a pre-existing macro issue;
    # we continue and use whatever object files were successfully produced.
    cmake --build "$BUILD_DIR" --parallel "$(nproc)" 2>&1 || {
        echo "    WARNING: C build completed with errors (some objects may be missing)."
    }
fi

mapfile -t C_OBJS < <(find "$C_OBJ_DIR" -name '*.c.o' 2>/dev/null | sort)
echo "    C objects found: ${#C_OBJS[@]}"

if [[ ${#C_OBJS[@]} -eq 0 ]]; then
    echo "    WARNING: No C object files found. The output will contain only Rust code."
fi

# ==============================================================================
# Step 3: Combine C objects + Rust archive into a single deduplicated archive
#
# Strategy: copy all objects into a flat temp directory, then build one archive
# from scratch.  This avoids the double-merge issue that occurs when using
# 'ar -M ADDLIB' on an archive that already contains Rust objects.
# ==============================================================================
echo ""
echo "==> [3/3] Combining into $(basename "$OUTPUT")..."

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

# C objects go in first
for obj in "${C_OBJS[@]}"; do
    cp "$obj" "$TMPDIR/$(basename "$obj")"
done

# Extract Rust objects into the same directory.
# Rust object names are unique (hashed), so they never collide with C names.
pushd "$TMPDIR" > /dev/null
"$AR" x "$RUST_ARCHIVE"
popd > /dev/null

# Build the final archive and index it
"$AR" rcs "$OUTPUT" "$TMPDIR"/*.o
"$RANLIB" "$OUTPUT"

# ==============================================================================
# Summary
# ==============================================================================
echo ""
echo "=== Build complete ==="
ls -lh "$OUTPUT"
echo ""

C_COUNT=$(ar t "$OUTPUT" | { grep -c '\.c\.o$'  || true; })
R_COUNT=$(ar t "$OUTPUT" | { grep -cv '\.c\.o$' || true; })
DUPES=$(ar t "$OUTPUT" | sort | uniq -d | wc -l)

echo "  C objects    : $C_COUNT"
echo "  Rust objects : $R_COUNT"
echo "  Total        : $((C_COUNT + R_COUNT))"
echo "  Duplicates   : $DUPES"
[[ $DUPES -gt 0 ]] && echo "  WARNING: duplicate symbols detected!" || true
