# JSON Factory Design

Status: implemented (module `json/`, embedded Python module `PyJson`).
Owner module: `json/` (top-level module, sibling of `settings/`).

This is the JSON counterpart of the INI settings system
(`docs/settings-ini-design.md`). It is **not** a replacement: `settings/`
keeps handling flat, hand-editable INI; `json/` adds a structured,
nested-document backend for callers that need trees, arrays, and mixed
types. The two modules are independent and store into separate folders.

## Relationship to `settings/`

The `SettingsManager` lifecycle layer (registry, scopes, anchor/staging,
debounced autosave, atomic save, cross-account copy, path sanitizing,
per-document mutex, `nodelete` Python handle) is backend-agnostic, and it
is reproduced here with the SAME behavior. What changes is the document
body: INI's flat `(section, key) -> string` model is replaced by a
`nlohmann::json` tree with **path-addressed** access and native types.

Deliberately a parallel module (not a shared/refactored base) so the
working Settings module is never touched. The two are expected to drift
only in their document bodies, not their lifecycle.

## Storage layout

Separate root folder so JSON and INI never collide:

- `JsonScope::Account` -> `json/<email>/<name>` (staged in memory until the
  account email anchor resolves, then bound).
- `JsonScope::Global`  -> `json/Global/<name>` (bound immediately; shared by
  every account/process on the machine).
- `JsonScope::Root`    -> `<name>` at the module directory (bound
  immediately; reserved for core files).

`<name>` is sanitized to a safe relative subpath (folders preserved,
`.`/`..`/drive segments stripped) exactly as in `settings/`.

## Document model (`JsonFile`)

One `nlohmann::json` object per document, behind its own mutex.

Addressing is a slash path that walks the tree, mirroring the settings
`"section/key"` feel but with arbitrary depth: `"ui/window/pos/x"`. A
numeric segment indexes an array (`"waypoints/0/x"`). Reads never throw:
a missing node, or a value that will not convert, returns the caller's
default. Writes create intermediate objects as needed and mark the
document dirty.

- Typed leaf access (never throws): `GetString/GetBool/GetInt/GetFloat`
  and `SetString/SetBool/SetInt/SetFloat`, all `(path, ...)`. Getters
  coerce across JSON scalar types where sensible (number<->string,
  bool<->0/1, string "true"/"yes"/...).
- Subtree access: `GetNode(path)` (a copy of the subtree, `null` if
  absent), `SetNode(path, value)`, `Append(path, value)` (push onto an
  array, creating it if absent).
- Structure: `Has(path)`, `Delete(path)`, `Keys(path)` (object member
  names at a path), `Size(path)` (array length / object member count),
  `IsArray(path)`, `IsObject(path)`.
- Escape hatches (never needed in normal flow): `Save()`, `Reload()`,
  plus `IsDirty()`, `IsBound()`, `Name()`, `Scope()`, `Path()`.

Round-trip note: standard JSON has no comments, so unlike the INI backend
there is no comment/raw-line preservation. Output is pretty-printed
(2-space indent) with stable key ordering from the in-memory tree.

## Persistence: autosave + atomic write

Identical debounce to settings: a dirty document flushes after ~2 s of
write silence, or at most ~10 s after the first unsaved change; `Update()`
is stepped from the runtime loop, `FlushAll()` runs on shutdown. Every
save is a temp-file write followed by an atomic rename, so disk always
holds a complete old or complete new file.

## Global scope: cross-process lock + write journal

Global documents are shared by every running client (multibox). A plain
"serialize my whole in-memory tree and write it" would make concurrent
writers last-writer-wins at WHOLE-FILE granularity: account B's save
would silently drop account A's just-written keys.

The JSON backend prevents this for `Global` scope with two mechanisms:

1. **Cross-process lock.** A named Windows mutex keyed by a hash of the
   document's absolute path (`CrossProcessLock`) serializes the
   read-modify-write of a shared file across processes. Acquisition BLOCKS
   until the holder releases - the loser of a race waits, it never gives up
   and clobbers. It is crash-safe: if a client dies while holding the lock,
   Windows marks the mutex abandoned and hands ownership to the next waiter
   (`WAIT_ABANDONED`), so a dead peer cannot deadlock persistence. The only
   unlocked path is `CreateMutex` itself failing (logged, best-effort
   write) so a document is never silently dropped forever.

2. **Write journal + merge-on-save.** Every mutating call records a
   `PendingOp` (a `(path, value)` set, or a `(path)` delete) in addition
   to updating the in-memory tree. When a `Global` document saves, it
   takes the lock, RE-READS the current on-disk tree, REPLAYS only its own
   journaled ops onto that fresh tree, writes, then adopts the merged tree
   and clears the journal. Two accounts writing different keys of the same
   global file both survive; only genuine same-path writes race (still
   last-writer-wins on that one path, which is unavoidable).

`Account`/`Root` scope are single-owner, so they skip the lock and journal
replay and just write their tree directly (the journal is still cleared).

## Cross-account copy

Same intent as settings: push this account's config into ANOTHER account's
file on disk (`json/<target_email>/<name>`), picked up on the target's
next reload. Overlay is a deep merge (RFC 7396 merge-patch): source keys
win, target-only keys are retained, an explicit `null` deletes.

- `CopyDocumentToAccount(name, target_email)` - the whole tree.
- `CopyPathToAccount(name, path, target_email)` - one subtree.
- `ApplyToAccount(name, path, value, target_email)` - a caller-supplied
  subtree, merged at `path`.

The target write takes the cross-process lock and reads-merges-writes, so
it is safe even if the target account is running.

## Python surface (`PyJson`)

No file lifecycle in Python; construction is the whole setup.

```python
from PyJson import JsonFile

doc = JsonFile("mybot.json")            # account-scoped by default
doc.set("ui/window/pos/x", 120)         # nested path, autosaved
x   = doc.get("ui/window/pos/x", 0)     # default declares type + fallback
x   = doc.get("ui/window/pos/x", int)   # type token -> typed read

doc.set_json("waypoints", [{"x": 1}, {"x": 2}])   # whole subtree from a list/dict
doc.append("waypoints", {"x": 3})                  # push onto an array
pts = doc.get_json("waypoints")                    # -> Python list of dicts

shared = JsonFile("global.json", scope="global")   # cross-account, locked
```

- `JsonFile(name, scope="account")` -> manager-owned document (`nodelete`).
  `scope` is `"account"`, `"global"`, or `"root"`.
- `set(path, value)` / `get(path, default_or_type)` - typed leaf access,
  same overload+type-token rules as `PySettings` (bool checked before int).
- `set_json(path, obj)` / `get_json(path)` - whole subtree as native
  Python dict/list/scalars.
- `append(path, value)`, `has(path)`, `delete(path)`, `keys(path)`,
  `size(path)`, `is_array(path)`, `is_object(path)`.
- Escape hatches: `save()`, `reload()`, `is_dirty()`, `is_bound()`,
  `path()`.
- Module helpers: `is_anchored()`, `get_json_directory()`, and the
  cross-account copy functions `copy_document_to_account`,
  `copy_path_to_account`, `apply_to_account`.

## Out of scope (seams left open)

- Per-node dirty tracking (the journal is whole-op, not diffed).
- JSON Schema validation on set.
- File watching / hot reload of external edits (`reload()` is explicit).
