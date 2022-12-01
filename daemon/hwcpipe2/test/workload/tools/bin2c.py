#!/usr/bin/env python3
#
# Copyright (c) 2022 ARM Limited.
#
# SPDX-License-Identifier: MIT
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

"""
Helper script to convert binary file to comma separated list of C chars.
"""

import sys
import argparse

_LINE_LENGTH = 120


def __convert(input_filename: str, output_filename: str) -> int:
    """
    Convert binary file to comma separated list of C chars.
    :param input_filename: Input file to convert.
    :param output_filename: Where to write the chars.
    :return: 0 on success.
    """
    with open(input_filename, "rb") as input_file:
        char_array = input_file.read()

    with open(output_filename, "w", encoding="UTF-8") as output_file:
        output_file.write(f"/* autogenerated from {input_filename} */\n")
        chars_on_line = 0
        for char in char_array:
            char_str = f"'\\x{char:02x}', "

            chars_on_line += len(char_str)
            if chars_on_line > _LINE_LENGTH:
                chars_on_line = len(char_str)
                output_file.write("\n")

            output_file.write(char_str)
        output_file.write("\n")

    return 0


def main() -> int:
    """
    Main entry point.
    :return: 0 on success.
    """
    parser = argparse.ArgumentParser(description="Convert binary file to char array.")
    parser.add_argument("input_file", help="Binary file to convert.")
    parser.add_argument("output_file", help="Where to write the char array.")

    args = parser.parse_args()

    return __convert(args.input_file, args.output_file)


if __name__ == "__main__":
    sys.exit(main())
