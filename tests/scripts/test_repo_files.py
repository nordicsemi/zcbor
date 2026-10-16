#!/usr/bin/env python3
#
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0

from unittest import TestCase, main, skipIf
from unittest.mock import patch
from pathlib import Path
from re import search, S, compile
from urllib import request
from urllib.error import HTTPError, URLError
from subprocess import Popen, check_output, PIPE, run, CalledProcessError
from shutil import rmtree
from platform import python_version_tuple
from sys import platform
from concurrent.futures import ThreadPoolExecutor
import os
from time import sleep
from http import HTTPStatus
from typing import List, Tuple, Optional

p_script = Path(__file__).absolute()
p_script_dir = p_script.parent
p_root = p_script_dir.parents[1]
p_zcbor_py = p_root / "zcbor" / "zcbor.py"
p_tests = p_root / "tests"
p_readme = p_root / "README.md"
p_pypi_readme = p_root / "pypi_README.md"
p_architecture = p_root / "ARCHITECTURE.md"
p_release_notes = p_root / "RELEASE_NOTES.md"
p_migration_guide = p_root / "MIGRATION_GUIDE.md"
p_add_helptext = p_root / "scripts" / "add_helptext.py"
p_regenerate_samples = p_root / "scripts" / "regenerate_samples.py"
p_hello_world_sample = p_root / "samples" / "hello_world"
p_hello_world_build = p_hello_world_sample / "build"
p_pet_sample = p_root / "samples" / "pet"
p_pet_cmake = p_pet_sample / "pet.cmake"
p_pet_include = p_pet_sample / "include"
p_pet_src = p_pet_sample / "src"
p_pet_build = p_pet_sample / "build"


DEFAULT_TARGET_REF = "main"
DEFAULT_TARGET_REPO = "nordicsemi/zcbor"


class GitRepoInfo:
    """Container for git repository information."""

    base_url: Optional[str] = None
    affected_files: Optional[List[Path]] = None
    current_branch: Optional[str] = None
    _instance = None
    _initialized = False

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super(GitRepoInfo, cls).__new__(cls)
        return cls._instance

    @staticmethod
    def _get_remote_tracking_branch() -> Optional[str]:
        """Get the remote tracking branch name."""
        try:
            remote_tr_args = ["git", "rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{u}"]
            result = run(remote_tr_args, capture_output=True)
            return result.stdout.decode("utf-8").strip() if result.returncode == 0 else None
        except CalledProcessError:
            return None

    @staticmethod
    def _get_repo_url(remote: str) -> Optional[str]:
        """Get repository URL for a given remote."""
        try:
            repo_url_args = ["git", "remote", "get-url", remote]
            repo_url = check_output(repo_url_args).decode("utf-8").strip().strip(".git")
            return repo_url if "github.com" in repo_url else None
        except CalledProcessError:
            return None

    @staticmethod
    def _build_base_url_from_github_env() -> Optional[str]:
        """Build base URL from GitHub environment variables."""
        if "GITHUB_SHA" not in os.environ or "GITHUB_REPOSITORY" not in os.environ:
            return None

        repo = os.environ["GITHUB_REPOSITORY"]
        sha = os.environ["GITHUB_SHA"]
        return f"https://github.com/{repo}/blob/{sha}/"

    @staticmethod
    def _prepare_local_target_ref():
        """Prepare a local ref for calculating affected files."""
        # Setup target reference for comparison
        target_ref = os.environ.get("GITHUB_BASE_REF", DEFAULT_TARGET_REF)
        local_target_ref = "_zcbor_doc_test_target_ref"
        target_repo = os.environ.get("GITHUB_REPOSITORY", DEFAULT_TARGET_REPO)
        target_repo_url = f"https://github.com/{target_repo}.git"

        # Fetch target reference
        check_output(["git", "fetch", target_repo_url, f"{target_ref}:{local_target_ref}"])
        return local_target_ref

    @staticmethod
    def _get_affected_files(target_ref: str) -> List[Path]:
        """Get list of files affected in the current branch compared to target."""
        diff_args = ["git", "diff", "--name-only", f"{target_ref}..HEAD"]
        diff = check_output(diff_args).decode("utf-8").strip()
        return [Path(p_root, p).absolute() for p in diff.split("\n") if p]

    @classmethod
    def _build_base_url_from_remote_tracking(cls, remote_tracking: str) -> Optional[str]:
        """Build base URL from remote tracking branch."""
        remote, remote_branch = remote_tracking.split("/", 1)
        repo_url = cls._get_repo_url(remote)
        return f"{repo_url}/tree/{remote_branch}/" if repo_url else None

    @classmethod
    def _determine_base_url(cls) -> Optional[str]:
        """Determine the base URL for the repository."""
        # Try remote tracking branch first
        remote_tracking = cls._get_remote_tracking_branch()
        if remote_tracking:
            base_url = cls._build_base_url_from_remote_tracking(remote_tracking)
            if base_url:
                return base_url

        # Fall back to GitHub environment
        return cls._build_base_url_from_github_env()

    @staticmethod
    def _get_current_branch() -> str:
        """Get the current branch name."""
        return check_output(["git", "rev-parse", "--abbrev-ref", "HEAD"]).decode("utf-8").strip()

    def __init__(self):
        """Initialize repository information."""

        if self._initialized:
            return

        self.base_url = self._determine_base_url()
        self.affected_files = self._get_affected_files(self._prepare_local_target_ref())
        self.current_branch = self._get_current_branch()
        self._initialized = True


