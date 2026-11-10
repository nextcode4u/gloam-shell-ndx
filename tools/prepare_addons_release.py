#!/usr/bin/env python3
"""Check upstream releases, refresh compatible pins, validate, then build add-ons."""

import argparse
import copy
from datetime import datetime, timezone
import difflib
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import urllib.parse
import urllib.request
import zipfile

if __package__:
    from .build_addons import archive_members, selected_applications
else:
    from build_addons import archive_members, selected_applications

ROOT = Path(__file__).resolve().parents[1]
LOCK = "config/addon-dependencies.json"
MAX_DOWNLOAD = 64 * 1024 * 1024


class ReviewRequired(ValueError):
    """A new release no longer fits the previously reviewed package recipe."""


def github(path):
    if shutil.which("gh"):
        return json.loads(subprocess.check_output(["gh", "api", path], text=True))
    request = urllib.request.Request("https://api.github.com/" + path,
                                     headers={"User-Agent": "Gloam-release-preparation"})
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.load(response)


def get_blob(url):
    if not url.startswith("https://"):
        raise ValueError("Release downloads require HTTPS")
    request = urllib.request.Request(url, headers={"User-Agent": "Gloam-release-preparation"})
    with urllib.request.urlopen(request, timeout=60) as response:
        data = response.read(MAX_DOWNLOAD + 1)
    if len(data) > MAX_DOWNLOAD:
        raise ValueError("Oversized release download")
    return data


def checksum(data):
    return hashlib.sha256(data).hexdigest()


def newer_release(releases, current_tag):
    current = next((r for r in releases if r["tag_name"] == current_tag), None)
    if current is None:
        raise ReviewRequired("Pinned release is absent from the release list; do not guess a replacement")
    if not current.get("published_at"):
        raise ReviewRequired("Pinned release has no publication date")
    choices = [r for r in releases if not r["draft"]
               and (current["prerelease"] or not r["prerelease"])
               and r["published_at"] > current["published_at"]]
    if not choices:
        return None
    latest = max(choices, key=lambda r: r["published_at"])
    old_numbers = re.match(r"^v?(\d+)(?:\.(\d+))?", current_tag)
    new_numbers = re.match(r"^v?(\d+)(?:\.(\d+))?", latest["tag_name"])
    if old_numbers and new_numbers:
        numbers = lambda match: tuple(int(part or 0) for part in match.groups())
        if numbers(new_numbers) < numbers(old_numbers):
            raise ReviewRequired("Newer publication appears to be an older version branch; select the track explicitly")
    return latest


def source_files(download, cache):
    path = cache / download["asset"]
    data = path.read_bytes() if path.exists() else get_blob(download["url"])
    if checksum(data) != download["sha256"]:
        raise ValueError("Pinned source cache/download hash mismatch")
    return archive_members(data, "tar.gz")


def relative_members(members):
    result = {}
    for name, data in members.items():
        if "/" not in name:
            raise ReviewRequired("Source archive has no repository root")
        relative = name.split("/", 1)[1]
        if relative in result:
            raise ReviewRequired("Source archive has duplicate relative paths")
        result[relative] = (name, data)
    return result


