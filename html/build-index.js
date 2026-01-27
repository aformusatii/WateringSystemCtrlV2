const fs = require("fs");
const path = require("path");
const zlib = require("zlib");
const { minify } = require("html-minifier-terser");

const INPUT_HTML = path.join(__dirname, "index.html");
const OUT_HTML = path.join(__dirname, "index.minified.html");
const TEMPLATE_FILE = path.join(__dirname, "webpage.h.template");
const HEADER_FILE = path.join(__dirname, "..", "src", "webpage.h");

function toCArray(buffer) {
  return buffer.reduce((acc, byte, idx) => {
    const value = `0x${byte.toString(16).padStart(2, "0")}`;
    const separator = idx === buffer.length - 1 ? "" : ",";
    const prefix = idx % 20 === 0 ? "\n  " : " ";
    return acc + prefix + value + separator;
  }, "");
}

function renderTemplate({ gzippedContent, gzippedLength, generatedOn }) {
  const template = fs.readFileSync(TEMPLATE_FILE, "utf8");

  return template
    .replace("{{gzippedContent}}", gzippedContent)
    .replace("{{gzippedLength}}", gzippedLength)
    .replace("{{generatedOn}}", generatedOn);
}

async function build() {
  const input = fs.readFileSync(INPUT_HTML, "utf8");

  const minifiedHtml = await minify(input, {
    collapseWhitespace: true,
    collapseInlineTagWhitespace: true,
    conservativeCollapse: false,
    removeComments: true,
    minifyCSS: true,
    minifyJS: true,
    removeAttributeQuotes: true,
    removeOptionalTags: false,
    removeRedundantAttributes: true,
    removeEmptyAttributes: true,
    removeTagWhitespace: true,
    removeScriptTypeAttributes: true,
    useShortDoctype: false,
    sortAttributes: true,
    sortClassName: true,
  });

  fs.writeFileSync(OUT_HTML, minifiedHtml, "utf8");
  console.log(`Wrote ${path.relative(process.cwd(), OUT_HTML)} (${minifiedHtml.length} chars)`);

  const gzipped = zlib.gzipSync(Buffer.from(minifiedHtml, "utf8"), { level: 9 });

  const header = renderTemplate({
    gzippedContent: toCArray(gzipped),
    gzippedLength: gzipped.length,
    generatedOn: new Date().toISOString(),
  });

  fs.writeFileSync(HEADER_FILE, header, "utf8");
  console.log(`Wrote ${path.relative(process.cwd(), HEADER_FILE)}`);
}

build().catch((err) => {
  console.error("Failed to build index:", err);
  process.exitCode = 1;
});
