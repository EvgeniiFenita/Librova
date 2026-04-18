---
name: analyze-logs
description: "Step-by-step guide for analyzing Librova import logs (host.log + ui.log): parse errors, genre mapping gaps, log noise, encoding fallbacks, duplicate detection, and actionable fix decisions. Specific to Librova FB2 import logs only — do NOT use for general log analysis. Use whenever the user provides log files after an import run."
---

# Import Log Analysis Playbook

## Goal

Turn `host.log` and `ui.log` into a short diagnosis: what failed, whether logging quality regressed, and which fixes should happen now versus later.

## When to Use

- use this skill when the user provides post-import logs
- use this skill when you need to compare two import runs or explain a cancellation timeline
- do **not** use this skill before you have actual `host.log` and `ui.log` data

## Expected Deliverable

Your final analysis should identify:

- the main failure or degradation pattern
- whether the logs are clean enough to trust
- whether the issue is parser / duplicate / conversion / rollback / noise / performance related
- which fixes are immediate and which ones can be queued

Log files after an import run live at `<LibraryRoot>\Logs\`:

- `host.log` — C++ native process (parsing, importing, IPC service)
- `ui.log` — C# UI process (job orchestration, IPC client calls)

---

## 1. Work Large-Log First, Not Line-By-Line

When the logs are hundreds of thousands of lines, **do not** start with `Get-Content` over the full file and do not retell the run chronologically. Start with:

1. severity and volume counts
2. top repeated message families
3. progress/throughput reconstruction
4. cancel timeline
5. only then drill into representative samples

Prefer streaming APIs over loading the entire file into PowerShell objects:

```powershell
$hostLog = "C:\...\Logs\host.log"
$uiLog   = "C:\...\Logs\ui.log"

# Cheap metadata
Get-Item $hostLog, $uiLog | Select FullName, Length, LastWriteTime

# Stream lines without Get-Content materializing the full file
[System.IO.File]::ReadLines($hostLog) | Select-Object -First 5
[System.IO.File]::ReadLines($hostLog) | Select-Object -Last 5
```

If you need grouped counts, prefer `Select-String ... | Measure-Object` or targeted regex extraction over broad full-file transforms.

---

## 2. Quick Health Check

Run these counts first to get a picture before reading individual lines:

```powershell
$log = "C:\...\Logs\host.log"

# Overall severity breakdown
Select-String -Path $log -Pattern "\[error\]|\[critical\]" | Measure-Object | Select Count   # hard failures
Select-String -Path $log -Pattern "\[warning\]"             | Measure-Object | Select Count   # soft skips
Select-String -Path $log -Pattern "\[info\]"                | Measure-Object | Select Count   # milestones
Select-String -Path $log -Pattern "\[debug\]"               | Measure-Object | Select Count   # should be 0 in file log

# Noise check: if info count is > 3× the book count, polling leaked to Info level
```

**Expected healthy ratios** after noise reduction work (task #118):

| Metric | Healthy |
|---|---|
| `[debug]` lines in log file | 0 |
| `[info]` lines | ~1–3 per imported book |
| `[warning]` lines | proportional to format quirks |
| `[error]` lines | only for genuine failures |

Treat these ratios as a baseline from large real-world import runs, not as a universal hard limit for every library.


---

## 3. Reconstruct Import Speed / Degradation

`ui.log` already contains enough progress snapshots to rebuild throughput over time.

```powershell
$uiLog = "C:\...\Logs\ui.log"
$re = '^(?<ts>\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d+ [+-]\d{2}:\d{2}) \[INF\] Import job in progress\. JobId=(?<job>\d+) Status="(?<status>[^"]+)" Percent=(?<percent>\d+) Imported=(?<imported>\d+) Failed=(?<failed>\d+) Skipped=(?<skipped>\d+) Message=(?<msg>.*)$'

$rows = foreach ($line in [System.IO.File]::ReadLines($uiLog)) {
    if ($line -match $re) {
        [pscustomobject]@{
            Ts       = [datetimeoffset]::Parse($matches.ts)
            Percent  = [int]$matches.percent
            Imported = [int]$matches.imported
            Failed   = [int]$matches.failed
            Skipped  = [int]$matches.skipped
            Msg      = $matches.msg
        }
    }
}

