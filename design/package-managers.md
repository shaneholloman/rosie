# Package Manager Distribution Plan

This document outlines the plan for distributing rosie through various package managers.

## Overview

We'll support the following package managers:

| Platform | Package Manager | Method |
|----------|----------------|--------|
| macOS | Homebrew | Custom tap (`matthewp/homebrew-rosie`) |
| Arch Linux | AUR | PKGBUILD auto-published |
| FreeBSD | pkg | Custom repository on GitHub Pages |
| Debian/Ubuntu | apt | Codeberg Package Registry |

## Current State

rosie is a C project with:
- Dependencies: `libcurl`, `libarchive`
- Build system: `make`
- Binary output: `rosie`
- No man page yet (TODO?)

## Architecture Decisions

### Primary Source Host

**GitHub** - everything in one place. Source, CI, releases, and package hosting (via GitHub Pages for FreeBSD repo).

### Release Flow

```
Tag pushed (v*)
    │
    ├─► Linux job
    │   ├─► Build binary (linux-amd64)
    │   ├─► Build .deb package
    │   └─► Upload to GitHub Releases
    │
    ├─► macOS job
    │   ├─► Build binary (macos-arm64)
    │   ├─► Update Homebrew tap
    │   └─► Upload to GitHub Releases
    │
    ├─► FreeBSD job
    │   ├─► Build binary (freebsd-amd64)
    │   ├─► Build pkg package
    │   ├─► Publish to GitHub Pages
    │   └─► Upload to GitHub Releases
    │
    └─► AUR job
        └─► Generate and push PKGBUILD
```

---

## 1. Homebrew (macOS)

### Setup Required

1. **Create tap repository**: `github.com/matthewp/homebrew-rosie`
2. **GitHub Actions workflow** to:
   - Build on `macos-latest`
   - Generate SHA256 checksums
   - Create/update `Formula/rosie.rb`
   - Push to tap repository

### Formula Template

```ruby
class Rosie < Formula
  desc "A robot helper for agent skills"
  homepage "https://github.com/matthewp/rosie"
  url "https://github.com/matthewp/rosie/releases/download/v#{version}/rosie-macos-arm64"
  sha256 "CHECKSUM_HERE"
  license "BSD-3-Clause"

  depends_on "curl"
  depends_on "libarchive"

  def install
    bin.install "rosie-macos-arm64" => "rosie"
  end

  test do
    system "#{bin}/rosie", "--version"
  end
end
```

### User Installation

```bash
brew tap matthewp/rosie
brew install rosie
```

### Open Questions

- [ ] Do we need to build for both arm64 and x86_64?
- [ ] Should formula build from source instead of binary? (allows both architectures)

---

## 2. Arch Linux (AUR)

### Setup Required

1. **Register package on AUR**: `aur.archlinux.org/packages/rosie`
2. **SSH key** for AUR push (stored as GitHub secret)
3. **GitHub Actions workflow** to generate and push PKGBUILD

### PKGBUILD Template

```bash
# Maintainer: Matthew Phillips <matthew@matthewphillips.info>
pkgname=rosie
pkgver=${VERSION}
pkgrel=1
pkgdesc="A robot helper for agent skills"
arch=('x86_64' 'aarch64')
url="https://github.com/matthewp/rosie"
license=('BSD-3-Clause')
depends=('curl' 'libarchive')
source=("$pkgname-$pkgver.tar.gz::https://github.com/matthewp/rosie/archive/refs/tags/v$pkgver.tar.gz")
sha256sums=('CHECKSUM_HERE')

build() {
    cd "$pkgname-$pkgver"
    make
}

package() {
    cd "$pkgname-$pkgver"
    make DESTDIR="$pkgdir" PREFIX=/usr install
    install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
```

### Required Makefile Changes

Current Makefile `install` target:
```makefile
install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/
```

Need to support `DESTDIR` and `PREFIX`:
```makefile
PREFIX ?= /usr/local
DESTDIR ?=

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/
```

### User Installation

```bash
yay -S rosie
# or
paru -S rosie
```

### Status

Live as of v0.1.2. Initial AUR repo bootstrap was done with `aur/publish-initial.sh`; subsequent releases are auto-published by the GitHub Actions workflow via `KSXGitHub/github-actions-deploy-aur@v4.1.3`.

---

## 3. FreeBSD (pkg)

### Setup Required

1. **FreeBSD package files** in `freebsd-package/`:
   - `pkg-manifest.ucl` - package metadata
   - `pkg-plist` - file list
   - `build-package.sh` - build script
2. **GitHub Pages** to host the repository
3. **GitHub Actions workflow** using `vmactions/freebsd-vm`

### Package Manifest Template (`pkg-manifest.ucl`)

```ucl
name: rosie
version: "${VERSION}"
origin: sysutils/rosie
comment: A robot helper for agent skills
maintainer: matthew@matthewphillips.info
www: https://github.com/matthewp/rosie
abi: FreeBSD:14:amd64
arch: freebsd:14:x86:64
prefix: /usr/local
licenselogic: single
licenses: [BSD-3-Clause]
desc: <<EOD
A fast, cross-platform package manager for AI agent skills.
Think npm, but for skills.
EOD
deps: {
    curl: {origin: ftp/curl, version: "0"}
    libarchive: {origin: archivers/libarchive, version: "0"}
}
```

### Package File List (`pkg-plist`)

```
bin/rosie
```

