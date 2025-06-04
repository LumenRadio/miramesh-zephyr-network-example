#!/usr/bin/env python3

import os
import re
import sys
import argparse
from intelhex import IntelHex

def read_file(file):
    data = bytearray()

    text = file.read()

    rm_comments = re.compile("^#.*$")
    for line in text.split("\n"):
        line = rm_comments.sub("", line)
        if line != "":
            data.extend(line.encode("utf-8"))
            data.extend(b"\n")

    data.extend([0])
    return data

def add_padding(data, length):
    padded = data + bytearray([0xFF] * length)
    padded = padded[:length]
    return padded

if __name__ == "__main__":

    class ArgHexInt(argparse.Action):
        def __call__(self, parser, namespace, values, option_string=None):
            setattr(namespace, self.dest, int(values, 0))

    parser = argparse.ArgumentParser(
        description="pack a command sequence file to a hex file for flashing"
    )
    parser.add_argument(
        "-f",
        "--config-file",
        dest="infile",
        required=True,
        type=argparse.FileType("r"),
    )
    parser.add_argument(
        "-o",
        "--output-file",
        dest="outfile",
        required=True,
        type=argparse.FileType("w"),
    )

    parser.add_argument(
        "-a",
        "--address",
        dest="address",
        required=True,
        action=ArgHexInt
    )
    parser.add_argument(
        "-l",
        "--length",
        dest="length",
        required=True,
        action=ArgHexInt
    )

    args = parser.parse_args()

    data_to_write = add_padding(read_file(args.infile), args.length)

    ih = IntelHex()
    ih.frombytes(data_to_write, args.address)

    ih.write_hex_file(args.outfile, eolstyle="CRLF")
