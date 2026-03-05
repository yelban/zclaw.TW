# Upstream sync workflow

## Remotes

| Remote | Repository | Purpose |
|--------|-----------|---------|
| `origin` | `git@github.com:yelban/zclaw.TW.git` | Our fork |
| `upstream` | `https://github.com/tnm/zclaw.git` | Original upstream |

## gh CLI default

After adding `upstream`, `gh` may default to the upstream (parent) repo. Fix with:

```bash
gh repo set-default yelban/zclaw.TW
```

## Sync commands

```bash
# Fetch upstream
git fetch upstream

# Check new upstream commits
git log HEAD..upstream/main --oneline

# Merge upstream into our main
git merge upstream/main

# Resolve conflicts if any, then push
git push origin main
```

## Versioning convention

Fork versions track upstream with a `-tw.N` suffix:

| Scenario | Version |
|----------|---------|
| Based on upstream 2.10.2, first fork release | `2.10.2-tw.1` |
| Another fork change on same upstream base | `2.10.2-tw.2` |
| Upstream releases 2.10.3, we merge and release | `2.10.3-tw.1` |

This uses SemVer pre-release syntax. Technically `-tw.1` sorts below `2.10.2` in strict SemVer precedence, but for a fork this is acceptable and maximizes human readability.

## Typical conflict points

- `VERSION` — we keep our `-tw.N` version, upstream has their version
- `CHANGELOG.md` — usually auto-merges cleanly (different sections)
- CI workflows — we have `build-nvs-tool.yml` which upstream doesn't have (no conflict)
