# Skill Versioning

Currently, skills are installed to flat, unversioned paths:

```
.agents/skills/my-skill/
```

Reinstalling a skill overwrites whatever was there before. The git ref used to fetch the skill (`owner/repo@v1.0.0`) is not persisted anywhere. Re-running `rosie install owner/repo` always pulls `main`, even if the user wanted whatever was current six months ago.

This document describes adding version awareness to rosie via a small, line-oriented lockfile.

## Why a lockfile (and not version-suffixed paths)

An earlier draft of this design encoded the version into the directory name (`.agents/skills/my-skill@v1.0.0/`). That has a fatal flaw: rosie isn't the only tool installing into `.agents/skills/`. The README itself lists seven alternatives. If rosie writes `my-skill@v1.0.0/` and another tool writes `my-skill/`, the agent sees two skills, and the user gets duplication or whichever symlink last won.

**The convention everyone agrees on is `.agents/skills/<skill-name>/`.** Rosie should follow it. Per-install metadata — source repo, resolved ref, SHA — lives in a separate file that other tools never look at.

A lockfile also gives us:

- **Reproducibility** when skills are gitignored. `rosie install` (no args) can read the lockfile and rebuild the tree from scratch, the same shape as `npm install`.
- **A clean target for `rosie update`.** Without a record of what was installed, "update everything" has nothing to iterate over.
- **Force-re-tag protection.** Storing the resolved SHA alongside the ref lets us notice when `v1.0.0` upstream silently changed.

Skill directories themselves stay at the conventional path. Whether they're committed or gitignored is the user's choice — the lockfile (always committed) supports both modes.

## Lockfile

### Location

`.agents/rosie.lock` — alongside the skills directory it describes.

Always checked into git. Small (one line per installed skill), human-readable, line-oriented for clean diffs.

### Format

One skill per line. Fields are space-separated and positional:

```
<skill-name> <source> <ref> <sha> <installed-at>
```

Example:

```
pdf anthropics/skills v1.2.0 a1b2c3d4e5f6789abcdef0123456789abcdef012 2026-05-02T14:32:18Z
react-best-practices vercel-labs/agent-skills main 0123456789abcdef0123456789abcdef01234567 2026-05-02T14:35:01Z
my-skill owner/repo v0.9.0 fedcba9876543210fedcba9876543210fedcba98 2026-05-01T09:11:44Z
```

Field rules:

- **skill-name** — must match an installed directory under `.agents/skills/`. No spaces (already enforced by directory naming).
- **source** — `owner/repo` form. The thing that follows `rosie install`.
- **ref** — the resolved ref: a tag (`v1.2.0`), branch (`main`), or `-` if the source was a bare commit.
- **sha** — full 40-char git SHA the tarball was extracted from.
- **installed-at** — ISO 8601 UTC timestamp.

A leading `#` makes a comment line; blank lines are ignored. Both are preserved on rewrite when feasible (nice-to-have, not required for v1).

Parsing is `sscanf("%s %s %s %s %s", ...)` plus a length check on the SHA. ~30 lines of C to read and write.

We deliberately do **not** use JSON, TOML, or YAML. The schema is too small to justify a parser dep, and a line-per-skill format diffs perfectly: adding/removing/upgrading a skill touches exactly one line.

## Version resolution

When the user runs `rosie install owner/repo` (no `@ref`):

1. Run `git ls-remote --tags https://github.com/owner/repo` to list tags.
2. Filter to semver-shaped tags (`v?\d+\.\d+\.\d+(-\S*)?`).
3. Pick the highest by semver ordering.
4. If any qualify → use it. If none do → fall back to `main`.

When the user runs `rosie install owner/repo@<ref>`:

- Use `<ref>` exactly as given. No "latest" lookup.

In both cases, after the tarball is downloaded we resolve the ref to a SHA (either via `git ls-remote` or by inspecting the tarball metadata) and record both in the lockfile.

We do not use the GitHub Releases API — it only returns *formal* releases, which most skill repo authors won't bother creating. We do not use the GitHub tags API either — `git ls-remote` is unauthenticated, has no rate limit, and works for any git remote (not just GitHub) if we ever care.

Resolved version is logged so the auto-latest behavior isn't invisible:

```
$ rosie install anthropics/skills
Resolving anthropics/skills... v1.2.0
Downloading...
```

## Impact on commands

