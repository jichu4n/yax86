#!/usr/bin/env node

const fs = require('fs/promises');
const path = require('path');

// Generates a module's header bundle, core/<module>.h, from the files its
// core/src/<module>/bundle.json names:
//
//   requires  other bundles this one includes rather than copies, such as
//             "util", which every module needs and which would otherwise be
//             copied into every module's bundle and again into yax86_core.h
//   public    files that make up the module's interface
//   private   files compiled only where YAX86_IMPLEMENTATION is defined
//
// A required bundle is included ahead of this one's extern "C" block, inside
// an #ifndef on its include guard, so that in yax86_core.h, where it comes
// first, the include is skipped without the file being opened.

const IMPLEMENTATION_MACRO = 'YAX86_IMPLEMENTATION';

function bundleIncludeGuard(moduleName) {
  return `YAX86_${moduleName.toUpperCase()}_BUNDLE_H`;
}

function requiredBundleIncludes(requires) {
  return requires
    .map((moduleName) => {
      const includeGuard = bundleIncludeGuard(moduleName);
      return [
        `#ifndef ${includeGuard}`,
        `#include "${moduleName}.h"`,
        `#endif  // ${includeGuard}`,
        '',
      ].join('\n');
    })
    .join('\n');
}

// A file named twice would be copied into the bundle twice. Its include guard
// makes the second copy compile to nothing, so it is only ever dead weight.
function checkNoFileNamedTwice(moduleName, fileNames) {
  const seen = new Set();
  for (const fileName of fileNames) {
    if (seen.has(fileName)) {
      throw new Error(
        `${moduleName}/bundle.json names ${fileName} more than once`
      );
    }
    seen.add(fileName);
  }
}

function wrapFileContent({relFilePath, fileContent}) {
  return [
    `// ${'='.repeat(78)}`,
    `// ${relFilePath} start`,
    `// ${'='.repeat(78)}`,
    '',
    `#line 1 "./${relFilePath}"`,
    fileContent,
    '',
    `// ${'='.repeat(78)}`,
    `// ${relFilePath} end`,
    `// ${'='.repeat(78)}`,
    '',
  ].join('\n');
}

async function readAndWrapFileContent({fileNames, rootDirPath, moduleDir}) {
  return (
    await Promise.all(
      fileNames.map(async (fileName) => {
        const relFilePath = path.join(moduleDir, fileName);
        const filePath = path.resolve(rootDirPath, relFilePath);
        let fileContent;
        try {
          fileContent = await fs.readFile(filePath, 'utf8');
        } catch (error) {
          console.error(`Error reading file "${filePath}":`, error);
          throw error;
        }
        return wrapFileContent({relFilePath, fileContent});
      })
    )
  ).join('\n');
}

async function generateHeaderBundle({
  moduleName,
  outputFilePath,
  requires = [],
  public,
  private,
  rootDirPath,
  moduleDir,
}) {
  checkNoFileNamedTwice(moduleName, [...public, ...private]);
  const includeGuard = bundleIncludeGuard(moduleName);
  const publicContent = await readAndWrapFileContent({
    fileNames: public,
    rootDirPath,
    moduleDir,
  });
  const implementationContent = await readAndWrapFileContent({
    fileNames: private,
    rootDirPath,
    moduleDir,
  });
  const headerBundleContent = [
    `// ${'='.repeat(78)}`,
    `// YAX86 ${moduleName.toUpperCase()} MODULE - GENERATED SINGLE HEADER BUNDLE`,
    `// ${'='.repeat(78)}`,
    '',
    `#ifndef ${includeGuard}`,
    `#define ${includeGuard}`,
    '',
    requiredBundleIncludes(requires),
    '#ifdef __cplusplus',
    'extern "C" {',
    '#endif  // __cplusplus',
    '',
    publicContent,
    '',
    `#ifdef ${IMPLEMENTATION_MACRO}`,
    '',
    implementationContent,
    '',
    `#endif  // ${IMPLEMENTATION_MACRO}`,
    '',
    '#ifdef __cplusplus',
    '}  // extern "C"',
    '#endif  // __cplusplus',
    '',
    `#endif  // ${includeGuard}`,
    '',
    '',
  ].join('\n');
  // console.log(`Writing header bundle to ${outputFilePath}`);
  await fs.writeFile(outputFilePath, headerBundleContent, 'utf8');
}

if (require.main === module) {
  const args = process.argv.slice(2);
  if (args.length < 1) {
    console.error('Usage: generate-header-bundle.js bundle.json');
    process.exit(1);
  }
  const moduleName = args[0];
  const rootDirPath = path.resolve(__dirname, '..');
  const moduleDir = path.join('src', moduleName);
  const bundleJsonPath = path.resolve(rootDirPath, moduleDir, 'bundle.json');
  // console.log(`Reading bundle configuration from ${bundleJsonPath}`);
  const outputFilePath = path.resolve(__dirname, '..', `${moduleName}.h`);

  (async () => {
    const bundleJson = JSON.parse(await fs.readFile(bundleJsonPath, 'utf8'));
    await generateHeaderBundle({
      moduleName,
      outputFilePath,
      rootDirPath,
      moduleDir,
      ...bundleJson,
    });
  })();
}
