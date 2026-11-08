# stb_image provenance

Based on stb_image.h from nothings/stb at 4e9b368bfc071e97206ea5c6adba68dd0cbf8897. Full MIT/public-domain notices are retained in the header. This build enables JPEG only, disables stdio/HDR/linear conversion, and limits dimensions to 512. The authored wrapper stays in arm9/source/image_decode.c.

Local changes (8 November 2026): optional STBI_JPEG_PROGRESS and
STBI_JPEG_CANCELLED callbacks in baseline/progressive entropy block decoding,
plus progress checkpoints during progressive finishing and output rows. The
macros default to no-ops/false. They do not change pixels or format limits;
cancellation uses the decoder's existing failure cleanup. The callbacks are
provided by arm9/source/image_job.c and enabled only while its context runs.
The header is therefore no longer an unmodified upstream copy.

The standalone LICENSE reproduces the complete dual-license block from the
header unchanged, so binary-package notices include its MIT/Unlicense terms.
