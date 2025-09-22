#!/usr/bin/env node
const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');

const vendorDir = path.join(__dirname, '.vendor');

// Candidate binary paths (cover extracted TGZ/ZIP and AppImage fallback)
const candidates = [];
if (process.platform === 'win32') {
  candidates.push(path.join(vendorDir, 'metasweep.exe'));
  candidates.push(path.join(vendorDir, 'bin', 'metasweep.exe'));
} else {
  candidates.push(path.join(vendorDir, 'metasweep'));
  candidates.push(path.join(vendorDir, 'bin', 'metasweep'));
}

const bin = candidates.find(p => fs.existsSync(p));
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
