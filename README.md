# metasweep — privacy-friendly metadata inspector & cleaner

Local-only. Transparent. Fast.

metasweep inspects and strips hidden metadata from common file types. It favors transparency (see what’s inside), choice (policies: aggressive/safe/custom), and trust (no network, open source).

## Features
- Multi-format: Images (EXIF/IPTC/XMP via Exiv2), PDFs (/Info), Audio (ID3/FLAC/Vorbis via TagLib), ZIP (comments/extra fields)
- Transparency: human-readable inspect output with risk highlights
- Flexible cleaning: `--inspect`, `--strip`, `--safe`, `--custom`
- Batch-friendly: works on files, globs, or directories
- Local & private: no telemetry, no network calls
- Reports: optional JSON output (`--report file.json`)
- Dry-run preview: show planned keep/drop with `--dry-run`

## Install

Prebuilt packages are published for tagged releases:
- Linux/macOS/Windows: see the Releases page artifacts (TGZ/ZIP) containing `metasweep` and built-in policies.
- Linux AppImage: a single self-contained `*.AppImage` that runs without extra packages.

Via npm (Linux only, for now)
- One-line install using an npm shim that downloads the native binary:
  - Global: `npm i -g metasweep` → run `metasweep ...`
  - On-demand: `npx metasweep --help`
- Notes:
  - Currently supports Linux x64 (AppImage) and fetches the artifact from GitHub Releases.
  - If you use a fork or a different GitHub org, set env `METASWEEP_GH_REPO=owner/repo` before install, or `METASWEEP_DOWNLOAD_URL` to a direct asset URL.

To install from a package:
```bash
# Example Linux (tar.gz)
tar -xzf metasweep-*.tar.gz
sudo install -m 0755 bin/metasweep /usr/local/bin/metasweep
sudo mkdir -p /usr/local/share/metasweep/policies
sudo cp -r share/metasweep/policies/* /usr/local/share/metasweep/policies/

# AppImage
chmod +x metasweep-*.AppImage
./metasweep-*.AppImage --help

# npm (Linux x64)
npm i -g metasweep
metasweep --help
```

## Build from source
```bash
# Ubuntu
sudo apt update
sudo apt install -y cmake build-essential libexiv2-dev libtag1-dev zlib1g-dev

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

```

## CLI
```
metasweep <command> [targets...] [options]

Commands:
  inspect     Show metadata summary (no changes)
  strip       Remove metadata according to policy
  explain     Describe risks & recommendations

Common options:
  --no-color              Disable ANSI colors
  -v, --verbose           Extra detail (repeatable on inspect/explain)
  -r, --recursive         Recurse into directories

Strip options:
  --safe                  Use built-in safe policy
  --custom FILE           Use policy file (YAML/JSON) [placeholder]
  --dry-run               Show what would be dropped/kept
  --in-place              Overwrite originals
  --yes                   Skip confirmation for --in-place
  -o, --out-dir DIR       Write cleaned files to DIR
  --keep FIELD            Keep specific fields (repeatable)
  --drop FIELD            Drop specific fields (repeatable)

Inspect output options:
  --format json|pretty    Stream JSON to stdout or show table
  --report FILE           Write JSON report to file
```

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

# Dry-run to preview what would be removed/kept
metasweep strip --dry-run photo.jpg

# Recursive with glob
metasweep strip -r "images/*.jpg" --safe -o cleaned/

# In-place with confirmation
metasweep strip -r ./to-share --in-place --yes
```

## Policies
Built-ins are embedded (aggressive by default; `--safe` available). Policy files are also shipped under `share/metasweep/policies` for reference.

Aggressive keeps only functional fields like `EXIF.Orientation` and `Image.ColorProfile`.
Safe keeps Orientation/ICC/DPI and drops identifiers like GPS, serials, authorship, and software tags.

## Privacy & Safety
- No network calls. Ever.
- Only metadata areas are read/rewritten. Document payloads remain untouched.
- Atomic writes are used by backends where possible.

## License
See `LICENSE`.
