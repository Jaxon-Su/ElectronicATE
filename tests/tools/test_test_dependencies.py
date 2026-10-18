"""Dependency preparation rejects corruption and incomplete/stale caches."""
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/"ci"))
import prepare_test_dependencies as dep


class DependencyTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def test_https_source_required(self):
        for url in ("http://download.ni.com/file", "https://example.com/file"):
            with self.subTest(url=url), self.assertRaisesRegex(ValueError, "HTTPS"):
                dep.checked_download({"url":url}, self.root/"file")

    def test_bad_hash_or_size_never_written(self):
        for size, sha in ((3, "wrong"), (2, dep.digest(b"abc"))):
            response = io.BytesIO(b"abc")
            response.url = "https://download.ni.com/file"
            with patch.object(dep.urllib.request, "urlopen", return_value=response):
                with self.assertRaisesRegex(ValueError, "SHA256"):
                    dep.checked_download({"url":response.url, "size":size, "sha256":sha}, self.root/"file")
            self.assertFalse((self.root/"file").exists())

    def test_valid_download_preserves_exact_bytes(self):
        response = io.BytesIO(b"abc")
        response.url = "https://download.ni.com/file"
        with patch.object(dep.urllib.request, "urlopen", return_value=response):
            dep.checked_download({"url":response.url, "size":3, "sha256":dep.digest(b"abc")}, self.root/"file")
        self.assertEqual((self.root/"file").read_bytes(), b"abc")

    def test_failed_prepare_leaves_fail_evidence(self):
        with patch.object(dep, "checked_download", side_effect=ValueError("corrupt package")):
            with self.assertRaisesRegex(ValueError, "corrupt package"):
                dep.prepare(self.root/"new")
        data = json.loads((self.root/"new/dependencies.json").read_text("utf-8"))
        self.assertEqual(data["status"], "FAIL")

    def test_nonempty_directory_preserved(self):
        (self.root/"keep").write_bytes(b"original")
        with self.assertRaisesRegex(ValueError, "new/empty"):
            dep.prepare(self.root)
        self.assertEqual((self.root/"keep").read_bytes(), b"original")

    def test_missing_or_changed_wfm_rejected(self):
        lock = {"wfm":{"files":[{"path":"sample.wfm","size":3,"sha256":dep.digest(b"abc")}]}}
        with self.assertRaises(OSError):
            dep.validate_wfm(self.root, lock)
        (self.root/"sample.wfm").write_bytes(b"bad")
        with self.assertRaisesRegex(ValueError, "SHA256"):
            dep.validate_wfm(self.root, lock)
        (self.root/"sample.wfm").write_bytes(b"abc")
        self.assertEqual(dep.validate_wfm(self.root, lock)["sample.wfm"], dep.digest(b"abc"))

    def test_truncated_or_duplicate_ar_member_rejected(self):
        def member(value):
            header = f'{"data.tar.gz/":<16}{"0":<12}{"0":<6}{"0":<6}{"100644":<8}{len(value):<10}`\n'.encode()
            return header + value + (b"\n" if len(value)%2 else b"")
        good = b"!<arch>\n" + member(b"hello")
        self.assertEqual(dep.ar_member(good, "data.tar.gz"), b"hello")
        for value in (good[:-3], good+member(b"hello"), b"bad"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                dep.ar_member(value, "data.tar.gz")

    def test_stale_or_failed_evidence_rejected(self):
        path = self.root/"dependencies.json"
        for value in ({"status":"FAIL"}, {"status":"PASS","lock_sha256":"old"}):
            path.write_text(json.dumps(value), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "Incomplete/stale"):
                dep.validate_dependencies(path)


if __name__ == "__main__":
    unittest.main(verbosity=2)
