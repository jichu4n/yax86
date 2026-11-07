#!/usr/bin/env node

// Checks that every file-scope name core/yax86_core.h brings into a host's
// translation unit is in the project's namespace, as AGENTS.md's naming rule
// says it must be:
//
// - a function, type or const object is Y, its owner, then CamelCase;
// - an enum value is Y_, its owner, _, then ALL_CAPS;
// - a macro is Y_ then ALL_CAPS.
//
// Names come from the compiler rather than from a grep: the top-level
// declarations in a clang AST dump of the bundle, and the macros it defines.
// A name's owner is the module directory of the file that defines it, read
// through the bundle's #line directives, so a name in the namespace of the
// wrong module fails as well as one in no namespace at all.
//
// Names listed in the second column of core/rename_map.txt are exempt while
// that file exists.
//
// Usage: tools/check-namespace.js

const childProcess = require('child_process');
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const BUNDLE = 'core/yax86_core.h';
const MAP_FILE = path.join(ROOT, 'core', 'rename_map.txt');
const CLANG = process.env.CLANG || 'clang';

// Every owner, with the words a CamelCase name in its namespace may begin with
// after the Y, and its spelling in an ALL_CAPS one.
const OWNERS = {
  BIOS: {camel: ['BIOS'], caps: 'BIOS'},
  CPU: {camel: ['CPU'], caps: 'CPU'},
  DMA: {camel: ['DMA'], caps: 'DMA'},
  FDC: {camel: ['FDC'], caps: 'FDC'},
  HDC: {camel: ['HDC'], caps: 'HDC'},
  Keyboard: {camel: ['Keyboard'], caps: 'KEYBOARD'},
  PIC: {camel: ['PIC'], caps: 'PIC'},
  PIT: {camel: ['PIT'], caps: 'PIT'},
  Platform: {camel: ['Platform'], caps: 'PLATFORM'},
  PPI: {camel: ['PPI'], caps: 'PPI'},
  Video: {camel: ['Video'], caps: 'VIDEO'},
  // Logger is the component's word for its state and config.
  Log: {camel: ['Logger', 'Log'], caps: 'LOG'},
  // C's v prefix for the va_list form of a function stays at the front.
  SNPrintF: {camel: ['SNPrintF', 'VSNPrintF'], caps: 'SNPRINTF'},
  StaticVector: {camel: ['StaticVector'], caps: 'STATIC_VECTOR'},
};

// The owner of each module, keyed by its directory under core/src.
const MODULE_OWNERS = {
  bios: 'BIOS',
  cpu: 'CPU',
  dma: 'DMA',
  fdc: 'FDC',
  hdc: 'HDC',
  keyboard: 'Keyboard',
  pic: 'PIC',
  pit: 'PIT',
  platform: 'Platform',
  ppi: 'PPI',
  video: 'Video',
};

// util is a directory of unrelated pieces rather than a module, so each of its
// headers is its own owner. common.h defines only macros, which need none.
const UTIL_OWNERS = {
  'log.h': 'Log',
  'snprintf.h': 'SNPrintF',
  'static_vector.h': 'StaticVector',
};

function clang(args, input) {
  const result = childProcess.spawnSync(CLANG, args, {
    cwd: ROOT,
    input,
    encoding: 'utf8',
    maxBuffer: 1 << 30,
  });
  if (result.error) {
    throw new Error(
      `Cannot run ${CLANG}, which this check needs for its AST dump: ` +
        result.error.message,
    );
  }
  if (result.status !== 0) {
    throw new Error(`${CLANG} ${args.join(' ')} failed:\n${result.stderr}`);
  }
  return result.stdout;
}

// clang's JSON reports physical locations, so the file a declaration came
// from is recovered from the #line directive preceding its offset. A location
// only names its file when that differs from the last one printed, so the
// walk has to visit every location in the order clang wrote them.
class LocationTracker {
  constructor() {
    this.file = null;
    this.lineDirectives = {};
  }

