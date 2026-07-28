# Database Manager - Pre-Design

Status: PRE-DESIGN. Not agreed for implementation, not implemented. This is a
LIVING document: it is updated as each decision lands, not at the end. It records
decisions, the alternatives that were rejected and why, and what is still open,
so that nothing has to be re-litigated.

Planned module: `database/` - a top-level module, sibling of `settings/` and
`json/`, NOT under `GW/` (it has no Guild Wars specificity). Following the
project's module anatomy:

```
include/database/database.h
src/database/database.cpp             lifecycle, daemon, registry
src/database/database_methods.cpp     the store's public surface
src/database/database_bindings.cpp    PYBIND11_EMBEDDED_MODULE
```

No `_patterns.cpp` - nothing to resolve. The embedded module must be added to
`kEmbeddedModules[]` in `src/base/python_runtime.cpp`, and CMake must be
re-configured once so the glob picks up the new folder.

## 1. Purpose

A container for data dumped from the game that grows over time. It replaces
per-module JSON files.

**What is wrong with a JSON file:** reads parse the whole file and writes rewrite
the whole file. Cost scales with the size of the store, not with the entry you
touched. There is also no safe story for several clients using it at once.

**What SQLite is being used for here:** it is a FILE FORMAT, not a query engine.
Three properties, none of which any alternative provides together:

1. **Incremental writes** - change one record, write one record.
2. **Correct multi-process concurrency** - WAL, locking, crash safety, already
   solved and already correct.
3. **Atomic commits** - including the change record in the same transaction as
   the data it describes.

SQL capability exists and is used internally where it helps, but the design does
NOT depend on the query planner: reads are served from memory (section 9).

## 2. Hard constraints

These come from the project, not from the design:

- **C++ implementation.** All management lives in the native DLL.
- **The user never handles locks, transactions, or SQL.** End users, including
  Python coders, are inexperienced. Anything that can be got wrong will be. The
  system must be self-managing in the same sense as `Settings` and `JsonFactory`.
- **The user cannot be relied on to use it efficiently.** They will not hoist
  reads out of a frame loop or batch them. The class must force the cheap path,
  because the expensive path must not exist.
- **Multibox is the normal case.** Several processes read and write the same
  data concurrently.
- **Write latency matters more than write volume.** Frequent small writes across
  many clients, not occasional bulk dumps.
- **Record shapes mutate at runtime.** A module's dictionary is not fixed. This
  single fact rules out declared schemas (section 5).
- **Data volume is bounded.** Tens of thousands of short, concise records per
  store. Not gigabytes, no massive intake. Everything fits in memory.
- **No telemetry databases.** Removed from scope.

## 3. Engine choice

SQLite, vendored as the amalgamation (`sqlite3.c` + `sqlite3.h`) under
`third_party/`, matching how freetype, imgui, minhook, nlohmann and pybind11 are
already vendored. Public domain, no transitive dependencies, no build system of
its own.

### Compile defines that matter here

- `SQLITE_THREADSAFE=1` - required. The Reforged Python worker thread free-runs
  and is not phase-locked to the render thread, so bindings can be entered
  off-thread.
- `SQLITE_DQS=0` - double-quoted string literals off, so a mistyped identifier
  is an error rather than silently becoming a string.
- `SQLITE_OMIT_LOAD_EXTENSION` - no arbitrary DLL load surface inside the game
  process.
- `SQLITE_DEFAULT_MEMSTATUS=0`, `SQLITE_OMIT_DEPRECATED` - smaller and faster.

Cost: roughly 700 KB to 1 MB of DLL size, and one large translation unit that
adds time to a clean build only (static library, so incremental builds pay
nothing).

### Rejected engines

- **A JSON file per store** - the problem being solved. Not incremental, no
  multi-process story.
- **A custom binary format** - means writing your own write-ahead log and
  cross-process locking. That is precisely the part of SQLite you would be
  reimplementing, worse.
- **Shared memory** - not durable. (It remains the correct home for live
  coordination state that does not need to survive a restart.)
