#!/usr/bin/env python3
import os
import argparse
import subprocess
import utils
from pathlib import Path
from typing import List
import compile


def run_language(executable_file: str, input_file: str, timeout: int) -> str:
    # Run our binary
    if input_file:
        with open(input_file, 'r') as ifile:
            binary_result = subprocess.run(
                [executable_file],
                stdin=ifile,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=timeout
            )

    else:
        binary_result = subprocess.run(
            [executable_file],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout
        )

    # Check for errors
    if (len(binary_result.stderr) != 0):
        error = binary_result.stderr.decode()
        utils.write_error(f'Error when running executable {binary_result}')
        return 'ERROR'

    return binary_result.stdout.decode()


def main():
    parser = argparse.ArgumentParser(description='Compiler run')
    parser.add_argument('root_dir',
                        help='The root directory from which to build')
    parser.add_argument('language',
                        help='Which language to build (L1, L2, L3, IR, LA, LB, LC, LD)')
    parser.add_argument('filename',
                        help='Program file location (relative or absolute)')
    parser.add_argument('build_dir',
                        help='Directory to store compilation artifacts (relative or absolute)')
    parser.add_argument('runtime',
                        help='The location of our runtime to compile (relative or absolute)')
    parser.add_argument('-i', '--input_file', type=str, default='',
                        help='An optional input file to pipe into our runtime (relative or absolute)')
    parser.add_argument('-t', '--timeout', type=int, default=240,
                        help='Timeout per test in seconds (default: 240)')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Detailed runtime messages')
    parser.add_argument('-q', '--quiet', action='store_true',
                        help='Disable all non-essential script messages')

    args = parser.parse_args()
    assert args.language in compile.LANGUAGES, \
        f'Invalid language choice {args.language}'

    utils.set_verbosity_from_args(args)

    # Absolute paths for consistency in calls
    root_dir = os.path.abspath(args.root_dir)
    assert os.path.exists(root_dir), f'Missing root directory {root_dir}'
    build_dir = os.path.abspath(args.build_dir)
    program_file = os.path.abspath(args.filename)
    assert os.path.exists(program_file), f'Missing program file {program_file}'
    runtime_location = os.path.abspath(args.runtime)
    assert os.path.exists(runtime_location), \
        f'Missing runtime {runtime_location}'

    # If there is an input file, check it
    input_file = args.input_file
    if input_file:
        input_file = os.path.abspath(input_file)
        assert os.path.exists(input_file), \
            f'Input file not found: {input_file}'
        
    # Check invariants
    assert args.timeout > 0, f'Expected a timeout > 0, got {args.timeout}'
    
    # Make the build directory
    os.makedirs(build_dir, exist_ok=True)

    # Compile the runtime
    runtime_binary = compile.build_runtime(
        runtime_location, build_dir, args.timeout)

    # Compile our code
    executable_file = ''
    if runtime_location:
        # Run tests
        executable_file = compile.compile_language(root_dir,
                                                   args.language,
                                                   program_file,
                                                   build_dir,
                                                   runtime_binary,
                                                   args.timeout)

    # Run the result

    result = run_language(executable_file, input_file, args.timeout)

    if result != 'ERROR':
        print(result)
    else:
        print('Runtime failed')


if __name__ == "__main__":
    main()
