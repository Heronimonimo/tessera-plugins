# Publishing a plugin

## Where a plugin comes from

A plugin reaches the Tessera app in one of four ways:

| Way | What a screen builds | What shows as an update |
|---|---|---|
| The index (this repository) | The commit the index names | A newer version in the index |
| A link to a repository with a release | The commit of the newest release, when it was read | A newer release |
| A link to a branch, or to a repository without a release (its default branch) | The commit the branch was at, when it was read | A newer commit on that branch |
| A folder in Home Assistant's config, `tessera-plugins/<id>/` | What the folder holds, at every build | Nothing: every build takes the folder |

Every way but a folder is pinned to one commit, so what a screen builds is what the editor showed. The app reads the
index at most every ten minutes and asks a linked repository for a newer release or commit at most every hour. A newer
one shows as an update on the screen's Plugins tab and on the Plugins page, and one tap builds it. Nothing reaches a
screen by itself. An update keeps what was filled in when the plugin was added; one that asks for other rights than
before waits for a yes.

A link follows its repository by GitHub's number for it, not by its name: a repository you rename keeps its screens, and
a new repository that takes an old name is never built.

## Which plugin: id and origin

The id is a plugin's name for ever. It is in the firmware, in every layout with one of its tiles
(`plugin:<id>.<tile>`) and in the secrets filled in for it: never change it.

Where a plugin comes from is its origin: `github.com/<owner>/<repo>`, with `/<folder>` when the plugin is in a folder of
the repository, or a folder in Home Assistant's config. Two plugins can have one id (a fork, a copy being made), and the
origin says which of them a screen runs.

- A secret is kept for the origin it was filled in for. A fork with the same id never gets it.
- A screen that runs another origin of an id is not updated to the index's plugin of that id. The person switches in the
  plugin's details, when they want to.

## The index

The app reads one file for the list: `index.json` in this repository. Each branch has its own (AGENTS.md, "Branches"):
the app of a Tessera release reads the one on `main`, whose plugins build on the firmware Tessera has released, and
Tessera's dev app reads the one on `dev` (and `main`'s while `dev` has none), where a plugin that needs a newer plugin
API waits for Tessera's next release. Each index lists every plugin with its
manifest, its texts, its README and its changelog at one commit, the plugin versions that are blocked, the plugins Tessera recommends,
and whose each id is. `tools/build_index.py` writes it; nobody edits it by hand. The app reads it with a conditional
request and keeps the last one for when the internet is away. Beside it, `likes.json` has how many people like each
plugin (below).

## Tessera's own plugins: `plugins/`

The plugins in `plugins/` are made and reviewed by Tessera. Each is pinned in the index to the last commit that changed
its folder, so a change elsewhere in this repository never makes a screen build something new.

To change one:

1. Change the plugin, raise its `version` in `tessera-plugin.yaml`, and write what a person notices under a heading
   of that version at the top of its `CHANGELOG.md` (and its translations, when it has them).
2. `python3 tools/check.py plugins/<id>` and a build on a real screen ([TESTING.md](TESTING.md)).
3. Commit, then `python3 tools/build_index.py` and commit `index.json` (CI does the second step after a merge too).

## Your own plugin, in a repository of your own

A plugin can live in a repository of its own, made from [`template/`](../template)
(`python3 tools/new_plugin.py <id> ../my-plugin`). It stays yours: you publish its releases, you answer its issues, and
Tessera never needs a pull request for a new version.

There are two ways for people to find it:

- **A link.** Anyone can add it in the app under Plugins, **Add with a link**: `https://github.com/<you>/<repo>`, or
  `https://github.com/<you>/<repo>/tree/main/<folder>` when the plugin is in a folder. With a release the app takes the
  newest, and every release you publish after it shows as an update. Without a release the link is a test of your
  default branch (the simplest way to try a plugin: push, and add the link), and every newer push shows as an update.
  To test another branch of a repository with releases, choose **Test a branch** and name it. Nothing goes through this
  repository.
- **The community list.** List it once, and the Plugins page shows it to everyone with the label Community:

  1. Make a release in your repository (a tag with a GitHub release).
  2. Open a pull request here that adds one file, `community/<id>.yaml`, named after the plugin's id:

     ```yaml
     repo: https://github.com/someone/tessera-bin-day
     path: .                        # the folder with tessera-plugin.yaml
     maintainer: someone            # you: the owner of the repository
     ```

  3. CI checks the entry and your release (below). Once it is merged, the index follows your releases by itself: it is
     built again every hour and takes your newest release, checked with the same rules, pinned to its commit. When
     your newest release does not pass, the plugin is left out of the index until a release passes; its id stays
     yours.

  Add `ref: v1.2.0` only to hold the list at one release; without it the list follows your newest.

**What is checked automatically**, when the pull request is opened and every time the index is built:

1. The file is `community/<id>.yaml`, named after the plugin's id, with `repo` and `maintainer`, and optionally `path`
   (a folder inside the repository) and `ref`.
