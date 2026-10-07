# Publishing a plugin

## How the app finds plugins

The Tessera app reads one file: `index.json` in this repository (on `main`). It lists every plugin with its manifest,
its texts and its README at one commit, and the plugin versions that are blocked. `tools/build_index.py` writes it;
nobody edits it by hand. The app reads it when its Plugins page opens, at most every six hours, and keeps the last one
for when the internet is away.

## Tessera's own plugins: `plugins/`

The plugins in `plugins/` are made and reviewed by Tessera. Each is pinned in the index to the last commit that changed
its folder, so a change elsewhere in this repository never makes a screen build something new.

To change one:

1. Change the plugin and raise its `version` in `tessera-plugin.yaml`.
2. `python3 tools/check.py plugins/<id>` and a build on a real screen ([TESTING.md](TESTING.md)).
3. Commit, then `python3 tools/build_index.py` and commit `index.json` (CI does the second step after a merge too).

A screen keeps the commit it was built with until someone presses Update for it in the app: a new version reaches no
screen by itself.

## Your own plugin, in a repository of your own

A plugin can live in a repository of its own, made from [`template/`](../template)
(`python3 tools/new_plugin.py <id> ../my-plugin`). The index will list such plugins through a file per plugin in
`community/`:

```yaml
# community/bin_day.yaml
repo: https://github.com/someone/tessera-bin-day
path: .                        # the folder with tessera-plugin.yaml
ref: v1.2.0                    # a tag of a release; the index pins its commit
maintainer: someone            # the owner of the repository
```

**This part is not open yet.** The plugin API is 0.1 and may still change, and an open index is a promise to the people
who build on it. It opens once three of Tessera's own plugins run on plugin API 1.0. Until then a plugin of your own
runs on your screens as a test folder ([TESTING.md](TESTING.md), part 2), and a pull request that adds it to
`plugins/` is welcome when it is useful to others.

What a community entry will need to pass, from the start:

1. The person who adds it owns the repository (or is a member of its organisation).
2. The repository is public, has issues on, and has at least one release.
3. `tools/check.py` passes on the release.
4. Its licence goes with AGPL-3.0.
5. It builds on the boards it names (or on Tessera's sample boards for `boards: any`).
6. Its permissions name everything its code does.

Tessera does not review community plugins: the app says so before anyone adds one, and asks them to trust the maker.

## Labels in the app

| Label | Means |
|---|---|
| From Tessera | In `plugins/` of this repository: made and reviewed by Tessera. |
| Community | Listed through `community/`: checked automatically, not reviewed. |
| Test | A folder in Home Assistant's config: someone's work in progress, never in the index. |

## Blocked versions

`blocked.yaml` lists versions the app refuses to build, with the reason; a screen that already runs one shows the
reason in its Plugins tab.

```yaml
- plugin: some_plugin
  versions: ["1.0.0"]
  reason: Sends the API key to a host it does not name.
```

## Versions

- `version` in the manifest: three numbers. Raise the last for a fix, the middle for something new, the first when a
  tile's options change in a way that loses what people set.
- `api`: the plugin API the plugin was written for. A core that offers another API refuses to build it and says why,
  so raise `api` only after building on that core.
