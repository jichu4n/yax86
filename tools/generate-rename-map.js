#!/usr/bin/env node

// Generates core/rename_map.txt, the list of every file-scope name in the core
// that does not yet follow the naming rule in AGENTS.md, and what each one is
// renamed to.
//
// Names come from the compiler rather than from a grep: the top-level
// declarations in a clang AST dump of core/yax86_core.h, and the macros it
// defines. A name's owner is the module directory of the file that defines it,
// read through the bundle's #line directives.
//
// Usage: tools/generate-rename-map.js [--check]
//
// --check compares the map that would be generated against the one on disk
// rather than writing it.

const childProcess = require('child_process');
const fs = require('fs');
const path = require('path');

const {
  ROOT,
  OWNERS,
  collectDeclarations,
  collectMacros,
  ruleBroken,
} = require('./check-namespace.js');

const MAP_FILE = path.join(ROOT, 'core', 'rename_map.txt');

// The order rename PRs apply them in.
const KINDS = ['type', 'function', 'object', 'enum', 'macro'];

// Names a rule cannot produce. Each is a decision, and the reason is beside it.
const OVERRIDES = {
  // Names that already say their owner drop the repeat.
  kDefaultVideoConfig: 'YVideoDefaultConfig',
  kNumVideoModes: 'Y_VIDEO_NUM_MODES',
  kNumVideoAdapters: 'Y_VIDEO_NUM_ADAPTERS',
  kNumPICPorts: 'Y_PIC_NUM_PORTS',
  // The one const object spelled as a variable rather than as a constant.
  opcode_table: 'YCPUOpcodeTable',
  // C's v prefix for the va_list form of a function stays at the front.
  VSNPrintF: 'YVSNPrintF',
  // Tables whose names would otherwise be their element type's. Spelled like
  // kFDCCommandMetadataTable, which already says it.
  kPITModeMetadata: 'YPITModeMetadataTable',
  kVideoAdapterMetadata: 'YVideoAdapterMetadataTable',
  kVideoModeMetadata: 'YVideoModeMetadataTable',
  // Callbacks the platform hands a module are named for that module, and
  // these two said Platform as well.
  KeyboardCallbackPlatformRaiseIRQ1: 'YPlatformKeyboardCallbackRaiseIRQ1',
  PICCallbackPlatformRaiseIRQ0: 'YPlatformPICCallbackRaiseIRQ0',
};

// Names whose owner appears again after the prefix and is meant to: the second
// one is part of what the name says rather than a repeat of who owns it.
const INTENDED_REPEATS = new Set([
  // The CGA control register's video enable bit.
  'kVideoControlVideoEnable',
]);

// Enum values that rename with the const objects rather than with the other
// enum values, because the ROM data generator emits them beside their arrays.
const OBJECT_ENUMS = new Set(['kBIOSROMDataSize', 'kHDCOptionROMDataSize']);

// Acronyms that can sit directly against another one in a name, where no
// change of case says where one ends: CGAVRAM is CGA and VRAM, BIOSROM is
// BIOS and ROM.
const ACRONYMS = ['BIOS', 'CGA', 'MDA', 'ROM', 'VRAM'];

// Splits a run of capitals into the known acronyms it is made of, or returns
// it whole if it is not made only of them.
function splitAcronyms(run) {
  for (const acronym of ACRONYMS) {
    if (run === acronym) {
      return [run];
    }
    if (run.startsWith(acronym)) {
      const rest = splitAcronyms(run.slice(acronym.length));
      if (rest.every((part) => ACRONYMS.includes(part))) {
        return [acronym, ...rest];
      }
    }
  }
  return [run];
}

