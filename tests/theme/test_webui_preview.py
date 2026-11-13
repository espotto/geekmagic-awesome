"""The web UI draws theme previews in the browser (the "preview" block of
src/webui.html, a port of the firmware's render path). These tests hold its
first frame to the native renderer pixel for pixel, with the font blob the
device serves at GET /api/font (writeSystemFont)."""
import datetime as dt
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import theme_native as native
from theme_pack import build as pack
from analog_fixture import AVAILABLE, REASON, make_analog

FRAME = 240*240*3
NODE = shutil.which('node')
CXX = shutil.which(os.environ.get('CXX', 'c++'))
needs_tools = unittest.skipUnless(AVAILABLE and NODE and CXX, 'native theme tool, node or a C++ compiler unavailable: '+REASON)


@needs_tools
class WebPreviewTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        tmp = Path(cls.tmp.name)
        dump = tmp/'dump_font'
        subprocess.run([CXX, '-std=c++11', '-Wall', '-Wextra', '-Werror', '-I'+str(ROOT/'src/features/theme'),
                        str(ROOT/'tests/theme/dump_font.cpp'), str(ROOT/'src/features/theme/SystemFont.cpp'),
                        '-o', str(dump)], check=True)
        cls.font = tmp/'font.bin'
        subprocess.run([str(dump), str(cls.font)], check=True)
        analog = tmp/'analog'
        make_analog(analog)
        sources = {name: ROOT/'tests/theme/fixtures'/name for name in ('pixel-room', 'terminal-ops', 'live-status')}
        sources['analog'] = analog
        cls.packages = {}
        for name, source in sources.items():
            path = tmp/f'{name}.stheme'
            path.write_bytes(pack(source))
            cls.packages[name] = path

    def frame_matches(self, name, start, values=None, lang=None):
        values = values or {}
        t = dt.datetime.fromisoformat(start)
        epoch = int(t.replace(tzinfo=dt.timezone.utc).timestamp())
        with tempfile.TemporaryDirectory() as d:
            raw, mine = Path(d)/'native.rgb', Path(d)/'web.rgb'
            native.run('preview', self.packages[name], raw, epoch, 1, 1,
                       *(['@lang='+lang] if lang else []), *(f'{k}={v}' for k, v in values.items()))
            theirs = raw.read_bytes()[12:]
            subprocess.run([NODE, str(ROOT/'tests/theme/webui_preview.js'), str(self.font), str(self.packages[name]), str(mine),
                            str(t.year), str(t.month), str(t.day), str(t.hour), str(t.minute), str(t.second),
                            str((t.weekday() + 1) % 7), lang or 'en', json.dumps(values)], check=True, cwd=ROOT)
            ours = mine.read_bytes()
        self.assertEqual(len(ours), FRAME)
        if ours != theirs[:FRAME]:
            diff = sum(ours[k:k+3] != theirs[k:k+3] for k in range(0, FRAME, 3))
            self.fail(f'{name} at {start}: {diff} pixels differ from the native renderer')

    def test_first_frame_equals_native_renderer(self):
        runs = [
            ('pixel-room', '2026-11-06T10:09:30', None, None),
            ('terminal-ops', '2026-09-23T23:58:58', None, None),
            ('live-status', '2026-11-06T10:09:30', None, None),
            ('live-status', '2026-11-06T10:09:30',
             {'status.label': 'Build pipeline is green again', 'status.level': '73', 'status.state': 'OK'}, None),
            ('analog', '2026-11-06T10:09:30', {'device.temperature': '21.5'}, None),
            ('analog', '2026-11-06T15:42:07', {'device.temperature': '-4'}, 'en'),
            ('analog', '2026-03-29T11:59:59', None, 'ru'),
            ('analog', '2026-11-12T10:09:30', None, 'ru'),
        ]
        for name, start, values, lang in runs:
            with self.subTest(theme=name, start=start, lang=lang):
                self.frame_matches(name, start, values, lang)

    def test_font_blob(self):
        b = self.font.read_bytes()
        self.assertEqual(b[:4], b'GMF1')
        self.assertEqual(b[4], 6)


if __name__ == '__main__':
    unittest.main()
