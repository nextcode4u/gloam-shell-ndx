#!/usr/bin/env python3
"""Build the pinned standalone loader and return stub using BlocksDS tools."""

import argparse
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--blocksds", type=Path, default=Path("/opt/blocksds/core"))
    parser.add_argument(
        "--tools", type=Path, default=Path("/opt/wonderful/toolchain/gcc-arm-none-eabi/bin")
    )
    args = parser.parse_args()
    source = ROOT / "third_party/nds-bootloader"
    output = ROOT / "build/homebrew-loader"
    output.mkdir(parents=True, exist_ok=True)
    cc = str(args.tools / "arm-none-eabi-gcc")
    objcopy = str(args.tools / "arm-none-eabi-objcopy")
    include = "-I" + str(args.blocksds / "libs/libnds/include")

    def run(command: list) -> None:
        subprocess.run(command, cwd=output, check=True)

    run(
        [
            cc,
            "-x",
            "assembler-with-cpp",
            include,
            "-nostdlib",
            "-Wl,-Ttext=0",
            str(source / "arm9code/mpu_reset.s"),
            "-o",
            "mpu_reset.elf",
        ]
    )
    run([objcopy, "-O", "binary", "mpu_reset.elf", "mpu_reset.bin"])
    objects = []
    for path in sorted((source / "source").iterdir()):
        if path.suffix not in (".c", ".s"):
            continue
        target = path.name + ".o"
        command = [
            cc,
            "-Os",
            "-mcpu=arm7tdmi",
            "-mthumb-interwork",
            "-marm" if path.name.endswith(".arm.c") else "-mthumb",
            "-DARM7",
            "-DNO_DLDI",
            include,
            "-I" + str(source / "source"),
            "-fno-strict-aliasing",
        ]
        if path.suffix == ".s":
            command += ["-x", "assembler-with-cpp"]
        run(command + ["-c", str(path), "-o", target])
        objects.append(target)
    run(
        [
            cc,
            "-mcpu=arm7tdmi",
            "-nostartfiles",
            "-Wl,-T," + str(source / "load.ld"),
            *objects,
            "-L" + str(args.blocksds / "libs/libnds/lib"),
            "-lnds7",
            "-lc",
            "-lgcc",
            "-o",
            "load.elf",
        ]
    )
    run([objcopy, "-O", "binary", "load.elf", "load.bin"])
    run(
        [
            cc,
            "-x",
            "assembler-with-cpp",
            "-nostdlib",
            "-Wl,-Ttext=0",
            str(source / "bootstub/bootstub.s"),
            "-o",
            "bootstub.elf",
        ]
    )
    run([objcopy, "-O", "binary", "bootstub.elf", "bootstub.bin"])
    if (output / "load.bin").stat().st_size + (output / "bootstub.bin").stat().st_size > 0x6000:
        raise ValueError("Loader and return stub exceed their reserved memory budget")


if __name__ == "__main__":
    main()
