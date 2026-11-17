"""Run the actual display driver with fake hardware; no secrets or network."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='seo-dashboard-epd-tests-') as build:
    binary=str(Path(build)/'epd-driver-test')
    command=['c++','-std=c++11','-fsanitize=address,undefined','-g',
        '-I'+str(root/'tests/epd_mocks'),'-I'+str(root/'firmware'/'SEO_Dashboard'),
        str(root/'tests/epd_driver_test.cpp'),
        str(root/'firmware/SEO_Dashboard/epd2in9_V2.cpp'),
        str(root/'firmware/SEO_Dashboard/epdif.cpp'),'-o',binary]
    subprocess.run(command,check=True)
    for scenario in ['success','spi_failure','begin_once','ram_planes',
                     'never_asserts','stuck_high','init_busy_timeout',
                     'delayed_assertion','late_assertion','recovery','rollover',
                     'init_trace','init_delayed_trace','reset_stretched','micros_rollover',
                     'spi_restart','restart_busy_guard','spi_restart_failure',
                     'guard_rollover']:
        subprocess.run([binary,scenario],check=True,timeout=10)
        print('PASS:',scenario)
    display=(root/'firmware/SEO_Dashboard/dashboard_display.cpp').read_text()
    helpers=display.split('void printDisplayBusyTrace(',1)[1].split('\n}\n\nbool prepare()',1)[0]
    result_header=(root/'firmware/SEO_Dashboard/dashboard_display.h').read_text()
    result_type='struct RefreshResult {'+result_header.split('struct RefreshResult {',1)[1].split('};',1)[0]+'};\n'
    policy=display.split('  if (transferDashboardFrame(1)) return {true, ""};',1)[1].split('\n}\n\nRefreshResult showDashboard(',1)[0]
    (Path(build)/'dashboard_policy.inc').write_text(
        result_type+'void printDisplayBusyTrace('+helpers+
        'RefreshResult tryDashboardFrame() {\n  if (transferDashboardFrame(1)) return {true, ""};'+policy+'\n}\n')
    subprocess.run(command+['-DTEST_DASHBOARD_POLICY','-I'+build],check=True)
    for scenario in ['policy_success','policy_never','policy_recovers',
                     'policy_late','policy_stuck','policy_spi_failure']:
        subprocess.run([binary,scenario],check=True,timeout=10)
        print('PASS:',scenario)
