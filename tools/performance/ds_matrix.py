"""Generate Switch test.txt comparisons and analyse real per-frame timing CSVs."""
import argparse
import csv
import json
import math
import random
import re
import statistics
import uuid
from pathlib import Path, PurePosixPath

FIELDS = ('total', 'emulate', 'scene', 'compose', 'present', 'wait', 'wait_3d', 'wait_wide')
SETTINGS = ('view=merged', 'wide=1', 'busy_detail=0', 'fast_lock=0',
            'slow_divisor=1', 'auto_state=0', 'auto_resume=0', 'rewind_mb=0',
            'live_glass=0', 'ra=0', 'aspect=16:9', 'pipeline=1',
            'frame_ahead=1', 'display_pacing=1', 'wide_gpu=1')


def relative_file(value):
    path = PurePosixPath(value)
    if (not value or value != value.strip() or any(c in value for c in '\r\n:\\')
            or path.is_absolute() or '..' in path.parts or str(path) == '.'):
        raise ValueError('Use a nonempty relative SD-folder filename without traversal or newlines')
    return value


def make_plan(scene, state, script=None, repeats=3, warmup=1800, measured=3600,
              seed=1701, session=None):
    relative_file(state)
    if script:
        relative_file(script)
    if not scene.strip() or not 1 <= repeats <= 10 or warmup < 0 or measured < 1:
        raise ValueError('A scene, 1-10 repeats and positive measured frame count are required')
    session = session or uuid.uuid4().hex[:8]
    if not re.fullmatch(r'[A-Za-z0-9_-]{1,32}', session):
        raise ValueError('Session must contain 1-32 letters, numbers, underscores or hyphens')
    cases = [(lines, interp) for lines in (192, 360, 720) for interp in (0, 1)]
    orders = []
    for repeat in range(repeats):
        order = cases.copy()
        random.Random(seed + repeat).shuffle(order)
        orders.append(order)
    runs = []
    for variant in ('baseline', 'patched'):
        for repeat, order in enumerate(orders, 1):
            for lines, interp in order:
                runs.append(dict(name=f'{session}-{variant}-r{repeat:02}-l{lines}-i{interp}',
                                 variant=variant, repeat=repeat, lines=lines, interpolation=interp))
    return dict(schema=1, scene=scene, state=state, input_script=script, session=session,
                repeats=repeats, warmup_frames=warmup, measured_frames=measured,
                nominal_loop_hz=59.8261, seed=seed, runs=runs)


def generate(out, plan):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=False)
    for variant in ('baseline', 'patched'):
        folder = out / variant
        (folder / 'perf-matrix').mkdir(parents=True)
        (folder / 'perf-matrix/config.cfg').write_text('\n'.join(SETTINGS) + '\n', encoding='utf-8')
        lines = ['# Private benchmark copy only; apply the timing-counter fix to BOTH builds.',
                 '# No screenshots: CSV logging and SSR_PERF use identical instrumentation.', 'restart yes']
        for run in plan['runs']:
            if run['variant'] != variant:
                continue
            lines += ['', 'run ' + run['name'], 'state ' + plan['state'],
                      'frames ' + str(plan['warmup_frames'] + plan['measured_frames']),
                      'arg --config perf-matrix/config.cfg', 'arg --no-save --keep-settings',
                      'arg --env SSR_PERF=1', 'arg --layout merged',
                      'arg ' + ('--interp60' if run['interpolation'] else '--no-interp60'),
                      'set wide_lines=' + str(run['lines']),
                      'set fps60=' + str(run['interpolation'])]
            if plan['input_script']:
                lines.append('input ' + plan['input_script'])
        (folder / 'test.txt').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    (out / 'plan.json').write_text(json.dumps(plan, indent=2) + '\n', encoding='utf-8')
    notes = dict(device='', power_mode='', display_mode='', renderer='', baseline_commit='',
                 patched_commit='', same_scene_confirmed=False, same_clock_policy_confirmed=False,
                 warmed_up=False, audio_input_checked=False)
    (out / 'device-notes.json').write_text(json.dumps(notes, indent=2) + '\n', encoding='utf-8')