# Hourly throughput
$buckets = for ($i = 1; $i -lt $rows.Count; $i++) {
    [pscustomobject]@{
        Hour         = $rows[$i].Ts.ToString('yyyy-MM-dd HH:00')
        ImportedDelta = $rows[$i].Imported - $rows[$i - 1].Imported
        SkippedDelta  = $rows[$i].Skipped  - $rows[$i - 1].Skipped
        Minutes       = ($rows[$i].Ts - $rows[$i - 1].Ts).TotalMinutes
    }
}

$buckets |
    Group-Object Hour |
    ForEach-Object {
        $mins = ($_.Group | Measure-Object Minutes -Sum).Sum
        $imp  = ($_.Group | Measure-Object ImportedDelta -Sum).Sum
        $skip = ($_.Group | Measure-Object SkippedDelta -Sum).Sum
        [pscustomobject]@{
            Hour          = $_.Name
            ImportedPerMin = [math]::Round($imp / $mins, 1)
            SkippedPerMin  = [math]::Round($skip / $mins, 1)
        }
    }
```

**Interpretation heuristics:**

| Symptom | Likely meaning |
|---|---|
| imported/min falls smoothly for hours | scaling issue, not a single broken stage |
| skipped/min rises while imported/min falls | duplicate detection is getting expensive |
| imported counters freeze but progress rows continue | UI polling noise / post-cancel waiting |
| end-of-run throughput much lower than start | late-phase bottleneck (often storage or dedup) |

For native per-archive confirmation, use `zip: done`:

```powershell
Select-String -Path $hostLog -Pattern "zip: done job=" |
    Select-Object -ExpandProperty Line
```

The final `[import-perf] SUMMARY` is especially valuable — it can directly name bottlenecks such as `storage`, `dedup`, `db_wait`, `parse`, `zip_extract`, `cover`, `convert`.

> These benchmarks apply to lib.rus.ec FB2 ZIP imports only.
> Numbers will differ for other sources or archive layouts.

**Healthy perf-summary benchmark (lib.rus.ec FB2 ZIPs):**
- `throughput` ≥ 100 bk/s — acceptable; ≥ 150 bk/s — good
- `storage` 40–50% — normal for parallel writer path
- `parse` 20–30% — normal for FB2 with XML recovery
- `db_wait` 10–20% — normal; above 30% → investigate writer queue saturation
- `writer_queue=0` — no backpressure; any non-zero value → possible saturation
- `cover` < 5% — normal for lib.rus.ec (few covers); higher values for well-formatted archives with many embedded covers

---

## 4. Parse Errors (`[error]`)

```powershell
Select-String -Path $log -Pattern "\[error\]" | Select-Object -ExpandProperty Line
```

### Common error patterns and actions

| Pattern in line | Root cause | Action |
|---|---|---|
| `Failed to parse FB2` | XML fatal / encoding issue not recovered | Check file manually; may need new recovery path |
| `Failed to open file` | ZIP entry extraction failed, temp path issue | Check Unicode in path; check `PathFromUtf8` usage |
| `Missing required FB2 node` | Required field absent (after #118 only truly required ones throw) | If field is optional in practice → make it warn+skip |
| `pugixml parse error` | Strict XML parse AND description-fallback also failed | Inspect the actual FB2 for exotic encoding or binary noise |
| `converter returned error` | fb2cng conversion failed | Check converter configuration and fb2cng version |
| `Failed to move file from ...\Temp\...\cover.jpg` | Cover finalization/storage bug | Investigate file handles, temp cleanup order, destination collision |

---

## 5. Genre Mapping Gaps (`[warning]` → `no mapping`)

```powershell
Select-String -Path $log -Pattern "FB2 unknown genre code" |
    ForEach-Object { ($_ -split "'")[1] } |
    Group-Object |
    Sort-Object Count -Descending, Name |
    Select-Object Count, Name
