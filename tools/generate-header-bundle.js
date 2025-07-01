#!/usr/bin/env node

const fs = require('fs/promises');
const path = require('path');

const IMPLEMENTATION_MACRO = 'YAX86_IMPLEMENTATION';

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
  public,
  private,
  rootDirPath,
  moduleDir,
}) {
  const includeGuard = `YAX86_${moduleName.toUpperCase()}_BUNDLE_H`;
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
    publicContent,
    '',
    `#ifdef ${IMPLEMENTATION_MACRO}`,
    '',
    implementationContent,
    '',
    `#endif // ${IMPLEMENTATION_MACRO}`,
    '',
    `#endif // ${includeGuard}`,
    '',
    '',
  ].join('\n');
  // console.log(`Writing header bundle to ${outputFilePath}`);
  await fs.writeFile(outputFilePath, headerBundleContent, 'utf8');
}

if (require.main === module) {
  const args = process.argv.slice(2);
  if (args.length < 1) {
    console.error('Usage: build-header-bundle.js bundle.json');
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
