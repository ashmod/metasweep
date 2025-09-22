#!/usr/bin/env node
const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');

const vendorDir = path.join(__dirname, '.vendor');
function platformKey() {
  const p = process.platform;
  const a = process.arch;
  if (p === 'linux' && a === 'x64') return 'linux-x64';
  if (p === 'darwin' && a === 'arm64') return 'darwin-arm64';
  if (p === 'darwin' && a === 'x64') return 'darwin-x64';
  if (p === 'win32' && a === 'x64') return 'win32-x64';
  return null;
}

// Candidate binary paths (cover extracted TGZ/ZIP and AppImage fallback)
const candidates = [];
const key = platformKey();
if (process.platform === 'win32') {
  if (key) {
    candidates.push(path.join(vendorDir, key, 'metasweep.exe'));
    candidates.push(path.join(vendorDir, key, 'bin', 'metasweep.exe'));
  }
  candidates.push(path.join(vendorDir, 'metasweep.exe'));
  candidates.push(path.join(vendorDir, 'bin', 'metasweep.exe'));
} else {
  if (key) {
    candidates.push(path.join(vendorDir, key, 'metasweep'));
    candidates.push(path.join(vendorDir, key, 'bin', 'metasweep'));
  }
  candidates.push(path.join(vendorDir, 'metasweep'));
  candidates.push(path.join(vendorDir, 'bin', 'metasweep'));
}

let bin = candidates.find(p => fs.existsSync(p));
if (!bin) {
  const want = process.platform === 'win32' ? 'metasweep.exe' : 'metasweep';
  const stack = [vendorDir];
  while (!bin && stack.length) {
    const dir = stack.pop();
    try {
      const items = fs.readdirSync(dir);
      for (const name of items) {
        const p = path.join(dir, name);
        const st = fs.statSync(p);
        if (st.isDirectory()) stack.push(p);
        else if (st.isFile() && name === want) { bin = p; break; }
      }
    } catch (_) { /* ignore */ }
  }
}
if (!bin) {
  console.error('[metasweep] Binary not found. Try reinstalling or set METASWEEP_DOWNLOAD_URL.');
  process.exit(1);
}

const args = process.argv.slice(2);
const env = { ...process.env };
// Avoid FUSE errors on AppImage by forcing extract-and-run on Linux
if (process.platform === 'linux') env.APPIMAGE_EXTRACT_AND_RUN = '1';

const child = spawn(bin, args, { stdio: 'inherit', env });
child.on('exit', code => process.exit(code));
