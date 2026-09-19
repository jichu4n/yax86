"""Mark every function definition in a generated bundle with YAX86_HOT.

The Pico harness places anything so marked in SRAM. Which functions are hot is
not worth arguing about on a part whose flash is several times slower than its
RAM - the whole core fits, so the whole core goes.

Rewrites the bundle in place. Idempotent.
"""
import re
import sys

# A definition starts at column zero. Either the whole signature is on the line
# (return type, name, open paren) or the return type wrapped onto the line
# before, leaving the line to start with the name.
kDefinition = re.compile(
    r'^([A-Za-z_][A-Za-z0-9_]*[\w\s\*]*?\**\s*)?'
    r'([A-Za-z_][A-Za-z0-9_]*)\s*\([^;]*$')
# Things that look like a definition but are not.
kNotDefinition = re.compile(
    r'^\s*(if|for|while|switch|return|else|do|typedef|struct|enum|union|'
    r'extern|#|//|/\*|\*)\b')


def EndsInABody(lines, index):
  """True if the signature starting at index is followed by a body, not a ';'.

  Distinguishes a definition from a forward declaration, which the bundle has
  plenty of and which cannot carry a section attribute usefully.
  """
  depth = 0
  for line in lines[index:index + 12]:
    for char in line:
      if char == '(':
        depth += 1
      elif char == ')':
        depth -= 1
        if depth == 0:
          rest = line[line.index(char) + 1:] if False else None
    if depth == 0 and '(' in ''.join(lines[index:index + 1]) or depth == 0:
      stripped = line.rstrip()
      if stripped.endswith('{'):
        return True
      if stripped.endswith(';'):
        return False
  return False


def IsDefinitionStart(lines, index):
  line = lines[index]
  if not line or line[0].isspace():
    return False
  if line.startswith('YAX86_HOT'):
    return False
  # A signature whose return type and attributes wrapped onto the line before
  # may already carry the mark there. This is also what makes the pass
  # idempotent for such definitions.
  if index and 'YAX86_HOT' in lines[index - 1]:
    return False
  if kNotDefinition.match(line):
    return False
  # Static inline functions live in the headers to be inlined at the call site.
  # Giving them a section of their own only matters for the copy the compiler
  # decides to emit anyway, and the reference build leaves them alone.
  if line.startswith('static inline') or line.startswith('inline'):
    return False
  # A table of function pointers is data, and code and read-only data cannot
  # share a section - the compiler rejects it outright.
  if '(*' in line:
    return False
  if not kDefinition.match(line):
    return False
  # A wrapped signature only counts when the line before it ends in a type,
  # not in a semicolon, brace or comment.
  if '(' in line and line.split('(')[0].strip().count(' ') == 0:
    stripped = lines[index - 1].strip() if index else ''
    if not stripped or stripped[-1] in ';{}/*)':
      return False
  return EndsInABody(lines, index)


def Hotify(text):
  lines = text.split('\n')
  return '\n'.join(
      'YAX86_HOT ' + line if IsDefinitionStart(lines, i) else line
      for i, line in enumerate(lines))


if __name__ == '__main__':
  for path in sys.argv[1:]:
    with open(path) as f:
      original = f.read()
    with open(path, 'w') as f:
      f.write(Hotify(original))
