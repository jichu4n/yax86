#!/usr/bin/env node

// Applies one kind of core/rename_map.txt across the repository, then deletes
// the lines it applied.
//
// Usage: tools/apply-rename-map.js <kind> [--filter <regex>] [--dry-run]
//
// kind is type, function, object, enum or macro. --filter applies only the
// lines whose new name matches the regex, which is how one kind is split
// across several PRs: --filter '^Y_FDC_'. --dry-run reports without writing.
//
// What it touches:
//
// - Every C and C++ source in the repository but the generated bundles:
//   whole identifiers in code, never inside a string or character literal, so
//   .rodata and log output do not change.
// - Comments in those files: whole identifiers too, except a name that is
//   also an English word (Flag, Instruction, Position, RGB, ...). One of those
//   is renamed only where it reads as code - followed by ( or inside
//   backticks - and every other use is printed for a person to decide, since
//   "Carry Flag (CF)" must not become "Carry YCPUFlag (CF)".
// - Markdown: only inside backtick spans and fenced code blocks. A name left
//   in prose is printed.
//
// It does not touch the generated bundles under core/, which a build
// regenerates, or anything else - CMake, JSON, shell and the generators under
// core/tools are edited by hand in the PRs that need them.

const childProcess = require('child_process');
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const MAP_FILE = path.join(ROOT, 'core', 'rename_map.txt');
const KINDS = ['type', 'function', 'object', 'enum', 'macro'];

function usage(message) {
  console.error(message);
  console.error(
    'Usage: tools/apply-rename-map.js <kind> [--filter <regex>] [--dry-run]',
  );
  process.exit(1);
}

function parseArgs(argv) {
  const args = {kind: null, filter: null, dryRun: false};
  for (let i = 0; i < argv.length; ++i) {
    if (argv[i] === '--dry-run') {
      args.dryRun = true;
    } else if (argv[i] === '--filter') {
      args.filter = new RegExp(argv[++i]);
    } else if (!args.kind) {
      args.kind = argv[i];
    } else {
      usage(`Unexpected argument ${argv[i]}`);
    }
  }
  if (!KINDS.includes(args.kind)) {
    usage(`kind must be one of ${KINDS.join(', ')}`);
  }
  return args;
}

function readMap() {
  return fs
    .readFileSync(MAP_FILE, 'utf8')
    .split('\n')
    .filter((line) => line.trim())
    .map((line) => {
      const [kind, from, to] = line.trim().split(/\s+/);
      return {line, kind, from, to};
    });
}

// A name that reads as an English word in a sentence: a single capitalized
// word, or a single acronym.
function isEnglishWord(name) {
  return /^[A-Z][a-z]+$/.test(name) || /^[A-Z]{2,}$/.test(name);
}

function lineOf(text, offset) {
  let line = 1;
  for (let i = 0; i < offset; ++i) {
    if (text.charCodeAt(i) === 10) {
      ++line;
    }
  }
  return line;
}

// core/*.h are the generated bundles, which a build regenerates.
function isCSource(file) {
  return /\.(c|h|cpp)$/.test(file) && !/^core\/[^/]+\.h$/.test(file);
}

// Renames the identifiers in a comment. Returns the new text and the English
// word uses it left alone, as offsets into the comment.
function renameInComment(comment, renames) {
  const kept = [];
  const text = comment.replace(/[A-Za-z_]\w*/g, (name, offset) => {
    if (!renames.has(name)) {
      return name;
    }
    if (isEnglishWord(name)) {
      const backticksBefore = comment.slice(0, offset).split('`').length - 1;
      const readsAsCode =
        comment[offset + name.length] === '(' || backticksBefore % 2 === 1;
      if (!readsAsCode) {
        kept.push({name, offset});
        return name;
      }
    }
    return renames.get(name);
  });
  return {text, kept};
}

