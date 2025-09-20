#!/usr/bin/env node
const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');

const vendor = path.join(__dirname, '.vendor', 'metasweep');

if (!fs.existsSync(vendor)) {
  console.error('[metasweep] Binary not found. Try reinstalling or set METASWEEP_DOWNLOAD_URL.');
  process.exit(1);
}

const args = process.argv.slice(2);
const child = spawn(vendor, args, { stdio: 'inherit' });
child.on('exit', code => process.exit(code));

