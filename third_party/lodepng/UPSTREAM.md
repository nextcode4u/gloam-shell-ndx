# LodePNG provenance

Based on LodePNG 20261102 by Lode Vandevenne, upstream revision
`7b2893e35891c0c85bee5595caafaf3c2044b409` from
https://github.com/lvandeve/lodepng. Original notices remain in lodepng.c and
lodepng.h. The initial integration used the BlocksDS PNG example. Encoder and ancillary chunk support are disabled in this frontend.

Local changes (8 November 2026): optional FRONTEND_DECODE_PROGRESS macros add
cooperative checkpoints in CRC/Adler processing, Huffman inflation, PNG
unfiltering and scalar/bulk RGBA conversion. Cancellation exits the existing
cleanup paths; the wrapper rejects cancelled output, including partially
converted buffers. Default macros are no-ops/false when compiled elsewhere.
No checksum, bounds or color-format validation is disabled.

The standalone LICENSE is an unmodified copy of the source file's complete
license comment, retained for binary-package notice collection.

The 20261102 refresh retains the optional cooperative checkpoints, including
the size_t-based Adler loop. Ancillary chunks and encoding remain disabled.
