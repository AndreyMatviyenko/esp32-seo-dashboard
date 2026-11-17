"""Test the real settings record/validation and deferred-restart policy on the host."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
subprocess.run(['python3', str(root / 'tools/embed_settings_web.py'), '--check'], check=True)
with tempfile.TemporaryDirectory(prefix='seo-settings-tests-') as build:
    binary = str(Path(build) / 'settings-test')
    subprocess.run(['c++', '-std=c++11', '-fsanitize=address,undefined', '-g',
                    '-I' + str(root / 'firmware/SEO_Dashboard'),
                    str(root / 'tests/settings_config_test.cpp'), '-o', binary], check=True)
    subprocess.run([binary], check=True, timeout=10)
