const fs = require('fs');
const { minify } = require('html-minifier-terser');

// Read your input file
const input = fs.readFileSync('index.html', 'utf8');

// Minify options: aggressive but safe
minify(input, {
  collapseWhitespace: true,
  collapseInlineTagWhitespace: true,
  conservativeCollapse: false,
  removeComments: true,
  minifyCSS: true,
  minifyJS: true,
  removeAttributeQuotes: true,
  removeOptionalTags: true,
  removeRedundantAttributes: true,
  removeEmptyAttributes: true,
  removeTagWhitespace: true,
  removeScriptTypeAttributes: true,
  useShortDoctype: true,
  sortAttributes: true,
  sortClassName: true,
}).then(minified => {

  // Remove all newlines, tabs, and carriage returns
  //minified = minified.replace(/[\n\r\t]/g, '');
  // Remove multiple spaces
  //minified = minified.replace(/\s{2,}/g, ' ');
  // Remove whitespace between tags
  //minified = minified.replace(/>\s+</g, '><');
  
  console.log(minified);
  fs.writeFileSync('out.html', minified, 'utf8');
  
  // Escape backslashes and quotes for C string
  const cString = minified
    .replace(/\\/g, '\\\\')
    .replace(/"/g, '\\"');

// add date and time when the H file was generated as a comment

  const result = `
#ifndef _WEBPAGE_H
#define _WEBPAGE_H

#include <pgmspace.h>

// Generated on ${new Date().toISOString()}

const char webpage[] PROGMEM = "${cString}";
  
#endif`.trim();

  console.log(result);

  fs.writeFileSync('../webpage.h', result, 'utf8');
});