DEFAULT_BACKOFF_TIME = 20
DEFAULT_MAX_ATTEMPTS = 5


class LinkTester:
    """Test the links in the documentation files."""

    known_good_urls = {
        "https://github.com/nordicsemi/zcbor/issues",
        "https://github.com/NordicSemiconductor/zcbor/issues",
        "https://github.com/zephyrproject-rtos/zephyr",
        "https://en.wikipedia.org/wiki/CBOR",
        "https://zephyrproject.org/",
        "https://github.com/NordicSemiconductor/cddl-gen/issues",
        "https://docs.python.org/3/library/json.html",
        "https://pypi.org/project/PyYAML/",
        "https://github.com/zephyrproject-rtos/zephyr/blob/main/subsys/net/lib/lwm2m/lwm2m_rw_senml_cbor.c",
        "https://github.com/mcu-tools/mcuboot/blob/main/boot/boot_serial/src/boot_serial.c",
        "https://github.com/zephyrproject-rtos/zephyr/blob/main/modules/zcbor/Kconfig",
        "https://datatracker.ietf.org/doc/html/rfc9682",
        "https://datatracker.ietf.org/doc/html/rfc8610",
        "https://datatracker.ietf.org/doc/draft-ietf-suit-manifest/",
        "https://datatracker.ietf.org/doc/rfc8610/",
        "https://github.com/zephyrproject-rtos/zephyr/blob/main/subsys/mgmt/mcumgr/grp/img_mgmt/src/img_mgmt.c",
        "https://github.com/nrfconnect/sdk-nrfxlib/blob/main/nrf_rpc/nrf_rpc_cbor.c",
        "https://github.com/nordicsemi/zcbor",
        "https://github.com/NordicSemiconductor/zcbor",
        "https://pypi.org/project/cbor2/",
        "https://github.com/zephyrproject-rtos/zephyr/blob/v3.6.0/doc/releases/migration-guide-3.6.rst",
        "https://pypi.org/project/zcbor/",
        "https://docs.zephyrproject.org/latest/kconfig.html#CONFIG_ZCBOR",
        "https://datatracker.ietf.org/doc/html/rfc9165",
        "https://docs.zephyrproject.org/latest/getting_started/index.html",
        "https://datatracker.ietf.org/doc/html/rfc9090",
        "https://www.iana.org/assignments/cbor-tags/cbor-tags.xhtml",
        "https://github.com/nrfconnect/sdk-nrf/blob/main/subsys/mgmt/fmfu/src/fmfu_mgmt.c",
    }

    urls_never_check = {
        "https://en.wikipedia.org/wiki/CBOR",  # Returns 403 for runs on GitHub Actions
    }

    def __init__(self, *args, force_check_all=False, **kwargs):
        super(LinkTester, self).__init__()
        self.repo_info = GitRepoInfo()
        self.link_regex = compile(r"\[.*?\]\((?P<link>.*?)\)")
        self.check_all = ("release/" in self.repo_info.current_branch) or (force_check_all)

    @staticmethod
    def _get_relative_path(file_path: Path) -> str:
        """Get relative path for local link processing."""
        relative_path = str(file_path.relative_to(p_root).parent)
        return "" if relative_path == "." else relative_path + "/"

    def _process_local_link(self, link: str, file_path: Path, relative_path: str) -> str:
        """Process local links (anchors and relative file paths)."""
        if link.startswith("#"):
            if not self.repo_info.base_url:
                raise ValueError("Base URL required for anchor links")
            return f"{self.repo_info.base_url}{relative_path}{file_path.name}{link}"
        else:
            return relative_path + link

    def extract_links(self, file_path: Path) -> Tuple[List[str], List[str], List[str]]:
        """Extract URLs and local links from a file."""
        text = file_path.read_text(encoding="utf-8")
        matches = self.link_regex.findall(text)

        urls = []
        processed_links = []
        local_links = []

        relative_path = self._get_relative_path(file_path)

        for link in matches:
            if not link.startswith("https://"):
                processed_link = self._process_local_link(link, file_path, relative_path)
                if processed_link.startswith("https://"):
                    processed_links.append(processed_link)
                else:
                    local_links.append(processed_link)
            else:
                urls.append(link)

        return urls, processed_links, local_links

    def should_check_file(self, file_path: Path) -> bool:
        """Determine if a file should be checked for links."""
        return self.check_all or file_path.absolute() in self.repo_info.affected_files

    @staticmethod
    def _check_url_status(
        url: str, backoff_time: int = DEFAULT_BACKOFF_TIME, max_attempts: int = DEFAULT_MAX_ATTEMPTS
    ) -> Tuple[int, Optional[str]]:
        """Check the HTTP status code of a URL with retry logic."""
        for attempt in range(max_attempts):
            try:
                response = request.urlopen(url)
                return response.getcode(), response
            except HTTPError as e:
                if (
                    e.code == HTTPStatus.TOO_MANY_REQUESTS
                    and backoff_time > 0
                    and attempt < max_attempts - 1
                ):
                    sleep(backoff_time)
                    continue
                return e.code, None
            except URLError as e:
                return HTTPStatus.SERVICE_UNAVAILABLE, None
        return HTTPStatus.TOO_MANY_REQUESTS, None

    @classmethod
    def check_urls_async(cls, urls: set[str]) -> List[Tuple[str, int, Optional[str]]]:
        """Check multiple URLs asynchronously and return status codes."""
        with ThreadPoolExecutor() as executor:
            results = [(url, *executor.submit(cls._check_url_status, url).result()) for url in urls]

        return results


class TestCodestyle(TestCase):
    def test_codestyle(self):
        black_res = Popen(["black", "--check", p_root, "-l", "105"], stdout=PIPE, stderr=PIPE)
        _, stderr = black_res.communicate()
        self.assertEqual(0, black_res.returncode, "black failed:\n" + stderr.decode("utf-8"))


class TestUnusedFunctions(TestCase):
    def do_test_unused_functions(self, file_path: Path) -> None:
        """Check for unused functions in a given file."""
        contents = file_path.read_text(encoding="utf-8")
        func_pattern = compile(r"def\s+(?P<func_name>\w+)\(")
        funcs = func_pattern.findall(contents)
        unused = [
            func
            for func in funcs
            if (not func.startswith("__")) and len(compile(rf"\W{func}\W").findall(contents)) < 2
        ]
        if len(unused) > 0:
            self.fail(f"Unused functions in {file_path}:\n" + "\n".join(unused))

    def test_unused_functions(self):
        for file_path in [p_zcbor_py]:
            self.do_test_unused_functions(file_path)


def version_int(in_str):
    return int(search(r"\A\d+", in_str)[0])  # e.g. '0rc' -> '0'


class TestSamples(TestCase):
    def popen_test(self, args, input="", exp_retcode=0, **kwargs):
        call0 = Popen(args, stdin=PIPE, stdout=PIPE, stderr=PIPE, **kwargs)
        stdout0, stderr0 = call0.communicate(input)
        self.assertEqual(exp_retcode, call0.returncode, stderr0.decode("utf-8"))
        return stdout0, stderr0

    def cmake_build_run(self, path, build_path):
        if build_path.exists():
            rmtree(build_path)
        with open(path / "README.md", "r", encoding="utf-8") as f:
            contents = f.read()

        to_build_patt = r"### To build:.*?```(?P<to_build>.*?)```"
        to_run_patt = r"### To run:.*?```(?P<to_run>.*?)```"
        exp_out_patt = r"### Expected output:.*?(?P<exp_out>(\n>[^\n]*)+)"
        to_build = search(to_build_patt, contents, flags=S)["to_build"].strip()
        to_run = search(to_run_patt, contents, flags=S)["to_run"].strip()
        exp_out = search(exp_out_patt, contents, flags=S)["exp_out"].replace("\n> ", "\n").strip()

        os.chdir(path)
        commands_build = [(line.split(" ")) for line in to_build.split("\n")]
        assert "\n" not in to_run, "The 'to run' section should only have one command."
        commands_run = to_run.split(" ")
        for c in commands_build:
            self.popen_test(c)
        output_run = ""
        for c in commands_run:
            output, _ = self.popen_test(c)
            output_run += output.decode("utf-8")
        self.assertEqual(exp_out, output_run.strip())

    @skipIf(platform.startswith("win"), "Skip on Windows because requires a Unix shell.")
    def test_hello_world(self):
        self.cmake_build_run(p_hello_world_sample, p_hello_world_build)

    @skipIf(platform.startswith("win"), "Skip on Windows because requires a Unix shell.")
    def test_pet(self):
        self.cmake_build_run(p_pet_sample, p_pet_build)

    def test_pet_regenerate(self):
        """Check the zcbor-generated code for the "pet" sample"""
        regenerate = Popen(["python3", p_regenerate_samples, "--check"])
        regenerate.communicate()
        self.assertEqual(0, regenerate.returncode)

    def test_pet_file_header(self):
        files = list(p_pet_include.iterdir()) + list(p_pet_src.iterdir()) + [p_pet_cmake]
        for p in [f for f in files if "pet" in f.name]:
            with p.open("r", encoding="utf-8") as f:
                f.readline()  # discard
                self.assertEqual(
                    f.readline().strip(" *#\n"),
                    "Copyright (c) 2022 Nordic Semiconductor ASA",
                )
                f.readline()  # discard
                self.assertEqual(f.readline().strip(" *#\n"), "SPDX-License-Identifier: Apache-2.0")
                f.readline()  # discard
                self.assertIn("Generated using zcbor version", f.readline())
                self.assertIn("https://github.com/nordicsemi/zcbor", f.readline())


