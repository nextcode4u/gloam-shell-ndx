#!/usr/bin/env python3
"""Test the pinned TLS library and Gloam transport using authored loopback servers."""

import argparse
from pathlib import Path
import shutil
import socket
import ssl
import subprocess
import tempfile
import threading

if __package__:
    from .build_mbedtls import ROOT, extract_source, source_archive
else:
    from build_mbedtls import ROOT, extract_source, source_archive


def run(command, **kwargs):
    subprocess.run([str(x) for x in command], check=True, **kwargs)


def certificates(root):
    for name, cn in (("ca", "Gloam authored test CA"), ("other", "Other authored test CA")):
        run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
             "-subj", "/CN=" + cn, "-keyout", root / (name + ".key"), "-out", root / (name + ".pem")],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    (root / "extensions").write_text("subjectAltName=DNS:api.github.com\n")
    (root / "wrong-extensions").write_text("subjectAltName=DNS:wrong.example.invalid\n")
    run(["openssl", "req", "-newkey", "rsa:2048", "-nodes", "-subj", "/CN=api.github.com",
         "-keyout", root / "server.key", "-out", root / "server.csr"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    run(["openssl", "x509", "-req", "-in", root / "server.csr", "-CA", root / "ca.pem",
         "-CAkey", root / "ca.key", "-CAcreateserial", "-days", "1", "-extfile", root / "extensions",
         "-out", root / "server.pem"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    run(["openssl", "x509", "-req", "-in", root / "server.csr", "-CA", root / "ca.pem",
         "-CAkey", root / "ca.key", "-CAcreateserial", "-days", "1", "-extfile", root / "wrong-extensions",
         "-out", root / "wrong-server.pem"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def check_case(root, executable, name, *, trust="ca.pem", ticks=5000, expected=False,
               drop_dns=False, response=None, certificate="server.pem", stall_tls=False):
    stopped = threading.Event()
    failures = []
    dns = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    dns.bind(("127.0.0.1", 0))
    server.bind(("127.0.0.1", 0))
    dns.settimeout(0.1)
    server.settimeout(0.1)
    server.listen(1)

    def resolve():
        while not stopped.is_set():
            try:
                query, peer = dns.recvfrom(512)
                if drop_dns:
                    continue
                reply = query[:2] + b"\x81\x80\x00\x01\x00\x01\x00\x00\x00\x00" + query[12:]
                reply += b"\xc0\x0c\x00\x01\x00\x01\x00\x00\x00\x3c\x00\x04\x7f\x00\x00\x01"
                dns.sendto(reply, peer)
            except socket.timeout:
                pass

    def serve():
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.minimum_version = context.maximum_version = ssl.TLSVersion.TLSv1_2
        context.load_cert_chain(root / certificate, root / "server.key")
        while not stopped.is_set():
            try:
                connection, _ = server.accept()
            except socket.timeout:
                continue
            try:
                connection.settimeout(3)
                if stall_tls:
                    stopped.wait(3)
                    connection.close()
                    return
                with context.wrap_socket(connection, server_side=True) as secure:
                    request = secure.recv(2048)
                    if not request.startswith(b"GET /repos/nextcode4u/gloam-shell-ndx/releases?"):
                        failures.append("unexpected request")
                    secure.sendall(response or b'HTTP/1.0 200 OK\r\n\r\n[{"tag_name":"v1.2.1","draft":false,"prerelease":false}]')
                    try:
                        secure.unwrap().close()
                    except (OSError, ssl.SSLError):
                        pass
            except (OSError, ssl.SSLError):
                connection.close()  # Expected for certificate rejection or cancellation.
            return

    threads = [threading.Thread(target=resolve), threading.Thread(target=serve)]
    for thread in threads:
        thread.start()
    try:
        run([executable, dns.getsockname()[1], server.getsockname()[1], root / trust, ticks,
             int(expected)], timeout=12)
        if failures:
            raise AssertionError(failures)
        print("PASS: TLS transport " + name, flush=True)
    finally:
        stopped.set()
        for thread in threads:
            thread.join(timeout=4)
        dns.close()
        server.close()
        if any(thread.is_alive() for thread in threads):
            raise AssertionError("loopback server did not stop")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--offline", action="store_true")
    args = parser.parse_args()
    for tool in ("cc", "make", "openssl"):
        if not shutil.which(tool):
            raise SystemExit("Missing test tool: " + tool)
    archive, pin = source_archive(ROOT, args.offline)
    with tempfile.TemporaryDirectory(prefix="gloam-tls-test-") as temporary:
        root = Path(temporary)
        source = extract_source(archive, root, pin["version"])
        sanitizer = "-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer"
        run(["make", "lib", "-j4", "CC=cc", "AR=ar", "CFLAGS=" + sanitizer], cwd=source,
            stdout=subprocess.DEVNULL)
        executable = root / "transport"
        run(["cc", *sanitizer.split(), "-std=c11", "-D_DEFAULT_SOURCE", "-I" + str(source / "include"),
             "-I" + str(ROOT / "arm9/include"), ROOT / "tests/tls_transport_probe.c",
             ROOT / "arm9/source/update_transport.c", ROOT / "arm9/source/update_checker.c",
             ROOT / "arm9/source/paths.c", "-L" + str(source / "library"),
             "-lmbedtls", "-lmbedx509", "-lmbedcrypto", "-Wl,--wrap=connect", "-o", executable])
        certificates(root)
        check_case(root, executable, "trusted certificate and release response", expected=True)
        check_case(root, executable, "untrusted certificate", trust="other.pem")
        check_case(root, executable, "wrong hostname", certificate="wrong-server.pem")
        check_case(root, executable, "TLS deadline", ticks=200, stall_tls=True)
        check_case(root, executable, "HTTP failure", response=b"HTTP/1.0 503 Unavailable\r\n\r\n[]")
        check_case(root, executable, "malformed response", response=b"HTTP/1.0 200 OK\r\n\r\n[broken]")
        check_case(root, executable, "cancellation", ticks=0)
        check_case(root, executable, "DNS deadline", ticks=200, drop_dns=True)


if __name__ == "__main__":
    main()