def percentile(values, fraction):
    values = sorted(values)
    position = (len(values) - 1) * fraction
    lo = int(position)
    hi = min(lo + 1, len(values) - 1)
    return values[lo] + (values[hi] - values[lo]) * (position - lo)


def read_run(root, run, plan):
    progress = (root / 'progress.txt').read_text(encoding='utf-8').splitlines()
    if 'failed ' + run['name'] in progress or 'done ' + run['name'] not in progress:
        raise ValueError('Run is failed or has no completed progress record')
    folder = root / run['name']
    log = (folder / 'game.log').read_text(encoding='utf-8', errors='replace')
    flag = '--interp60' if run['interpolation'] else '--no-interp60'
    if f"[test] run {run['name']}" not in log or flag not in log or f"wide_lines={run['lines']}" not in log:
        raise ValueError('Game log does not confirm the requested run/settings')
    with (folder / 'timing.csv').open(encoding='utf-8-sig', newline='') as stream:
        reader = csv.DictReader(stream)
        if not set(('frame', 'wide_lines', 'shot', *FIELDS)).issubset(reader.fieldnames or []):
            raise ValueError('Timing CSV is missing required columns')
        rows = []
        previous = None
        for raw in reader:
            frame = int(raw['frame'])
            if previous is not None and frame != previous + 1:
                raise ValueError('Nonconsecutive frame rows; benchmark cannot be compared reliably')
            previous = frame
            row = {key: float(raw[key]) for key in FIELDS}
            if any(not math.isfinite(v) or v < 0 or v > 86400000 for v in row.values()):
                raise ValueError('Invalid timing; check counter underflow or corrupt/truncated data')
            if row['total'] <= 0:
                raise ValueError('Frame total must be positive')
            row.update(lines=int(raw['wide_lines']), shot=int(raw['shot']))
            rows.append(row)
    expected = plan['warmup_frames'] + plan['measured_frames']
    # The last iteration may exit before its pending CSV row is written.
    if not expected - 2 <= len(rows) <= expected:
        raise ValueError(f'Incomplete or unexpected run length: {len(rows)} rows, expected about {expected}')
    measured = rows[plan['warmup_frames']:]
    if any(row['shot'] for row in measured):
        raise ValueError('Screenshot-contaminated performance run; repeat without captures')
    if any(row['lines'] != run['lines'] for row in measured):
        raise ValueError('Detail cap changed or does not match the requested setting')
    if len(measured) < max(1, plan['measured_frames'] - 2):
        raise ValueError('Insufficient post-warm-up samples')
    stats = {key: dict(median_ms=statistics.median([row[key] for row in measured]),
                       p95_ms=percentile([row[key] for row in measured], .95),
                       p99_ms=percentile([row[key] for row in measured], .99)) for key in FIELDS}
    budget = 1000 / plan['nominal_loop_hz']
    return dict(**run, samples=len(measured), stats=stats,
                observed_loop_hz=1000 / statistics.mean([row['total'] for row in measured]),
                over_110pct_budget_percent=100 * sum(row['total'] > budget * 1.1 for row in measured) / len(measured))