  see(loc) {
    if (!loc || typeof loc !== 'object') {
      return null;
    }
    if (loc.spellingLoc || loc.expansionLoc) {
      this.see(loc.spellingLoc);
      return this.see(loc.expansionLoc);
    }
    if (loc.file) {
      this.file = loc.file;
    }
    return {file: this.file, offset: loc.offset};
  }

  walk(node) {
    if (Array.isArray(node)) {
      node.forEach((child) => this.walk(child));
      return;
    }
    if (!node || typeof node !== 'object') {
      return;
    }
    for (const [key, value] of Object.entries(node)) {
      if (key === 'loc') {
        this.see(value);
      } else if (key === 'range') {
        this.see(value.begin);
        this.see(value.end);
      } else if (typeof value === 'object') {
        this.walk(value);
      }
    }
  }

  // The file a location was written in, as its #line directive names it, or
  // null for a location outside the core: a system header, or the scratch
  // space clang spells a pasted token in.
  presumedFile(loc) {
    if (!loc || !loc.file || !loc.file.startsWith('core/')) {
      return null;
    }
    const {file, offset} = loc;
    if (!this.lineDirectives[file]) {
      const text = fs.readFileSync(path.join(ROOT, file), 'utf8');
      const directives = [];
      const re = /^#line \d+ "([^"]+)"/gm;
      let match;
      while ((match = re.exec(text))) {
        directives.push([match.index, match[1]]);
      }
      this.lineDirectives[file] = directives;
    }
    let presumed = null;
    for (const [directiveOffset, name] of this.lineDirectives[file]) {
      if (directiveOffset > offset) {
        break;
      }
      presumed = name;
    }
    return presumed;
  }
}

// The owner of a name defined in core/src/<dir>/<file>.
function ownerOf(presumedFile) {
  const match = /^\.\/src\/([^/]+)\/([^/]+)$/.exec(presumedFile || '');
  if (!match) {
    throw new Error(`Not a core source file: ${presumedFile}`);
  }
  const [, dir, file] = match;
  const owner = dir === 'util' ? UTIL_OWNERS[file] : MODULE_OWNERS[dir];
  if (!owner) {
    throw new Error(`No owner for ${dir}/${file}`);
  }
  return owner;
}

// Every top-level name the bundle declares, once each, as {name, kind, owner,
// pasted}. kind is type, function, object or enum. pasted is set on a
// function whose name a macro pasted together, which is what
// STATIC_VECTOR_TYPE does.
function collectDeclarations() {
  const ast = JSON.parse(
    clang([
      '-std=c99',
      '-fsyntax-only',
      '-DYAX86_IMPLEMENTATION',
      '-Xclang',
      '-ast-dump=json',
      '-x',
      'c',
      BUNDLE,
    ]),
  );
  const tracker = new LocationTracker();
  const declarations = new Map();

  function add(name, kind, owner, pasted) {
    const existing = declarations.get(name);
    if (existing && (existing.kind !== kind || existing.owner !== owner)) {
      throw new Error(
        `${name} is both a ${existing.kind} of ${existing.owner} and a ` +
          `${kind} of ${owner}`,
      );
    }
    declarations.set(name, {name, kind, owner, pasted});
  }

  for (const decl of ast.inner) {
    let loc = null;
    let pasted = false;
    if (decl.loc) {
      if (decl.loc.spellingLoc) {
        pasted = tracker.presumedFile(tracker.see(decl.loc.spellingLoc)) === null;
        loc = tracker.see(decl.loc.expansionLoc);
      } else {
        loc = tracker.see(decl.loc);
      }
    }
    if (decl.range) {
      tracker.see(decl.range.begin);
      tracker.see(decl.range.end);
    }
    tracker.walk(decl.inner || []);

    const definedIn = tracker.presumedFile(loc);
    if (definedIn === null || decl.isImplicit) {
      continue;
    }
    const owner = ownerOf(definedIn);
    switch (decl.kind) {
      case 'TypedefDecl':
      case 'RecordDecl':
      case 'EnumDecl':
        if (decl.name) {
          add(decl.name, 'type', owner, false);
        }
        for (const constant of decl.inner || []) {
          if (constant.kind === 'EnumConstantDecl') {
            add(constant.name, 'enum', owner, false);
          }
        }
        break;
      case 'FunctionDecl':
        add(decl.name, 'function', owner, pasted);
        break;
      case 'VarDecl':
        add(decl.name, 'object', owner, false);
        break;
      default:
        throw new Error(`Unexpected top-level ${decl.kind} ${decl.name}`);
    }
  }
  return [...declarations.values()];
}

