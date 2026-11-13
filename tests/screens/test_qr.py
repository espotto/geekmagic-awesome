"""The firmware's QR encoder (src/QrCode.cpp) against the reference `qrcode`
package, module for module, for every version, level and mask it uses; then
OpenCV decodes the encoder's own choices back to the payload.

    python tests/screens/test_qr.py   (needs: pip install qrcode opencv-python-headless)
"""
import os
import subprocess
import sys

import numpy as np
import qrcode
import qrcode.util

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
BIN = '/tmp/geekmagic-qr-dump'
subprocess.check_call(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror', '-Isrc',
                       'tests/screens/qr_dump.cpp', 'src/QrCode.cpp', '-o', BIN], cwd=ROOT)


def firmware(cases):
    out = subprocess.run([BIN], input='\n'.join('%d %d %d %s' % c for c in cases) + '\n',
                         capture_output=True, text=True, check=True).stdout
    return [m.split('\n') for m in out.strip('\n').split('\n\n')]


LEVEL = {0: qrcode.constants.ERROR_CORRECT_M, 1: qrcode.constants.ERROR_CORRECT_L}


def reference(ver, mask, ecc, payload):
    q = qrcode.QRCode(version=ver, error_correction=LEVEL[ecc], mask_pattern=mask, border=0)
    q.add_data(qrcode.util.QRData(payload.encode(), mode=qrcode.util.MODE_8BIT_BYTE))
    q.make(fit=False)
    return [''.join('1' if v else '0' for v in row) for row in q.get_matrix()]


payloads = ['WIFI:T:WPA;S:gm-hotspot;P:secret-pass;;', 'HELLO', 'x' * 60, 'WIFI:T:nopass;S:GeekMagic-Setup;;']
cases = []
for ver in range(1, 9):
    for ecc in (0, 1):
        for mask in range(8):
            p = payloads[(ver + mask) % len(payloads)]
            cap = qrcode.util.BIT_LIMIT_TABLE[LEVEL[ecc]][ver] // 8 - 2
            cases.append((ver, mask, ecc, p[:max(1, cap)]))
got = firmware(cases)
for c, m in zip(cases, got):
    assert m == reference(*c), 'mismatch at version %d mask %d ecc %d' % c[:3]
print('%d forced matrices match the reference' % len(cases))

# The encoder's own choices decode back to the payload.
import cv2  # noqa: E402

auto = ['WIFI:T:WPA;S:gm-hotspot;P:secret-pass;;', 'WIFI:T:nopass;S:GeekMagic-Setup;;',
        'WIFI:T:WPA;S:Очень длинное имя сети\;x;P:' + 'p' * 63 + ';;']
det = cv2.QRCodeDetector()
for p, m in zip(auto, firmware([(-1, -1, -1, p) for p in auto])):
    a = np.array([[0 if ch == '1' else 255 for ch in row] for row in m], np.uint8)
    img = cv2.resize(np.pad(a, 4, constant_values=255), None, fx=8, fy=8, interpolation=cv2.INTER_NEAREST)
    text, _, _ = det.detectAndDecode(img)
    assert text == p, (text, p)
    print('decoded %d modules: %s' % (len(m), p[:40]))

assert firmware([(-1, -1, -1, 'x' * 200)]) == [['-']]   # past version 8 L: refused
print('qr tests passed')
