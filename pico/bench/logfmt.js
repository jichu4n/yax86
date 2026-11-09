// logfmt lines, as the core's logger writes them - see core/src/util/logfmt.h
// and "util — logging" in AGENTS.md: space separated key=value pairs, with a
// value quoted when it has to be, and \", \\ and \xNN escapes inside quotes.

// Parse a line into its pairs, in order, as an object of strings. Returns null
// for a line that is not logfmt - an empty one, or anything with a token that
// is not a key=value pair - so that a reader can pass other text through.
function parseLine(line) {
  const record = {};
  let i = 0;
  const n = line.length;
  let pairs = 0;
  while (i < n) {
    while (i < n && line[i] === ' ') {
      ++i;
    }
    if (i >= n) {
      break;
    }
    const keyStart = i;
    while (i < n && line[i] !== '=' && line[i] !== ' ') {
      ++i;
    }
    if (i >= n || line[i] !== '=' || i === keyStart) {
      return null;
    }
    const key = line.slice(keyStart, i);
    ++i;
    let value = '';
    if (line[i] === '"') {
      ++i;
      let closed = false;
      while (i < n) {
        const c = line[i];
        if (c === '"') {
          closed = true;
          ++i;
          break;
        }
        if (c === '\\' && i + 1 < n) {
          const next = line[i + 1];
          if (next === 'x' && i + 3 < n) {
            value += String.fromCharCode(parseInt(line.slice(i + 2, i + 4), 16));
            i += 4;
          } else {
            value += next;
            i += 2;
          }
          continue;
        }
        value += c;
        ++i;
      }
      if (!closed) {
        return null;
      }
    } else {
      const valueStart = i;
      while (i < n && line[i] !== ' ') {
        ++i;
      }
      value = line.slice(valueStart, i);
    }
    record[key] = value;
    ++pairs;
  }
  return pairs > 0 ? record : null;
}

// A value as a number: decimal, or hex with a 0x prefix. Counts and times up
// to 2^53 are exact, which is about 285 years of microseconds.
function toNumber(value) {
  if (value === undefined) {
    return undefined;
  }
  return value.startsWith('0x') ? parseInt(value.slice(2), 16) : Number(value);
}

// Whether a value can be written without quotes: the same rule as the C
// writer's.
function isBare(value) {
  if (value.length === 0) {
    return false;
  }
  for (const c of value) {
    const code = c.charCodeAt(0);
    if (code <= 0x20 || code >= 0x7f || c === '"' || c === '=' || c === '\\') {
      return false;
    }
  }
  return true;
}

// Write a value, quoting and escaping it if it has to be.
function formatValue(value) {
  const text = String(value);
  if (isBare(text)) {
    return text;
  }
  let out = '"';
  for (const c of text) {
    const code = c.charCodeAt(0);
    if (c === '"' || c === '\\') {
      out += '\\' + c;
    } else if (code < 0x20 || code >= 0x7f) {
      out += '\\x' + code.toString(16).toUpperCase().padStart(2, '0');
    } else {
      out += c;
    }
  }
  return out + '"';
}

// Write a line from an object's pairs, in order.
function formatLine(record) {
  return Object.entries(record)
    .map(([key, value]) => `${key}=${formatValue(value)}`)
    .join(' ');
}

module.exports = {parseLine, toNumber, formatValue, formatLine};
