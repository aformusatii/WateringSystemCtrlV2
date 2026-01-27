const fs = require("fs");
const path = require("path");
const https = require("https");
const zlib = require("zlib");
const terser = require("terser");

const VUE_URL =
  "https://unpkg.com/vue@3.5.27/dist/vue.global.prod.js";
const VUE_FILE = path.join(__dirname, "vue.js");
const TEMPLATE_FILE = path.join(__dirname, "vuejs.h.template");
const HEADER_FILE = path.join(__dirname, "..", "src", "vuejs.h");

async function download(url) {
  const res = await fetch(url, { redirect: "follow" });
  if (!res.ok) throw new Error(`HTTP ${res.status}`);

  const len = Number(res.headers.get("content-length") || 0);
  if (len && len > 10 * 1024 * 1024) throw new Error("Too large"); // 10MB limit

  const buf = Buffer.from(await res.arrayBuffer());
  return buf;
}

function toCArray(buffer) {
  return buffer.reduce((acc, byte, idx) => {
    const value = `0x${byte.toString(16).padStart(2, "0")}`;
    const separator = idx === buffer.length - 1 ? "" : ",";
    const prefix = idx % 20 === 0 ? "\n  " : " ";
    return acc + prefix + value + separator;
  }, "");
}

async function minifyVue(buffer) {
  const code = buffer.toString("utf8");
  const result = await terser.minify(code, {
    compress: {
      passes: 3,
      ecma: 2020,
      toplevel: true,
    },
    mangle: true,
    toplevel: true,
    format: {
      comments: false,
    },
  });

  if (result.error) {
    throw new Error(result.error);
  }
  if (!result.code) {
    throw new Error("Terser returned no code");
  }

  return Buffer.from(result.code, "utf8");
}

function renderTemplate({ gzippedContent, gzippedLength, generatedOn }) {
  const template = fs.readFileSync(TEMPLATE_FILE, "utf8");

  return template
    .replace("{{gzippedContent}}", gzippedContent)
    .replace("{{gzippedLength}}", gzippedLength)
    .replace("{{generatedOn}}", generatedOn);
}

async function build() {
  try {
    console.log(`Downloading Vue -> ${VUE_URL}`);
    const vueContent = await download(VUE_URL);
    let finalContent = vueContent;

    /* try {
      const minified = await minifyVue(vueContent);
      console.log(
        `Minified Vue: ${vueContent.length} -> ${minified.length} bytes`,
      );
      finalContent = minified;
    } catch (minErr) {
      console.warn(
        `Minification failed, using downloaded content: ${minErr.message}`,
      );
    }*/

    fs.writeFileSync(VUE_FILE, finalContent);
    console.log(
      `Saved ${path.relative(process.cwd(), VUE_FILE)} (${finalContent.length} bytes)`,
    );

    const gzipped = zlib.gzipSync(finalContent, { level: 9 });
    console.log(`Gzip size: ${gzipped.length} bytes`);
    
    const generatedOn = new Date().toISOString();

    const header = renderTemplate({
      gzippedContent: toCArray(gzipped),
      gzippedLength: gzipped.length,
      generatedOn,
    });

    fs.writeFileSync(HEADER_FILE, header, "utf8");
    console.log(`Wrote ${path.relative(process.cwd(), HEADER_FILE)}`);
  } catch (err) {
    console.error("Failed to build vuejs.h:", err);
    process.exitCode = 1;
  }
}

build().catch((err) => {
  console.error("Failed to build index:", err);
  process.exitCode = 1;
});
