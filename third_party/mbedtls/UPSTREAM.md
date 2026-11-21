# Mbed TLS for BlocksDS

Gloam builds Mbed TLS 3.6.8 from the official release archive pinned in
`UPSTREAM.json`. Upstream's Apache-2.0 OR GPL-2.0-or-later license is retained
unchanged in `LICENSE`. Release source archives include the exact upstream
archive under `dependencies/mbedtls/`, together with this port patch and builder.

`blocksds.patch` retains Antonio Niño Díaz's portability changes from
[the BlocksDS package recipe](https://github.com/blocksds/packages/tree/3dfd6051fc950d6e9189462afbd9dc26ab886d06/blocksds-mbedtls):
DSWiFi hardware entropy, platform time, and configuration for the DS runtime.
The recipe is CC0-1.0; library source retains its upstream licenses. Patch offsets change for
3.6.8; the portability changes themselves remain unchanged.

`tools/build_mbedtls.py` verifies the archive before extraction and builds ARM9
Thumb libraries under `build/mbedtls`. It preserves the system SDK package.
Gloam's HTTPS client continues to require certificate verification and TLS 1.2.
