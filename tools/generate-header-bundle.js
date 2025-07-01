#!/usr/bin/env node

const fs = require('fs/promises');
const path = require('path');

const IMPLEMENTATION_MACRO = 'YAX86_IMPLEMENTATION';

function wrapFileContent({fileName, fileContent}) {
  return [
    `// ${'-'.repeat(20)}`,
    `// ${fileName} start`,
    `// ${'-'.repeat(20)}`,
    '',
    fileContent,
    '',
    `// ${'-'.repeat(20)}`,
    `// ${fileName} end`,
    `// ${'-'.repeat(20)}`,
    '',
  ].join('\n');
}

async function readAndWrapFileContent({fileNames, dirPath}) {
  return (
    await Promise.all(
      fileNames.map(async (fileName) => {
        const filePath = path.resolve(dirPath, fileName);
        let fileContent;
        try {
          fileContent = await fs.readFile(filePath, 'utf8');
        } catch (error) {
          console.error(`Error reading file "${filePath}":`, error);
          throw error;
        }
        return wrapFileContent({fileName, fileContent});
      })
    )
  ).join('\n');
}

async function generateHeaderBundle({
  moduleName,
  outputFilePath,
  public,
  private,
  dirPath,
}) {
  const includeGuard = `YAX86_${moduleName.toUpperCase()}_BUNDLE_H`;
  const publicContent = await readAndWrapFileContent({
    fileNames: public,
    dirPath,
  });
  const implementationContent = await readAndWrapFileContent({
    fileNames: private,
    dirPath,
  });
  const headerBundleContent = [
    `// ${'='.repeat(78)}`,
    `// MODULE ${moduleName.toUpperCase()} - GENERATED SINGLE HEADER BUNDLE`,
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
  const dirPath = path.resolve(__dirname, '..', 'src', moduleName);
  const bundleJsonPath = path.resolve(dirPath, 'bundle.json');
  // console.log(`Reading bundle configuration from ${bundleJsonPath}`);
  const outputFilePath = path.resolve(__dirname, '..', `${moduleName}.h`);

  (async () => {
    const bundleJson = JSON.parse(await fs.readFile(bundleJsonPath, 'utf8'));
    await generateHeaderBundle({
      moduleName,
      outputFilePath,
      dirPath,
      ...bundleJson,
    });
  })();
}
