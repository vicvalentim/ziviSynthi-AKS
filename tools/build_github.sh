#!/usr/bin/env bash
# Build the same artifacts locally and on a hosted Ubuntu runner.
set -euo pipefail
cd "$(dirname "$0")/.."

deps="${ZIVI_BUILD_DEPS:-$PWD/.deps}"
dist="${ZIVI_BUILD_DIST:-$PWD/dist}"
fm1_ref=2bb8e74f2962475eeeefd5179420bd8d73a3da6c
smk_ref=eafac162b4d7bc59176919a36eed179955315f93
sdk_ref=d179b4484759423312073f5fbb232501aa491047
open_ref=b2d6bcbcd724adb13bcf85e44cfa16bb2c8153a8
toolchain_dir=jieli-linux-toolchains-20260730.1
toolchain_sha=a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd

checkout() {
    local path=$1 url=$2 commit=$3 ref="${4:-$3}"
    if [[ ! -e "$path/.git" ]]; then
        mkdir -p "$path"
        git -C "$path" init -q
    fi
    if ! git -C "$path" cat-file -e "$commit^{commit}" 2>/dev/null; then
        git -C "$path" fetch --depth 1 "$url" "$ref"
    fi
    git -C "$path" checkout --detach "$commit"
}

base() {
    local path=$1 commit=$2
    checkout "$path" https://github.com/charlesvestal/fm1-base.git "$commit"
    git -C "$path" submodule update --init --depth 1 bsp
}

smk_dependencies() {
    FM1_BASE=${FM1_BASE:-$deps/fm1-base-smk}
    if [[ "$FM1_BASE" == "$deps/fm1-base-smk" ]]; then base "$FM1_BASE" "$smk_ref"; fi
    jieli_dependencies
}

jieli_dependencies() {
    if [[ -z "${AC79_SDK:-}" ]]; then
        AC79_SDK="$deps/ac79"
        checkout "$AC79_SDK" "${ZIVI_AC79_GIT_URL:-https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git}" \
            "$sdk_ref" refs/tags/AC79NN_SDK_V1.2.1_2023-12-13
    fi
    if [[ -z "${JIELI_TOOLCHAIN:-}" ]]; then
        JIELI_TOOLCHAIN="$deps/$toolchain_dir"
        if [[ ! -x "$JIELI_TOOLCHAIN/pi32v2/bin/clang" ]]; then
            local archive="$deps/$toolchain_dir.tar.xz"
            mkdir -p "$deps"
            if [[ ! -f "$archive" ]]; then
                curl --fail --location --retry 3 --silent --show-error --max-time 180 \
                    "${ZIVI_JIELI_ARCHIVE_URL:-https://pkgman.jieliapp.com/s/linux-toolchain}" --output "$archive.part"
                mv "$archive.part" "$archive"
            fi
            if ! printf '%s  %s\n' "$toolchain_sha" "$archive" | sha256sum --check; then
                printf 'Use ZIVI_JIELI_ARCHIVE_URL com o arquivo JieLi 20260730.1 correspondente ao SHA-256 fixado.\n' >&2
                return 1
            fi
            tar -xJf "$archive" -C "$deps"
        fi
    fi
}

checksums() {
    (cd "$1" && find . -type f ! -name SHA256SUMS.txt -print0 \
        | LC_ALL=C sort -z | xargs -0 sha256sum > SHA256SUMS.txt)
}

build_pro() {
    local out="$dist/smk37-pro" kit="$PWD/build/github/fm1-base-source.tar.gz"
    mkdir -p "$out"
    smk_dependencies
    make test-smk-felucca FM1_BASE="$FM1_BASE"
    make smk-felucca-firmware FM1_BASE="$FM1_BASE" AC79_SDK="$AC79_SDK" \
        JIELI_TOOLCHAIN="$JIELI_TOOLCHAIN" FELUCCA_SOURCE_KIT="$kit"
    python3 tests/verify_felucca_image.py build/smk37-felucca "$FM1_BASE"
    local rollback=()
    if [[ -n "${FIRMWARE_ROLLBACK:-}" ]]; then rollback=(--rollback "$FIRMWARE_ROLLBACK"); fi
    python3 tools/stage_github_assets.py build/smk37-felucca "$kit" "$out" "${rollback[@]}"
    ZIVI_EDITOR_HTML="$out/zivisynthi-aks-editor.html" node --test tests/test_firmware_web.cjs
    checksums "$out"
}