```

This gives both the unique codes and their frequency. High-count misses should become immediate mapper fixes; singletons may be malformed data and should be reviewed before adding blindly.

This run also showed that **dirty real-world genre values are common**, not just canonical lib.rus.ec codes. Expect:

- localized labels (`Православие`, `Христианство`, `поэзия`)
- English free-text (`Prose`, `Religion`)
- mixed malformed values (`sci_history История`)
- obvious garbage / source artefacts (`105`, `fictionbook.cs`, `st`, `N/K`)
- mojibake (`accountingР‘СѓС…СѓС‡РµС‚`)

For each code:

1. Search lib.rus.ec genre reference (or just trust the code itself — usually self-descriptive).
2. Add to `libs/Parsing/Fb2GenreMapper.cpp` in the static map.
3. If the value is localized / malformed, decide whether it belongs in a normalization layer rather than the canonical map.
4. Add assertion to `tests/Unit/TestFb2GenreMapper.cpp` (first `TEST_CASE`, at end of the known-codes block).
5. Rebuild and re-run tests.

**Watch out for:**
- `literature_rus_classsic` — three 's', this is the actual code in files, not a typo
- Codes with hyphens vs underscores — match exactly what appears in the warning

---

## 6. Parser Soft Skips (`[warning]`)

```powershell
Select-String -Path $log -Pattern "\[warning\]" | Group-Object {
    if ($_ -match "sequence number") { "non-numeric sequence" }
    elseif ($_ -match "publish year")  { "non-integer year" }
    elseif ($_ -match "lang")          { "missing lang" }
    elseif ($_ -match "CP1251")        { "CP1251 fallback" }
    elseif ($_ -match "recovered metadata from <description> block") { "body XML recovery" }
    elseif ($_ -match "FB2 unknown genre code") { "unknown genre" }
    elseif ($_ -match "no non-empty author nodes") { "author fallback" }
    elseif ($_ -match "cover: image node present but binary id missing") { "cover missing binary id" }
    elseif ($_ -match "cover-reference-missing-binary-id") { "cover final not stored" }
    else                               { "other" }
} | Sort-Object Count -Descending | Select Name, Count
```

**Interpretation:**

| Warning type | Action |
|---|---|
| `non-numeric sequence` | Normal for lib.rus.ec guillemet corruption — already handled |
| `non-integer year` | Normal for "March 2004" style — already handled |
| `missing lang` | Normal for some files — already handled |
| `CP1251 fallback` | Expected for misdeclared Russian files — no action |
| `body XML recovery` | Expected for lib.rus.ec `<...>` / `<:>` body tokens — no action; ~7.5% rate is normal |
| `author fallback` | Usually metadata-quality issue only; keep as data-quality signal |
| `cover missing binary id` / `cover final not stored` | Cover-quality problem; ~0.7% rate normal for lib.rus.ec; investigate if > 5% |
| `unknown genre` | **Act**: add code to mapper (see §5) |
| `other` (new pattern) | Investigate — may be a new quirk to handle |

---

## 7. Encoding Fallbacks

```powershell
# CP1251 fallback rate
$cp1251 = (Select-String -Path $log -Pattern "CP1251 fallback").Count
$total  = (Select-String -Path $log -Pattern "Parsed FB2").Count
Write-Host "CP1251 fallbacks: $cp1251 / $total books"
```

A rate above 30–40% for a Russian-language archive is normal (many lib.rus.ec files declare UTF-8 but are actually CP1251).

---

## 8. Duplicate Detection

```powershell
Select-String -Path $log -Pattern "duplicate|Skipping.*already" | Measure-Object | Select Count
```

Duplicates are expected and intentional — same book appearing in multiple ZIPs or re-importing the same archive. No action unless the count seems unexpectedly low (detection broken) or unexpectedly high (hash collision false positives).

**Important noise note:** one duplicate may currently emit multiple warning lines:

- `duplicate: decision`
- `Duplicate candidate fields`
- `ZIP entry skipped ... rejected-duplicate`

Treat these as one logical event when judging severity.

---

## 9. Log Noise Check

If the log is suspiciously large or info count seems very high:

```powershell
# Check for polling leakage
Select-String -Path $log -Pattern "WaitImportJob|GetImportJobSnapshot|TryGetSnapshotAsync|WaitAsync" |
    Select-Object -First 5
