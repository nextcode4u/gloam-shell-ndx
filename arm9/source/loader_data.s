.section .rodata
.balign 4
.global frontend_loader_start, frontend_loader_end
frontend_loader_start:
.incbin "build/homebrew-loader/load.bin"
frontend_loader_end:
.balign 4
.global frontend_bootstub_start, frontend_bootstub_end
frontend_bootstub_start:
.incbin "build/homebrew-loader/bootstub.bin"
frontend_bootstub_end:
