#!/usr/bin/env python3
"""Fetch OpenSprinkler UI source and assemble its web assets into one HTML file."""

import argparse
import base64
import io
import json
import mimetypes
import posixpath
import re
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path


UI_REPOSITORY = "OpenSprinkler/OpenSprinkler-App"
PINNED_UI_COMMIT = "0af653d87d254c61bdeb10e4d23a4dabb2ef5b3f"
GITHUB_API_URL = "https://api.github.com/repos/" + UI_REPOSITORY + "/commits?per_page=1"
ARCHIVE_URL_TEMPLATE = "https://codeload.github.com/" + UI_REPOSITORY + "/zip/{commit}"
DEFAULT_OUTPUT = Path("html/offline_ui.html")
DEFAULT_DEFINES = Path("defines.h")
DEFAULT_ARCHIVE_CACHE_DIR = Path(".pio/ui-cache")
HTML_SCRIPT_RE = re.compile(r"<script\b(?P<attrs>[^>]*)>(?P<body>.*?)</script\s*>", re.I | re.S)
HTML_LINK_RE = re.compile(r"<link\b(?P<attrs>[^>]*)/?>\s*(?:</link\s*>)?", re.I | re.S)
ATTR_RE = re.compile(r"""([^\s=]+)\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+))""")
CSS_URL_RE = re.compile(r"""url\(\s*(?:"([^"]*)"|'([^']*)'|([^)]*?))\s*\)""", re.I)
CSS_IMPORT_RE = re.compile(
    r"""@import\s+(?:url\(\s*)?(?:"([^"]+)"|'([^']+)'|([^;\s)]+))\s*\)?\s*;""",
    re.I,
)
LANGUAGE_LOAD_RE = re.compile(
    r"""\$\.getJSON\(\s*OSApp\.UIDom\.getAppURLPath\(\)\s*\+\s*"locale/"\s*"""
    r"""\+\s*lang\s*\+\s*"\.js"\s*,\s*function\s*\(\s*store\s*\)\s*\{"""
    r"""(?P<body>.*?)\}\s*\)\s*\.fail\(\s*OSApp\.Language\.setLang\s*\)\s*;""",
    re.S,
)
SERVICE_WORKER_RE = re.compile(
    r"""\tif\s*\(\s*"serviceWorker"\s+in\s+navigator\s*\)\s*\{"""
    r""".*?navigator\.serviceWorker\.register\(\s*"/sw\.js"\s*\)\s*;"""
    r""".*?\n\t\}\s*\n""",
    re.S,
)


def parse_attributes(text):
    return {
        match.group(1).lower(): next(
            value for value in match.groups()[1:] if value is not None
        )
        for match in ATTR_RE.finditer(text)
    }