def updated_recipe(app, release, cache, report_dir):
    """Refresh unchanged layouts; never infer new redistribution permission."""
    package = app["package"]
    if (app["id"] in {"snemulds", "snemulds-shim"}
            or not package.get("source")
            or package.get("source_role") == "reference-only"
            or package.get("nested_notices") or package.get("runtime_notices")
            or package.get("project_notices")
            or not package["source"]["url"].startswith("https://codeload.github.com/")):
        raise ReviewRequired("Specialized source, notices or paired backend needs a recipe review")
    repo = app["repository"].removeprefix("https://github.com/")
    commit = github(f"repos/{repo}/commits/{urllib.parse.quote(release['tag_name'], safe='')}")["sha"]
    url = f"https://codeload.github.com/{repo}/tar.gz/{commit}"
    data = get_blob(url)
    source = {"asset": f"{app['id']}-{commit}-source.tar.gz", "url": url, "sha256": checksum(data)}
    old_files = relative_members(source_files(package["source"], cache))
    new_files = relative_members(archive_members(data, "tar.gz"))
    if old_files.get(".gitmodules", (None, b""))[1] != new_files.get(".gitmodules", (None, b""))[1]:
        raise ReviewRequired("Submodule configuration changed")
    if ".gitmodules" in new_files:
        def pins(ref):
            tree = github(f"repos/{repo}/git/trees/{ref}?recursive=1")
            if tree.get("truncated"):
                raise ReviewRequired("Truncated submodule tree")
            return {i["path"]: i["sha"] for i in tree["tree"] if i["mode"] == "160000"}
        if pins(app["commit"]) != pins(commit):
            raise ReviewRequired("Submodule pins changed; refresh their sources/notices explicitly")
    legal_name = re.compile(r"license|copying|copyright|notice|rpsl|rcsl|cbios", re.I)
    legal_paths = lambda files: {n for n in files if legal_name.search(Path(n).name)}
    if legal_paths(old_files) != legal_paths(new_files):
        raise ReviewRequired("Source license/notice inventory changed")
    result = copy.deepcopy(app)
    result["package"]["source"] = source
    for item in result["package"]["notices"]:
        relative = item["member"].split("/", 1)[1]
        if relative not in new_files:
            raise ReviewRequired(f"Notice removed or renamed: {relative}")
        old, new = old_files[relative][1], new_files[relative][1]
        if old != new:
            report_dir.mkdir(parents=True, exist_ok=True)
            diff = difflib.unified_diff(old.decode(errors="replace").splitlines(True),
                                        new.decode(errors="replace").splitlines(True),
                                        fromfile="previous/" + relative, tofile="candidate/" + relative)
            (report_dir / (app["id"] + "-notices.diff")).write_text("".join(diff))
            raise ReviewRequired(f"Retained notice changed: {relative}; inspect the notice diff")
        item["member"] = new_files[relative][0]
    evidence = app.get("argv_evidence", "").split("/blob/" + app["commit"] + "/", 1)
    if len(evidence) == 2:
        relative = evidence[1]
        if relative not in new_files:
            raise ReviewRequired("Argv source evidence moved")
        if (app["argv"] == "direct" and old_files[relative][1] != new_files[relative][1]
                and not re.search(rb"\bargv\s*\[\s*1\s*\]", new_files[relative][1])):
            raise ReviewRequired("Changed argv code needs a content-loading review")
    assets = {a["name"]: a for a in release["assets"]}
    downloaded = {}
    for item in result["package"]["files"]:
        if item["format"] not in {"raw", "zip"}:
            raise ReviewRequired("Custom payload archive requires a recipe review")
        name = urllib.parse.unquote(urllib.parse.urlparse(item["download"]["url"]).path.rsplit("/", 1)[1])
        if not re.fullmatch(r"[A-Za-z0-9_.-]+", name):
            raise ReviewRequired("Release asset filename needs an explicit mapping")
        if name not in assets:
            raise ReviewRequired(f"Release asset removed or renamed: {name}")
        asset = assets[name]
        if name not in downloaded:
            blob = get_blob(asset["browser_download_url"])
            digest = checksum(blob)
            if asset.get("digest") and asset["digest"] != "sha256:" + digest:
                raise ValueError("Upstream release digest mismatch")
            download = {"asset": f"{app['id']}-{digest[:16]}-{name}",
                        "url": asset["browser_download_url"], "sha256": digest}
            downloaded[name] = (blob, download)
            (cache / download["asset"]).write_bytes(blob)
        blob, download = downloaded[name]
        item["download"] = download
        if item["format"] == "zip":
            members = archive_members(blob, "zip")
            if item["member"] not in members:
                raise ReviewRequired("ZIP member layout changed")
            blob = members[item["member"]]
        item["sha256"] = checksum(blob)
    (cache / source["asset"]).write_bytes(data)
    result.update(version=release["tag_name"], commit=commit, release=release["html_url"],
                  release_assets=[{"name": a["name"], "url": a["browser_download_url"]}
                                  for a in release["assets"]])
    for key in ("argv_evidence", "bios_evidence", "license_evidence"):
        result[key] = result[key].replace(app["commit"], commit)
    return result


