"""Prepare hash-pinned official VISA headers and WFM fixtures in an empty directory."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import time
import urllib.request
from urllib.parse import urlparse

LOCK = Path(__file__).with_name("test-dependencies.lock.json")


def digest(data):
    return hashlib.sha256(data).hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_lock(path=LOCK):
    data = json.loads(Path(path).read_text("utf-8"))
    require(data["version"] == 1, "Unsupported dependency lock")
    return data


def checked_download(item, target):
    parsed = urlparse(item["url"])
    require(parsed.scheme == "https" and parsed.hostname in ("download.ni.com", "raw.githubusercontent.com"),
            "Dependency requires an approved HTTPS source")
    for attempt in range(3):
        try:
            with urllib.request.urlopen(item["url"], timeout=60) as response:
                require(urlparse(response.url).scheme == "https", "Insecure download redirect")
                data = response.read(item["size"] + 1)
            break
        except OSError:
            if attempt == 2:
                raise
            time.sleep(1 + attempt)
    require(len(data) == item["size"] and digest(data) == item["sha256"],
            "Dependency size/SHA256 mismatch: " + target.name)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)
    return data


def ar_member(data, wanted):
    require(data.startswith(b"!<arch>\n"), "Invalid NI package archive")
    offset, found = 8, []
    while offset < len(data):
        header = data[offset:offset + 60]
        require(len(header) == 60 and header[58:60] == b"`\n", "Invalid ar header")
        name = header[:16].decode("ascii").strip().rstrip("/")
        size = int(header[48:58])
        require(size >= 0 and offset + 60 + size <= len(data), "Truncated ar member")
        if name == wanted:
            found.append(data[offset + 60:offset + 60 + size])
        offset += 60 + size + size % 2
    require(offset == len(data) and len(found) == 1, "Missing/duplicate ar member: " + wanted)
    return found[0]


def extract_headers(package, root, spec):
    # Read only the pinned cabinet; never extract paths supplied by an archive.
    with tarfile.open(fileobj=io.BytesIO(ar_member(package, "data.tar.gz"))) as archive:
        members = [m for m in archive if m.name == "./" + spec["cabinet"]]
        require(len(members) == 1 and members[0].isfile(), "Missing/duplicate VISA header cabinet")
        require(members[0].size <= 1024 * 1024, "Oversized VISA cabinet")
        cabinet = archive.extractfile(members[0]).read()
    require(digest(cabinet) == spec["cabinet_sha256"], "VISA cabinet SHA256 mismatch")
    cabinet_path = root/spec["cabinet"]
    cabinet_path.write_bytes(cabinet)
    expanded = root/"expanded"
    expanded.mkdir()
    expand = Path(os.environ.get("SystemRoot", "C:/Windows"))/"System32/expand.exe"
    result = subprocess.run([str(expand), "-F:*", str(cabinet_path), str(expanded)],
                            capture_output=True, timeout=60)
    (root/"expand.log").write_bytes(result.stdout + result.stderr)
    require(result.returncode == 0, "VISA cabinet extraction failed")
    require({p.name for p in expanded.iterdir()} == {h["member"] for h in spec["headers"].values()},
            "Unexpected VISA cabinet contents")
    include = root/"include"
    include.mkdir()
    for name, item in spec["headers"].items():
        data = (expanded/item["member"]).read_bytes()
        require(digest(data) == item["sha256"], "VISA header SHA256 mismatch: " + name)
        (include/name).write_bytes(data)
    return include


def validate_wfm(folder, lock=None):
    lock = lock or read_lock()
    hashes = {}
    for item in lock["wfm"]["files"]:
        path = Path(folder)/item["path"]
        data = path.read_bytes()
        require(len(data) == item["size"] and digest(data) == item["sha256"],
                "WFM fixture SHA256 mismatch: " + item["path"])
        hashes[item["path"]] = item["sha256"]
    return hashes


def validate_dependencies(path):
    path = Path(path).resolve()
    data = json.loads(path.read_text("utf-8"))
    lock = read_lock()
    require(data.get("status") == "PASS" and data.get("lock_sha256") == digest(LOCK.read_bytes()),
            "Incomplete/stale dependency evidence")
    for key in ("visa_include", "wfm_dir"):
        require(Path(data[key]).resolve().is_relative_to(path.parent), "Dependency outside prepared directory")
    for name, item in lock["visa"]["headers"].items():
        require(digest((Path(data["visa_include"])/name).read_bytes()) == item["sha256"],
                "Prepared VISA header changed: " + name)
    validate_wfm(data["wfm_dir"], lock)
    return data


def prepare(root):
    root = Path(root).resolve()
    require(not root.exists() or not any(root.iterdir()), "Dependency directory must be new/empty")
    root.mkdir(parents=True, exist_ok=True)
    result = {"status": "FAIL", "lock_sha256": digest(LOCK.read_bytes())}
    try:
        lock = read_lock()
        visa = root/"visa"
        visa.mkdir()
        package = checked_download(lock["visa"], visa/"headers.nipkg")
        result["visa_include"] = str(extract_headers(package, visa, lock["visa"]))
        result["visa_version"] = lock["visa"]["version"]
        wfm = root/"wfm"
        for item in lock["wfm"]["files"]:
            checked_download(item, wfm/item["path"])
        result["wfm_dir"] = str(wfm)
        result["wfm_revision"] = lock["wfm"]["revision"]
        result["wfm_sha256"] = validate_wfm(wfm, lock)
        result["visa_sha256"] = {k: v["sha256"] for k, v in lock["visa"]["headers"].items()}
        result["status"] = "PASS"
    except Exception as error:
        result["error"] = str(error)
        raise
    finally:
        (root/"dependencies.json").write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
    return root/"dependencies.json"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--root", type=Path)
    group.add_argument("--verify-wfm", type=Path)
    args = parser.parse_args()
    if args.verify_wfm:
        validate_wfm(args.verify_wfm)
        print("WFM fixtures verified")
    else:
        path = prepare(args.root)
        validate_dependencies(path)
        print("Test dependencies ready: " + str(path))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError, KeyError, tarfile.TarError, subprocess.SubprocessError) as error:
        print("Test dependencies FAILED:", error, file=sys.stderr)
        sys.exit(1)