// Splits a CamelCase name into its words. An acronym runs until the capital
// that starts the next word. A resolution such as 320x200 is a word of its
// own. Any other digit group stays with the word it qualifies: the one before
// it (ST0, Motor0, Reserved16), unless it follows a whole word and leads into
// an acronym, which it then quantifies (360KB).
function splitWords(name) {
  const tokens = name.match(
    /\d+x\d+|[A-Z]+(?=[A-Z][a-z])|[A-Z]?[a-z]+|[A-Z]+|\d+/g,
  );
  if (!tokens || tokens.join('') !== name) {
    throw new Error(`Cannot split ${name} into words`);
  }
  const words = [];
  for (let i = 0; i < tokens.length; ++i) {
    const token = tokens[i];
    if (/^\d+x\d+$/.test(token)) {
      words.push(token);
      continue;
    }
    if (!/^\d/.test(token)) {
      words.push(...(/^[A-Z]+$/.test(token) ? splitAcronyms(token) : [token]));
      continue;
    }
    const previous = words[words.length - 1];
    const next = tokens[i + 1];
    const leadsIntoAcronym = next && /^[A-Z]{2,}$/.test(next);
    const followsWord = previous && /[a-z]$/.test(previous);
    if (!previous || (leadsIntoAcronym && followsWord)) {
      words.push(token + (next || ''));
      ++i;
    } else {
      words[words.length - 1] = previous + token;
    }
  }
  return words;
}

function toAllCaps(name) {
  return name
    .split('_')
    .filter(Boolean)
    .map((part) => splitWords(part).join('_').toUpperCase())
    .join('_');
}

function startsWithWord(name, word) {
  return (
    name.startsWith(word) &&
    (name.length === word.length || /[A-Z0-9_]/.test(name[word.length]))
  );
}

function withoutConstantPrefix(name) {
  return /^k[A-Z]/.test(name) ? name.slice(1) : name;
}

// A name ending in its owner says it twice once the owner leads it:
// kLogModuleCPU is YCPULogModule, kLogModuleIDDMA is Y_DMA_LOG_MODULE_ID.
function withoutTrailingOwner(name, owner) {
  const head = name.slice(0, name.length - owner.length);
  if (name.endsWith(owner) && head.length > 0 && !startsWithWord(name, owner)) {
    return head.replace(/_$/, '');
  }
  return name;
}

function renameCamel(name, owner) {
  const bare = withoutTrailingOwner(withoutConstantPrefix(name), owner);
  if (OWNERS[owner].camel.some((word) => startsWithWord(bare, word))) {
    return `Y${bare}`;
  }
  return `Y${owner}${bare}`;
}

function renameAllCaps(name, owner) {
  let bare = withoutTrailingOwner(withoutConstantPrefix(name), owner);
  if (startsWithWord(bare, owner)) {
    bare = bare.slice(owner.length).replace(/^_/, '');
  }
  return `Y_${OWNERS[owner].caps}_${toAllCaps(bare)}`;
}

// Whether the owner appears again as a word after the prefix that names it.
function repeatsOwner(renamed, owner) {
  if (renamed.startsWith('Y_')) {
    const {caps} = OWNERS[owner];
    return `${renamed}_`.slice(`Y_${caps}`.length).includes(`_${caps}_`);
  }
  const rest = renamed.slice(`Y${owner}`.length);
  return new RegExp(`(^|[a-z0-9])${owner}([A-Z0-9]|$)`).test(rest);
}

function renameMacro(name) {
  if (name.startsWith('YAX86_')) {
    return `Y_${name.slice('YAX86_'.length)}`;
  }
  return `Y_${name}`;
}

// The map kind a declaration renames with.
function mapKind({name, kind, pasted}) {
  // STATIC_VECTOR_TYPE pastes its functions' names from the type it declares,
  // so they rename with the type.
  if (pasted) {
    return 'type';
  }
  if (OBJECT_ENUMS.has(name)) {
    return 'object';
  }
  return kind;
}

