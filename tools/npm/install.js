#!/usr/bin/env node
/*
  metasweep npm installer
  - Detects platform/arch
  - Downloads the appropriate prebuilt artifact from GitHub Releases
  - Prefers TGZ/ZIP binaries (no FUSE), falls back to AppImage on Linux
  - Stores under ./bin/.vendor and the bin shim runs it

  You can override with env vars:
   - METASWEEP_GH_REPO=owner/repo    (default parsed from package.json repository.url)
   - METASWEEP_DOWNLOAD_URL=...      (direct asset URL)
*/

const fs = require('fs');
const path = require('path');
const https = require('https');
const { spawn } = require('child_process');

const pkg = require('./package.json');

function log(msg) { console.log(`[metasweep] ${msg}`); }
function fail(msg) { console.error(`[metasweep] ${msg}`); process.exit(1); }

const platform = process.platform; // 'linux', 'darwin', 'win32'
const arch = process.arch;         // 'x64', 'arm64', ...

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

const vendorDir = path.join(__dirname, 'bin', '.vendor');
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

function mapArch() {
  if (arch === 'x64') return 'x86_64';
  if (arch === 'arm64') return 'arm64';
  if (arch === 'ia32') return 'x86';
  return arch;
}

function guessAsset() {
  if (process.env.METASWEEP_DOWNLOAD_URL) return { url: process.env.METASWEEP_DOWNLOAD_URL, kind: 'direct' };
  if (!repo) fail('Cannot determine GitHub repo (set METASWEEP_GH_REPO=owner/repo).');
  const a = mapArch();
  const base = `https://github.com/${repo}/releases/download/${tag}`;
  if (platform === 'linux') {
    return {
      primary: `${base}/metasweep-${version}-Linux-${a}.tar.gz`,
      fallback: `${base}/metasweep-x86_64.AppImage`,
      kind: 'linux'
    };
  } else if (platform === 'darwin') {
    return { primary: `${base}/metasweep-${version}-Darwin-${a}.tar.gz`, kind: 'tgz' };
  } else if (platform === 'win32') {
    const winArch = arch === 'x64' ? 'AMD64' : a;
    return { primary: `${base}/metasweep-${version}-Windows-${winArch}.zip`, kind: 'zip' };
  }
  return null;
}

function extractTgz(archive, dest) {
  return new Promise((res, rej) => {
    fs.mkdirSync(dest, { recursive: true });
    const p = spawn('tar', ['-xzf', archive, '-C', dest], { stdio: 'inherit' });
    p.on('exit', code => code === 0 ? res() : rej(new Error(`tar exit ${code}`)));
  });
}

function extractZip(archive, dest) {
  return new Promise((res, rej) => {
    fs.mkdirSync(dest, { recursive: true });
    if (platform === 'win32') {
      const p = spawn('powershell', ['-NoProfile', '-Command', `Expand-Archive -LiteralPath "${archive}" -DestinationPath "${dest}" -Force`], { stdio: 'inherit' });
      p.on('exit', code => code === 0 ? res() : rej(new Error(`Expand-Archive exit ${code}`)));
    } else {
      const p = spawn('unzip', ['-o', archive, '-d', dest], { stdio: 'inherit' });
      p.on('exit', code => code === 0 ? res() : rej(new Error(`unzip exit ${code}`)));
    }
  });
}

(async () => {
  try {
    const plan = guessAsset();
    if (!plan) {
      log(`No prebuilt binary plan for ${platform}/${arch}. Skipping.`);
      process.exit(0);
    }

    if (plan.primary) {
      const archivePath = path.join(vendorDir, path.basename(plan.primary));
      log(`Downloading ${plan.primary}`);
      await download(plan.primary, archivePath);
      if (plan.kind === 'zip') {
        await extractZip(archivePath, vendorDir);
      } else {
        await extractTgz(archivePath, vendorDir);
      }
      const binCandidates = [
        path.join(vendorDir, 'bin', 'metasweep'),
        path.join(vendorDir, 'metasweep'),
        path.join(vendorDir, 'bin', 'metasweep.exe'),
        path.join(vendorDir, 'metasweep.exe'),
      ];
      const found = binCandidates.find(p => fs.existsSync(p));
      if (found && !found.endsWith('.exe')) fs.chmodSync(found, 0o755);
      log(`Installed to ${vendorDir}`);
      return;
    }

    if (plan.fallback) {
      const outPath = path.join(vendorDir, 'metasweep');
      log(`Downloading ${plan.fallback}`);
      await download(plan.fallback, outPath);
      fs.chmodSync(outPath, 0o755);
      log(`Installed AppImage to ${outPath}`);
      return;
    }

    log('No install plan matched.');
  } catch (e) {
    fail(`Failed to download binary: ${e.message}`);
  }
})();
