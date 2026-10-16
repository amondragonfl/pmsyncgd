"""Draws res/pmsyncgd.ico: a smiley, Geometry Dash-style cube. Python stdlib only.

    python tools/make_icon.py
"""
import pathlib
import struct
import zlib

ART = [
    "................",
    ".KKKKKKKKKKKKKK.",
    ".KPPPPPPPPPPPPK.",
    ".KPPPPPPPPPPPPK.",
    ".KPPKKKKKKKKPPK.",
    ".KPPKCCCCCCKPPK.",
    ".KPPKCKCCKCKPPK.",
    ".KPPKCKCCKCKPPK.",
    ".KPPKCCCCCCKPPK.",
    ".KPPKCKCCKCKPPK.",
    ".KPPKCCKKCCKPPK.",
    ".KPPKKKKKKKKPPK.",
    ".KPPPPPPPPPPPPK.",
    ".KPPPPPPPPPPPPK.",
    ".KKKKKKKKKKKKKK.",
    "................",
]

COLORS = {
    ".": (0, 0, 0, 0),
    "K": (27, 20, 45, 255),
    "P": (255, 122, 182, 255),
    "C": (92, 225, 230, 255),
}

SIZES = [16, 24, 32, 48, 64, 128, 256]


def png(size: int) -> bytes:
    n = len(ART)
    rows = []
    for y in range(size):
        line = bytearray(b"\x00")  # filter: none
        for x in range(size):
            line += bytes(COLORS[ART[y * n // size][x * n // size]])
        rows.append(bytes(line))

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    header = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)  # 8-bit RGBA
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header)
            + chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b""))


def main() -> None:
    images = [png(s) for s in SIZES]
    out = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    for size, data in zip(SIZES, images):
        out += struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    out += b"".join(images)

    target = pathlib.Path(__file__).resolve().parent.parent / "res" / "pmsyncgd.ico"
    target.parent.mkdir(exist_ok=True)
    target.write_bytes(out)
    print(f"wrote {target} ({len(out)} bytes)")


if __name__ == "__main__":
    main()
