#!/usr/bin/env python3

""" Program to create update image """

import argparse
import functools
import struct
import zlib

from intelhex import IntelHex, hex2bin


def parse_arguments():
    """Parse the command line arguments"""
    parser = argparse.ArgumentParser("Create upgrade file")
    parser.add_argument(
        "--input",
        "-I",
        help="Path to app_moved_test_update.hex file",
        default="build/zephyr/app_moved_test_update.hex",
    )
    parser.add_argument(
        "--output",
        "-O",
        help="Path to app_moved_test_update.hex file",
        required=True,
    )
    parser.add_argument(
        "--page-size",
        type=functools.partial(int, base=0),
        help="Size of flash page",
        default=4096,
    )
    parser.add_argument(
        "--mcuboot-pad-size",
        type=functools.partial(int, base=0),
        help="Size of mcuboot_pad",
        default=0x200,
    )
    parser.add_argument(
        "--backup-header-start",
        type=functools.partial(int, base=0),
        help="Address of the backup header",
        default=0xFD000,
    )
    parser.add_argument(
        "--backup-trailer-start",
        type=functools.partial(int, base=0),
        help="Address of the backup trailer",
        default=0xFE000,
    )
    parser.add_argument(
        "--gateway",
        action="store_true",
        default=False,
        help="Generate .bin file for use with Mira gateway FOTA. \
            Only --input and --output arguments are used"
    )
    return parser.parse_args()


def create_fota_header(blob, fota_type, flags, version):
    """Create a FOTA SWAP area header"""
    size = len(blob)
    crc = zlib.crc32(blob)
    return struct.pack("<IIHBB", size, crc, fota_type, flags, version)


def main():
    """Translate a program to update format with backup pages and FOTA header"""
    args = parse_arguments()
    app_hex = args.input
    output_hex = args.output
    if args.gateway:
        hex2bin(app_hex, args.output, None, None, None, None)
    else:
        page_size = args.page_size
        backup_header_start = args.backup_header_start
        backup_trailer_start = args.backup_trailer_start

        mcuboot_pad_size = args.mcuboot_pad_size

        hex_data = IntelHex(app_hex)
        binary_data = hex_data.tobinarray()

        total_pages = (len(binary_data) + page_size - 1) // page_size

        # Add header backup page:
        hex_data.frombytes(binary_data[0:page_size], backup_header_start)

        # Add fota header:
        fota_header = create_fota_header(binary_data, 0, 0, 0)
        hex_data.frombytes(
            fota_header, backup_header_start + mcuboot_pad_size - len(fota_header)
        )

        # Add trailer backup page:
        hex_data.frombytes(
            binary_data[((total_pages - 1) * page_size) :], backup_trailer_start
        )

        hex_data.tofile(output_hex, format="hex")


main()