function renameInC(text, renames, report) {
  return text.replace(
    /\/\/[^\n]*|\/\*[\s\S]*?\*\/|"(?:\\[\s\S]|[^"\\\n])*"|'(?:\\[\s\S]|[^'\\\n])*'|\d[\w.]*|[A-Za-z_]\w*/g,
    (token, offset) => {
      if (token.startsWith('//') || token.startsWith('/*')) {
        const {text: renamed, kept} = renameInComment(token, renames);
        for (const {name, offset: within} of kept) {
          report(lineOf(text, offset + within), name, 'English word in a comment');
        }
        return renamed;
      }
      if (/^[A-Za-z_]/.test(token)) {
        return renames.get(token) || token;
      }
      return token;
    },
  );
}

function renameIdentifiers(text, renames) {
  return text.replace(/[A-Za-z_]\w*/g, (name) => renames.get(name) || name);
}

function renameInMarkdown(text, renames, report) {
  const lines = text.split('\n');
  let fence = null;
  return lines
    .map((line, index) => {
      const opening = /^\s*(`{3,}|~{3,})/.exec(line);
      if (fence) {
        if (opening && opening[1][0] === fence[0] && opening[1].length >= fence.length) {
          fence = null;
          return line;
        }
        return renameIdentifiers(line, renames);
      }
      if (opening) {
        fence = opening[1];
        return line;
      }
      // Alternate between prose and code spans, a code span being delimited
      // by a run of backticks and closed by a run of the same length.
      let result = '';
      let rest = line;
      while (rest.length > 0) {
        const open = /`+/.exec(rest);
        const prose = open ? rest.slice(0, open.index) : rest;
        for (const match of prose.matchAll(/[A-Za-z_]\w*/g)) {
          if (renames.has(match[0]) && !isEnglishWord(match[0])) {
            report(index + 1, match[0], 'in Markdown prose');
          }
        }
        result += prose;
        if (!open) {
          break;
        }
        const after = rest.slice(open.index + open[0].length);
        const close = after.indexOf(open[0]);
        if (close < 0) {
          result += rest.slice(open.index);
          break;
        }
        result += open[0] + renameIdentifiers(after.slice(0, close), renames) + open[0];
        rest = after.slice(close + open[0].length);
      }
      return result;
    })
    .join('\n');
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  const map = readMap();
  const applied = map.filter(
    (entry) => entry.kind === args.kind && (!args.filter || args.filter.test(entry.to)),
  );
  if (applied.length === 0) {
    console.log(`Nothing in ${path.relative(ROOT, MAP_FILE)} to apply.`);
    return;
  }
  const renames = new Map(applied.map((entry) => [entry.from, entry.to]));

  const files = childProcess
    .execFileSync('git', ['ls-files', '-z'], {cwd: ROOT, encoding: 'utf8'})
    .split('\0')
    .filter(Boolean)
    .filter((file) => !fs.lstatSync(path.join(ROOT, file)).isSymbolicLink());

  const notes = [];
  let changed = 0;
  for (const file of files) {
    let rename;
    if (isCSource(file)) {
      rename = renameInC;
    } else if (file.endsWith('.md')) {
      rename = renameInMarkdown;
    } else {
      continue;
    }
    const before = fs.readFileSync(path.join(ROOT, file), 'utf8');
    const after = rename(before, renames, (line, name, why) =>
      notes.push(`${file}:${line}: ${name} (${why})`),
    );
    if (after !== before) {
      ++changed;
      if (!args.dryRun) {
        fs.writeFileSync(path.join(ROOT, file), after);
      }
    }
  }

  if (!args.dryRun) {
    const remaining = map.filter((entry) => !applied.includes(entry));
    fs.writeFileSync(
      MAP_FILE,
      remaining.map((entry) => entry.line + '\n').join(''),
    );
  }

  if (notes.length > 0) {
    console.log('Left for a person to decide:');
    console.log(notes.join('\n'));
    console.log();
  }
  console.log(
    `${args.dryRun ? 'Would apply' : 'Applied'} ${applied.length} ${args.kind} ` +
      `rename(s) across ${changed} file(s); ${notes.length} use(s) left to decide.`,
  );
}

main();