### `rosie install owner/repo[@ref] [skill]`

Existing behavior, plus:

1. Resolve the ref (as above).
2. After successful install of each skill, write/update its line in `.agents/rosie.lock`.
3. If a lockfile entry already exists for that skill, replace it (this is the upgrade path).

### `rosie install` (no args)

New form. Reads `.agents/rosie.lock` and re-installs each entry at its recorded ref. Used when skills are gitignored, or when cloning fresh.

Pinned to the exact ref recorded in the lockfile — does not re-resolve "latest." That's what `update` is for.

### `rosie update [skill]`

New command. For each lockfile entry (or the named skill):

1. Re-resolve the source's "latest" ref (only meaningful if the original ref was a tag or `main`; an explicit pinned ref is left alone unless the user says otherwise).
2. If the resolved ref or SHA differs from what's in the lockfile, reinstall.
3. Update the lockfile.

If skills are committed, the upgrade shows up as a normal diff in `git status`.

### `rosie remove <skill>`

Existing behavior, plus: remove the corresponding line from the lockfile.

Use the lockfile as the authoritative list of what rosie installed. If a skill exists on disk but has no lockfile entry, leave it alone — it was installed by something else.

### `rosie list` (local)

Optional small win: when run inside a project with a lockfile, show installed skills with their version and source. (`rosie list owner/repo` keeps its current meaning of "list skills available in a remote repo.")

## Multi-skill repos

A single `owner/repo@v1.0.0` can ship several skills. They get separate lockfile entries, each with the same source/ref/sha. This is intentional — the entries are per-skill so `rosie remove pdf` is unambiguous, but the shared SHA makes it clear they came from one upstream snapshot.

`rosie update` on one skill of a multi-skill repo upgrades only that skill's entry. We do not try to be clever about "siblings should move together," because the user might want to pin one and float another.

## Implementation phases

### Phase 1 — Lockfile read/write, recorded on install

- Add `lockfile.c`/`lockfile.h` with `lockfile_load`, `lockfile_save`, `lockfile_upsert`, `lockfile_remove`.
- After a successful install, capture source/ref/sha and upsert.
- After a successful remove, drop the entry.
- No behavior change for users yet beyond the lockfile appearing.

### Phase 2 — Latest-tag resolution

- Add a `resolve_latest_tag(spec)` that shells out to `git ls-remote --tags` (or uses libcurl + the smart-HTTP info/refs endpoint, decide during implementation).
- Wire into `install_package`: when `spec->ref` is the implicit `"main"` default and the user didn't supply `@ref`, run resolution first.
- Distinguishing "user didn't pass @ref" from "user explicitly said @main" requires a small change to `package_spec_parse` (e.g., a `ref_explicit` flag).

### Phase 3 — `rosie install` (no args) and `rosie update`

- New code path in `cmd_install` for the zero-arg case: iterate lockfile, call install per entry with the recorded ref.
- New `cmd_update` mirroring `cmd_install` shape, with re-resolution and a "no changes" no-op path.

### Phase 4 — Tarball ref bug fix (prerequisite for tags)

- `build_tarball_url` currently always uses `archive/refs/heads/<ref>` — this 404s for tags. Need to either try heads then tags, or use `git ls-remote` results to know which.
- This is a precondition for Phase 1 actually working with `@v1.0.0` style installs.

(Phase 4 is logically first but called out separately because it's a pre-existing bug, not new versioning work.)

## Open questions

1. **Pre-release tags** — `v1.0.0-rc1` and `v1.0.0-beta` qualify as semver. Should "latest" skip pre-releases by default? (Probably yes, with a `--pre` flag if we ever need it.)
2. **Non-semver tags** — what about repos that tag with `release-2026-04`? Currently we'd ignore them for "latest" and require explicit `@ref`. Acceptable?
3. **Lockfile during `rosie install owner/repo` when an unrelated skill is in the lockfile** — we just upsert the one we touched, leave others alone. Confirm this is the right read.
4. **Global installs** — same lockfile mechanism at `~/.agents/rosie.lock`? Or skip lockfile for global since reproducibility matters less? Lean toward "yes, same mechanism" for consistency.
5. **`git` as a runtime dependency** — Phase 2 uses `git ls-remote`. We don't currently depend on git at runtime. Alternative is implementing the smart-HTTP info/refs request via libcurl directly (~50 lines). Pick during Phase 2.