class TestDocs(TestCase, LinkTester):
    """Run tests on the documentation in the repo."""

    files_to_check = [
        (p_readme, True),
        (p_architecture, True),
        (p_release_notes, True),
        (p_migration_guide, True),
        (p_hello_world_sample / "README.md", True),
        (p_pet_sample / "README.md", True),
        (p_pypi_readme, False),
    ]

    def __init__(self, *args, **kwargs):
        TestCase.__init__(self, *args, **kwargs)
        LinkTester.__init__(self, *args, **kwargs)

    def _validate_local_links(self, local_links: List[str], processed_links, allow_local: bool) -> None:
        """Validate that local file links exist."""
        all_locals = local_links + processed_links
        self.assertTrue(
            allow_local or not all_locals,
            f"Local link(s) ({all_locals}) found (not allowed).",
        )

        for link in local_links:
            resolved_link = (p_root / link).resolve()
            self.assertTrue(p_root in resolved_link.parents, f"Link '{link}' is outside the repo.")
            self.assertTrue(resolved_link.exists(), f"Local file '{link}' does not exist")

    def _check_results(self, results: List[Tuple]) -> None:
        """Check the results of URL checks."""
        for url, status_code, response in results:
            self.assertEqual(status_code, HTTPStatus.OK, f"'{url}' returned status code {status_code}")
            if "#" in url and "https://docs.zephyrproject.org/latest/kconfig.html" not in url:
                # https://docs.zephyrproject.org/latest/kconfig.html works in a different way
                anchor = url.split("#", 1)[1]
                html_content = response.read().decode("utf-8")
                self.assertTrue(anchor in html_content, f"Anchor '{anchor}' not found at '{url}'")

    def _validate_urls(self, urls: set[str]) -> None:
        """Validate that URLs return successful status codes."""
        if not urls:
            return

        self.assertTrue(all(u.startswith("https://") for u in urls), "All URLs must be https")

        check_urls = urls - self.known_good_urls

        inv_urls = list(u for u in check_urls if not self.repo_info.base_url in u or "#" not in u)
        self.assertFalse(inv_urls, "URLs not added to known good list:\n" + "\n".join(inv_urls))

        results = self.check_urls_async(check_urls)
        self._check_results(results)

    def do_test_doc_links(self, files_to_parse) -> None:
        """Main method to test documentation links."""
        # Collate URLs from all files to avoid duplicates
        all_urls = set()

        for file_path, allow_local in files_to_parse:
            if self.should_check_file(file_path):
                urls, processed_links, local_links = self.extract_links(file_path)
                self._validate_local_links(local_links, processed_links, allow_local)
                all_urls.update(urls + processed_links)
            else:
                print(f"Skipping link checking of {file_path} (not changed in this PR)")

        self._validate_urls(all_urls)

    def test_doc_links(self) -> None:
        self.do_test_doc_links(self.files_to_check)

    def test_doc_links_fail(self) -> None:
        if not p_script in self.repo_info.affected_files:
            self.skipTest("Skipping doc link negative tests.")

        def assertAssert(func, *args, **kwargs):
            self.assertRaises(AssertionError, func, *args, **kwargs)

        check_all = self.check_all
        self.check_all = True  # Force checking all files
        p_doc_fail_cases = p_script_dir / "doc_fail_cases"

        assertAssert(self.do_test_doc_links, [(p_readme, False)])
        assertAssert(self.do_test_doc_links, [(p_release_notes, False)])
        assertAssert(self.do_test_doc_links, [(p_doc_fail_cases / "fail1.md", True)])
        assertAssert(self.do_test_doc_links, [(p_doc_fail_cases / "fail2.md", True)])
        assertAssert(self.do_test_doc_links, [(p_doc_fail_cases / "fail3.md", True)])
        with patch("urllib.request.urlopen", side_effect=URLError("Could not connect to server")):
            results = self.check_urls_async({"https://google.com"})
        assertAssert(self._check_results, results)

        self.check_all = check_all  # Restore original check_all state

    def test_known_good_urls(self) -> None:
        if not self.check_all:
            self.skipTest("Skipping known good URL tests (not on main or release branch).")
        ext_urls = (sum(self.extract_links(fp)[0:2], []) for fp, _ in self.files_to_check)
        all_urls = set(sum(ext_urls, []))
        unused_urls = self.known_good_urls - all_urls
        self.assertFalse(unused_urls, "Known good URLs not in the docs:\n" + "\n".join(unused_urls))

        results = self.check_urls_async(self.known_good_urls - self.urls_never_check)
        self._check_results(results)

    @skipIf(platform.startswith("win"), "Skip on Windows because of path/newline issues.")
    def test_cli_doc(self):
        """Check the auto-generated CLI docs in the top level README.md file."""
        add_help = Popen(["python3", p_add_helptext, "--check"])
        add_help.communicate()
        self.assertEqual(0, add_help.returncode)


if __name__ == "__main__":
    main()
