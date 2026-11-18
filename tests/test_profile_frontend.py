import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]


class FrontendProfileTests(unittest.TestCase):
    @unittest.skipUnless((ROOT/'build/host/preview').exists(), 'Build the host renderer first')
    def test_authored_profile_reports_bounded_batch(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as directory:
            output=Path(directory)/'report.json'
            subprocess.run([sys.executable,str(ROOT/'tools/profile_frontend.py'),'--items','8','--output',str(output)],check=True,capture_output=True)
            report=json.loads(output.read_text())
            self.assertEqual(set(report['themes']),{'phosphor','eclipse','prism'})
            for measurement in report['themes'].values():
                self.assertLessEqual(measurement['visible_batch_items'],512)
                self.assertGreater(measurement['app_state_bytes'],0)
                self.assertEqual(measurement['framebuffer_bytes'],256*192*2*2)

    @unittest.skipUnless((ROOT/'build/host/preview').exists(), 'Build the host renderer first')
    def test_mixed_theme_cycles_report_heap_samples(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as directory:
            output=Path(directory)/'report.json'
            subprocess.run([sys.executable,str(ROOT/'tools/profile_frontend.py'),
                            '--items','16','--mixed','--cycles','2','--output',str(output)],
                           check=True,capture_output=True)
            for sample in json.loads(output.read_text())['themes'].values():
                self.assertEqual(sample['cycles'],2)
                self.assertEqual(sample['theme_changes'],2)
                self.assertEqual(sample['preview_cancellations'],1)
                self.assertEqual(sample['locale_changes'],2)
                self.assertFalse(sample['heap_comparable'])
                self.assertGreaterEqual(sample['sampled_heap_peak_bytes'],sample['heap_end_bytes'])
                self.assertLessEqual(sample['visible_batch_items'],512)

    @unittest.skipUnless((ROOT/'build/host/preview').exists(), 'Build the host renderer first')
    def test_top_content_mode_uses_a_private_mixed_fixture(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as directory:
            output=Path(directory)/'context.json'
            subprocess.run([sys.executable,str(ROOT/'tools/profile_frontend.py'),'--items','8','--mixed','--top-content','context','--output',str(output)],check=True,capture_output=True)
            report=json.loads(output.read_text())
            self.assertEqual(report['top_content'],'context')
            self.assertEqual(set(report['themes']),{'phosphor','eclipse','prism'})
            self.assertTrue(all(row['visible_batch_items'] <= 512 for row in report['themes'].values()))