- **LMDB** - also a schemaless key-value store, so it is the SAME model, plus
  32-bit address-space risk: it memory-maps the whole store and needs a
  pre-declared maximum size.
- **LevelDB** - single-process only (exclusive lock file). Multibox rules it out.
- **RocksDB** - large, C++17 heavy, runs background compaction threads inside
  the game process.
- **DuckDB** - analytical engine, tens of MB, poor 32-bit story, optimized for
  scans rather than per-row updates.
- **Pickle** - forbidden by project persistence rules, unreadable by any other
  tool, fragile across versions.

## 4. Storage layout

One file per module. A module names its store the way it already names a settings
file:

```python
DataStore('scanner.db')
DataStore('party.db')
```

That naming act is the whole of the user's decision.

```
db/
  Global/<name>.db      shared by every client on the machine
  <email>/<name>.db     per client (see section 10)
```

### Why one file per module

- **Write lock isolation.** SQLite allows one writer at a time per FILE. Two
  modules in one file serialize; in two files they never meet.
- **WAL and checkpoint pressure stays contained.** A noisy module does not slow
  everyone else's readers.
- **Reset and corruption blast radius.** Wiping a regenerable store is deleting
  a file.

What splitting does NOT buy: anything on reads. WAL already gives unlimited
concurrent readers that never block each other or the writer. It also does
nothing when several clients contend on the SAME store; it isolates different
stores from one another.

What splitting costs: no `JOIN` across files and no atomic transaction spanning
files (SQLite will not commit atomically across attached databases in WAL mode).
For a key/value store that cost is close to zero.

### Distribution and version control

Both tables are created by C++ on first connect. There are no schema files to
ship, so the whole tree is generated at runtime:

```gitignore
db/**
```

`db/**` covers `-wal` and `-shm` automatically. A clean clone plus one launch
must produce a working store with no manual step. That is the acceptance test.

## 5. Tables

Two tables, fixed forever, created by C++. **No DDL ever runs after creation.**

```sql
CREATE TABLE entries (
    key     TEXT PRIMARY KEY,      -- 'agents/1234'
    value                          -- JSON text, or raw bytes (section 6)
    updated INTEGER NOT NULL,      -- GetTickCount64() at write; informational
    owner   TEXT                   -- which client wrote it
) WITHOUT ROWID;

CREATE TABLE sync (
    id    INTEGER PRIMARY KEY AUTOINCREMENT,  -- monotonic; drives propagation
    owner TEXT,                               -- which client committed
    keys  TEXT,                               -- JSON array of keys touched
    ts    INTEGER
);
```

- `key` is a slash-namespaced string. A prefix lookup uses the primary-key index.
- `owner` answers "what did client X publish", a real multibox question that no
  module should have to remember to record.
- `updated` is informational. It does NOT drive synchronization; `sync` does.
- `sync` is written in the same transaction as the rows it describes. Section 9
  explains why that property is the whole point.

### Why there is no schema, and no registration

An earlier direction had modules `register()` a dictionary shape, which C++ would
turn into a real table with typed columns and real indexes. It was attractive:
indexed field queries, smaller storage, a file that reads well in SQLiteStudio.

**It was rejected because record shapes mutate at runtime.** Declared tables mean
DDL, and DDL for a shape that keeps changing is exactly the fragility to avoid:
tables reshaped mid-run, columns out of step with what a module now stores, and a
migration problem on every change of mind. Registration is only safe when shapes
are stable, and these are not.

Schemaless has the corresponding virtue: **there is no shape to break.** A record
is whatever the module put there, and a module changing its mind mid-run is just
a different dict in the same column.

What is given up, and why each is acceptable here:

- **Indexed field queries at the engine level** - unnecessary, because everything
  is in memory and filtering happens there (section 7).
- **Typed columns** - costs storage, not correctness.
- **A tidy view in SQLiteStudio** - a debugging convenience.

If a single field ever does become hot enough to want a real index, SQLite can
add one over JSON additively, with no migration and no data rewrite:

```sql
ALTER TABLE entries ADD COLUMN f_map_id INTEGER
    GENERATED ALWAYS AS (json_extract(value,'$.map_id')) VIRTUAL;
CREATE INDEX ix_f_map_id ON entries(f_map_id);
```

This is recorded as an available escape hatch, not as part of the design. It is
also the reason the value encoding is JSON specifically rather than a binary
format: `json_extract` is what keeps that door open.

### Rejected table shapes

- **Registered per-shape tables** - see above; runtime-mutable shapes.
- **A dictionary table plus a multipurpose table with generated columns** -
  considered, and a good idea when shapes are stable. Superseded by having no
  schema at all, which needs neither.
- **Entity-attribute-value (one row per field)** - reconstructing one object
  becomes N rows, type information is lost, and a write becomes many rows instead
  of one. Worse for the fast-write case that matters here.
- **Sync state as a reserved key prefix inside `entries`** - pollutes the key
  space; every `keys()` / `count()` result would need filtering.

## 6. Values and types

**The Python contract is dicts in, dicts out.** JSON is the storage and in-memory
encoding only; it is never visible from Python. No method accepts or returns a
JSON string.

```
Python           binding layer            everything below
------           -------------            ----------------
dict      ->     PyToJson()        ->     nlohmann::json  ->  mirror, queue, SQLite
dict      <-     JsonToPy()        <-     nlohmann::json  <-  mirror
```

The binding layer is the ONLY place the two worlds meet.

`bytes` bypasses JSON entirely and is stored raw as a BLOB, so binary dumps go in
as-is.

**Note on the usual JSON objection.** "You have to parse the whole thing on every
read" is true of a JSON *file* and is not true here: parsing happens once at open,
and thereafter only for records a peer actually changed. Reads never parse.

### The converter

`JsonToPy()` and `PyToJson()` already exist and are in production use - they are
what makes `PyJson.get_json()` return real dicts today. They currently live as
file-local helpers in `src/json/json_factory_bindings.cpp` (lines 30 and 65).

**DECIDED: lift them into a shared header, `include/base/json_convert.h`**, used
by both the json module and the database module. One implementation, one place to
fix a type-mapping bug.