// The macros the bundle defines, less those its system headers do.
function collectMacros() {
  const defined = (text) =>
    new Set(
      text
        .split('\n')
        .map((line) => /^#define (\w+)/.exec(line))
        .filter(Boolean)
        .map((match) => match[1]),
    );
  const args = ['-std=c99', '-E', '-dM', '-DYAX86_IMPLEMENTATION', '-x', 'c'];
  const bundle = defined(clang([...args, BUNDLE]));
  const system = defined(
    clang(
      [...args, '-'],
      ['stdint.h', 'stdbool.h', 'stddef.h', 'stdarg.h']
        .map((header) => `#include <${header}>\n`)
        .join(''),
    ),
  );
  // Defined by a host rather than by the header, and so by both dumps.
  return [...bundle]
    .filter((name) => !system.has(name))
    .concat('YAX86_IMPLEMENTATION');
}

// Why a name breaks the rule for its kind and owner, or null if it does not.
// kind is type, function, object, enum or macro; owner is ignored for a macro.
function ruleBroken(name, kind, owner) {
  if (kind === 'macro') {
    return /^Y_[A-Z0-9_]+$/.test(name) ? null : 'a macro is Y_ then ALL_CAPS';
  }
  const {camel, caps} = OWNERS[owner];
  if (kind === 'enum') {
    return new RegExp(`^Y_${caps}_[A-Z0-9_]+$`).test(name)
      ? null
      : `an enum value of ${owner} is Y_${caps}_ then ALL_CAPS`;
  }
  return new RegExp(`^Y(${camel.join('|')})([A-Z0-9]|$)`).test(name)
    ? null
    : `a ${kind} of ${owner} is Y${owner} then CamelCase`;
}

// The old names core/rename_map.txt still lists.
function exemptions() {
  if (!fs.existsSync(MAP_FILE)) {
    return new Set();
  }
  return new Set(
    fs
      .readFileSync(MAP_FILE, 'utf8')
      .split('\n')
      .filter((line) => line.trim())
      .map((line) => line.trim().split(/\s+/)[1]),
  );
}

function main() {
  const exempt = exemptions();
  const failures = [];
  for (const {name, kind, owner} of collectDeclarations()) {
    const broken = ruleBroken(name, kind, owner);
    if (broken && !exempt.has(name)) {
      failures.push(`${kind} ${name}: ${broken}`);
    }
  }
  for (const name of collectMacros()) {
    const broken = ruleBroken(name, 'macro', null);
    if (broken && !exempt.has(name)) {
      failures.push(`macro ${name}: ${broken}`);
    }
  }
  if (failures.length > 0) {
    console.error(failures.join('\n'));
    console.error(
      `\n${failures.length} name(s) in ${BUNDLE} are outside the namespace. ` +
        'See "Naming" in AGENTS.md.',
    );
    process.exit(1);
  }
}

module.exports = {
  ROOT,
  OWNERS,
  collectDeclarations,
  collectMacros,
  ruleBroken,
};

if (require.main === module) {
  try {
    main();
  } catch (e) {
    console.error(e.message);
    process.exit(1);
  }
}