function buildMap() {
  const declarations = collectDeclarations();
  const entries = [];

  for (const {name, kind, owner, pasted} of declarations) {
    if (pasted || !ruleBroken(name, kind, owner)) {
      continue;
    }
    let renamed;
    if (name in OVERRIDES) {
      renamed = OVERRIDES[name];
    } else if (kind === 'enum') {
      renamed = renameAllCaps(name, owner);
    } else {
      renamed = renameCamel(name, owner);
    }
    if (
      !(name in OVERRIDES) &&
      !INTENDED_REPEATS.has(name) &&
      repeatsOwner(renamed, owner)
    ) {
      throw new Error(
        `${name} -> ${renamed} repeats its owner; add it to OVERRIDES or ` +
          `INTENDED_REPEATS`,
      );
    }
    const broken = ruleBroken(renamed, kind, owner);
    if (broken) {
      throw new Error(`${name} -> ${renamed}, but ${broken}`);
    }
    entries.push({kind: mapKind({name, kind}), from: name, to: renamed});
  }

  // A pasted function renames with the type whose name it was pasted from.
  const typeRenames = new Map(
    entries.filter((e) => e.kind === 'type').map((e) => [e.from, e.to]),
  );
  for (const {name, owner, pasted} of declarations) {
    if (!pasted || !ruleBroken(name, 'function', owner)) {
      continue;
    }
    const type = [...typeRenames.keys()]
      .filter((t) => name.startsWith(t))
      .sort((a, b) => b.length - a.length)[0];
    if (!type) {
      throw new Error(`No STATIC_VECTOR_TYPE instance generates ${name}`);
    }
    entries.push({
      kind: 'type',
      from: name,
      to: typeRenames.get(type) + name.slice(type.length),
    });
  }

  for (const name of collectMacros()) {
    if (ruleBroken(name, 'macro', null)) {
      entries.push({kind: 'macro', from: name, to: renameMacro(name)});
    }
  }

  entries.sort(
    (a, b) =>
      KINDS.indexOf(a.kind) - KINDS.indexOf(b.kind) ||
      (a.from < b.from ? -1 : a.from > b.from ? 1 : 0),
  );
  return entries;
}

function gitFiles() {
  return childProcess
    .execFileSync('git', ['ls-files', '-z'], {cwd: ROOT, encoding: 'utf8'})
    .split('\0')
    .filter(Boolean);
}

// C and C++ sources outside core/src, the generated bundles and the
// generated ROM data, which is where a host or a test defines names of its
// own.
function isHostSource(file) {
  return (
    /\.(c|cc|cpp|h|hpp)$/.test(file) &&
    !file.startsWith('core/src/') &&
    !/^core\/[^/]+\.(h|c)$/.test(file)
  );
}

function stripCommentsAndStrings(text) {
  return text.replace(
    /\/\/[^\n]*|\/\*[\s\S]*?\*\/|"(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])*'/g,
    (match) => match.replace(/[^\n]/g, ' '),
  );
}

const NOT_A_TYPE = new Set([
  'return',
  'case',
  'sizeof',
  'else',
  'do',
  'goto',
  'throw',
  'new',
  'delete',
  'typename',
  'co_return',
]);

