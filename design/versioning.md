# Skill Versioning

Currently, skills are installed to flat, unversioned paths:

```
.agents/skills/my-skill/
```

Reinstalling a skill overwrites whatever was there before. The git ref used to fetch the skill (`owner/repo@v1.0.0`) is not persisted anywhere.

This document explores adding version awareness to rosie.

## Do we need a lockfile?

**No, for local installs.** Local skills are copied into the project tree (`.agents/skills/`) and are intended to be committed to version control. The checked-in files *are* the source of truth — the actual skill content is right there in git. A lockfile tracking "this came from v1.0.0" would be redundant metadata; `git log` already tells you when it changed and `git diff` shows what changed.

This is different from npm/pip where packages live in an ignored `node_modules/` or virtualenv. Skills are checked-in source, not ephemeral dependencies.

**Maybe, for global installs.** Global skills (`~/.claude/skills/`, etc.) aren't in any repo, so there's no record of where they came from. A lightweight manifest at `~/.agents/rosie.json` could help with `rosie update` for global skills. But this is a weaker case — global installs are personal setup, not reproducible environments.

## Goals

1. **Version in the path** - Make it visible which version of a skill is installed by encoding the ref in the directory name.
2. **Install specific versions** - `rosie install owner/repo@v2.0.0` should produce a meaningfully different result than `@v1.0.0`.
3. **Upgrade/downgrade** - Install a new version and update symlinks, with the old version still in git history.

## Directory Structure

Version suffix style — the ref is encoded in the directory name:

```
.agents/skills/my-skill@v1.0.0/
    SKILL.md
    scripts/
.claude/skills/my-skill -> ../../.agents/skills/my-skill@v1.0.0
```

Only one version exists on disk at a time. Installing a new version removes the old directory and creates a new one. Symlinks in agent dirs always use the clean name (`my-skill`), so agents never need to care about versions.

## Version Resolution

When installing, the version is resolved in this order:

### 1. Explicit ref: `rosie install owner/repo@v1.0.0`

Use exactly what the user asked for.

```
.agents/skills/my-skill@v1.0.0/
```

### 2. No ref, repo has tags: `rosie install owner/repo`

Query the repo for the latest tag and use that.

```
.agents/skills/my-skill@v2.3.0/
```

This is the common case — most users will just `rosie install owner/repo` and expect to get the latest stable version.

### 3. No ref, repo has no tags: `rosie install owner/repo`

Fall back to `main` with no version in the path.

```
.agents/skills/my-skill/
```

The unversioned path is a clear signal: this skill isn't pinned to anything specific. It's a snapshot of `main` at the time of install.

## Impact on Commands

### `rosie install owner/repo@v1.0.0`

- Download the tarball at that ref (already works)
- Resolve version → `v1.0.0`
- Store to `.agents/skills/my-skill@v1.0.0/`
- If an older version dir exists (`my-skill@v0.9.0/` or `my-skill/`), remove it
- Create/update symlinks in agent dirs pointing to the new path

### `rosie install owner/repo` (no ref)

- Query repo for latest tag
- If tags exist: resolve to latest tag, install as `my-skill@v2.3.0/`
- If no tags: download `main`, install as `my-skill/` (unversioned)

### `rosie remove skill-name`

- Find the skill directory (with or without version suffix) by matching on skill name
- Remove the directory and symlinks from agent dirs

### `rosie update [skill-name]`

Possible future command. For local installs, it's essentially `rosie install` again — run the same resolution logic, and if the resolved version is different from what's on disk, replace it. The diff shows up in `git status`.

## Implementation Phases

### Phase 1: Latest tag resolution

- Query GitHub API for latest tag when no `@ref` is given
- Fall back to `main` if no tags exist
- Pass the resolved ref through to the install step

### Phase 2: Versioned directories

- Update `install_to_canonical()` to include the ref in the path (when a version is resolved)
- On install, find and remove any existing directory for the same skill (regardless of version suffix)
- Update symlink creation to point to the versioned path
- Update `rosie remove` to match by skill name prefix

## Open Questions

1. **Tag format** — Do we only look for semver-style tags (`v1.0.0`)? Or any tag? Some repos use non-semver tags.
2. **"Latest" tag heuristic** — Sort tags by semver? By date? Use GitHub's "latest release" API?
3. **GitHub API auth** — Unauthenticated API calls are rate-limited (60/hour). Should we support a `GITHUB_TOKEN` env var for higher limits?
4. **Global installs** — Should versioned directories also apply to global installs? Or is it only useful for local (where files are committed)?