This knowingly departs from the precedent in `json-factory-design.md` ("a parallel
module, not a shared/refactored base, so the working Settings module is never
touched"). That rule was written about a large lifecycle layer where the modules
were expected to evolve apart. This is roughly forty lines of pure, stateless
conversion that will never differ. Editing `json_factory_bindings.cpp` to consume
the header is accepted as part of this change.

### No type column, and nothing returns as a string

Two type systems already carry that information:

- SQLite stores a storage class per VALUE, not per column, so one untyped column
  holds JSON text in one row and a binary blob in the next.
  `sqlite3_column_type()` reports which.
- JSON is self-describing, so nlohmann restores number, bool and string
  faithfully.

**Consequence: no typed getter family.** `get_int` / `get_float` / `get_str` /
`get_bool` exist on `Settings` only because an INI file literally is text and the
default has to select the type. That does not carry over. A single `get()` returns
the right Python type; `default` is a fallback for a missing key, not a type hint.
One fewer concept than the sibling classes, not one more.

**Round-trip caveat:** JSON has no tuple and no non-string object keys. A `tuple`
returns as a `list`, and `{1: 'a'}` returns as `{'1': 'a'}`. Everything else
(nested dicts, lists, int, float, bool, str, None) round-trips exactly.

## 7. Python surface

No `begin`, `commit`, `transaction`, `cursor`, `execute`, or SQL anywhere.
Nothing to hold between calls, nothing to leak, nothing to get wrong.

```python
store = DataStore('scanner.db')

store.set('agents/1234', {'hp': 480, 'alive': True})
store.get('agents/1234', default=None)
store.has('agents/1234')
store.delete('agents/1234')

store.set_many({...})
store.get_many('agents/')
store.keys('agents/')
store.count('agents/')
store.clear('agents/')

store.find('agents/', {'map_id': 42})               # see below
```

### Filtering: `find()` (answers open question 1)

`find()` scans the mirror in C++ and converts only the matches.

The reason it exists rather than leaving users to filter in Python is conversion
cost. Building a Python dict costs 1-2 us per record, and it dominates everything
else:

| 10,000 entries, 50 matches | cost |
| -------------------------- | ---- |
| `get_many()` then filter in Python | build 10,000 dicts (~15 ms), discard 9,950 |
| `find()` filtering in C++          | compare 10,000 in memory (~0.5-1 ms), build 50 |

An inexperienced user will write the Python filter, because it is the obvious
thing to write. `find()` makes the cheap path the easy one.

**Scope, and the deliberate stop line.** The principle is **C++ narrows, Python
refines**: the filter's job is not to express every question, but to cut the
result set down until Python work is free.

Built now:

```python
store.find('agents/', {'map_id': 42})
store.find('agents/', {'map_id': 42, 'alive': True})   # fields ANDed
store.find('agents/', {'pos/x': 100})                  # nested path, JsonFactory style
```

Named but deferred (purely additive, no call-site or data change): comparison
operators, `sort_by`, `limit`.

**Never:** OR, nested boolean expressions, joins, aggregates, expression strings,
user-supplied SQL. Anything beyond the above is done in Python on the narrowed
set. Without this line, "a querying and filtering system" becomes SQL rebuilt on
a hash map.

Rejected: a Python predicate callback (`find(prefix, lambda v: ...)`). Maximum
expressiveness and no API to design, but running the callback means building a
Python dict for every candidate - exactly the cost being avoided.

**The recommended pattern that avoids needing `find()` at all:** put the
discriminator in the key.

```python
store.set('agents/map42/1234', {...})
store.keys('agents/map42/')      # "all agents in map 42" is now a prefix lookup
```

No scan, no filter, no conversions of non-matches. It is the same act as choosing
a sensible filename and should be documented as the default advice.

### Atomic operations (the part that actually protects users)

The mistake an inexperienced user makes is not forgetting a lock. It is **read,
modify, write**: two clients both read an entry, both change a field, both write,
and one silently loses. Internal locking cannot fix that, because the gap is
between two of their calls.

So mutations that would otherwise be read-modify-write are single atomic calls
executed inside C++:

```python
store.merge('party/state', {'leader': me})     # patch fields; concurrent merges
                                               # to other fields survive
store.increment('stats/runs', 'completed', 1)
store.append('log/events', {...})              # push onto a stored list
```

### What must never appear in the Python surface

- No method taking or returning a JSON string.
- No `json.dumps` / `json.loads` in the Python wrapper.
- No `import json` in the wrapper at all.
- No "raw" or "as_text" accessor that leaks the encoding.

## 8. Concurrency

All inside C++. None of it visible from Python.

- `PRAGMA journal_mode = WAL` - unlimited concurrent readers that never block
  each other or the writer.
- `PRAGMA synchronous = NORMAL` - no fsync per commit. Commits become buffered
  appends; fsync happens at checkpoint. What is given up is loss of recent
  commits on power loss or OS crash. A **process** crash cannot lose them, since
  the WAL is already in the OS page cache. Given that the failure mode that
  actually happens is the game client dying, this is the right trade and is the
  standard WAL recommendation.
- `PRAGMA busy_timeout` - cross-process contention retry.
- **`BEGIN IMMEDIATE` always.** `busy_timeout` does NOT rescue a deferred
  transaction that upgrades from read to write: `BEGIN`, `SELECT`, then `INSERT`
  with a peer having written in between returns `SQLITE_BUSY` immediately and the
  busy handler is never invoked, because the read snapshot is stale and SQLite
  cannot safely retry. Taking the write lock up front avoids it. One word, and it
  is the difference between working and failing randomly under multibox.

### Rejected concurrency approaches

- **A named cross-process mutex on top** (as `json/` uses). Redundant; SQLite's
  locking is already correct and a second layer only adds deadlock ordering.
- **Holding a transaction open across a throttle window.** The intuitive
  translation of the `Settings` autosave model, and it breaks multibox: an open
  write transaction holds the file's write lock, so every other client blocks for
  the full window. Eight clients become eight two-second queues.
- **Table-level locking.** Does not exist. SQLite locks the file.

## 9. Internals: mirror, queue, daemon

This is the core of the design. Reads must be cheap because consuming code pulls
data every frame and the user cannot be relied on to batch or hoist. So the class
does it for them, and the expensive path is not offered.

### Five pieces

| piece | what it is |
| ----- | ---------- |
| the file | one SQLite database per store: `entries` + `sync` |
| the mirror | in-memory map, key -> parsed `nlohmann::json`. Reads hit only this. |
| the write queue | pending ops, coalesced by key (newest wins) |
| the daemon | one background thread, owned by the registry |
| the registry | singleton owning every open store and the daemon |

### What each user call does

- **`get(key)`** - lock, look up in the mirror, convert that one value to a dict,
  unlock. No SQLite, no parse, no disk.
- **`set(key, value)`** - convert dict to json on the calling thread, lock, write
  to the mirror, put the op in the queue replacing any pending op for the same
  key, unlock. The caller never waits for disk or for a lock held by a peer.
- **`find(...)`** - lock, scan the mirror, convert only the matches, unlock.
- **`delete(key)`** - erase from the mirror, queue a delete op.

Every one is memory work under a short lock. Nothing blocks on I/O.

### Open

Resolve the path, apply pragmas, create the two tables if missing,
`SELECT key, value FROM entries` into the mirror, record the current
`data_version` and max `sync.id` as the baseline, register with the daemon.

### The daemon loop, per store, every N ms

**Phase 1 - drain writes**

```
lock -> swap the queue out for an empty one -> unlock      (callers keep working)
if anything to write:
    BEGIN IMMEDIATE
      INSERT INTO entries ... ON CONFLICT(key) DO UPDATE    (or DELETE)
      INSERT INTO sync (owner, keys, ts)                    -- same transaction
    COMMIT
    advance last_seen_id past our own sync row
```

**Phase 2 - pull peer changes**

```
v = PRAGMA data_version
if v unchanged:  done
else:
    SELECT id, keys FROM sync WHERE id > last_seen_id
    if last_seen_id is below the pruned floor -> full reload instead
    SELECT key, value FROM entries WHERE key IN (...)
    parse them (C++ only, no GIL)
    lock briefly -> splice into the mirror -> unlock
```

**Phase 3 - housekeeping**, occasionally: prune old `sync` rows. WAL
checkpointing takes care of itself.

### Shutdown

Signal the daemon, join it, final drain, close connections. Not detached, never
started from `DllMain`, joined before unload.

### Invariants

- **Only the daemon touches SQLite.** Every threading question collapses to "one
  thread does that."
- **The mirror holds C++ json, never Python objects**, so the daemon never needs
  the GIL. Conversion happens in `get()` on the calling thread, which already
  holds it. This costs nothing, because a fresh dict per call is required anyway
  (see the footgun in section 11).
- **Locks cover memory operations only, never I/O.** The queue is swapped out
  before writing; the mirror is spliced after parsing.
- One thread in the registry, **not** one per store.
- Local writes are visible instantly. Peer writes land within one poll interval.

### Two races, and how they are handled

- **A caller writes while the daemon is mid-drain.** The queue was already
  swapped, so the new op lands in the fresh queue and goes out next cycle. The
  mirror already has it, so nothing looks stale locally.
- **The daemon pulls a peer change for a key with a local write still pending.**
  Left alone, the peer's older value would clobber the newer local one in the
  mirror. So when splicing, **skip any key with a pending write in the queue** -
  the local value is newer and is about to be committed anyway. Easy to miss, and
  it would present as a value that mysteriously reverts for a frame.

### The sync log, and why it lives inside each database

The change record is written **in the same transaction as the data**. That single
property is why sync state lives inside each store file rather than in a separate
system database or a shared-memory flag: a peer can never see a signal for data
that is not there yet, or data with no signal. There is no "commit then signal"
ordering to get wrong, because there is no gap. It also puts the signal under the
same write lock as the data it describes, so contention stays exactly as
distributed as the data - no central hot spot.

**What the log fixes that a timestamp delta could not:**

- **No ties, nothing missed.** An `updated >= ?` delta needed a fudge because two
  writes in the same millisecond could tie. A monotonic integer has no ties.
- **Deletions need no tombstones.** A delete is simply a key listed in a change
  record, so `entries` stays clean and there is nothing to purge from it.

### `data_version` as the gate

`PRAGMA data_version` changes whenever the file has been modified by any
connection OTHER than yours. Reading it touches no table, takes no lock and hits
no disk. Unchanged means there is nothing to do. It also backstops writes that
never went through our path at all - an external tool, a migration script, or
SQLiteStudio - which would leave no sync record.

### Rejected propagation mechanisms

- **Inferring changes from `updated` timestamps alone.** We control the write
  path, so the system should state what changed rather than deduce it.
- **A separate central "system database" for signalling.** Still requires polling
  (a database cannot push), and every client signalling on every commit turns
  that one file into a write-lock hot spot - the exact thing splitting files was
  meant to avoid. It also cannot be atomic with the data commit.
- **A shared-memory flag or named event.** Cheaper to poll and capable of true
  push, but outside the transaction, so it reintroduces the ordering gap, and it
  is a second system to keep consistent with the database.
- **Watching the `-wal` / `-shm` files.** Racy, expensive, and only reports that
  something changed, not what.
- **`sqlite3_update_hook`.** Fires only for writes on your OWN connection, so a
  peer's write is invisible to it.

### Write path and coalescing

The queue exists to decouple the caller from **lock waiting**, not to batch disk
I/O - with `synchronous = NORMAL` a commit is already a cheap memory append. With
frequent small writes across several clients, a caller writing directly sits in
`busy_timeout` while a peer holds the lock. Enqueueing returns immediately and the
daemon absorbs the contention.

**Coalescing is the main win.** If a client republishes its current state
repeatedly, only the last value matters. A queue keyed by `key` collapses pending
writes, keeping the newest and discarding the superseded, turning (for example)
60 writes/sec into a handful of commits with an identical final state. That is an
order-of-magnitude reduction in lock acquisitions per client and it is only
possible with a queue in front.

Trade to be explicit about: a coalesced write that has not drained is lost if the
client dies. For *current state* that is correct - it is a value that would have
been overwritten anyway. For a store where every write is an event that must not
vanish, coalescing is wrong and that store needs a no-coalesce path (open
question 5).

**The mirror removes the queue's usual drawback.** Normally a queued write is not
visible to a subsequent read. Here the read is served from memory, where the write
already landed. The user gets synchronous semantics with asynchronous disk,
without knowing either exists.

### Staleness bound

Peer data is up to **one poll interval** stale. That is inherent to polling. A
module cannot see another client's write within the same frame it happened. Local
writes are exempt.

### Log pruning and the full-reload fallback

The `sync` log grows and must be pruned to a bound. A client away long enough for
its `last_seen_id` to be pruned cannot compute a delta and must fall back to a
**full reload** of the mirror. This is normal for change-log designs, is correct
behaviour rather than a failure, and doubles as the recovery path if a client ever
loses its place.

## 10. Shared vs per-client files

- `db/Global/<name>.db` - one file all clients share. Simple. Writers serialize
  on one lock.
- `db/<email>/<name>.db` - a file per client. **No write contention at all**, not
  reduced but absent, by construction. Readers open peers' files read-only and
  C++ merges. WAL readers never block, so reading eight files concurrently costs
  nothing in contention.

For the frequent-small-writes-from-many-clients profile, per-client is the
stronger default. The user does not choose a locking strategy; they name a store
and the module decides.

## 11. Cost model

Measured figures are required before any optimization (project rule:
reasoning-only performance changes have historically been neutral or
regressions). The estimates below are for sizing, not for justifying a change.

**With the mirror**, a `get()` is:

| step | rough cost |
| ---- | ---------- |
| pybind11 call boundary | ~1 us |
| mutex plus in-memory lookup | < 1 us |
| `JsonToPy` building a Python dict | 1-2 us |

Roughly **2-4 us per small record**, with no SQLite access and no JSON parsing on
the calling thread at all. An uncached read would add a SQLite index lookup
(1-5 us, page-cache warm) plus an nlohmann parse (1-3 us), i.e. 5-15 us.

The deciding number is keys-per-frame: 10 keys/frame at 60 FPS is about
0.1 ms/frame; 500 keys/frame uncached is about 5 ms/frame, roughly 30 percent of
a 16.6 ms frame. The mirror is what keeps this off the frame budget.

**Memory sizing (answers open question 2).** Tens of thousands of short records.
`nlohmann::json` is not compact - a small record with six fields is realistically
300-800 bytes resident. At 50,000 records that is roughly **25-40 MB per client**,
versus perhaps 5 MB if values were packed typed tuples. Survivable in a 32-bit
process, and each client pays its own copy, but worth knowing rather than
discovering.

If it ever does bite, the mitigation is a compact in-memory representation rather
than json trees. Noted as available; not built.

**Footgun the mirror must avoid:** do not hand back the SAME Python dict from
memory. The user mutates it and has silently changed the mirror without writing to
disk, while every other reader sees the edit. Hand back a fresh dict per call.
(This is also why the mirror holds C++ json rather than Python objects.)

## 12. Deliberately out of scope

Not designed, not to be added without being asked for:

- Schema versioning and migrations (there is no schema to migrate).
- Streaming cursors for large result sets.
- Read-only bundled catalogs shipped in the source tree.
- Backup, export, and import.
- Corruption recovery beyond `PRAGMA integrity_check` on connect.
- Telemetry.

## 13. Open questions

1. ~~Field-level search~~ - **ANSWERED.** `find()` scans the mirror in C++ and
   converts only matches; equality and ANDed fields built now, comparison / sort
   / limit named but deferred, hard stop line recorded (section 7).
2. ~~Store size~~ - **ANSWERED.** Tens of thousands of short records; everything
   fits in memory. Sizing estimate in section 11.
3. **Default placement.** Shared (`db/Global/`) or per-client (`db/<email>/`) as
   the default, and is there a case for a module asking for the other?
4. **Names.** Class and embedded module names are not settled (`DataStore`,
   `PyDatabase`, and `DBMgr` have all been used informally).
5. **Which stores must never coalesce**, i.e. where every write is an event
   rather than a current-state republish.
6. **Poll interval**, which sets the staleness bound for peer data. Fixed, or
   per-store?
7. **Sync log retention bound**, which sets how long a client can be absent
   before it must full-reload.

## 14. Decision log

Newest last. Recorded so settled points are not reopened.

- SQLite chosen; JSON files, custom binary formats, shared memory, LMDB, LevelDB,
  RocksDB, DuckDB and pickle all rejected, each for a stated reason.
- No SQL surface exposed to Python at all (reverses an early proposal to expose
  `execute(sql, params)`).
- Telemetry removed from scope.
- One file per module.
- Values stored as JSON; Python contract is dicts in, dicts out; no typed getter
  family.
- Change propagation is an explicit sync log inside each database file, written in
  the same transaction as the data - not inference, not a central system database,
  not a shared-memory flag.
- The registry singleton runs a daemon thread that owns all SQLite access; reads
  become pure memory lookups.
- The mirror holds `nlohmann::json`, not Python objects, so the daemon never needs
  the GIL.
- `JsonToPy` / `PyToJson` lifted into a shared `include/base/json_convert.h`.
- Open question 1 answered: `find()` in C++ over the mirror, with an explicit stop
  line, plus key-prefix design as the recommended way to avoid needing it.
- Open question 2 answered: everything fits in memory (tens of thousands of short
  records).
- **Declared schemas / `register()` considered and rejected** - record shapes
  mutate at runtime, so DDL would be a recurring fragility. Schemaless has no
  shape to break. Generated columns over `json_extract` recorded as an additive
  escape hatch if a single field ever becomes hot.
- SQLite is used as a **file format** (incremental writes, multi-process safety,
  atomic commits), not as a query engine. Reads are served from memory.
