from pathlib import Path
import csv
import importlib.util
import json
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('ds_matrix', ROOT / 'tools/performance/ds_matrix.py')
matrix = importlib.util.module_from_spec(spec)
spec.loader.exec_module(matrix)


class DsPerformanceMatrixTests(unittest.TestCase):
    def plan(self):
        return matrix.make_plan('synthetic fixture only', 'private scene.state',
                                repeats=1, warmup=2, measured=4, session='fixture')

    def fixture(self, root, plan):
        roots = {v: root / v for v in ('baseline', 'patched')}
        for folder in roots.values():
            folder.mkdir()
        for run in plan['runs']:
            folder = roots[run['variant']]
            with (folder / 'progress.txt').open('a', encoding='utf-8') as stream:
                stream.write('done ' + run['name'] + '\n')
            case = folder / run['name']
            case.mkdir()
            flag = '--interp60' if run['interpolation'] else '--no-interp60'
            (case / 'game.log').write_text(f"[test] run {run['name']}: {flag} --set wide_lines={run['lines']}\n")
            with (case / 'timing.csv').open('w', newline='', encoding='utf-8') as stream:
                writer = csv.DictWriter(stream, fieldnames=['frame', *matrix.FIELDS, 'wide_lines', 'shot'])
                writer.writeheader()
                for i in range(6):
                    row = {k: 1 for k in matrix.FIELDS}
                    # High warm-up costs must not enter the measured statistics.
                    row['total'] = 100 if i < 2 else (10 if run['variant'] == 'baseline' else 8)
                    row.update(frame=9000 + i, wide_lines=run['lines'], shot=0)
                    writer.writerow(row)
        return roots

    def test_all_settings_and_balanced_variant_order(self):
        plan = matrix.make_plan('scene', 'scene.state', repeats=3, session='stable')
        self.assertEqual(len(plan['runs']), 36)
        for repeat in range(1, 4):
            orders = [[(r['lines'], r['interpolation']) for r in plan['runs']
                       if r['variant'] == v and r['repeat'] == repeat] for v in ('baseline', 'patched')]
            self.assertEqual(orders[0], orders[1])
            self.assertEqual(set(orders[0]), {(h, i) for h in (192, 360, 720) for i in (0, 1)})

    def test_generated_plan_is_private_and_does_not_overwrite(self):
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp) / 'matrix'
            matrix.generate(out, self.plan())
            for variant in ('baseline', 'patched'):
                text = (out / variant / 'test.txt').read_text()
                self.assertEqual(text.count('\nrun '), 6)
                self.assertIn('--no-save --keep-settings', text)
                self.assertNotIn('--headless', text)
                self.assertNotIn('shots ', text)
                self.assertIn('ra=0', (out / variant / 'perf-matrix/config.cfg').read_text())
            with self.assertRaises(FileExistsError):
                matrix.generate(out, self.plan())
            for invalid in ('../outside.state', 'sdmc:/save.state', 'state\nrun injected', ''):
                with self.assertRaises(ValueError):
                    matrix.make_plan('scene', invalid)

    def test_altered_plan_cannot_hide_missing_settings(self):
        plan = self.plan()
        plan['runs'].pop()
        with self.assertRaises(ValueError):
            matrix.analyse(plan, Path('absent'), Path('absent'))

    def test_real_csv_analysis_removes_warmup_and_requires_device_confirmation(self):
        with tempfile.TemporaryDirectory() as temp:
            plan = self.plan()
            roots = self.fixture(Path(temp), plan)
            report = matrix.analyse(plan, roots['baseline'], roots['patched'])
            self.assertEqual(report['status'], 'logs-complete')
            self.assertFalse(report['ready_for_review'])
            self.assertEqual(len(report['comparisons']), 6)
            self.assertEqual(report['runs'][0]['samples'], 4)
            self.assertAlmostEqual(report['comparisons'][0]['timings']['total']['change_percent'], -20)
            notes = dict(device='fixture', power_mode='fixture', display_mode='fixture', renderer='fixture',
                         baseline_commit='fixture', patched_commit='fixture', same_scene_confirmed=True,
                         same_clock_policy_confirmed=True, warmed_up=True, audio_input_checked=True)
            self.assertTrue(matrix.analyse(plan, roots['baseline'], roots['patched'], notes)['ready_for_review'])
            self.assertAlmostEqual(matrix.percentile([1, 2, 3, 4], .95), 3.85)

    def test_missing_failed_and_corrupt_runs_never_produce_comparison(self):
        with tempfile.TemporaryDirectory() as temp:
            plan = self.plan()
            roots = self.fixture(Path(temp), plan)
            case = roots['baseline'] / plan['runs'][0]['name']
            csv_path = case / 'timing.csv'
            original = csv_path.read_text()
            for replacement in ('nan', '-1', '18446744073709'):
                csv_path.write_text(original.replace(',1,', ',' + replacement + ',', 1))
                report = matrix.analyse(plan, roots['baseline'], roots['patched'])
                self.assertEqual(report['status'], 'incomplete')
                self.assertEqual(report['comparisons'], [])
            csv_path.write_text(original)
            with (roots['baseline'] / 'progress.txt').open('a') as stream:
                stream.write('failed ' + plan['runs'][0]['name'] + '\n')
            self.assertEqual(matrix.analyse(plan, roots['baseline'], roots['patched'])['comparisons'], [])
        with tempfile.TemporaryDirectory() as temp:
            report = matrix.analyse(self.plan(), Path(temp) / 'absent', Path(temp) / 'also-absent')
            self.assertEqual(len(report['errors']), 12)
            self.assertFalse(report['ready_for_review'])

    def test_short_runs_changed_caps_screenshots_and_frame_gaps_are_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            plan = self.plan()
            roots = self.fixture(Path(temp), plan)
            run = plan['runs'][0]
            path = roots['baseline'] / run['name'] / 'timing.csv'
            with path.open(newline='') as stream:
                reader = csv.DictReader(stream)
                fields = reader.fieldnames
                original = list(reader)
            for mutation in ('short', 'cap', 'shot', 'gap'):
                rows = [row.copy() for row in original]
                if mutation == 'short':
                    rows = rows[:2]
                elif mutation == 'cap':
                    rows[-1]['wide_lines'] = '193'
                elif mutation == 'shot':
                    rows[-1]['shot'] = '1'
                else:
                    rows[-1]['frame'] = '9999'
                with path.open('w', newline='') as stream:
                    writer = csv.DictWriter(stream, fieldnames=fields)
                    writer.writeheader()
                    writer.writerows(rows)
                self.assertEqual(matrix.analyse(plan, roots['baseline'], roots['patched'])['status'], 'incomplete')

    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_generated_runs_parse_with_actual_switch_reader(self):
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp) / 'matrix'
            matrix.generate(out, self.plan())
            cpp = Path(temp) / 'parse.cpp'
            cpp.write_text('''#include "testrun.h"
#include <cassert>
#include <algorithm>
int main(int argc,char**argv){assert(argc==2);std::string errors;bool restart=false;
auto runs=TestRun::Parse(argv[1],errors,&restart);assert(errors.empty()&&restart&&runs.size()==6);
for(const auto&r:runs){assert(std::find(r.args.begin(),r.args.end(),"--no-save")!=r.args.end());
assert(std::find(r.args.begin(),r.args.end(),"--keep-settings")!=r.args.end());
assert(std::find(r.args.begin(),r.args.end(),"private scene.state")!=r.args.end());}
return 0;}
''')
            exe = Path(temp) / 'parse'
            subprocess.run(['c++', '-std=c++17', '-I' + str(ROOT / 'frontend'), str(cpp), '-o', str(exe)], check=True)
            subprocess.run([str(exe), str(out / 'baseline/test.txt')], check=True)
