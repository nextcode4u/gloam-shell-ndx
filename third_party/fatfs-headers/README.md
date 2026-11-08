# BlocksDS FatFs directory headers

Unmodified headers for the FatFs already linked by BlocksDS 1.25.0. No second
filesystem implementation is compiled. Copyright and license notices are in
each header.

- `ff.h`: blocksds/wf-fatfs `2b5c26e2a1eb4cdbbcb57070d49a6ebf7b60c7f9`, `source/ff.h`.
- `ffconf.h`: blocksds/libnds `e478cc660a8c7254fbffc54bda3ce0b93ab14c15`, `source/arm9/libc/fatfs/ffconf.h`.

The directory adapter uses f_opendir/f_readdir/f_closedir to obtain names,
attributes, sizes and dates in one pass, avoiding repeated path searches.
It uses no private POSIX DIR layout. The optimization is enabled only for
SDK 1.25.0; other versions fall back to POSIX enumeration until these headers
and their ABI are reviewed. Do not change configuration macros independently
of the SDK library.

BlocksDS namespaces the linked directory symbols as `libnds_f_*`; the adapter
renames only those three declarations before including the unmodified header.
