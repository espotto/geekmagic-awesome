"""The spec 2 analog test face (analog_fixture.json plus drawn images) and
whether the native theme tool can be built, for the tests that need both."""
import os
from pathlib import Path
import shutil
import sys
import unittest

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
import theme_native as native

ANALOG = Path(__file__).resolve().parent/'analog_fixture.json'


def native_available():
    try:
        native.executable()
        return True, ''
    except ValueError as error:
        return False, str(error)


AVAILABLE, REASON = native_available()
# Locally a missing toolchain skips; in CI it must fail, not hide a mismatch.
if not AVAILABLE and os.environ.get('CI'):
    raise RuntimeError('Native theme tool unavailable in CI: '+REASON)
needs_native = unittest.skipUnless(AVAILABLE, 'native theme tool unavailable: '+REASON)


def make_analog(root):
    """The spec 2 test theme: ticks, arcs with colour stops, the two-hand AM/PM
    hour, a moon-bound radius, localized text, an animation and a scroll."""
    (root/'images').mkdir(parents=True)
    (root/'animations/dot').mkdir(parents=True)
    shutil.copy(ANALOG, root/'theme.json')
    im = Image.new('RGBA', (240, 240), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    for r in range(118, 0, -6):
        d.ellipse((120-r, 120-r, 120+r, 120+r), fill=(11+r//8, 20+r//6, 32+r//4, 255))
    im.save(root/'images/bg.png')
    badge = Image.new('RGBA', (40, 20), (0, 0, 0, 0))
    ImageDraw.Draw(badge).rectangle((2, 2, 37, 17), fill=(71, 194, 255, 140))
    badge.save(root/'images/badge.png')
    for i in range(4):
        f = Image.new('RGBA', (12, 12), (0, 0, 0, 0))
        ImageDraw.Draw(f).ellipse((i, i, 11-i, 11-i), fill=(255, 90, 71, 255))
        f.save(root/f'animations/dot/{i:03d}.png')
