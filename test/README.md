# Test infrastructure

Seven files here do not ship in the public GitHub mirror. Five read `configs/boards/*.conf` directly: `test/config-sweep.sh`, `test/build-matrix.sh`, `test/misc-ioctl-guard.sh`, `scripts/support-list.sh` and `test/support-list-check.sh`. Two more go with them: `test/misc-ioctl-guard-harness.c` (the harness `test/misc-ioctl-guard.sh` builds) and `test/publish-github-check.sh` (the test for the mirror script itself). Board configuration files are supplied with each board, not bundled with every copy of this tree, so a copy without `configs/boards/` — the public GitHub mirror, for example — does not carry these seven files either. See "Publish-to-GitHub guard check" below for the script that builds that copy. Five more files, unrelated to board configs, are also stripped from the mirror: `docs/push.sh`, `docs/README.md`, `test/push-check.sh`, `docs/publish-local-wiki.sh` and `test/publish-local-wiki-check.sh` — see "Wiki publish guard check" below. (`.gitlab-ci.yml`, which calls the local-wiki publisher, is stripped with the other dev-only dotfiles.)

## Building a real kernel 4.15 tree

`/kernels/linux-4.15.18` on this build host is an incomplete dev tree: a
plain `make` against it stops before the compiler runs, and the tree is also
missing `scripts/mod/modpost` and `tools/objtool/objtool`. That is our host
tree being incomplete, not a fact about the 4.15 kernel line -- see
`README.md`'s Supported Kernels section for what a complete 4.15 tree
actually measures.

`test/build-matrix.sh` carries this tree's break as a committed exception in
`EXPECTED_FAIL_KERNELS`, so a run does not fail because of it: the row still
prints, graded `FAIL (expected)`, and the run exits 0. The same list is
honoured by every grading site the script has -- the `$KERNELS_DIR` loop,
the `$KERNELS_CACHE_DIR` loop, and the HAL source-file coverage pass -- not
only the `/kernels/linux-4.15.18` case this page describes. `EXCLUDE_KERNELS`
is a different, per-run acknowledgement (set by the caller, not committed)
that excuses a tree's failure or absence for one run only; where a tree is
named in both, `EXCLUDE_KERNELS` wins.

To get a complete 4.15 tree, fetch Ubuntu 18.04's own kernel packages
(still served from `archive.ubuntu.com`):

- `linux-headers-4.15.0-101_4.15.0-101.102_all.deb`
- `linux-headers-4.15.0-101-generic_4.15.0-101.102_amd64.deb`

Extract both into the same directory with `dpkg-deb -x <pkg> <dir>` (no
install needed -- both unpack under `usr/src/`). The result carries real
`objtool`, `modpost`, `include/generated`, and its own `Module.symvers` --
none of the dev trees under `/kernels` have that last file.

`test/build-matrix.sh` picks up a tree prepared this way automatically.
Place it under `$KERNELS_CACHE_DIR` (default `/kernels-cache`), laid out as
`<name>/usr/src/linux-headers-*-generic/` -- the exact directory name
(`<name>`) does not matter, and the exact suffix on `linux-headers-*-generic`
(e.g. `-101`) is globbed, not hard-coded. See the script's own header comment
for why this is a second, separate source of trees from `$KERNELS_DIR`, and
how a tree found there is graded differently.

A `/kernels-cache` tree that is missing or not prepared now fails
`test/build-matrix.sh` instead of being skipped quietly -- see the script's
own header comment for the required-tree list this applies to.

A bare `make` (every subsystem the board declares, in one run) fails on 4.15
for a reason in that kernel's own build system (a `KBUILD_MODNAME` conflict
in a file the subsystems share) -- on this kernel line, build one subsystem
at a time instead (`make watchdog`, `make gpio`, `make hwmon`, `make misc`).

## HAL source-file coverage pass

Beyond the kernel matrix above, `test/build-matrix.sh` runs a second pass
that builds one representative board per distinct HAL "shape" -- the exact
set of `src/hal/` files a board's `.conf` selects -- so every HAL `.c` file
any board can reach is compiled at least once, not just the ones the two
kernel-matrix boards happen to use. `COVERAGE_KERNEL` (default
`linux-5.4.302`) names the tree under `$KERNELS_DIR` this pass builds on.
`COVERAGE_BOARDS` overrides the derived shape representatives with an
explicit, space-separated board list -- useful for testing the pass itself,
or for reproducing a narrower run's blind spot on purpose. A HAL source file
that no build in the whole run (this pass or the kernel matrix above)
compiles fails the run; see the script's own header comment for the full
rule.