```

If these appear at `[info]` level → noise reduction regressed. Check:
- `libs/ProtoServices/LibraryJobServiceAdapter.cpp` — polling calls must use `LogDebugIfInitialized`
- `apps/Librova.UI/Logging/UiLogging.cs` — file sink must have `restrictedToMinimumLevel: LogEventLevel.Information`
- `apps/Librova.UI/ImportJobs/ImportJobsService.cs` — `TryGetSnapshotAsync` and `WaitAsync(completed=false)` must call `UiLogging.Debug`

Also check for **success-path flood** in `host.log`:

```powershell
$patterns = @(
    '^\[.*\] \[info\] cover: final ',
    '^\[.*\] \[info\] cover: stored ',
    'duplicate: decision',
    'Duplicate candidate fields:',
    'ZIP entry skipped:',
    '\[import-perf\].*Slow book:'
)

foreach ($pattern in $patterns) {
    "{0}`t{1}" -f (Select-String -Path $hostLog -Pattern $pattern).Count, $pattern
}
```

Recent large-run evidence:

- `cover: final` and `cover: stored` can dominate **almost the entire info log**
- duplicate handling can fan out into **three warning lines per logical duplicate**
- `Slow book` warnings are useful, but thousands of them need aggregation, not eyeballing

For `ui.log`, repeated `Import job in progress` lines with **unchanged counters** are also pure noise and should be treated as polling flood, especially after cancel.

---

## 10. Cancellation Timeline

Always reconstruct cancellation from **both** logs.

```powershell
Select-String -Path $uiLog -Pattern 'Cancelling import job|CancelAsync completed|TryGetResultAsync completed|finished with status "Cancelled"' |
    Select-Object -ExpandProperty Line

Select-String -Path $hostLog -Pattern 'CancelImportJob|\[import-perf\] SUMMARY|Rolled back .* after cancellation|Database compacted after cancellation rollback|completed with status 4' |
    Select-Object -ExpandProperty Line
```

Then answer four separate questions:

1. **When was cancel requested?**
2. **When did active import work actually stop?**
3. **How long did rollback / cleanup / compaction take?**
4. **When did UI finally observe the terminal state?**

To detect post-cancel polling flood:

```powershell
# If every post-cancel row has identical counters, the UI is polling a frozen snapshot.
```

If import counters freeze almost immediately after cancel but completion comes much later, the problem is **not** active-stage cancellation checks — it is usually rollback / cleanup / database finalization.

---

## 11. ui.log Analysis

```powershell
$uilog = "C:\...\Logs\ui.log"

# Error summary
Select-String -Path $uilog -Pattern "\[ERR\]|\[FTL\]" | Select-Object -ExpandProperty Line

# Noise check
Select-String -Path $uilog -Pattern "TryGetSnapshotAsync|WaitAsync" | Measure-Object | Select Count
# Should be 0 — these are Debug and must not reach the file sink
```

Also inspect the progress stream itself:

```powershell
Select-String -Path $uilog -Pattern "Import job in progress\." |
    Select-Object -First 3

Select-String -Path $uilog -Pattern "Import job in progress\." |
    Select-Object -Last 3
```

If the tail shows many repeated rows with the same `Imported/Failed/Skipped/Percent/Message`, file logging is capturing polling rather than state changes.

---

## 12. Decision Matrix: Fix Now vs. Log Only

| Situation | Decision |
|---|---|
| New genre code appearing ≥ 5 times | Add to mapper now |
| New genre code appearing < 5 times | Document it in the findings; add or update a backlog note if it repeats across runs |
| New parser error pattern (not in §4) | Investigate file; add warn+skip if field is optional |
| Error causes complete import failure for many files | Fix immediately — critical |
| CP1251 / body-recovery warnings increasing | Expected; no action unless rate doubles |
| Polling messages in log | Investigate noise regression immediately |
| Final perf summary says `storage` or `db_wait` dominates and throughput falls over time | Treat as a real scaling bug |
| Cancel is accepted quickly but terminal state arrives much later | Investigate rollback / compaction observability and cost |

---

## 13. After Fixing Genre Codes

1. Add codes to `libs/Parsing/Fb2GenreMapper.cpp`
2. Add `REQUIRE` assertions in `tests/Unit/TestFb2GenreMapper.cpp`
3. Build: `cmake --build --preset x64-debug --config Debug`
4. Test: `ctest --test-dir out\build\x64-debug -C Debug --output-on-failure`
5. Optionally update backlog task note if a genre-mapper expansion was tracked
