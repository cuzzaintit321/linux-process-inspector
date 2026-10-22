# Linux Process Inspector

![linux-process-inspector project banner](docs/project-banner.webp)

A read-only **C11** command-line app that reads Linux `/proc` process status files, sorts processes by resident memory/PID/name, filters by name or UID, and prints a table or JSON.

Built as a new learning and portfolio project for Jason Edwards. It does not claim production deployment or employer use. The app itself is C; a small Python standard-library test harness exercises the compiled binary.

## Build and run

Requires Linux, a C compiler, and Make. On Fedora:

```bash
sudo dnf install -y gcc make
make
./build/process-inspector
```

No Python runtime is needed to use the app. Python 3 is used only by the CLI tests. No network connections, extra C libraries, root privileges, or package-manager installs are performed by the app.

```bash
./build/process-inspector --sort rss --limit 15
./build/process-inspector --sort pid --uid "$(id -u)"
./build/process-inspector --name firefox --sort name
./build/process-inspector --json --limit 10
```

Name matching is a case-sensitive substring match against `/proc/PID/status`'s **Name** field. That field is the kernel process name, not the full command line, and can be truncated by the kernel. UID refers to the first (real) UID reported in the status file.

## Output

| Field | Meaning |
| --- | --- |
| PID | Numeric process ID |
| UID | Real numeric user ID |
| Name | Kernel process name |
| State | Kernel state text, such as sleeping or running |
| RSS KiB | Resident memory from `VmRSS`; `-`/JSON null if absent |

Default sorting is RSS descending, with PID as a deterministic tie breaker. Processes without RSS data sort last. `--sort pid` sorts numerically ascending; `--sort name` sorts bytewise ascending. `--limit` accepts 1–10,000 rows. A nonexistent filter match is a valid empty result.

This is a **one-shot, best-effort snapshot**. Reading multiple process files is not atomic: processes may exit, change state, or reuse IDs during inspection. Inaccessible, missing, or malformed status files count as skipped. The summary reports matched/shown/skipped counts and whether a safety cap was reached. Skipped counts apply before filters, so unrelated inaccessible processes can make a filtered check exit 1.

| Exit | Meaning |
| --- | --- |
| 0 | Enumeration completed without skipped numeric entries or scan cap |
| 1 | Output produced, but some numeric entries were skipped or scan cap reached |
| 2 | Invalid options, unreadable root, allocation/enumeration error, or detected output failure |

The scan is capped at 100,000 numeric entries; stored results grow dynamically with allocation checks. A process status line longer than the fixed 4 KiB buffer is rejected. `--proc-root PATH` allows controlled test fixtures or another mounted proc directory; use only trusted filesystem trees. The root/process directories and status files are opened with no-follow flags; only regular status files are read. There is no general hostile-filesystem sandbox or per-file deadline for arbitrary mounted filesystems.

## Accuracy and privacy

- RSS is resident memory, **not CPU utilization** and not unique physical memory. Shared pages can be counted in several processes, and kernel accounting is approximate.
- Kernel threads can lack `VmRSS`; missing values are not converted to fake zeroes.
- The app does not read command lines, environment variables, open-file lists, or process memory. It never sends signals or modifies processes.
- Visibility depends on permissions, namespaces, and proc mount settings. Do not run it as root merely to get a longer list.
- Terminal output replaces non-ASCII/control bytes with `?`, preventing process names from injecting terminal escape sequences. JSON escapes non-ASCII proc-name bytes as individual Latin-1 Unicode code points. ASCII names round-trip normally; this preserves byte values rather than decoding arbitrary names as UTF-8.
- JSON is written to stdout. Process names and UIDs can still reveal private information; review before sharing. Put local captures in ignored `reports/` and use a private umask/new filename if saving them. Shell redirection is separate from the app and can overwrite files.

## Tests and sanitizers

```bash
make test
make sanitize
```

The native C assertion suite covers numeric overflow/invalid input, status parsing, missing RSS, malformed fields, deterministic sorting, and JSON escaping. Eight Python-driven CLI tests cover fixture enumeration, filters, limits, sort modes, missing processes, symlink refusal, terminal/JSON escaping, invalid options, malformed status files, and a missing proc root. Tests use temporary synthetic process trees.

Build flags enable C11, optimization, `-Wall -Wextra -Wpedantic`, and `-Werror`. The sanitizer target rebuilds with AddressSanitizer and UndefinedBehaviorSanitizer, then runs the same tests. In restricted/ptraced environments LeakSanitizer may fail before reporting application findings; there, `ASAN_OPTIONS=detect_leaks=0 make sanitize` runs address/undefined checks without leak detection. This limitation occurred in the preparation environment, so no successful leak-detection claim is made.

After sanitizer testing, `make clean all` restores the ordinary optimized build. GitHub Actions includes both test targets; check the actual workflow result after publishing. Sanitizers and fixture tests improve confidence but do not prove freedom from all C memory errors.

## Publish to GitHub

With the GitHub CLI signed in:

```bash
bash publish.sh
```

This creates public repository `cuzzaintit321/linux-process-inspector`, sets its description, commits source/tests/docs/workflow, and pushes. Built binaries, reports, and local config are ignored. It stops when a local checkout or remote already exists; inspect partial publication before retrying. No software license has been selected on your behalf.
