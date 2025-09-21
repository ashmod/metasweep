# metasweep: metadata inspector & cleaner


<picture>
    <source srcset="assets/logo_dark.svg"  media="(prefers-color-scheme: dark)">
    <!-- markdown-link-check-disable-next-line -->
    <img src="assets/logo_light.svg">
</picture>


<div align="center">
    Local. Transparent. Fast.
</div>

<br />


<div align="center">

[![Release CI](https://github.com/ashmod/metasweep/actions/workflows/release.yml/badge.svg)](https://github.com/ashmod/metasweep/actions/workflows/release.yml)
[![Latest Release](https://img.shields.io/github/v/release/ashmod/metasweep?display_name=tag&sort=semver)](https://github.com/ashmod/metasweep/releases)
[![npm](https://img.shields.io/npm/v/metasweep?logo=npm)](https://www.npmjs.com/package/metasweep)
[![License](https://img.shields.io/github/license/ashmod/metasweep)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-20-blue)](https://en.wikipedia.org/wiki/C%2B%2B20)
[![Ubuntu](https://img.shields.io/badge/Ubuntu-E95420?logo=ubuntu&logoColor=white)](https://ubuntu.com/)
[![macOS](https://img.shields.io/badge/macOS-000000?logo=apple&logoColor=white)](https://www.apple.com/macos/)
[![Windows](https://img.shields.io/badge/Windows-0078D4?logo=windows&logoColor=white)](https://www.microsoft.com/windows/)

</div>



**metasweep** inspects and strips hidden metadata from common file types. It favors transparency (see what’s inside), choice (policies: aggressive/safe/custom), and trust (no network, open source).

<picture>
    <source srcset="assets/preview_dark.png"  media="(prefers-color-scheme: dark)">
    <!-- markdown-link-check-disable-next-line -->
    <img src="assets/preview_light.png" alt="metasweep preview">
</picture>

<br /><br />

**Quick install (Linux only for now):**

```bash
npm i -g metasweep
metasweep --help
```

Or run once without installing:

```bash
npx metasweep inspect photo.jpg
```

---

## Features

* Multi-format: Images (EXIF/IPTC/XMP via Exiv2), PDFs (/Info), Audio (ID3/FLAC/Vorbis via TagLib), ZIP (comments/extra fields)
* Transparency: human-readable inspect output with risk highlights
* Flexible cleaning: `--inspect`, `--strip`, `--safe`, `--custom`
* Batch-friendly: works on files, globs, or directories
* Local & private: no telemetry, no network calls
* Reports: optional JSON output (`--report file.json`)
* Dry-run preview: show planned keep/drop with `--dry-run`

---

## Install

### Option 1: npm (Linux only)

* Downloads the prebuilt **AppImage** binary from GitHub Releases.
* Currently supports **Linux x64**.

```bash
# Global install
npm i -g metasweep
metasweep --help

# One-off usage
npx metasweep inspect file.jpg
```

Environment overrides:

* `METASWEEP_GH_REPO=owner/repo` → use a different GitHub repo.
* `METASWEEP_DOWNLOAD_URL=URL` → use a direct asset URL.

---

### Option 2: Prebuilt packages

Check the [Releases page](https://github.com/ashmod/metasweep/releases) for binaries:

* **Linux/macOS/Windows:** tar.gz/zip archives with `metasweep` binary + policies.
* **Linux AppImage:** portable, runs without extra deps.

Example (Linux tar.gz):

```bash
tar -xzf metasweep-*.tar.gz
sudo install -m 0755 bin/metasweep /usr/local/bin/metasweep
sudo mkdir -p /usr/local/share/metasweep/policies
sudo cp -r share/metasweep/policies/* /usr/local/share/metasweep/policies/
```

Example (AppImage):

```bash
chmod +x metasweep-*.AppImage
./metasweep-*.AppImage --help
```

---

### Option 3: Build from source

```bash
# Ubuntu
sudo apt update
sudo apt install -y cmake build-essential libexiv2-dev libtag1-dev zlib1g-dev

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

---

## CLI

```
metasweep <command> [targets...] [options]

Commands:
  inspect     Show metadata summary (no changes)
  strip       Remove metadata according to policy
  explain     Describe risks & recommendations
```

(…see [full CLI docs](#cli-documentation) below)

---

## CLI Documentation

### Commands

#### `inspect` - Inspect metadata

Show metadata summary without making changes.

```
metasweep inspect [OPTIONS] files...
```

**Positionals:**
- `files`: Files to inspect (required, multiple allowed)

**Options:**
- `-v, --verbose`: Verbose field listing (can be repeated for more verbosity)
- `-r, --recursive`: Recurse into directories
- `--report TEXT`: Write JSON report to file
- `--format TEXT`: Output format: `auto`, `json`, or `pretty` (default: auto)

#### `strip` - Strip metadata

Remove metadata according to the specified policy.

```
metasweep strip [OPTIONS] files...
```

**Positionals:**
- `files`: Files to strip (required, multiple allowed)

**Options:**
- `--dry-run`: Show plan without writing any files
- `--in-place`: Overwrite original files (no backup)
- `-o, --out-dir TEXT`: Output directory for cleaned files
- `-r, --recursive`: Recurse into directories
- `--yes`: Skip confirmation prompts
- `--report TEXT`: Write JSON report to file
- `--format TEXT`: Output format: `auto`, `json`, or `pretty` (default: auto)
- `--safe`: Use built-in safe policy (keeps Orientation/ICC/DPI, drops identifiers)
- `--custom TEXT`: Policy file (YAML/JSON)
- `--keep TEXT`: Keep specific field(s) (repeatable)
- `--drop TEXT`: Drop specific field(s) (repeatable)

#### `explain` - Explain risks for a file

Describe metadata risks and recommendations for a specific file.

```
metasweep explain [OPTIONS] file
```

**Positionals:**
- `file`: File to explain (required)

**Options:**
- `-v, --verbose`: Verbose field listing (can be repeated for more verbosity)

---

## Examples

```bash
# Inspect an image
metasweep inspect photo.jpg

# Strip aggressively (default), write photo.cleaned.jpg
metasweep strip photo.jpg

# Safer policy (keeps Orientation/ICC/DPI)
metasweep strip --safe photo.jpg

# JSON report for a batch
metasweep inspect ./to-share -r --format json > report.json
```

---

## Policies

* **Aggressive (default):** keeps only essential fields like Orientation and ColorProfile.
* **Safe (`--safe`):** keeps Orientation/ICC/DPI, drops identifiers like GPS, serials, authorship, software tags.
* Reference policy files are under [`policies/`](./policies/).

---

## Privacy & Safety

* No network calls. Ever.
* Only metadata areas are read/rewritten. File content remains untouched.
* Atomic writes used where possible.

---

## License

See [LICENSE](./LICENSE).