build_fm1_device() {
    local source=${FM1_HOST_BASE:-$deps/fm1-base-host}
    local out="$dist/fm-1"
    if [[ -z "${FM1_HOST_BASE:-}" ]]; then base "$source" "$fm1_ref"; fi
    jieli_dependencies
    make build/factory-presets.inc build/smk37-dsp-tables.inc
    mkdir -p "$out"
    python3 tools/build_fm1_device.py --fm1-base "$source" --sdk "$AC79_SDK" \
        --toolchain "$JIELI_TOOLCHAIN" --output "$out"
    cp LICENSE NOTICES.md "$out/"
    checksums "$out"
}

build_open() {
    local model=$1
    local kit=${SMK37_OPEN_SDK:-$deps/smk37-open}
    local out="$dist/smk37-open-$model"
    if [[ -z "${SMK37_OPEN_SDK:-}" ]]; then
        checkout "$kit" https://github.com/m-vave/smk37_open.git "$open_ref"
    fi
    jieli_dependencies
    make build/factory-presets.inc build/smk37-dsp-tables.inc
    mkdir -p "$out"
    python3 tools/build_smk37_open.py --model "$model" --open-sdk "$kit" --toolchain "$JIELI_TOOLCHAIN" \
        --libm "$AC79_SDK/include_lib/newlib/pi32v2-lib/libm.a" --output "$out"
    cp LICENSE NOTICES.md "$kit/LICENSE.md" "$kit/THIRD_PARTY.md" "$out/"
    checksums "$out"
}

build_fm1_host() {
    local source=${FM1_HOST_BASE:-$deps/fm1-base-host}
    local out="$dist/fm-1-host"
    if [[ -z "${FM1_HOST_BASE:-}" ]]; then base "$source" "$fm1_ref"; fi
    make test-fm1 FM1_BASE="$source"
    mkdir -p "$out"
    tar -czf "$out/zivisynthi-fm1-host-linux-x86_64.tar.gz" \
        -C platform/fm1_base/build/host zivisynthi_host
    cp docs/development/BUILDING.md "$out/BUILDING.md"
    cp NOTICES.md LICENSE "$out/"
    cp "$source/LICENSE" "$out/fm1-base-MPL-2.0.txt"
    checksums "$out"
}

build_web() {
    local out="$dist/web-studio"
    make web
    node --test tests/test_studio.cjs
    mkdir -p "$out"
    cp build/web/index.html "$out/zivisynthi-aks-editor.html"
    cp NOTICES.md LICENSE "$out/"
    checksums "$out"
}

if [[ "${1:-}" != "web-studio" && "$(uname -s)-$(uname -m)" != Linux-x86_64 ]]; then
    printf 'Os alvos nativos usam Linux x86-64. Selecione o alvo no GitHub Actions.\n' >&2
    exit 2
fi

case "${1:-}" in
    smk37-pro) build_pro ;;
    smk37-open-pro) build_open pro ;;
    smk37-open-elite) build_open elite ;;
    fm-1) build_fm1_device ;;
    fm-1-host) build_fm1_host ;;
    web-studio) build_web ;;
    all) build_web; build_fm1_host; build_pro; build_fm1_device; build_open pro; build_open elite ;;
    *) printf 'Uso: %s {smk37-pro|smk37-open-pro|smk37-open-elite|fm-1|fm-1-host|web-studio|all}\n' "$0" >&2; exit 2 ;;
esac
