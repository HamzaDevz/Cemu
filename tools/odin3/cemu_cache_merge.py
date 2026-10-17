# cemu_cache_merge.py <out> <in>...: merges Cemu transferable caches (<titleid>_shaders.bin or <titleid>_vkpipeline.bin)
# into <out>, keeping the first copy of each entry. The header gets the extra version Cemu expects for the title ID that
# starts the name of <out>, so a cache made for another region of the same game loads too. Cemu replaces a cache whose
# extra version does not match with an empty one.
import os
import struct
import sys

MAGIC_V2, MAGIC_V3 = 0x8371b695, 0x8371b696
TABLE = (0xEFEFEFEFEFEFEFEF, 0xFEFEFEFEFEFEFEFE)
HEADER_SIZE = 128


def extra_version(path):
    # LatteShaderCache_getShaderCacheExtraVersion / LatteShaderCache_getPipelineCacheExtraVersion
    title = int(os.path.basename(path)[:16], 16)
    v = (title >> 32) + (title & 0xFFFFFFFF) * 3 + 1
    if path.endswith('_shaders.bin'):
        v += 0xe97af1ad
    return v & 0xFFFFFFFF


def entries(path):
    d = open(path, 'rb').read()
    magic, _, data_off, table_off, table_size = struct.unpack_from('<IIQQI', d)
    if magic not in (MAGIC_V2, MAGIC_V3):
        sys.exit(f'{path}: not a Cemu cache')
    for i in range(table_size // 32):
        n1, n2, off, size, flags = struct.unpack_from('<QQQIB', d, data_off + table_off + 32 * i)
        if (n1, n2) in ((0, 0), TABLE):
            continue
        # V2 caches are never compressed and leave the flags undefined.
        yield (n1, n2), flags if magic == MAGIC_V3 else 0, d[data_off + off:data_off + off + size]


out, inputs = sys.argv[1], sys.argv[2:]
merged = {}
for path in inputs:
    added = 0
    for key, flags, raw in entries(path):
        if key not in merged:
            merged[key] = (flags, raw)
            added += 1
    print(f'{path}: +{added}')
blob, table = bytearray(), []
for (n1, n2), (flags, raw) in merged.items():
    table.append(struct.pack('<QQQIB3x', n1, n2, len(blob), len(raw), flags))
    blob += raw
# The file table describes itself in its first entry.
table_size = (len(table) + 1) * 32
table.insert(0, struct.pack('<QQQIB3x', *TABLE, len(blob), table_size, 0))
header = struct.pack('<IIQQI', MAGIC_V3, extra_version(out), HEADER_SIZE, len(blob), table_size).ljust(HEADER_SIZE, b'\0')
with open(out, 'wb') as f:
    f.write(header + blob + b''.join(table))
print(f'{out}: {len(merged)} entries')