def analyse(plan, baseline, patched, notes=None):
    canonical = make_plan(plan['scene'], plan['state'], plan['input_script'], plan['repeats'],
                          plan['warmup_frames'], plan['measured_frames'], plan['seed'], plan['session'])
    # Validate recorded case coverage, not a re-created shuffle: analysis may use
    # a different Python version from the machine that generated the plan.
    fields = ('variant', 'repeat', 'lines', 'interpolation')
    expected = {tuple(run[k] for k in fields) for run in canonical['runs']}
    actual = [tuple(run[k] for k in fields) for run in plan['runs']]
    if ({k: v for k, v in plan.items() if k != 'runs'} !=
            {k: v for k, v in canonical.items() if k != 'runs'} or
            len(actual) != len(expected) or set(actual) != expected):
        raise ValueError('Plan is incomplete, altered or uses an unsupported schema; generate a fresh plan')
    for run in plan['runs']:
        name = (f"{plan['session']}-{run['variant']}-r{run['repeat']:02}"
                f"-l{run['lines']}-i{run['interpolation']}")
        if run['name'] != name:
            raise ValueError('Run name does not match its recorded settings')
    for repeat in range(1, plan['repeats'] + 1):
        orders = [[(r['lines'], r['interpolation']) for r in plan['runs']
                   if r['variant'] == v and r['repeat'] == repeat] for v in ('baseline', 'patched')]
        if orders[0] != orders[1]:
            raise ValueError('Baseline and patched case order must match')
    results, errors = [], []
    for run in plan['runs']:
        try:
            results.append(read_run(Path(baseline if run['variant'] == 'baseline' else patched), run, plan))
        except (OSError, ValueError, TypeError, KeyError) as error:
            errors.append(dict(run=run['name'], problem=str(error)))
    comparisons = []
    if not errors:
        for lines in (192, 360, 720):
            for interp in (0, 1):
                grouped = {variant: [r for r in results if r['variant'] == variant and
                                     r['lines'] == lines and r['interpolation'] == interp]
                           for variant in ('baseline', 'patched')}
                change = {}
                for field in FIELDS:
                    old = statistics.median([r['stats'][field]['median_ms'] for r in grouped['baseline']])
                    new = statistics.median([r['stats'][field]['median_ms'] for r in grouped['patched']])
                    change[field] = dict(baseline_median_ms=old, patched_median_ms=new,
                                         change_percent=(new / old - 1) * 100 if old else None)
                comparisons.append(dict(lines=lines, interpolation=interp, timings=change))
    notes = notes or {}
    confirmed = (all(notes.get(key) for key in ('device', 'power_mode', 'display_mode', 'renderer',
                                               'baseline_commit', 'patched_commit')) and
                 all(notes.get(key) is True for key in ('same_scene_confirmed', 'same_clock_policy_confirmed',
                                                        'warmed_up', 'audio_input_checked')))
    return dict(status='incomplete' if errors else 'logs-complete', device_checks_confirmed=bool(confirmed),
                ready_for_review=not errors and bool(confirmed), scene=plan['scene'], runs=results,
                errors=errors, comparisons=comparisons, device_notes=notes,
                limits=['Loop rate is not game FPS or displayed FPS.',
                        'Phases overlap; do not sum them or interpret their sum as GPU time.',
                        'wide_lines is the selected detail cap, not proof of actual render-target height.',
                        'No automatic setting recommendation or hardware speedup certification.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    make = commands.add_parser('generate')
    make.add_argument('--out', type=Path, required=True)
    make.add_argument('--scene', required=True)
    make.add_argument('--state', required=True)
    make.add_argument('--input-script')
    make.add_argument('--repeats', type=int, default=3)
    make.add_argument('--warmup-frames', type=int, default=1800)
    make.add_argument('--measured-frames', type=int, default=3600)
    make.add_argument('--seed', type=int, default=1701)
    make.add_argument('--session')
    report = commands.add_parser('analyse')
    report.add_argument('--plan', type=Path, required=True)
    report.add_argument('--baseline-results', type=Path, required=True)
    report.add_argument('--patched-results', type=Path, required=True)
    report.add_argument('--notes', type=Path)
    report.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == 'generate':
            plan = make_plan(args.scene, args.state, args.input_script, args.repeats,
                             args.warmup_frames, args.measured_frames, args.seed, args.session)
            generate(args.out, plan)
            print(f"Prepared {len(plan['runs'])} runs; no games executed, no device measurements collected.")
            return 0
        plan = json.loads(args.plan.read_text(encoding='utf-8'))
        notes = json.loads(args.notes.read_text(encoding='utf-8')) if args.notes else None
        result = analyse(plan, args.baseline_results, args.patched_results, notes)
        args.out.write_text(json.dumps(result, indent=2, allow_nan=False) + '\n', encoding='utf-8')
        print(f"{result['status']}: {len(result['runs'])} valid runs, {len(result['errors'])} missing/invalid; "
              f"device checks confirmed: {result['device_checks_confirmed']}")
        return 0 if result['ready_for_review'] else 2
    except (OSError, ValueError, TypeError, KeyError) as error:
        parser.error(str(error))


if __name__ == '__main__':
    raise SystemExit(main())