// Old names a host or test source defines for itself, found from token shapes
// rather than a parse: a declarator after a type, an enumerator, or a macro.
// A whole-word rename would rename that definition along with the core's, so
// each has to be given a name of its own first.
function definitionsOutsideCore(files, oldNames) {
  const found = [];
  for (const file of files.filter(isHostSource)) {
    const text = stripCommentsAndStrings(
      fs.readFileSync(path.join(ROOT, file), 'utf8'),
    );
    const tokens = [
      ...text.matchAll(/#\s*define\s+\w+|[A-Za-z_]\w*|\d\w*|::|->|\S/g),
    ].map((m) => m[0]);
    let enumDepth = 0;
    let braceDepth = 0;
    let enumBrace = -1;
    let expectEnumerator = false;
    for (let i = 0; i < tokens.length; ++i) {
      const token = tokens[i];
      // A host defining one of the core's macros is overriding a hook the
      // core leaves open with #ifndef, such as YAX86_HOT, and renames with it.
      if (/^#\s*define/.test(token)) {
        continue;
      }
      if (token === 'enum') {
        enumDepth = 1;
        continue;
      }
      if (token === '{') {
        ++braceDepth;
        if (enumDepth === 1) {
          enumBrace = braceDepth;
          enumDepth = 2;
          expectEnumerator = true;
        }
        continue;
      }
      if (token === '}') {
        if (braceDepth === enumBrace) {
          enumBrace = -1;
          enumDepth = 0;
        }
        --braceDepth;
        continue;
      }
      if (enumDepth === 2 && braceDepth === enumBrace) {
        if (token === ',') {
          expectEnumerator = true;
        } else if (expectEnumerator) {
          if (oldNames.has(token)) {
            found.push(`${file}: enumerator ${token}`);
          }
          expectEnumerator = false;
        }
        continue;
      }
      if (token === ';' && enumDepth === 1) {
        enumDepth = 0;
      }
      if (!oldNames.has(token)) {
        continue;
      }
      const before = tokens[i - 1] || '';
      const after = tokens[i + 1] || '';
      const isIdentifier = (t) => /^[A-Za-z_]\w*$/.test(t || '');
      const afterTypeName =
        (isIdentifier(before) && !NOT_A_TYPE.has(before)) || before === '>';
      // After * or & only a declaration that starts a statement, since
      // otherwise these are far more often multiplication and address-of.
      const afterPointer =
        (before === '*' || before === '&') &&
        isIdentifier(tokens[i - 2]) &&
        /^([;{}]|const|static|constexpr)$/.test(tokens[i - 3] || ';') &&
        /^[=;[]$/.test(after);
      if (
        (afterPointer || (afterTypeName && /^[(=;[,{):]$/.test(after))) &&
        tokens[i - 2] !== '::'
      ) {
        found.push(`${file}: ${before} ${token} ${after}`);
      }
    }
  }
  return found;
}

function validate(entries) {
  const problems = [];

  const targets = new Map();
  for (const {from, to} of entries) {
    if (targets.has(to)) {
      problems.push(`${targets.get(to)} and ${from} both map to ${to}`);
    }
    targets.set(to, from);
  }

  // Source and scripts only, less the migration's own: Markdown names the new
  // spellings in its examples, and so do the map and the tools that make it.
  const migration = new Set([
    'core/rename_map.txt',
    'tools/apply-rename-map.js',
    'tools/check-namespace.js',
    'tools/generate-rename-map.js',
  ]);
  const files = gitFiles();
  const identifiers = new Set();
  const sources = files.filter(
    (f) => /\.(c|h|cpp|js|sh|txt|cmake)$/.test(f) && !migration.has(f),
  );
  for (const file of sources) {
    let text;
    try {
      text = fs.readFileSync(path.join(ROOT, file), 'utf8');
    } catch (e) {
      continue;
    }
    for (const match of text.matchAll(/[A-Za-z_]\w*/g)) {
      identifiers.add(match[0]);
    }
  }
  for (const {from, to} of entries) {
    if (identifiers.has(to)) {
      problems.push(`${from} -> ${to}, which the repository already uses`);
    }
  }

  const oldNames = new Set(entries.map((e) => e.from));
  for (const definition of definitionsOutsideCore(files, oldNames)) {
    problems.push(`defined outside core/src: ${definition}`);
  }

  return problems;
}

function format(entries) {
  const kindWidth = Math.max(...entries.map((e) => e.kind.length)) + 2;
  const fromWidth = Math.max(...entries.map((e) => e.from.length)) + 2;
  return entries
    .map((e) => e.kind.padEnd(kindWidth) + e.from.padEnd(fromWidth) + e.to)
    .join('\n')
    .concat('\n');
}

function main() {
  const check = process.argv.includes('--check');
  const entries = buildMap();
  const problems = validate(entries);
  if (problems.length > 0) {
    console.error(problems.join('\n'));
    console.error(`\n${problems.length} problem(s); the map was not written.`);
    process.exit(1);
  }
  const text = format(entries);
  if (check) {
    const current = fs.existsSync(MAP_FILE) ? fs.readFileSync(MAP_FILE, 'utf8') : '';
    if (current !== text) {
      console.error(`${path.relative(ROOT, MAP_FILE)} is not what would be generated.`);
      process.exit(1);
    }
    return;
  }
  fs.writeFileSync(MAP_FILE, text);
  const counts = KINDS.map(
    (kind) => `${entries.filter((e) => e.kind === kind).length} ${kind}`,
  );
  console.log(`Wrote ${path.relative(ROOT, MAP_FILE)}: ${counts.join(', ')}`);
}

main();