## Board file sweep (scripts/config.sh over every board)

`test/config-sweep.sh` runs `scripts/config.sh`'s own guard -- the
presence-and-count check on each subsystem's `_NUM`/`_MAP` key pair -- over
every board file, not just the ones something else happens to build.
`scripts/config.sh` already stops a real build before the compiler runs when
a board declares a subsystem (its `MAKE_*_DEVICE`/`MAKE_*_CHIPSET` pair) but
leaves a `_NUM` or `_MAP` key unset, or sets a `_NUM` that disagrees with its
own `_MAP`'s element count -- both would otherwise surface 200 lines into the
compiler as "excess elements in array initializer". The sweep just widens
who gets that check: `test/build-matrix.sh` builds a handful of representative
board files, one per kernel and one per HAL shape; the sweep reads every
board file under `configs/boards/` and hands each to `scripts/config.sh` in
turn, so a board nobody's build happens to reach still gets graded before it
ships to a customer.

A `_MAP` value that does not parse as a `{ ... }` list -- a typo, a missing
brace, any shape this counter has not been taught -- is named by board and
key and counted as **not gradable** in the sweep's own summary line, rather
than being silently skipped or treated as a hard failure.

It compiles nothing and needs no kernel tree. Each board's generated header
goes into a `mktemp -d` scratch directory removed when the run ends, so the
committed tree -- including `src/configs/board.h` -- is never touched;
`scripts/config.sh` alone decides pass or fail. A run prints how many board
names it read, how many distinct files those names resolve to (a symlinked
board file counts once), and how many `_NUM`/`_MAP` pairs it actually
compared -- exit 0 with every board clean, or exit 1 naming every board that
failed.

