#!/usr/bin/env node

/**
 * Wrapper script that finds and executes the ue-cli binary.
 * This is the bin entry point for the npm package.
 */

const path = require("path");
const os = require("os");
const { execFileSync } = require("child_process");

const isWindows = os.platform() === "win32";
const binaryName = isWindows ? "ue-cli.exe" : "ue-cli";
const binaryPath = path.join(__dirname, "bin", binaryName);

try {
  execFileSync(binaryPath, process.argv.slice(2), {
    stdio: "inherit",
    env: process.env,
  });
} catch (err) {
  if (err.status !== undefined) {
    process.exit(err.status);
  }
  console.error(`Failed to run ue-cli: ${err.message}`);
  console.error(`Expected binary at: ${binaryPath}`);
  console.error(`Try reinstalling: npm install -g unrealmcp`);
  process.exit(1);
}