def inspect_catalog(root, lock):
    proposed = copy.deepcopy(lock)
    report = []
    cache = root / "build/addons-cache"
    cache.mkdir(parents=True, exist_ok=True)
    report_dir = root / "build/addon-updates"
    for index, app in enumerate(lock["applications"]):
        if "package" not in app:
            continue
        row = {"id": app["id"], "previous": app["version"]}
        print(f"Checking add-on release: {app['id']}", flush=True)
        try:
            release_url = app.get("release") or ""
            if "/releases/tag/" not in release_url:
                # Host snapshots have no trusted latest-version API; verify unchanged bytes.
                binary = app.get("binary_distribution")
                if binary and checksum(get_blob(binary["url"])) != binary["sha256"]:
                    raise ReviewRequired("Unversioned host snapshot changed; source/argv must be reviewed")
                row.update(status="pinned-snapshot", detail="Retained explicit snapshot; no guessed branch update")
            else:
                repo = app["repository"].removeprefix("https://github.com/")
                tag = release_url.split("/releases/tag/", 1)[1]
                releases = github(f"repos/{repo}/releases?per_page=100")
                newer = newer_release(releases, tag)
                if newer:
                    row["candidate"] = newer["tag_name"]
                    proposed["applications"][index] = updated_recipe(app, newer, cache, report_dir)
                    row["status"] = "updated"
                else:
                    current = next(r for r in releases if r["tag_name"] == tag)
                    digests = {a["browser_download_url"]: a.get("digest") for a in current["assets"]}
                    checked = {}
                    component_assets = {}
                    for item in app["package"]["files"]:
                        assets = digests
                        component_release = item.get("release", release_url)
                        if component_release != release_url:
                            match = re.fullmatch(r"https://github\.com/([A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+)/releases/tag/([^?#]+)", component_release)
                            if not match:
                                raise ReviewRequired("Invalid component release URL")
                            if component_release not in component_assets:
                                route = f"repos/{match[1]}/releases/tags/{urllib.parse.quote(match[2], safe='')}"
                                component = github(route)
                                if component.get("draft") or component["tag_name"] != match[2]:
                                    raise ReviewRequired("Component release does not match its explicit pin")
                                component_assets[component_release] = {
                                    a["browser_download_url"]: a.get("digest") for a in component["assets"]}
                            assets = component_assets[component_release]
                        if item["download"]["url"] not in assets:
                            raise ReviewRequired("Pinned release asset was removed or renamed upstream")
                        expected = assets.get(item["download"]["url"])
                        if expected and expected != "sha256:" + item["download"]["sha256"]:
                            raise ReviewRequired("Pinned release asset was replaced upstream")
                        if not expected:
                            url = item["download"]["url"]
                            if url not in checked:
                                checked[url] = checksum(get_blob(url))
                            if checked[url] != item["download"]["sha256"]:
                                raise ReviewRequired("Pinned release bytes changed without a version update")
                    row["status"] = "current"
        except ReviewRequired as error:
            row.update(status="review-required", detail=str(error))
        except (OSError, ValueError, KeyError, tarfile.TarError, zipfile.BadZipFile,
                subprocess.CalledProcessError) as error:
            row.update(status="error", detail=str(error))
        report.append(row)
    if any(r["status"] == "updated" for r in report):
        proposed["reviewed"] = datetime.now(timezone.utc).date().isoformat()
    return proposed, report


def refresh_audit_table(root, lock):
    path = root / "docs/guides/ADDONS.md"
    if not path.exists():
        return
    text = path.read_text()
    start = text.index("| App / pinned version |")
    end = text.find("\n\n", start)
    if end < 0:
        end = len(text)
    rows = ["| App / pinned version | Argv | Delivery / terms |", "| --- | --- | --- |"]
    for app in lock["applications"]:
        delivery = {"approved": "Bundled when pinned", "noncommercial": "Noncommercial opt-in",
                    "download-only": "Direct upstream download", "blocked": "Manual / held"}[app["redistribution"]]
        if "package" not in app:
            delivery = "Manual setup; no reviewed runtime pin"
        rows.append(f"| [{app['id']} {app['version']}]({app.get('release') or app['repository']}) | "
                    f"[{app['argv']}]({app['argv_evidence']}) | {delivery}; {app['license']} |")
    path.write_text(text[:start] + "\n".join(rows) + text[end:])


def prepare(root, check_only=False, skip_build=False, noncommercial=False, only=None):
    path = root / LOCK
    lock = json.loads(path.read_text())
    selected_applications(lock, noncommercial, only)
    proposed, report = inspect_catalog(root, lock)
    folder = root / "build/addon-updates"
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    (folder / "proposed-lock.json").write_text(json.dumps(proposed, indent=2) + "\n")
    if any(r["status"] in {"review-required", "error"} for r in report):
        raise ReviewRequired("Build stopped; inspect build/addon-updates/report.json before refreshing complex pins")
    changes = sum(r["status"] == "updated" for r in report)
    print(f"Release check complete: {len(report)} apps; {changes} compatible pin updates.", flush=True)
    if check_only:
        return changes
    if changes:
        path.write_text(json.dumps(proposed, indent=2) + "\n")
        refresh_audit_table(root, proposed)
    python = next((p for p in (root / ".venv/bin/python", root / ".venv/Scripts/python.exe")
                   if p.is_file()), Path(sys.executable))
    log = folder / "validation.log"
    with log.open("w") as output:
        for command in ([str(python), "tools/make_emulator_presets.py"],
                        [str(python), "-m", "unittest", "discover", "-s", "tests", "-p", "test_*.py"],
                        [str(python), "tools/check_tree.py"]):
            subprocess.run(command, cwd=root, stdout=output, stderr=subprocess.STDOUT, check=True)
    command = [str(python), "tools/build_addons.py", "--pinned"]
    if skip_build:
        command.append("--skip-build")
    if noncommercial:
        command.append("--noncommercial")
    if only:
        command.extend(["--only", *only])
    print("Validation passed; building add-on and companion archives.", flush=True)
    subprocess.run(command, cwd=root, check=True)
    return changes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check-only", action="store_true", help="Write the report/proposal without changing pins or building")
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--noncommercial", action="store_true")
    parser.add_argument("--only", nargs="+", help="Restrict bundled selection; still check the complete updater catalog")
    args = parser.parse_args()
    try:
        changes = prepare(ROOT, args.check_only, args.skip_build, args.noncommercial, args.only)
        if args.check_only and changes:
            parser.exit(2, "Compatible updates proposed; run without --check-only to apply and build.\n")
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Add-on release preparation failed: {error}\n")


if __name__ == "__main__":
    main()