`scripts/config.sh` also reports (never fails the build for) a voltage
channel whose `_LABEL` names a board input rail -- `VIN`, `VIN_L` or
`DCIN`, case-insensitively -- while its `_R1` and `_R2` are both `0`: with
no divider, the driver reports the raw ADC counts as if they were the
finished voltage (issue #75). This is scoped to those three labels only,
since plenty of other channels (`DIMM`, `Threm_*`, `VRTC`, `5V`, etc.)
legitimately carry `R1=R2=0` and must not be reported. A board file can
silence one channel's report deliberately with a comment line containing
`UNDIVIDED-ON-PURPOSE` placed immediately above that channel's own
`_ENABLE=` key, e.g. `# UNDIVIDED-ON-PURPOSE: measured directly, no
divider on this rail`. `test/config-sweep.sh` surfaces every `[CONFIG]:
Report:` line the same way it already surfaces `[CONFIG]: Note:` lines,
board by board, plus a sweep-wide total; `test/undivided-vin-check.sh`
proves the rule itself on a synthetic fixture (a divided channel stays
quiet, an undivided input-rail channel is reported, an annotated one goes
quiet again, and a non-input-rail label with `R1=R2=0` is never reported).

## Debug-goal scope check (`make <subsystem>-debug` must match its plain target)

`test/debug-goal-check.sh` proves that a `make <subsystem>-debug` goal (e.g.
`make hwmon-debug`) selects exactly the one subsystem its plain counterpart
does, with `CONFIG_DEBUG=y` the only difference, and that a board unable to
build that subsystem stops the debug goal the same way it stops the plain
one (same exit code, same message) rather than silently building every
subsystem the board declares instead.

For every `configs/boards/*.conf` board and each of the four subsystems
(`watchdog gpio hwmon misc`), it runs the plain goal as the oracle -- an
error if the board cannot build that subsystem, a `MAKE_*=[...]` echo line
with exactly that one subsystem `m` and the rest `n` otherwise -- and checks
the matching `-debug` goal against it: same exit code always; when the plain
goal succeeds, the same `MAKE_*` set plus `CONFIG_DEBUG` going from `[]` to
`[y]`; when it fails, byte-identical error text. It also checks, once, that
an unknown goal like `make wibble-debug` is refused by name, naming all four
valid subsystems, instead of quietly building the board's full declared set.
That is 110 boards x 4 subsystems x 2 goal shapes (plain and `-debug`), 440
board/goal pairs compared, plus the one unknown-goal case.

It needs no kernel tree and touches nothing committed: the whole repo
(`Makefile`, `configs/`, `scripts/`, `src/`) is copied into a `mktemp -d`
scratch directory once, and every `make` invocation runs against that copy
with `KERNEL_SOURCE` pointed at a scratch stand-in whose `modules` target
only echoes back the `MAKE_*` variables and `CONFIG_DEBUG` it was handed --
no compiler involved, and `src/configs/board.h` in the real checkout is
never touched.

## Misc ioctl dispatch guard (undefined commands must not reach hardware)

`test/misc-ioctl-guard.sh` builds the real `hal_misc_ioctl()` dispatch code
-- sed-extracted verbatim from `src/hal/ec/ite_misc.c`, not a hand copy --
against each misc-capable board's own generated `config.h`, with the EC bus
stubbed to record every register access. For every board that sets
`MAKE_MISC_DEVICE=ec`, it checks that command `0` -- which every board
reserves for "no command here", and which one board's disabled write
command also happens to equal -- is rejected with `-ENOTTY` and touches no
EC register, and that the board's own real read and write commands still
dispatch to the right register.

It compiles a small userspace harness per board and needs no kernel tree.
Each board's generated header goes into a `mktemp -d` scratch directory
removed when the run ends, so the committed tree is never touched.

## Wiki page claim check (docs/wiki/Linux-X86-API.md must stay true)

`test/wiki-page-check.sh` reads `docs/wiki/Linux-X86-API.md` (or the page
passed as its own `$1`, so a red-before run can point it at a scratch copy)
and pulls three kinds of claims straight out of the page's own text with
sed/awk -- not from a list copied into this script: every `/dev/<node>`
path, every `/sys/class/<...>/` path, and every `make <target>` command the
page names. `make <target>` claims are read only from the page's own code --
fenced ``` blocks and inline `...` spans -- so a plain-English sentence that
happens to contain the word "make" is never scanned; a bare `make` with no
target is graded as the implicit `all` target.

Each claim is checked against the source tree at run time, not a hard-coded
"expected paths" list: a `/dev/gpiochipN` claim needs
`src/drivers/gpio.c` to call `devm_gpiochip_add_data`; `/dev/watchdogN` and
`/sys/class/watchdog/` need `src/drivers/watchdog.c` to call
`watchdog_register_device`; `/dev/misc` needs `src/drivers/misc.c` to
register a `miscdevice` named `"misc"`; `/sys/class/hwmon/` needs
`src/drivers/hwmon.c` to call `hwmon_device_register_with_info` or
`_with_groups`. A `make <target>` claim is checked against the
Makefile's own `DRIVERS :=` line (plus each subsystem's `-debug` variant)
and the fixed lifecycle targets (`all`, `modules`, `clean`, `install`,
`uninstall`, `help`, `config`) -- also read from the Makefile at run time,
not copied in by hand. Any claim that fails is named individually, by kind;
the run does not stop at the first failure, and it fails a run that checked
zero claims.

`/sys/class/misc/<segment>/` claims are graded to the full path, not just
the class: `src/drivers/misc.c` has one real compile-time constant for this
-- the `miscdevice`'s own name, `"misc"` -- so a claim's `<segment>` must
equal that literal string, checked by `sysfs_create_group` also being
present. `hwmon`/`watchdog` stay class-level, because their device number
(`hwmon3`, `watchdog0`) is assigned at runtime and has no compile-time
string to check against. The page names exactly one `/sys/class/misc/`
path, in Section 4, so this extraction runs over the page's whole text with
no section carved out.

It compiles nothing and needs no kernel tree -- only the source tree and the
Makefile, both grepped and parsed as they stand. It proves the page's paths
and make targets are real; it does not check that a command's example
output or the prose around a claim is accurate, and it does not run
anything the page tells a customer to run (no `gpiodetect`, no `sensors`, no
actual `make` build) -- only that what the page names exists in this
source tree.

**Red-before cases this check catches**, run against a scratch copy of the
page (`bash test/wiki-page-check.sh /tmp/scratch.md`), never the committed
one:

1. A `/dev/<node>` path not backed by source, e.g. appending
   `` `/dev/nosuchdevice` `` -> `FAILED: device path claim not real:
   /dev/nosuchdevice`.
2. A `make <target>` not in the Makefile's own `DRIVERS :=` line or its
   fixed lifecycle targets, e.g. appending `` `make nosuchtarget` `` ->
   `FAILED: make target claim not real: make nosuchtarget`.
3. A `/sys/class/misc/<segment>/` path whose segment is not the driver's
   real device name, e.g. appending
   `` `/sys/class/misc/totally-made-up-device-name/COM1_Mode` `` ->
   `FAILED: sysfs misc path claim not real:
   /sys/class/misc/totally-made-up-device-name/`.
## Support list generator check (docs/wiki/Supported-Boards.md must track the board files)

`test/support-list-check.sh` and the `scripts/support-list.sh` it drives are
two of the internal-only files named above; neither ships in the public
GitHub mirror. Documented here for engineers working in this repo.

`test/support-list-check.sh` copies every `configs/boards/*.conf` into a
scratch directory, makes several targeted edits to that copy, then runs the
real `scripts/support-list.sh` against it and checks the *generated* table
changed in exactly those ways:

- flips one currently-`NOT HARDWARE-VALIDATED` board to
  `HARDWARE-VALIDATED` with a new date -- the row must turn `Yes` and carry
  that date;
- adds a new board file that sets `CONFIG_BOARD_NAME` but none of the four
  `MAKE_*_DEVICE` keys -- its `driver` cell must be empty;
- deletes the `# STATUS:` line entirely from a third board -- the row must
  still read `No`. This is the case the `verified` column exists to get
  right: a missing `# STATUS:` line is not evidence a board was tested, so
  it must never read blank or `Yes`;
- symlinks a fourth board file to a different board's real `.conf`, the
  way `configs/boards/ADP-226-01.conf` really points at `ADP-226.conf` --
  the row must be named after the symlink's own filename, not collapse
  into a duplicate of the board it points to;
- flips a fifth board to `HARDWARE-VALIDATED` with **two** dates on one
  line (a bench date and a retest date, the way a second bench pass really
  writes one) -- the row must stay on one line, with only the first date
  in the `date` column. A single `grep -m1` still yields both matches from
  one matching line, so taking the first date needs `head -1` as well;
  see `scripts/support-list.sh` for the fix;
- writes an isolated one-board directory carrying
  `# STATUS: PARTIALLY HARDWARE-VALIDATED` -- a real form,
  `ESM-KX60G.conf` carried it for nine days -- and runs the generator on it
  twice, with and without that line, comparing the two runs' own summary
  counts. The board must read `No` either way, but the marker must move it
  from the "no marker" bucket into the "explicit" one: a partial marker is
  still a marker, and folding it into "no marker" would tell a reader "not
  even attempted" about a board that was;
- regenerates the table from the real, unedited `configs/boards/` and
  diffs it against the committed `docs/wiki/Supported-Boards.md` -- the
  file's own header says "do not hand-edit", and this is what actually
  enforces that a forgotten regeneration fails the check instead of
  shipping a stale page.

It compiles nothing and needs no kernel tree. Every edit lands in a
`mktemp -d` scratch directory removed when the run ends, so the committed
`configs/boards/` is never touched (the drift check above only *reads* the
committed `docs/wiki/Supported-Boards.md`, to compare it against a fresh
regeneration). It proves the generator reads the real board files at run
time rather than printing a fixed table; it does not check the generator's
Markdown formatting beyond the one-line-per-row shape, or grade any
board's actual `verified` status against reality -- that reality lives in
the board files themselves, not in this check.

## Wiki publish guard check (docs/push.sh must not go live by accident)

`docs/push.sh`, `docs/README.md` and `test/push-check.sh` are internal-only
and do not ship in the public GitHub mirror (issue #65 box 11) — the public
repo has no internal wiki to publish to. Documented here for engineers
working in this repo.

`docs/push.sh` is the one script in this repo whose mistakes land on a real
public page with no review and no undo (it publishes reviewed
`docs/wiki/` pages to the GitHub wiki). `test/push-check.sh` drives the
real `docs/push.sh` against scratch git repos it builds itself under
`mktemp -d`: a scratch "source" repo standing in for this repo (a real
clone of the actual `docs/push.sh` under test, plus fixture wiki pages),
and one or more scratch "wiki" repos (any local bare repo whose origin
remote ends in `.wiki.git` -- that is the only thing `docs/push.sh` itself
checks, so a throwaway repo can stand in for the real wiki in tests). It
never touches the real wiki, the real `/workspace/Avalue-wiki` clone (if
present on the machine), this repo's own committed `docs/wiki/`, or the
network.

It proves:

- every refusal path exits non-zero and names the real reason: a
  `WIKI_DIR` that does not exist, is not a git repository, has no
  `origin` remote, or has an origin not ending in `.wiki.git`; an unset
  `WIKI_GITHUB_TOKEN`; a source repo not on `master`, with a dirty
  working tree, or whose `HEAD` has not been pushed to `origin/master`;
  and a wiki clone that is out of sync with its own origin;
- declining the `publish` prompt, and `--dry-run` even after typing
  `publish`, leave the wiki clone byte-for-byte untouched (same `HEAD`
  SHA, same file list, clean `git status`);
- a real publish lands exactly the owned page(s), byte-identical to the
  source clone's own `docs/wiki/` copies, naming the source commit SHA in
  the wiki commit message, and leaves every non-owned file (including a
  `NOT_PUBLISHED_PAGES` entry) untouched -- and that the push actually
  reaches the bare wiki repo, not just the local clone;
- the `WIKI_GITHUB_TOKEN` value never reaches the script's own
  stdout/stderr, the new wiki commit's message or diff, or any file left
  in the wiki clone;
- a `docs/wiki/` file named in neither `OWNED_PAGES` nor
  `NOT_PUBLISHED_PAGES` is refused by name, not silently skipped -- this
  is the ownership-drift guard issue #66 added after a deferral ("add
  `Supported-Boards.md` once issue #63 lands") sat as a comment nobody
  enforced, silently going stale once #63 actually landed;
- the freshness check tells apart a wiki clone that is strictly *behind*
  its own origin (fix: `git pull`), strictly *ahead* of it (the state a
  failed push leaves, per `docs/push.sh`'s own push-failure message --
  fix: retry the push), and *truly diverged*, ahead and behind at once
  (fix: resolve it by hand) -- instead of calling all three "diverged"
  and telling every one of them to `git pull`, which used to be actively
  wrong advice for the ahead case;
- the script runs correctly when the wiki clone sits nested inside the
  driver checkout itself (the real layout, e.g.
  `/workspace/Avalue-wiki` under `/workspace`) -- the dirty-tree check no
  longer mistakes that clone's own untracked directory for a dirty
  source tree, while still refusing a real uncommitted edit or a genuine
  new untracked file elsewhere in the tree, even with a nested wiki
  clone also present at the same time (issue #67);
- the ownership check now walks `docs/wiki/` recursively, at any depth,
  and refuses by name any file that is not a top-level `*.md` page --
  one inside a subdirectory (e.g. `docs/wiki/images/diagram.png`) or a
  top-level non-`.md` file (e.g. `docs/wiki/notes.txt`) -- instead of
  silently skipping it, which is the same silent gap issue #66 added the
  ownership guard to close, one level down (issue #67).

It compiles nothing and needs no kernel tree. It does not exercise the
real GitHub wiki, the real credential path beyond confirming the token
never leaks, or `docs/push.sh`'s interaction with a genuinely remote
(non-local) git origin -- every repo it drives `docs/push.sh` against is
a local path on this machine. It also still does not exercise the real
GitHub network or the real `/workspace/Avalue-wiki` clone -- that
real-environment run is done separately, by hand, outside this script.

### The local wiki: `test/publish-local-wiki-check.sh`

`docs/publish-local-wiki.sh` is the other half of the same subject and has
the opposite risk profile: it publishes every `docs/wiki/` page to this
project's OWN GitLab wiki, automatically, from the pipeline in
`.gitlab-ci.yml`. Nothing it does is public or irreversible, so it has no
confirmation prompt -- which puts the whole weight on this harness.

`test/publish-local-wiki-check.sh` drives the real script against scratch
bare repos it builds under `mktemp -d`, the same shape as `push-check.sh`,
and proves:

- an unchanged wiki is a genuine no-op -- exit 0, and the remote's tip is
  where it was, so a re-run leaves no empty commit;
- a changed page lands byte-identical and the wiki commit names the source
  SHA, while a page nobody edited keeps its history untouched;
- a page the wiki does not have yet is created, and reported as new;
- a page live on the wiki with no source under `docs/wiki/` survives the
  publish and is reported by name -- this script never deletes;
- `--dry-run` reports the change and pushes nothing;
- a CI run with no `WIKI_PUSH_TOKEN` stops, naming the variable and the
  scope the token needs, rather than reaching a credential prompt and
  hanging until the job times out;
- the token reaches neither the output nor any file left in either repo --
  a job log is readable by everyone who can read the project;
- a human run off the default branch is refused by name, and
  `--allow-unreviewed` is how that is overridden on purpose;
- the flat-wiki shape rules refuse a subdirectory or a non-`.md` file by
  name, as `docs/push.sh` does;
- a directory git refuses to read is reported as exactly that, repeating
  git's own reason, rather than as `on branch ''` -- which would send the
  reader off to check out a branch when the branch was never the problem;
- the first publish to a wiki with no commits at all creates the branch --
  the state every new GitLab wiki starts in.

It does not exercise the pipeline itself. Two things stand in for that, and
neither of them is this script:

- **The container rehearsal**, run by hand. It is what caught the job image's
  entrypoint (see the comment in `.gitlab-ci.yml`) and the `on branch ''`
  message above -- both of which every check here passed straight over,
  because every check here runs the script on this host, in this shell:

  ```
  git clone <this repo> /tmp/rehearse
  docker run --rm --entrypoint "" -v /tmp/rehearse:/builds/x -w /builds/x \
      -e GIT_CONFIG_COUNT=1 -e GIT_CONFIG_KEY_0=safe.directory -e GIT_CONFIG_VALUE_0='*' \
      alpine/git:latest sh -c 'apk add --no-cache bash >/dev/null; bash docs/publish-local-wiki.sh --dry-run'
  ```

  The `safe.directory` setting is needed only because a bind mount makes the
  build directory look foreign to git; a real runner clones it itself and
  does not need it.

- **The first real pipeline run**, which is the only thing that exercises the
  runner and the `WIKI_PUSH_TOKEN` credential path against a real GitLab.
  Nothing above authenticates to anything.

## Publish-to-GitHub guard check (.local/publish-github.sh must ship no board file)

`test/publish-github-check.sh` and `.local/publish-github.sh` are internal-only
and do not ship in the public GitHub mirror — the mirror has no reason to carry
the script that builds it. Documented here for engineers working in this repo.

`test/publish-github-check.sh` drives the real `.local/publish-github.sh` against scratch git repos it builds itself: a fixture standing in for this repo (with its own `configs/boards/*.conf`, dev-only files, the seven internal-only scripts named above, and the internal wiki-publish tooling `docs/push.sh`, `docs/README.md` and `test/push-check.sh` — issue #65 box 11) and a scratch bare repo standing in for the GitHub target. It proves every refusal path (missing `PUBLISH_GITHUB_TOKEN`, a dirty or unreviewed source tree, a missing argument), that `--dry-run` builds and verifies the tree but never pushes, that the built tree and its whole git history carry no `configs/` directory and no `.conf` file, that the seven internal-only scripts, the internal wiki-publish tooling (`docs/push.sh`, `docs/README.md`, `test/push-check.sh` — issue #65 box 11) and the usual dev-only files (`.git`, `CLAUDE.md`, `.local`, `.gitignore`, `.clang-format`, `.editorconfig`) are absent from what ships, that a nested git clone (e.g. a wiki clone checked out inside this repo, the real `/workspace` layout) does not trip the dirty-tree check and is never copied into the built tree even when it carries its own `.conf` files, and that the token never reaches disk, argv, or output. It also proves issue #68: a **second** real publish to the same, now non-empty target succeeds, the target ends up with two commits (the second a child of the first, an ordinary fast-forward, no `--force` anywhere in the script), neither commit's history carries a board file, `--dry-run` against such a target says what it would push on top of rather than just a bare sha, and a genuine push failure (write access denied) prints a message naming what a re-run will and will not fix. It also proves issue #69: `.local/publish-github.sh` reads the target's actual default branch with `git ls-remote --symref <url> HEAD` rather than assuming a name, so a target whose default branch already carries a commit under a name other than the one this script publishes to (`master`) is refused before any push, with a message naming both branches, and that refusal holds identically on a second attempt against the same still-conflicting target; and a target whose default branch name was set (e.g. to `main`) but which is otherwise genuinely empty still publishes normally, exactly as an ordinary first publish would. Both new target shapes — a `main` branch already carrying a commit, and a `main` default with no commits anywhere — are run through the whole publish twice, not once, so the repeat path from issue #68 keeps working under the new default-branch detection. It never touches the real `configs/`, the real GitHub, or the network — every repo it uses is a fresh local git repo under a `mktemp -d` scratch directory removed when the run ends.
