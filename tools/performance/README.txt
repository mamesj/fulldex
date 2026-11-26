DS/Switch six-setting comparison

This tool PREPARES runs and analyses logs. It does not execute a game, connect to
a Switch, install a build, copy saves, or change settings on a device.

Use 192/360/720-line world detail, each with interpolation off/on. Actual game
emulation remains at its normal rate. The interpolation switch changes extra
3D pictures, not the DS CPU's clock or the intended game speed.

Builds to compare:
  baseline = your chosen source baseline plus the wait-counter diagnostic fix
  patched  = the SAME source and diagnostic fix plus the three rendering patches
Use the exact same toolchain, assets, power policy and settings on both builds.
The diagnostic fix must be on BOTH: old SSR_PERF resets wait counters every 300
frames, producing an unsigned-underflow error in the next CSV frame. Synthetic
actual-source regression reproduces this at frame 301 and passes with the fix.

PREPARE
python3 tools/performance/ds_matrix.py generate --out PRIVATE_NEW_FOLDER \
  --scene "game and exact scene description" --state "private-scene.state" \
  --input-script "private-input.txt"

--input-script is optional for a stationary scene. It and the snapshot must be
matching, player-supplied COPIES. The generator reads neither. Input-script
frame numbers are relative to the snapshot, as in the existing TestRun system.
Do not use a script that toggles fast forward, pause, or settings.

The plan has 18 runs per build (six settings, three repetitions) with the same
seeded case order on both builds, shuffled between repetitions. Defaults are
1800 warm-up frames then 3600 measured frames, about 90 seconds per run at normal
speed: around 27 minutes per build, 54 minutes for both, plus starts and any
slowdown. Warm the device under load before each batch as well; compare warm
versus sustained behavior separately. Repeat another session in reversed build
order if heat or battery state could bias results.

ON THE SWITCH - HARDWARE OPERATOR
Use an isolated private copy of the app's SD folder, not the normal playing
folder. Preserve any existing test.txt and test-results. Copy the matching
snapshot and optional input script beside the test copy of the program; confirm
both exist and the last selected game is the matching DS game BEFORE activating
test.txt. A missing snapshot makes the existing runner do an ordinary start.

Copy only the chosen variant's generated test.txt and perf-matrix/config.cfg
into that private app folder. The existing Switch TestRun runner executes one
case per start and restarts itself when possible. It records game.log and
timing.csv under test-results/<unique-run-name> plus progress.txt. If automatic
restart is unavailable, start the same private program again for the next run.
Keep both result batches and the generated plan.json together off the device.
Do not reuse the same session after modifying a build; generate a new one so
progress records cannot silently skip old run names.

Runs use --no-save, --keep-settings, no auto-states, no screenshots, no adaptive
detail, no fast forward, and a private settings file with achievements disabled.
The private launcher still maintains its normal startup/progress marker files;
this is why an isolated app folder is required. No clock changes are requested.
Verify from logs that JIT/recompilation mode and the actual deko3D/software
backend are the same. If a renderer falls back, repeat or label it separately.
Keep handheld/docked/display modes fixed. At least 720 output lines are needed
for this comparison; wide_lines in the CSV is a cap, not actual target size.

Fill device-notes.json honestly: device, power/display mode, actual renderer,
both source commits, matching scene, clock policy, prior warm-up and audio/input
checks. Record temperatures/clocks and unexpected backend changes from game.log
alongside it. Do not infer dropped pictures, sound underruns or GPU time from a
single CSV column; the CSV cannot certify those by itself.

ANALYSE
python3 tools/performance/ds_matrix.py analyse --plan PRIVATE_NEW_FOLDER/plan.json \
  --baseline-results COPIED_BASELINE/test-results \
  --patched-results COPIED_PATCHED/test-results \
  --notes PRIVATE_NEW_FOLDER/device-notes.json --out comparison.json

Exit 0 means complete logs AND the manual device checks were confirmed. Exit 2
means incomplete logs or pending device checks; errors are listed in the report.
Missing, failed, short, nonconsecutive, screenshot-contaminated, changed-detail,
negative, non-finite and huge/underflow-like timings are rejected. No performance
comparison is generated unless every planned run is valid.

The first warm-up rows are discarded. Medians/p95/p99 are reported per run;
paired results compare the medians across repeats. Negative change_percent is
less elapsed time. observed_loop_hz is NOT game FPS or displayed FPS, and time
over 110% of the loop budget is NOT a measured count of dropped pictures.
Pipeline phases overlap; do not add them together as a frame or GPU total.

Compare settings within each build as well as baseline versus patched. Lower
detail helping while emulation time stays similar suggests rendering pressure;
interpolation off helping suggests extra-world-picture work. These are leads
for profiling, not automatic bottleneck diagnoses. If all settings are equally
slow, investigate CPU execution, memory, audio, thread contention or throttling.

No Switch hardware results are supplied with this tool. No publishing is
authorised; all builds, game data and collected results remain local.