class OfflineBundle:
    def __init__(self, files):
        self.files = files
        self.css_cache = {}
        self.asset_count = 0

    def read_text(self, path):
        try:
            return self.files[path].decode("utf-8")
        except KeyError as exc:
            raise RuntimeError("Pinned UI archive is missing " + path) from exc

    def resolve_path(self, reference, base_path):
        parsed = urllib.parse.urlsplit(reference)
        if parsed.scheme or parsed.netloc:
            if parsed.scheme == "data":
                return None
            raise RuntimeError("External UI resource is not bundleable: " + reference)

        path = urllib.parse.unquote(parsed.path)
        if not path or path.startswith("#"):
            return None
        if path.startswith("/"):
            path = path[1:]
        else:
            path = posixpath.join(posixpath.dirname(base_path), path)

        normalized = posixpath.normpath(path)
        if normalized == ".." or normalized.startswith("../"):
            raise RuntimeError("UI resource escapes its web root: " + reference)
        return normalized

    def data_uri(self, path):
        try:
            content = self.files[path]
        except KeyError as exc:
            raise RuntimeError("Pinned UI archive is missing referenced asset " + path) from exc
        mime_type = mimetypes.guess_type(path)[0] or "application/octet-stream"
        encoded = base64.b64encode(content).decode("ascii")
        self.asset_count += 1
        return "data:" + mime_type + ";base64," + encoded

    def inline_css(self, path, import_stack=()):
        if path in import_stack:
            raise RuntimeError("Circular CSS import detected: " + path)
        if path in self.css_cache:
            return self.css_cache[path]

        css = self.read_text(path)

        def inline_import(match):
            reference = next(value for value in match.groups() if value is not None)
            imported_path = self.resolve_path(reference, path)
            if imported_path is None:
                raise RuntimeError("Invalid CSS @import in " + path + ": " + reference)
            return self.inline_css(imported_path, import_stack + (path,))

        css = CSS_IMPORT_RE.sub(inline_import, css)

        def inline_url(match):
            reference = next(value for value in match.groups() if value is not None).strip()
            if reference.startswith(("data:", "#")):
                return match.group(0)
            asset_path = self.resolve_path(reference, path)
            if asset_path is None:
                return match.group(0)
            uri = self.data_uri(asset_path)
            return "url(" + json.dumps(uri) + ")"

        css = CSS_URL_RE.sub(inline_url, css)
        self.css_cache[path] = css
        return css

    def inline_page(self, html):
        html = re.sub(
            r"<script\b(?=[^>]*\bsrc\s*=\s*(['\"]?)cordova\.js\1)[^>]*>.*?</script\s*>",
            "",
            html,
            flags=re.I | re.S,
        )

        def inline_link(match):
            attrs = parse_attributes(match.group("attrs"))
            reference = attrs.get("href")
            rel = attrs.get("rel", "").lower().split()
            if "manifest" in rel:
                return ""
            if not reference:
                raise RuntimeError("UI link tag has no href")
            if reference.startswith("https://fonts.googleapis.com/"):
                print("Using system font fallback instead of fetching Google Fonts.")
                return ""
            if "stylesheet" in rel:
                css_path = self.resolve_path(reference, "index.html")
                if css_path is None:
                    raise RuntimeError("Invalid stylesheet reference: " + reference)
                return "<style>\n" + self.inline_css(css_path) + "\n</style>"
            if "icon" in rel or "apple-touch-icon" in rel:
                asset_path = self.resolve_path(reference, "index.html")
                if asset_path is None:
                    raise RuntimeError("Invalid icon reference: " + reference)
                return '<link rel="icon" href="' + self.data_uri(asset_path) + '">'
            raise RuntimeError("Unsupported UI link in index.html: " + reference)

        html = HTML_LINK_RE.sub(inline_link, html)

        def inline_script(match):
            attrs = parse_attributes(match.group("attrs"))
            src = attrs.get("src")
            if not src:
                return match.group(0)
            script_path = self.resolve_path(src, "index.html")
            if script_path is None:
                raise RuntimeError("Invalid script reference: " + src)
            script = self.read_text(script_path)
            script = self.patch_script(script_path, script)
            script = re.sub(r"</script", r"<\\/script", script, flags=re.I)
            return "<script>\n" + script + "\n</script>"

        html = HTML_SCRIPT_RE.sub(inline_script, html)
        html = self.embed_locales(html)
        return html

    def patch_script(self, path, script):
        if path.endswith("modules/language.js"):
            script, count = LANGUAGE_LOAD_RE.subn(
                """var store = window.__OSAPP_OFFLINE_LOCALES[ lang ];
\n\tif ( store ) {
\t\tOSApp.uiState.language = store.messages;
\t}
\tOSApp.Language.setLang();""",
                script,
            )
            if count != 1:
                raise RuntimeError("Could not patch locale loader in " + path)

        if path.endswith("modules/ui-dom.js"):
            script, count = SERVICE_WORKER_RE.subn("", script)
            if count != 1:
                raise RuntimeError("Could not disable service worker registration in " + path)
            url_path_re = re.compile(
                r"""OSApp\.UIDom\.getAppURLPath\s*=\s*function\s*\(\s*\)\s*\{.*?\n\};""",
                re.S,
            )
            script, count = url_path_re.subn(
                "OSApp.UIDom.getAppURLPath = function() { return ''; };", script
            )
            if count != 1:
                raise RuntimeError("Could not patch app asset path in " + path)

        if path.endswith("modules/analog.js"):
            image_uri = json.dumps(self.data_uri("img/check-black.png"))
            script, count = re.subn(
                r'OSApp\.UIDom\.getAppURLPath\(\)\s*\+\s*"img/check-black\.png',
                image_uri,
                script,
            )
            if count != 1:
                raise RuntimeError("Could not inline check icon in " + path + " (matches: " + str(count) + ")")

        if path.endswith("modules/dashboard.js"):
            image_uri = json.dumps(self.data_uri("img/placeholder.png"))
            script, count = re.subn(
                r'OSApp\.UIDom\.getAppURLPath\(\)\s*\+\s*"img/placeholder\.png"',
                image_uri,
                script,
            )
            if count != 2:
                raise RuntimeError("Could not inline placeholder images in " + path)
        return script

    def embed_locales(self, html):
        locales = {}
        prefix = "locale/"
        for path, content in self.files.items():
            if not path.startswith(prefix) or not path.endswith(".js"):
                continue
            language = posixpath.basename(path)[:-3]
            try:
                locales[language] = json.loads(content.decode("utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError) as exc:
                raise RuntimeError("Invalid localization file in pinned UI: " + path) from exc

        if not locales:
            raise RuntimeError("Pinned UI archive contains no localization files")
        payload = json.dumps(locales, ensure_ascii=True, separators=(",", ":"))
        if "</script" in payload.lower():
            raise RuntimeError("Localization data contains an unsafe script-closing sequence")
        bootstrap = (
            '<script>window.__OSAPP_OFFLINE_LOCALES='
            + payload
            + ";</script>\n"
            '<script id="os-firmware-bootstrap">'
            "var query = window.location.search;"
            "var verMatch = query.match(/[?&]ver=(\\d+)/);"
            "var ipasMatch = query.match(/[?&]ipas=(\\d+)/);"
            "var ver = verMatch ? Number(verMatch[1]) : 0;"
            "var ipas = ipasMatch ? Number(ipasMatch[1]) : 0;"
            "</script>\n"
        )
        first_script = re.search(r"<script\b", html, re.I)
        if not first_script:
            raise RuntimeError("index.html contains no script tags")
        return html[: first_script.start()] + bootstrap + html[first_script.start() :]


def read_default_ui_url(defines_path):
    source = defines_path.read_text(encoding="utf-8")
    match = re.search(
        r'^\s*#\s*define\s+DEFAULT_JAVASCRIPT_URL\s+"([^"]*)"',
        source,
        flags=re.M,
    )
    if not match:
        raise RuntimeError("Could not find DEFAULT_JAVASCRIPT_URL in " + str(defines_path))
    return match.group(1)


def fetch_latest_commit():
    request = urllib.request.Request(
        GITHUB_API_URL,
        headers={
            "Accept": "application/vnd.github+json",
            "User-Agent": "OpenSprinkler-Firmware-UI-Bundler",
        },
    )
    with urllib.request.urlopen(request, timeout=30) as response:
        try:
            commits = json.loads(response.read().decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise RuntimeError("GitHub returned invalid commit metadata") from exc

    if not isinstance(commits, list) or not commits:
        raise RuntimeError("GitHub returned no commits for " + UI_REPOSITORY)
    commit = commits[0].get("sha") if isinstance(commits[0], dict) else None
    if not isinstance(commit, str) or not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise RuntimeError("GitHub returned an invalid latest commit SHA")
    return commit


def read_archive_cache(cache_path, commit, rebuild):
    if cache_path.is_file() and not rebuild:
        return cache_path.read_bytes()

    request = urllib.request.Request(
        ARCHIVE_URL_TEMPLATE.format(commit=commit),
        headers={"User-Agent": "OpenSprinkler-Firmware-UI-Bundler"},
    )
    with urllib.request.urlopen(request, timeout=60) as response:
        archive_data = response.read()

    validate_archive(archive_data, commit)
    cache_path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        mode="wb", dir=cache_path.parent, suffix=".tmp", delete=False
    ) as archive_file:
        temporary_path = Path(archive_file.name)
        archive_file.write(archive_data)
    temporary_path.replace(cache_path)
    return archive_data


def validate_archive(archive_data, commit):
    try:
        archive = zipfile.ZipFile(io.BytesIO(archive_data))
        expected_prefix = "OpenSprinkler-App-" + commit + "/www/index.html"
        if expected_prefix not in archive.namelist():
            raise RuntimeError("OpenSprinkler UI archive does not contain the expected web app")
    except zipfile.BadZipFile as exc:
        raise RuntimeError("Downloaded OpenSprinkler UI archive is not a valid zip file") from exc


def download_archive(cache_path, commit, rebuild):
    archive_data = read_archive_cache(cache_path, commit, rebuild)
    archive = zipfile.ZipFile(io.BytesIO(archive_data))
    prefix = "OpenSprinkler-App-" + commit + "/www/"
    license_path = "OpenSprinkler-App-" + commit + "/LICENSE"
    files = {}
    for entry in archive.infolist():
        if entry.filename == license_path:
            files["LICENSE"] = archive.read(entry)
            continue
        if entry.is_dir() or not entry.filename.startswith(prefix):
            continue
        relative_path = entry.filename[len(prefix) :]
        if relative_path:
            files[relative_path] = archive.read(entry)
    archive.close()
    if "index.html" not in files or "LICENSE" not in files:
        raise RuntimeError("OpenSprinkler UI archive does not contain its web app and license")
    return files


def create_bundle(output_path, cache_path, commit, rebuild):
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise RuntimeError("UI commit must be a full 40-character Git commit SHA")
    files = download_archive(cache_path, commit, rebuild)
    bundle = OfflineBundle(files)
    html = bundle.inline_page(bundle.read_text("index.html"))

    license_text = files["LICENSE"].decode("utf-8")
    license_markup = (
        '<template id="opensprinkler-app-license"><pre>'
        + license_text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
        + "</pre></template>"
    )
    html = re.sub(r"</body\s*>", license_markup + "\n</body>", html, count=1, flags=re.I)
    if not re.search(r"</body\s*>", html, re.I):
        raise RuntimeError("index.html has no closing body tag")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8", newline="\n") as output_file:
        output_file.write(html)
    print("Bundled OpenSprinkler-App commit " + commit)
    print("Inlined local scripts, styles, CSS assets, icons, and translations.")
    print("Embedded " + str(bundle.asset_count) + " binary asset references.")
    print("Generated " + str(output_path) + " (" + str(len(html.encode("utf-8"))) + " bytes).")
    print("Firmware version and password-mode values are read from the serving URL query.")
    print("The separate map.html uses Google Maps and remains an online-only feature.")


def remove_stale_bundle(output_path):
    if output_path.exists():
        output_path.unlink()
        print("Removed stale offline UI bundle: " + str(output_path))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help="generated one-page HTML path (default: html/offline_ui.html)",
    )
    parser.add_argument(
        "--defines",
        type=Path,
        default=DEFAULT_DEFINES,
        help="firmware defines.h file containing DEFAULT_JAVASCRIPT_URL",
    )
    parser.add_argument(
        "--archive-cache",
        type=Path,
        help="override the commit-specific archive cache path",
    )
    parser.add_argument(
        "--rebuild",
        action="store_true",
        help="fetch the latest GitHub commit and rebuild the bundle",
    )
    args = parser.parse_args()
    try:
        commit = fetch_latest_commit() if args.rebuild else PINNED_UI_COMMIT
        cache_path = args.archive_cache or (
            DEFAULT_ARCHIVE_CACHE_DIR / ("OpenSprinkler-App-" + commit + ".zip")
        )
        print("Using OpenSprinkler-App commit " + commit)
        ui_url = read_default_ui_url(args.defines)
        if not ui_url.startswith("/app"):
            if args.rebuild:
                read_archive_cache(cache_path, commit, rebuild=True)
            remove_stale_bundle(args.output)
            print(
                "Skipping offline UI bundle: DEFAULT_JAVASCRIPT_URL does not start with /app."
            )
            return 0

        create_bundle(args.output, cache_path, commit, args.rebuild)
    except (OSError, RuntimeError, urllib.error.URLError, zipfile.BadZipFile) as exc:
        print("Offline UI bundle failed: " + str(exc), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
