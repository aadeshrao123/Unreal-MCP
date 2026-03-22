#!/usr/bin/env node

/**
 * Postinstall script for the unrealmcp npm package.
 *
 * Downloads the correct ue-cli binary for the current platform from
 * the GitHub Releases page and places it in the bin/ directory.
 */

const https = require("https");
const http = require("http");
const fs = require("fs");
const path = require("path");
const os = require("os");
const { execSync } = require("child_process");

const VERSION = require("./package.json").version;
const REPO = "aadeshrao123/Unreal-MCP";

const PLATFORM_MAP = {
  "win32-x64": "ue-cli-windows-amd64.exe",
  "linux-x64": "ue-cli-linux-amd64",
  "linux-arm64": "ue-cli-linux-arm64",
  "darwin-x64": "ue-cli-darwin-amd64",
  "darwin-arm64": "ue-cli-darwin-arm64",
};

function getPlatformKey() {
  const platform = os.platform();
  const arch = os.arch();
  return `${platform}-${arch}`;
}

function getBinaryName() {
  const key = getPlatformKey();
  const name = PLATFORM_MAP[key];
  if (!name) {
    console.error(`Unsupported platform: ${key}`);
    console.error(`Supported: ${Object.keys(PLATFORM_MAP).join(", ")}`);
    process.exit(1);
  }
  return name;
}

function getDownloadUrl(binaryName) {
  return `https://github.com/${REPO}/releases/download/v${VERSION}/${binaryName}`;
}

function followRedirects(url, callback) {
  const client = url.startsWith("https") ? https : http;
  client.get(url, { headers: { "User-Agent": "unrealmcp-npm" } }, (res) => {
    if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
      followRedirects(res.headers.location, callback);
    } else {
      callback(res);
    }
  });
}

function download(url, destPath) {
  return new Promise((resolve, reject) => {
    console.log(`Downloading ue-cli v${VERSION}...`);
    console.log(`  ${url}`);

    followRedirects(url, (res) => {
      if (res.statusCode !== 200) {
        reject(new Error(`Download failed: HTTP ${res.statusCode}`));
        return;
      }

      const totalBytes = parseInt(res.headers["content-length"], 10) || 0;
      let downloadedBytes = 0;

      const file = fs.createWriteStream(destPath);
      res.on("data", (chunk) => {
        downloadedBytes += chunk.length;
        if (totalBytes > 0) {
          const pct = Math.round((downloadedBytes / totalBytes) * 100);
          process.stdout.write(`\r  Progress: ${pct}%`);
        }
      });
      res.pipe(file);

      file.on("finish", () => {
        process.stdout.write("\n");
        file.close();
        resolve();
      });

      file.on("error", (err) => {
        fs.unlink(destPath, () => {});
        reject(err);
      });
    });
  });
}

async function main() {
  const binaryName = getBinaryName();
  const url = getDownloadUrl(binaryName);

  const binDir = path.join(__dirname, "bin");
  if (!fs.existsSync(binDir)) {
    fs.mkdirSync(binDir, { recursive: true });
  }

  // On Windows keep .exe, on Unix use plain name
  const isWindows = os.platform() === "win32";
  const destName = isWindows ? "ue-cli.exe" : "ue-cli";
  const destPath = path.join(binDir, destName);

  try {
    await download(url, destPath);

    // Make executable on Unix
    if (!isWindows) {
      fs.chmodSync(destPath, 0o755);
    }

    console.log(`  Installed: ${destPath}`);
    console.log(`  Run: ue-cli health_check`);
  } catch (err) {
    console.error(`\nFailed to install ue-cli: ${err.message}`);
    console.error(`You can manually download from:`);
    console.error(`  https://github.com/${REPO}/releases/tag/v${VERSION}`);
    process.exit(1);
  }
}

main();
