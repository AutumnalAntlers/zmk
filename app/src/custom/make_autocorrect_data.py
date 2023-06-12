#!/usr/bin/env python3
# Copyright 2021-2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Python program to make autocorrect_data.h.

This program reads "autocorrect_dict.txt" from the current directory and
generates a C source file "autocorrect_data.h" with a serialized trie
embedded as an array. Run this program without arguments like

$ python3 make_autocorrect_data.py

Or specify a dict file as the first argument like

$ python3 make_autocorrect_data.py mykeymap/dict.txt

The output is written to "autocorrect_data.h" in the same directory as the
dictionary. Or optionally specify the output .h file as well like

$ python3 make_autocorrect_data.py dict.txt somewhere/out.h

Each line of the dict file defines one typo and its correction with the syntax
"typo -> correction". Blank lines or lines starting with '#' are ignored.
Example:

    :thier     -> their
    dosen't    -> doesn't
    fitler     -> filter
    lenght     -> length
    ouput      -> output
    widht      -> width

See autocorrect_dict_extra.txt for a larger example.

For full documentation, see
https://getreuer.info/posts/keyboards/autocorrect
"""

import os.path
import sys
import textwrap
from typing import Any, Dict, Iterator, List, Tuple

try:
  from english_words import english_words_lower_alpha_set as CORRECT_WORDS
except ImportError:
  print('Autocorrection will falsely trigger when a typo is a substring of a '
        'correctly spelled word. To check for this, install the english_words '
        'package and rerun this script:\n\n  pip install english_words\n')
  # Use a minimal word list as a fallback.
  CORRECT_WORDS = ('apparent', 'association', 'available', 'classification',
                   'effect', 'entertainment', 'fantastic', 'information',
                   'integrate', 'international', 'language', 'loosest',
                   'manual', 'nothing', 'provides', 'reference', 'statehood',
                   'technology', 'virtually', 'wealthier', 'wonderful')

KC_A = 4
KC_SPC = 0x2c
KC_QUOT = 0x34
HIGH_BIT_MASK= 1073741823 # (2**32 >> 2) - 1

TYPO_CHARS = dict(
  [
    ("'", KC_QUOT),
    (':', KC_SPC),  # "Word break" character.
  ] +
  # Characters a-z.
  [(chr(c), c + KC_A - ord('a')) for c in range(ord('a'), ord('z') + 1)]
)


def parse_file(file_name: str) -> List[Tuple[str, str]]:
  """Parses autocorrects dictionary file.

  Each line of the file defines one typo and its correction with the syntax
  "typo -> correction". Blank lines or lines starting with '#' are ignored. The
  function validates that typos only have characters in TYPO_CHARS, that
  typos are not substrings of other typos, and checking that typos don't trigger
  on CORRECT_WORDS.

  Args:
    file_name: String, path of the autocorrects dictionary.
  Returns:
    List of (typo, correction) tuples.
  """

  autocorrects = []
  typos = set()
  for line_number, typo, correction in parse_file_lines(file_name):
    if typo in typos:
      print(f'Warning:{line_number}: Ignoring duplicate typo: "{typo}"')
      continue

    # Check that `typo` is valid.
    if not(all([c in TYPO_CHARS for c in typo])):
      print(f'Error:{line_number}: Typo "{typo}" has '
            'characters other than ' + ''.join(TYPO_CHARS.keys()))
      sys.exit(1)
    for other_typo in typos:
      if typo in other_typo or other_typo in typo:
        print(f'Error:{line_number}: Typos may not be substrings of one '
              f'another, otherwise the longer typo would never trigger: '
              f'"{typo}" vs. "{other_typo}".')
        sys.exit(1)
    if len(typo) < 5:
      print(f'Warning:{line_number}: It is suggested that typos are at '
            f'least 5 characters long to avoid false triggers: "{typo}"')

    check_typo_against_dictionary(line_number, typo)

    autocorrects.append((typo, correction))
    typos.add(typo)

  return autocorrects


def make_trie(autocorrects: List[Tuple[str, str]]) -> Dict[str, Any]:
  """Makes a trie from the the typos, writing in reverse.

  Args:
    autocorrects: List of (typo, correction) tuples.
  Returns:
    Dict of dict, representing the trie.
  """
  trie = {}
  for typo, correction in autocorrects:
    node = trie
    for letter in typo[::-1]:
      node = node.setdefault(letter, {})
    node['LEAF'] = (typo, correction)

  return trie


def parse_file_lines(file_name: str) -> Iterator[Tuple[int, str, str]]:
  """Parses lines read from `file_name` into typo-correction pairs."""

  line_number = 0
  for line in open(file_name, 'rt'):
    line_number += 1
    line = line.strip()
    if line and line[0] != '#':
      # Parse syntax "typo -> correction", using strip to ignore indenting.
      tokens = [token.strip() for token in line.split('->', 1)]
      if len(tokens) != 2 or not tokens[0]:
        print(f'Error:{line_number}: Invalid syntax: "{line}"')
        sys.exit(1)

      typo, correction = tokens
      typo = typo.lower()  # Force typos to lowercase.
      typo = typo.replace(' ', ':')

      yield line_number, typo, correction


def check_typo_against_dictionary(line_number: int, typo: str) -> None:
  """Checks `typo` against English dictionary words."""

  if typo.startswith(':') and typo.endswith(':'):
    if typo[1:-1] in CORRECT_WORDS:
      print(f'Warning:{line_number}: Typo "{typo}" is a correctly spelled '
            'dictionary word.')
  elif typo.startswith(':') and not typo.endswith(':'):
    for word in CORRECT_WORDS:
      if word.startswith(typo[1:]):
        print(f'Warning:{line_number}: Typo "{typo}" would falsely trigger '
              f'on correctly spelled word "{word}".')
  elif not typo.startswith(':') and typo.endswith(':'):
    for word in CORRECT_WORDS:
      if word.endswith(typo[:-1]):
        print(f'Warning:{line_number}: Typo "{typo}" would falsely trigger '
              f'on correctly spelled word "{word}".')
  elif not typo.startswith(':') and not typo.endswith(':'):
    for word in CORRECT_WORDS:
      if typo in word:
        print(f'Warning:{line_number}: Typo "{typo}" would falsely trigger '
              f'on correctly spelled word "{word}".')


def serialize_trie(autocorrects: List[Tuple[str, str]],
                   trie: Dict[str, Any]) -> List[int]:
  """Serializes trie and correction data in a form readable by the C code.

  Args:
    autocorrects: List of (typo, correction) tuples.
    trie: Dict of dicts.
  Returns:
    List of ints in the range 0-255.
  """
  table = []

  # Traverse trie in depth first order.
  def traverse(trie_node: Dict[str, Any]) -> Dict[str, Any]:
    if 'LEAF' in trie_node:  # Handle a leaf trie node.
      typo, correction = trie_node['LEAF']
      word_boundary_ending = typo[-1] == ':'
      typo = typo.strip(':')
      i = 0  # Make the autocorrect data for this entry and serialize it.
      while i < min(len(typo), len(correction)) and typo[i] == correction[i]:
        i += 1
      backspaces = len(typo) - i - 1 + word_boundary_ending
      assert 0 <= backspaces <= 63
      correction = correction[i:]
      data = [backspaces + 128] + list(bytes(correction, 'ascii')) + [0]

      entry = {'data': data, 'links': [], 'byte_offset': 0}
      table.append(entry)
    elif len(trie_node) == 1:  # Handle trie node with a single child.
      c, trie_node = next(iter(trie_node.items()))
      entry = {'chars': c, 'byte_offset': 0}

      # It's common for a trie to have long chains of single-child nodes. We
      # find the whole chain so that we can serialize it more efficiently.
      while len(trie_node) == 1 and 'LEAF' not in trie_node:
        c, trie_node = next(iter(trie_node.items()))
        entry['chars'] += c

      table.append(entry)
      entry['links'] = [traverse(trie_node)]
    else:  # Handle trie node with multiple children.
      entry = {'chars': ''.join(sorted(trie_node.keys())), 'byte_offset': 0}
      table.append(entry)
      entry['links'] = [traverse(trie_node[c]) for c in entry['chars']]
    return entry

  traverse(trie)

  def serialize(e: Dict[str, Any]) -> List[int]:
    if not e['links']:  # Handle a leaf table entry.
      return e['data']
    elif len(e['links']) == 1:  # Handle a chain table entry.
      return [TYPO_CHARS[c] for c in e['chars']] + [0]
    else:  # Handle a branch table entry.
      data = []
      for c, link in zip(e['chars'], e['links']):
        data += [TYPO_CHARS[c] | (0 if data else 64)] + encode_link(link)
      return data + [0]

  byte_offset = 0
  for e in table:  # To encode links, first compute byte offset of each entry.
    e['byte_offset'] = byte_offset
    byte_offset += len(serialize(e))

  return [b for e in table for b in serialize(e)]  # Serialize final table.


def encode_link(link: Dict[str, Any]) -> List[int]:
  """Encodes a node link as two bytes."""
  byte_offset = link['byte_offset']
  if not (0 <= byte_offset <= 0xffff):
    print('Error: The autocorrect table is too large, a node link exceeds '
          '64KB limit. Try reducing the autocorrect dict to fewer entries.')
    sys.exit(1)
  return [byte_offset & 255, byte_offset >> 8]


def write_generated_code(autocorrects: List[Tuple[str, str]],
                         data: List[int],
                         file_name: str) -> None:
  """Writes autocorrect data as generated C code to `file_name`.

  Args:
    autocorrects: List of (typo, correction) tuples.
    data: List of ints in 0-255, the serialized trie.
    file_name: String, path of the output C file.
  """
  assert all(0 <= b <= 255 for b in data)

  def typo_len(e: Tuple[str, str]) -> int:
    return len(e[0])

  min_typo = min(autocorrects, key=typo_len)[0]
  max_typo = max(autocorrects, key=typo_len)[0]

  def decode_keycode(d: int) -> str:
    if d in [0, KC_SPC, KC_QUOT]:
      return str(d)
    elif ord('A') <= d + 61 <= ord('Z'):
      return str(d)
    else:
      raise ValueError

  def decode(d:int) -> str:
    if (d & 64):
      return "(" + decode_keycode((d & 63) - 29) + " | " + str(HIGH_BIT_MASK + 1) + ")"
    elif (d & 128):
      return "(" + str(d & 63) + " | " + str(2 * (HIGH_BIT_MASK + 1)) + ")"
    else:
      return decode_keycode(d)

  generated_code = ''.join([
    '// Generated code.\n\n',
    f'// Autocorrect dictionary ({len(autocorrects)} entries):\n',
    ''.join(sorted(f'//   {typo:<{len(max_typo)}} -> {correction}\n'
                   for typo, correction in autocorrects)),
    f'\n#include <dt-bindings/zmk/keys.h>\n',
    f'\n#define AUTOCORRECT_MIN_LENGTH {len(min_typo)}  // "{min_typo}"\n',
    f'#define AUTOCORRECT_MAX_LENGTH {len(max_typo)}  // "{max_typo}"\n\n',
    textwrap.fill('static const uint32_t autocorrect_data[%d] = {%s};' % (
      len(data), ', '.join(map(decode, data))), width=80, subsequent_indent='  '),
    '\n\n'])

  with open(file_name, 'wt') as f:
    f.write(generated_code)


def get_default_h_file(dict_file: str) -> str:
  return os.path.join(os.path.dirname(dict_file), 'autocorrect_data.h')


def main(argv):
  dict_file = argv[1] if len(argv) > 1 else 'autocorrect_dict.txt'
  h_file = argv[2] if len(argv) > 2 else get_default_h_file(dict_file)

  autocorrects = parse_file(dict_file)
  trie = make_trie(autocorrects)
  data = serialize_trie(autocorrects, trie)
  print(f'Processed %d autocorrect entries to table with %d bytes.'
        % (len(autocorrects), len(data)))
  write_generated_code(autocorrects, data, h_file)


if __name__ == '__main__':
  main(sys.argv)
