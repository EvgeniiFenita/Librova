---
name: analyze-logs
description: "Step-by-step guide for analyzing Librova import logs (host.log + ui.log): parse errors, genre mapping gaps, log noise, encoding fallbacks, duplicate detection, and actionable fix decisions. Use whenever the user provides log files after an import run."
---

# Import Log Analysis Playbook

Log files after an import run live at `<LibraryRoot>\Logs\`:

- `host.log` — C++ native process (parsing, importing, IPC service)
- `ui.log` — C# UI process (job orchestration, IPC client calls)

---

## 1. Quick Health Check

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

---

## 2. Parse Errors (`[error]`)

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

---

## 3. Genre Mapping Gaps (`[warning]` → `no mapping`)

```powershell
Select-String -Path $log -Pattern "no mapping for FB2 genre" |
    ForEach-Object { ($_ -split "'")[1] } |
    Sort-Object | Get-Unique
```

This gives the exact list of unrecognized genre codes. For each code:

1. Search lib.rus.ec genre reference (or just trust the code itself — usually self-descriptive).
2. Add to `libs/Fb2Parsing/Fb2GenreMapper.cpp` in the static map.
3. Add assertion to `tests/Unit/TestFb2GenreMapper.cpp` (first `TEST_CASE`, at end of the known-codes block).
4. Rebuild and re-run tests.

**Watch out for:**
- `literature_rus_classsic` — three 's', this is the actual code in files, not a typo
- Codes with hyphens vs underscores — match exactly what appears in the warning

---

## 4. Parser Soft Skips (`[warning]`)

```powershell
Select-String -Path $log -Pattern "\[warning\]" | Group-Object {
    if ($_ -match "sequence number") { "non-numeric sequence" }
    elseif ($_ -match "publish year")  { "non-integer year" }
    elseif ($_ -match "lang")          { "missing lang" }
    elseif ($_ -match "CP1251")        { "CP1251 fallback" }
    elseif ($_ -match "description-only") { "body XML recovery" }
    elseif ($_ -match "no mapping")    { "unknown genre" }
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
| `body XML recovery` | Expected for lib.rus.ec `<...>` / `<:>` body tokens — no action |
| `unknown genre` | **Act**: add code to mapper (see §3) |
| `other` (new pattern) | Investigate — may be a new quirk to handle |

---

## 5. Encoding Fallbacks

```powershell
# CP1251 fallback rate
$cp1251 = (Select-String -Path $log -Pattern "CP1251 fallback").Count
$total  = (Select-String -Path $log -Pattern "Parsed FB2").Count
Write-Host "CP1251 fallbacks: $cp1251 / $total books"
```

A rate above 30–40% for a Russian-language archive is normal (many lib.rus.ec files declare UTF-8 but are actually CP1251).

---

## 6. Duplicate Detection

```powershell
Select-String -Path $log -Pattern "duplicate|Skipping.*already" | Measure-Object | Select Count
```

Duplicates are expected and intentional — same book appearing in multiple ZIPs or re-importing the same archive. No action unless the count seems unexpectedly low (detection broken) or unexpectedly high (hash collision false positives).

---

## 7. Log Noise Check

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

---

## 8. ui.log Analysis

```powershell
$uilog = "C:\...\Logs\ui.log"

# Error summary
Select-String -Path $uilog -Pattern "\[ERR\]|\[FTL\]" | Select-Object -ExpandProperty Line

# Noise check
Select-String -Path $uilog -Pattern "TryGetSnapshotAsync|WaitAsync" | Measure-Object | Select Count
# Should be 0 — these are Debug and must not reach the file sink
```

---

## 9. Decision Matrix: Fix Now vs. Log Only

| Situation | Decision |
|---|---|
| New genre code appearing ≥ 5 times | Add to mapper now |
| New genre code appearing < 5 times | Log as known gap; add to mapper in next batch |
| New parser error pattern (not in §4) | Investigate file; add warn+skip if field is optional |
| Error causes complete import failure for many files | Fix immediately — critical |
| CP1251 / body-recovery warnings increasing | Expected; no action unless rate doubles |
| Polling messages in log | Investigate noise regression immediately |

---

## 10. After Fixing Genre Codes

1. Add codes to `libs/Fb2Parsing/Fb2GenreMapper.cpp`
2. Add `REQUIRE` assertions in `tests/Unit/TestFb2GenreMapper.cpp`
3. Build: `cmake --build --preset x64-debug --config Debug`
4. Test: `ctest --test-dir out\build\x64-debug -C Debug --output-on-failure`
5. Optionally update backlog task note if a genre-mapper expansion was tracked
