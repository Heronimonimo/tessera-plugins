# Publishing a plugin

## How the app finds plugins

The Tessera app reads one file: `index.json` in this repository (on `main`). It lists every plugin with its manifest,
its texts and its README at one commit, and the plugin versions that are blocked. `tools/build_index.py` writes it;
nobody edits it by hand. The app reads it when the editor opens, at most every ten minutes (a conditional request), and keeps the last one
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
(`python3 tools/new_plugin.py <id> ../my-plugin`). It stays yours: you publish its releases, you answer its issues, and
Tessera never needs a pull request for a new version.

There are two ways for people to find it:

- **A link.** Anyone can add it in the app under Plugins, Add with a link: `https://github.com/<you>/<repo>`, or
  `https://github.com/<you>/<repo>/tree/main/<folder>` when the plugin is in a folder. The app takes your newest
  release, and asks your repository for a newer one every hour: when you publish a release, the people who added it
  see an update. Nothing goes through this repository.
- **The community list.** List it once, and the Plugins page shows it to everyone with the label Community:

  1. Make a release in your repository (a tag with a GitHub release).
  2. Open a pull request here that adds one file, `community/<id>.yaml`, named after the plugin's id:

     ```yaml
     repo: https://github.com/someone/tessera-bin-day
     path: .                        # the folder with tessera-plugin.yaml
     maintainer: someone            # you: the owner of the repository
     ```

  3. CI checks that you own the repository and that your release passes `tools/check.py`. Once it is merged, the
     index follows your releases by itself: it is built again every hour and takes your newest release, checked with
     the same rules, pinned to its commit. A release that does not pass stays out, and the one before it stays listed.

  Add `ref: v1.2.0` only to hold the list at one release; without it the list follows your newest.

What a community plugin needs:

1. The person who lists it owns the repository (or is a member of its organisation).
2. The repository is public, has issues on, and has at least one release.
3. `tools/check.py` passes on the release.
4. Its licence goes with AGPL-3.0.
5. It builds on the boards it names (or on Tessera's sample boards for `boards: any`).
6. Its permissions name everything its code does.

Tessera does not review community plugins or their releases: the app says so before anyone adds one, and asks them to
trust the maker. What it can do is stop a version: `blocked.yaml` (below) refuses it in every app, and a plugin that
breaks the rules leaves the list. The plugin API is 0.x while it settles: a change that breaks a plugin raises its
major, and the app refuses a plugin written for another major instead of building it.

## Labels in the app

| Label | Means |
|---|---|
| From Tessera | In `plugins/` of this repository: made and reviewed by Tessera. |
| Community | Listed through `community/`, or added with a link to its repository: checked automatically, not reviewed. Its maker publishes its updates. |
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
