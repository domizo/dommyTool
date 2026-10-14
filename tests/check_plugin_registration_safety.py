#!/usr/bin/env python3
"""Actual registration/solver callback ownership, allocation and API failures."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--mutations',action='store_true')
    args=parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    header = (root/'src/plugin/registration/PluginRegistration.h').read_text()
    solver = (root/'src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp').read_text()
    methods = extract(header,'namespace dommy {')+'\n'+extract(solver,'MStatus DBSolverNode::registerCallbacks(')+'\n'+extract(solver,'MStatus DBSolverNode::removeCallbacks(')
    with tempfile.TemporaryDirectory(prefix='dommy-plugin-registration-') as directory:
        temp=Path(directory)
        include=temp/'plugin_registration_methods.inc'
        include.write_text(methods)
        binary=temp/'test'
        command=shlex.split(os.environ.get('CXX','clang++'))+[
            '-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror',
            '-fsanitize=address,undefined','-fno-sanitize-recover=all',
            '-I'+str(temp),'-I'+str(root/'tests/stubs'),'-I'+str(root/'src/core'),
            str(root/'tests/cpp/plugin_registration_fixture.cpp'),'-o',str(binary)]
        env=dict(os.environ, ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1'))
        subprocess.run(command,check=True)
        subprocess.run([str(binary)],check=True,env=env)
        print('Registration allocation, rollback, unload/retry and solver callbacks passed.')
        if args.mutations:
            cases=(
                ('failed cleanup forgotten', methods.replace('if (status == MS::kSuccess) *it = CleanupAction();', '*it = CleanupAction();')),
                ('failed host registration deregistered', methods.replace('status != MS::kSuccess && !retainOnFailure', 'false && status != MS::kSuccess && !retainOnFailure')),
                ('existing lifetime removed by new transaction', methods.replace('ownsLifetime_(lifetime.empty())', 'ownsLifetime_(true)')),
                ('partial subsystem cleanup discarded', methods.replace('std::move(cleanup), true', 'std::move(cleanup), false')),
                ('callback removal failure erases ID', methods.replace('if (status == MS::kSuccess)\n            updateSceneCallbackId = 0;', 'if (true)\n            updateSceneCallbackId = 0;')),
            )
            for label,changed in cases:
                if changed == methods: raise AssertionError('Mutation not applied: '+label)
                include.write_text(changed)
                subprocess.run(command,check=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
                result=subprocess.run([str(binary)],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
                if result.returncode == 0: raise AssertionError('Undetected mutation: '+label)
                print('Rejected mutation:',label)


if __name__ == '__main__':
    main()
