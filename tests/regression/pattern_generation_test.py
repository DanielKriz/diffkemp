from diffkemp.simpll.library import run_generate_pattern
import yaml
import pytest
import os
import subprocess


def get_test_specs():
    specsfile = os.path.dirname(os.path.realpath(
        __file__)) + '/pattern_generation/pattern_generation_specs.yaml'
    with open(specsfile, 'r') as cfile:
        something = yaml.safe_load(cfile)
        return something['test_specs']


@pytest.fixture(scope="module")
def rootdir():
    return os.path.dirname(os.path.realpath(__file__)) + '/../..'


@pytest.fixture(scope="module")
def patterndir():
    return os.path.dirname(os.path.realpath(__file__)) + '/pattern_generation'


@pytest.fixture(scope="function")
def diffkemp_cmd(rootdir):
    def _cmd(args):
        out = subprocess.run(
            [
                rootdir + '/bin/diffkemp',
                *args,
            ],
            capture_output=True,
        )
        return (
            out.returncode,
            out.stdout.decode('utf-8'),
            out.stderr.decode('utf-8'))
    return _cmd


@pytest.mark.parametrize('specs', get_test_specs())
def test_something(specs, diffkemp_cmd, patterndir):
    rc, out, err = diffkemp_cmd([
        'build',
        patterndir + '/' + specs['name'] + '/new',
        patterndir + '/' + specs['name'] + '/new_snapshot',
    ])

    rc, out, err = diffkemp_cmd([
        'build',
        patterndir + '/' + specs['name'] + '/old',
        patterndir + '/' + specs['name'] + '/old_snapshot',
    ])

    rc, out, err = diffkemp_cmd([
        'generate',
        '-c',
        patterndir + '/' + specs['name'] + '/' + specs['conf']
    ])

    assert rc == 0
    if specs['should_generate']:
        assert err == ''
    else:
        assert err != ''

    if specs['should_match_out']:
        out = out.split('\n')
        patterns = []
        for idx, line in enumerate(out):
            if line.startswith('Generated'):
                patterns.append(out[:idx])
                out = out[idx:]
        patterns.append(out)

        for pattern, output in zip(patterns, specs['outputs']):
            outfile = patterndir + '/' + specs['name'] + '/' + output
            with open(outfile, 'r') as file:
                contents = file.readlines()
            for line1, line2 in zip(pattern[1:], contents):
                if line1 == "" and line2 == "":
                    continue
                assert line1.strip() == line2.strip()

    rc, out, err = diffkemp_cmd([
        'compare',
        '--stdout',
        '--patterns',
        patterndir + '/' + specs['name'] + '/patterns.yaml',
        '--report-stat',
        patterndir + '/' + specs['name'] + '/old_snapshot',
        patterndir + '/' + specs['name'] + '/new_snapshot',
    ])

    for line in err.split('\n'):
        if line.startswith('warning'):
            continue
        assert line == ''

    for line in out.split('\n'):
        if line.startswith('Equal'):
            assert line.split(' ')[-1] == '(100%)'
