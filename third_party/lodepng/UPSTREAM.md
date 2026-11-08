# LodePNG provenance

Based on LodePNG 20230410 by Lode Vandevenne, retained via the
BlocksDS PNG example. Original notices remain in lodepng.c and
lodepng.h. Encoder and ancillary chunk support are disabled in this frontend.

Local changes (8 November 2026): optional FRONTEND_DECODE_PROGRESS macros add
cooperative checkpoints in CRC/Adler processing, Huffman inflation, PNG
unfiltering and scalar/bulk RGBA conversion. Cancellation exits the existing
cleanup paths; the wrapper rejects cancelled output, including partially
converted buffers. Default macros are no-ops/false when compiled elsewhere.
No checksum, bounds or color-format validation is disabled.

The standalone LICENSE is an unmodified copy of the source file's complete
license comment, retained for binary-package notice collection.
