const fs = require("fs");
const zlib = require("zlib");
const { minify } = require("html-minifier-terser");

const args = process.argv.slice(2);
const writeGzipFile = args.includes("--gzip");
const gzipLevelArg = args.find((arg) => arg.startsWith("--gzip-level="));
const gzipLevel = gzipLevelArg ? Number(gzipLevelArg.split("=")[1]) : 9;

// Read your input file
const input = fs.readFileSync("index.html", "utf8");

// Minify options: aggressive but safe
minify(input, {
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
}).then((minified) => {
  // Remove all newlines, tabs, and carriage returns
  //minified = minified.replace(/[\n\r\t]/g, '');
  // Remove multiple spaces
  //minified = minified.replace(/\s{2,}/g, ' ');
  // Remove whitespace between tags
  //minified = minified.replace(/>\s+</g, '><');

  const vueJs = fs.readFileSync("vue.js", "utf8");
  minified = minified
    .toString()
    .replace(
      "<script src=vue.js></script>",
      () => "<script>" + vueJs + "</script>",
    );

  console.log(minified);
  fs.writeFileSync("out.html", minified, "utf8");

  const level = Number.isFinite(gzipLevel)
    ? Math.min(9, Math.max(0, gzipLevel))
    : 9;
  const gzipped = zlib.gzipSync(Buffer.from(minified, "utf8"), { level });

  if (writeGzipFile) {
    fs.writeFileSync("out.html.gz", gzipped);
    console.log(
      `Gzipped -> out.html.gz (${gzipped.length} bytes, level ${level})`,
    );
  }

  const gzipArray = gzipped.reduce((acc, byte, idx) => {
    const value = `0x${byte.toString(16).padStart(2, "0")}`;
    const separator = idx === gzipped.length - 1 ? "" : ",";
    const prefix = idx % 20 === 0 ? "\n  " : " ";
    return acc + prefix + value + separator;
  }, "");

  // Escape backslashes and quotes for C string
  const cString = minified.replace(/\\/g, "\\\\").replace(/"/g, '\\"');

  // add date and time when the H file was generated as a comment
  const webpageTemplate = fs.readFileSync("webpage.h.template", "utf8");
  let webpage = webpageTemplate
                  .replace("{{gzippedContent}}", gzipArray)
                  .replace("{{gzippedLength}}", gzipped.length);

  console.log(webpage);

  fs.writeFileSync("../src/webpage.h", webpage, "utf8");
});