### Build Script (`build-package.sh`)

```bash
#!/bin/sh
set -e

VERSION=$1
STAGING_DIR="staging"

# Create directory structure
mkdir -p "$STAGING_DIR/usr/local/bin"

# Copy binary
cp rosie "$STAGING_DIR/usr/local/bin/"

# Generate manifest with version
sed "s/\${VERSION}/$VERSION/g" freebsd-package/pkg-manifest.ucl > "$STAGING_DIR/+MANIFEST"
cp freebsd-package/pkg-plist "$STAGING_DIR/plist"

# Create package
pkg create -M "$STAGING_DIR/+MANIFEST" -r "$STAGING_DIR" -p "$STAGING_DIR/plist" -o .

# Create repository
mkdir -p repo
mv rosie-*.pkg repo/
pkg repo repo/
```

### User Installation

```bash
# Add repository
sudo mkdir -p /usr/local/etc/pkg/repos
cat <<'EOF' | sudo tee /usr/local/etc/pkg/repos/rosie.conf
rosie: {
  url: "https://matthewp.github.io/rosie/freebsd/",
  enabled: yes
}
EOF

# Install
sudo pkg update
sudo pkg install rosie
```

---

## 4. Debian/Ubuntu (apt)

### Setup Required

1. **Debian control file** in `debian/control`
2. **GitHub Actions workflow** to build .deb
3. **Package hosting** - options:
   - GitHub Releases (simple, manual download)
   - Launchpad PPA
   - GitHub Pages apt repository

### Control File Template (`debian/control`)

```
Package: rosie
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: amd64
Depends: libcurl4, libarchive13
Maintainer: Matthew Phillips <matthew@matthewphillips.info>
Description: A robot helper for agent skills
 A fast, cross-platform package manager for AI agent skills.
 Think npm, but for skills.
```

### Build Script

```bash
#!/bin/bash
set -e

VERSION=$1
PKG_DIR="rosie_${VERSION}_amd64"

mkdir -p "$PKG_DIR/usr/local/bin"
mkdir -p "$PKG_DIR/DEBIAN"

cp rosie "$PKG_DIR/usr/local/bin/"
sed "s/\${VERSION}/$VERSION/g" debian/control > "$PKG_DIR/DEBIAN/control"

dpkg-deb --build "$PKG_DIR"
```

### User Installation (GitHub Releases)

```bash
# Download and install directly
curl -LO https://github.com/matthewp/rosie/releases/download/v0.1.0/rosie_0.1.0_amd64.deb
sudo dpkg -i rosie_0.1.0_amd64.deb
sudo apt-get install -f  # Install dependencies
```

---

## Implementation Plan

### Phase 0: Basic CI ✅
- [x] Create `.github/workflows/ci.yaml`
- [x] Build on Linux (ubuntu-latest)
- [x] Build on macOS (macos-latest)

### Phase 1: Foundation
- [x] Update Makefile to support `DESTDIR` and `PREFIX`
- [ ] Create `debian/control` template
- [ ] Create `freebsd-package/` directory with templates
- [x] Create `.github/workflows/release.yaml`

### Phase 2: GitHub Actions - Linux
- [ ] Build Linux binary
- [ ] Build .deb package
- [ ] Upload to GitHub Releases

### Phase 3: GitHub Actions - macOS (Homebrew) ✅
- [x] Create `matthewp/homebrew-rosie` repository
- [x] Auto-update Homebrew formula on tag (builds from source on user's Mac)

### Phase 4: GitHub Actions - FreeBSD
- [ ] Set up FreeBSD VM build
- [ ] Build FreeBSD binary and package
- [ ] Publish to GitHub Pages
- [ ] Upload to GitHub Releases

### Phase 5: AUR ✅
- [x] Register `rosie` on AUR (initial push via `aur/publish-initial.sh`)
- [x] Set up SSH key for AUR (CI key separate from personal)
- [x] Auto-generate and push PKGBUILD

### Phase 6: Documentation
- [x] README install instructions for Homebrew + AUR
- [ ] README install instructions for FreeBSD + Debian once shipped
- [ ] Add man page? (`rosie.1.scd` with scdoc)

---

## Files to Create

```
rosie/
├── .github/
│   └── workflows/
│       └── release.yaml      # Main release workflow
├── debian/
│   └── control              # Debian package template
├── freebsd-package/
│   ├── build-package.sh     # FreeBSD build script
│   ├── pkg-manifest.ucl     # Package manifest template
│   └── pkg-plist            # File list
└── Makefile                 # Updated with DESTDIR/PREFIX
```

---

## Secrets Required

| Secret | Purpose |
|--------|---------|
| `HOMEBREW_TAP_TOKEN` | Push to homebrew-rosie repo |
| `AUR_SSH_KEY` | Push to AUR |

---

## Open Questions

1. ~~**macOS architectures**: arm64 only, or also x86_64?~~ → Moot. Homebrew formula builds from source, works on both.
2. ~~**Debian hosting**: GitHub Releases only, or also a proper apt repo on GitHub Pages?~~ → Proper apt repo on GitHub Pages (GitHub Packages doesn't support apt/deb)
3. **Man page**: Should we add one? Would need `scdoc` as build dependency.
4. **Linux architectures**: amd64 only, or also arm64?
5. ~~**Version tagging**: Do we use `v0.1.0` format?~~ → Yes, `vMAJOR.MINOR.PATCH`.
