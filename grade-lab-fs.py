#!/usr/bin/env python3

import re
from gradelib import *

r = Runner(save("xv6.out"))

@test(40, "running bigfile")
def test_bigfile():
    r.run_qemu(shell_script([
        'bigfile'
    ]), timeout=180)
    r.match('^wrote 65803 blocks$')
    r.match('^bigfile done; ok$')

@test(0, "running symlinktest")
def test_symlinktest():
    r.run_qemu(shell_script([
        'symlinktest'
    ]), timeout=20)

@test(20, "symlinktest: symlinks", parent=test_symlinktest)
def test_symlinktest_symlinks():
    r.match("^test symlinks: ok$")

@test(20, "symlinktest: concurrent symlinks", parent=test_symlinktest)
def test_symlinktest_symlinks():
    r.match("^test concurrent symlinks: ok$")

@test(19, "usertests")
def test_usertests():
    r.run_qemu(shell_script([
        'usertests'
    ]), timeout=360)
    r.match('^ALL TESTS PASSED$')

@test(1, "time")
def test_time():
    check_time()

run_tests()