2. `maintainer` is the GitHub account that owns the repository, and the pull request comes from that account. An
   organisation cannot open a pull request itself: for a repository of an organisation, ask in an issue, and a
   maintainer of this repository opens it.
3. The repository can be read without an account (it is public), and has a release, or the tag that `ref` names.
4. `tools/check.py` passes on that release: the manifest (with a licence that goes with AGPL-3.0, and an `api` the core
   can build), the folder and its files, the translations, the README and the changelog, `plugin.yaml` and the code
   rules, and the component's Python ("What a plugin's Python may do", below). The manifest's id is the file's name.
5. The id is not one of Tessera's own, and no other repository or folder published it first ("Whose id it is", below).
6. What it needs is in the index: every plugin of `requires.plugins`, and a plugin or a board for every feature of
   `requires.features`, for one of its boards; and no plugins that need each other in a circle.

**What is yours to get right**, because nothing checks it:

- The repository has issues on, so people can tell you what goes wrong ([TESTING.md](TESTING.md), "Reporting a
  problem with a plugin").
- It builds and works on the boards it names, and with `boards: any` on a small and a large screen at least. Say in the
  README which boards you tried.
- `permissions` name everything its code does. The app enforces `network` and `ha_commands`; nothing checks
  `read_entities` or `home_assistant_actions` against the code.
- `memory` and `flash_kb` are honest.
- It has a `store.png` that shows what it does ("The picture on the Plugins page", below). Without one its card shows
  only its icon.
- The code rules of `tools/check.py` look for a few patterns that are mistakes (a colour as a number, a timer of its
  own, a connection from the screen). They catch slips, not intent.

## What listing means

A plugin is C++ compiled into the screen's firmware. On the screen it has full access to the device: it can call any
Home Assistant action the screen may call, read what the screen reads, use the board's hardware and the network the
screen is on, and nothing on the screen stops it. The manifest's `permissions` are enforced only where the app does the
work for the plugin: a fetch reaches only the hosts of `permissions.network`, and `tessera::send` only the commands of
`permissions.ha_commands`.

- Tessera's own plugins, in `plugins/`, are made and reviewed by Tessera.
- Listing a community plugin is not a review. CI checks the shape of each release (above), not what its code does. The
  app says so before anyone adds a community plugin, and asks them to trust the maker.
- A plugin added with a link or from a folder has passed only the app's own manifest check.
- `blocked.yaml` is how a harmful version is stopped: every app refuses to build it, a screen that runs it shows the
  reason on its Plugins tab, and its next build leaves the plugin out ("Blocked versions", below). A plugin that breaks
  the rules also leaves the list.

The ESPHome component of a plugin has a second half: its Python (`components/<id>/__init__.py`) runs where the screen is
built, inside the Tessera app, when the app builds the screen. The app hands that build nothing of its own (not its
token for Home Assistant), and the rules below hold the Python to what a component needs. The Tessera editor says both
halves before anyone adds a plugin.

Found a plugin that does harm? Open an issue in this repository.

## What a plugin's Python may do

ESPHome runs a component's `__init__.py`, and what it imports, on the computer that builds the screen: for most people
the Tessera app in Home Assistant, beside Home Assistant's config folder and on its network. So `tools/check.py` reads
every `.py` under `components/` as a syntax tree, at every release the index takes and for Tessera's own plugins alike,
and refuses:

- an import outside `esphome` and a short list of the standard library (`PYTHON_MODULES` in `tools/check.py`: pathlib,
  json, re, math, struct, typing, dataclasses, enum, functools, itertools, collections, logging, voluptuous and a few
  more; never os, sys, subprocess, socket, urllib, shutil, importlib);
- the parts of ESPHome that run commands or fetch (`esphome.util`, `esphome.git`, `esphome.external_files`, ...);
- `open()`, `eval()`, `exec()`, `__import__()` and the like, and a dunder attribute that is a way around all of this;
- reading a file that is not beside the plugin: `.read_text()`, `.read_bytes()`, `.glob()` and so on go through a
  `Path` made from `__file__` (`Path(__file__).parent / "sounds" / "tap.wav"`), and nothing is written at all;
- a path of the computer in a string (`/config`, `/data`, `/homeassistant`, `/etc`, ...).

And in `plugin.yaml` and the parts, which ESPHome reads on the same computer: no `!secret` (the screen's Wi-Fi and
keys are the screen's; a value of a plugin is an input of its manifest), and no `!include` that leaves the plugin's
folder. `packages` and `external_components` were already the core's alone.

These rules catch the plain `import os` and the honest slip, and make anything else stand out in a diff. They are not
a sandbox: listing stays what "What listing means" says.

## Whose id it is

In the index an id belongs to the repository and the folder that first published it. `index.json` keeps that under
`claims`, also for a plugin that is left out for a while (a release that fails the check). A community entry under an id
that another repository or folder published first is left out of the index, with the reason in the index workflow's
log. Repositories are known by GitHub's number for them here too: a repository you rename keeps its ids, and a new
repository that takes an old name gets none of them. Tessera's own `plugins/` win a clash with a community entry.

To hand an id on (you move the plugin to another repository, or give it to someone else), add a line to
`transfers.yaml` in a pull request, from the account that has the id or with its consent in the pull request:

```yaml
- plugin: bin_day
  from: https://github.com/someone/tessera-bin-day
  to: https://github.com/someone-else/tessera-plugins/tree/main/bin_day
  reason: The maker handed it over in the pull request.
```

`from` and `to` are a repository, with `/tree/<branch>/<folder>` when the plugin is in a folder. The index then takes
the plugin from its new place, and the id belongs there from then on. A screen that runs the plugin from its old place
keeps that origin until the person switches.

## Finding a plugin

The Plugins page has a tab per type (Tiles, Functions, Hardware, read from the manifest: [MANIFEST.md](MANIFEST.md),
"The type of plugin") and one for what the screens have In use. The maker's `topics` are chips under the tabs. The list
is sorted by most liked, or by newest or by name; a filter shows what Tessera made, what the community made, or both;
and "Only what fits my screens" folds away what fits none of them, with the reason.

`featured.yaml` lists the plugins Tessera recommends, chosen by hand. The index carries them as `featured` (an id that
is not in the index is left out), and on the Plugins page a recommendation breaks a tie in the number of likes.

## The picture on the Plugins page

A plugin can have a picture: `store.png` beside its `tessera-plugin.yaml`. The Plugins page shows it across the top of
the plugin's card and large above its details; without one the card shows the plugin's icon, as before.

- A PNG of 16:9, from 640 x 360 to 2560 x 1440, at most 1 MB. 1280 x 720 is best. `tools/check.py` checks it.
- It goes with the release like every other file: the index and a link take it at the release's commit, so a new
  picture reaches the app with the next release.
- `tools/store_image.py` makes one from the bench, like a video's thumbnail: the plugin's name and summary
  (`translations/en.json`, `app`) large at the top left, and a page of the plugin on a 10-inch screen, a 4-inch square one
  and the CYD, drawn by the real firmware:

  ```sh
  python3 tools/store_image.py plugins/my_idea          # renders on the bench, writes plugins/my_idea/store.png
  python3 tools/store_image.py ../my-plugin             # a plugin in a repository of its own
  ```

  It lays each page out from the bench's pictures of the tiles, with the first examples of `preview.yaml` first, so put
  the example that shows the plugin best at the top. A plugin with a card shows the card a tap opens on the square
  screen. A plugin without tiles (a function, hardware) has nothing for it to show: make a picture of your own.
- A picture of your own is just as welcome. Show what the plugin does on a screen, keep words few and large, and leave
  out what is not true of the plugin: the name of a product it does not support, a feature it does not have.

## Likes

A like is a heart a person gives a plugin in the Tessera app: a thank-you to its maker and a hint for others. Only a
plugin of the index that runs on one of their screens can have their like, one per app installation, and the app asks
once that the like counts in a public number. Tessera's website counts the likes, and `tools/build_likes.py` copies the
counts into `likes.json` every hour, in the index's commit. The app reads them from there, never from the website. The
file holds only plugins of the index.

## Labels in the app

| Label | Means |
|---|---|
| From Tessera | In `plugins/` of this repository, made and reviewed by Tessera, or a release of one of Tessera's own repositories added with a link. |
| Community | Listed through `community/`, or a release added with a link to its repository: checked automatically, not reviewed. Its maker publishes its updates. |
| Test | A branch, a repository without a release, or a folder in Home Assistant's config: someone's work in progress. |

## Blocked versions

`blocked.yaml` lists versions the app refuses to build, with the reason; a screen that already runs one shows the
reason in its Plugins tab, and its next build leaves the plugin out.

```yaml
- plugin: some_plugin
  versions: ["1.0.0"]
  reason: Sends the API key to a host it does not name.
```

## Versions

- `version` in the manifest: three numbers. Raise the last for a fix, the middle for something new, the first when a
  tile's options change in a way that loses what people set.
- `CHANGELOG.md`: a heading for every version, newest first, the first one the manifest's. When the app offers an
  update it shows the lines of every version between the one a screen runs and the new one, so say there what a person
  notices and what they must do ([MAKING_A_PLUGIN.md](MAKING_A_PLUGIN.md#the-changelog)). The index carries it as
  `changelog`, one text per language, beside `readme`.
- `api`: the plugin API the plugin was written for. It builds on a core with the same major and at least that minor,
  unless a minor after its own changed a name; a core it does not fit refuses it and says why. While the API is 0.x a
  minor may still change a name or a signature, the core lists the minors that did (only 0.4 so far), and Tessera's
  own plugins move with them. From 1.0 on a minor only adds and only a break raises the major
  ([FIRMWARE_API.md](FIRMWARE_API.md), "Versions").
