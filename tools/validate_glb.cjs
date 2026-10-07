#!/usr/bin/env node
// Optional validation with the Khronos validator, installed into build/validation.
const fs = require('node:fs');
const path = require('node:path');
const validator = require(path.resolve(__dirname, '../build/validation/node_modules/gltf-validator'));
(async () => {
    const files = process.argv.slice(2);
    if (!files.length) throw new Error('Usage: node tools/validate_glb.cjs model.glb [...]');
    let failed = false;
    for (const file of files) {
        const report = await validator.validateBytes(new Uint8Array(fs.readFileSync(file)), {uri: path.basename(file)});
        console.log(JSON.stringify({file, errors: report.issues.numErrors, warnings: report.issues.numWarnings, messages: report.issues.messages}));
        failed ||= report.issues.numErrors > 0;
    }
    process.exitCode = failed ? 1 : 0;
})().catch(error => { console.error(error.message); process.exitCode = 1; });
