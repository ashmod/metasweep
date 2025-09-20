#!/usr/bin/env node
/*
  metasweep npm installer
  - Detects platform/arch
  - Downloads the appropriate prebuilt artifact from GitHub Releases
  - Stores it under ./bin/.vendor and makes the bin shim run it

  Notes:
   - Currently supports Linux x64 via the AppImage produced by CI
   - You can override the repository or URL with env vars:
       METASWEEP_GH_REPO=owner/repo  (default from package.json repository.url)
       METASWEEP_DOWNLOAD_URL=...    (direct URL to an asset)
*/

const fs = require('fs');
const path = require('path');
const https = require('https');

const pkg = require('./package.json');

function log(msg) { console.log(`[metasweep] ${msg}`); }
function fail(msg) { console.error(`[metasweep] ${msg}`); process.exit(1); }

const platform = process.platform; // 'linux', 'darwin', 'win32'
const arch = process.arch;         // 'x64', 'arm64', ...

// Only Linux x64 for now
if (!(platform === 'linux' && arch === 'x64')) {
  log(`No prebuilt binary for ${platform}/${arch} yet. Skipping download.`);
  log(`You can download from Releases manually or use the AppImage on Linux.`);
  process.exit(0);
}

const version = pkg.version;
const tag = `v${version}`;

function parseRepo() {
  if (process.env.METASWEEP_GH_REPO) return process.env.METASWEEP_GH_REPO; // owner/repo
  const repo = pkg.repository && pkg.repository.url;
  if (!repo) return null;
  const m = repo.match(/github\.com[:/](.+?)\/(.+?)(?:\.git)?$/);
  if (!m) return null;
  return `${m[1]}/${m[2]}`;
}

const repo = parseRepo();
let url = process.env.METASWEEP_DOWNLOAD_URL || null;
if (!url) {
  if (!repo) fail('Cannot determine GitHub repo (set METASWEEP_GH_REPO=owner/repo).');
  // Default AppImage name from linuxdeploy is typically '<name>-x86_64.AppImage'
  const asset = `metasweep-x86_64.AppImage`;
  url = `https://github.com/${repo}/releases/download/${tag}/${asset}`;
}

const vendorDir = path.join(__dirname, 'bin', '.vendor');
const outPath = path.join(vendorDir, 'metasweep');

fs.mkdirSync(vendorDir, { recursive: true });

function download(u, dest) {
  return new Promise((resolve, reject) => {
    const file = fs.createWriteStream(dest, { mode: 0o755 });
    https.get(u, (res) => {
      if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
        // redirect
        res.resume();
        return resolve(download(res.headers.location, dest));
      }
      if (res.statusCode !== 200) {
        return reject(new Error(`HTTP ${res.statusCode}`));
      }
      res.pipe(file);
      file.on('finish', () => file.close(resolve));
    }).on('error', (err) => {
      fs.unlink(dest, () => reject(err));
    });
  });
}

(async () => {
  try {
    log(`Downloading ${url}`);
    await download(url, outPath);
    fs.chmodSync(outPath, 0o755);
    log(`Installed binary to ${outPath}`);
  } catch (e) {
    fail(`Failed to download binary: ${e.message}`);
  }
})();

