"""Fixture tests for the compiled C binary; no third-party Python packages."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

BINARY = Path(__file__).resolve().parents[1] / 'build/process-inspector'


class CliTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.entry(10, 'web worker', 4096, uid=1000)
        self.entry(2, 'database', 8192, uid=1000)
        self.entry(99, 'kernel-demo', None, uid=0)

    def tearDown(self):
        self.tmp.cleanup()

    def entry(self, pid, name, rss, uid=0):
        folder = self.root / str(pid); folder.mkdir()
        text = f'Name:\t{name}\nState:\tS (sleeping)\nUid:\t{uid}\t{uid}\t{uid}\t{uid}\n'
        if rss is not None: text += f'VmRSS:\t{rss} kB\n'
        (folder / 'status').write_text(text)

    def run_cli(self, *args):
        return subprocess.run([str(BINARY),'--proc-root',str(self.root),*args],capture_output=True,text=True)

    def test_rss_sort_and_unknown_memory(self):
        result = self.run_cli('--json')
        self.assertEqual(result.returncode,0,result.stderr)
        rows=json.loads(result.stdout)['processes']
        self.assertEqual([x['pid'] for x in rows],[2,10,99])
        self.assertIsNone(rows[-1]['rss_kib'])

    def test_name_uid_filter_and_limit(self):
        result=self.run_cli('--json','--name','worker','--uid','1000','--limit','1')
        data=json.loads(result.stdout)
        self.assertEqual(data['matched'],1)
        self.assertEqual(data['processes'][0]['name'],'web worker')

    def test_pid_and_name_sort(self):
        self.assertEqual(json.loads(self.run_cli('--json','--sort','pid').stdout)['processes'][0]['pid'],2)
        self.assertEqual(json.loads(self.run_cli('--json','--sort','name').stdout)['processes'][0]['name'],'database')

    def test_skipped_process_and_symlink_status(self):
        (self.root/'123').mkdir()
        (self.root/'456').mkdir()
        (self.root/'456/status').symlink_to(self.root/'10/status')
        result=self.run_cli('--json')
        self.assertEqual(result.returncode,1)
        self.assertEqual(json.loads(result.stdout)['skipped'],2)

    def test_json_escaping_and_terminal_controls(self):
        self.entry(123,'quote" and\x1b[31m',128)
        data=json.loads(self.run_cli('--json','--name','quote').stdout)
        self.assertIn('\x1b',data['processes'][0]['name'])
        self.assertNotIn('\x1b',self.run_cli('--name','quote').stdout)

    def test_invalid_options(self):
        for args in [('--limit','-1'),('--limit','10001'),('--uid','abc'),('--sort','cpu'),('--unknown',),('--name',)]:
            self.assertEqual(self.run_cli(*args).returncode,2,args)

    def test_empty_result_and_malformed_status(self):
        self.assertEqual(json.loads(self.run_cli('--json','--name','not-there').stdout)['shown'],0)
        self.entry(123,'bad',1)
        (self.root/'123/status').write_text('not a complete status file\n')
        result=self.run_cli('--json')
        self.assertEqual(result.returncode,1)
        self.assertEqual(json.loads(result.stdout)['skipped'],1)

    def test_missing_root(self):
        result=subprocess.run([str(BINARY),'--proc-root',str(self.root/'missing')],capture_output=True)
        self.assertEqual(result.returncode,2)


if __name__ == '__main__':
    unittest.main(verbosity=2)
