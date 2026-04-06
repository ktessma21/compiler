#!/usr/bin/env python3
import os
import shutil
import argparse
import subprocess
import utils
from pathlib import Path
from typing import List, Optional, Tuple
import compile
import compile_and_run


def test_language(root_dir: str,
                  starting_language: str,
                  test_file: str,
                  build_dir: str,
                  runtime_binary: str,
                  timeout: int,
                  output_dir: Optional[str],
                  executable_save_dir: Optional[str]) -> str:
    """
    Runs a single test case in a local isolated build directory.
    Returns an empty string if the test succeeded, 
     and the name of the test (based on the test file) + the error otherwise
    """
    test_name = Path(test_file).stem

    # Define Input/Output paths
    # We look for .in and .out files in the same directory as the test file
    input_file_loc = test_file + '.in'
    input_file = None
    if os.path.exists(input_file_loc):
        input_file = input_file_loc
    oracle_file = test_file + '.out'

    # Removes whitespace from comparison
    def clean_data(data):
        return list(filter(lambda line: len(line) > 0,
                           map(str.strip, data.strip().split('\n'))))

    # Explicitly error if we are missing an oracle file
    assert os.path.exists(oracle_file), f'Missing oracle for {test_name}'
    with open(oracle_file, 'r') as ifile:
        expected = clean_data(ifile.read())

    # Get a hook to our output file location
    # NOTE: Writes errors to that file
    output_file = None
    if output_dir is not None:
        output_file = os.path.join(output_dir, f"{test_name}.result")

    try:
        # Check for cached result
        if os.path.exists(output_file):
            with open(output_file, 'r') as ifile:
                result = clean_data(ifile.read())
            if result == expected:
                utils.write_verbose(f'Skipping cached test {test_name}')
                return True

        if output_dir is not None:
            Path(output_file).touch()
            utils.set_file_capture(output_file)

        executable_file = compile.compile_language(root_dir,
                                                   starting_language,
                                                   test_file,
                                                   build_dir,
                                                   runtime_binary,
                                                   timeout)

        if executable_file == '':
            return False

        if executable_save_dir is not None:
            os.makedirs(executable_save_dir, exist_ok=True)
            shutil.copy(executable_file,
                        os.path.join(
                            executable_save_dir,
                            Path(test_file).stem,
                        ))

        result = compile_and_run.run_language(executable_file,
                                              input_file,
                                              timeout)

        if result == 'ERROR':
            return False

        utils.save(result, output_file)

        cleaned_result = clean_data(result)
        if cleaned_result == expected:
            utils.write_verbose(f'Passed test {test_name}')
            return True
        utils.write_error(
            f'{test_name}: Expected\n{expected}\n\nGot\n{cleaned_result}')
        return False

    except Exception as e:
        utils.write_error(f'Test {test_name} failed with error: {str(e)}')
        return False


def run_tests(root_dir: str,
              language: str,
              test_dir: str,
              build_dir: str,
              runtime_binary: str,
              timeout: int,
              output_dir: Optional[str],
              executable_save_dir: Optional[str]) -> Tuple[int, List[str]]:
    """
    Runs all tests in the given test directory for the given language
    If output_dir is none, does not write the results anywhere
    Returns the number of passing and failing tests (respectively)
    """

    # Get each test file
    test_files = os.listdir(test_dir)
    num_passing = 0
    failing_tests = []
    if output_dir is not None:
        os.makedirs(output_dir, exist_ok=True)

    valid_files = list([s for s in test_files if s.endswith(f'.{language}')])

    for filename in valid_files:
        test_file = os.path.join(test_dir, filename)
        test_passed = test_language(root_dir,
                                    language,
                                    test_file,
                                    build_dir,
                                    runtime_binary,
                                    timeout,
                                    output_dir,
                                    executable_save_dir)

        utils.reset_file_capture()  # in case we wrote to a file
        if test_passed:
            num_passing += 1
        else:
            failing_tests.append(filename)
        test_count = num_passing + len(failing_tests)
        if test_count % 5 == 4:
            utils.write_normal(f'Test #{test_count+1}/{len(valid_files)}')

    return num_passing, failing_tests


def main():
    parser = argparse.ArgumentParser(description='Compiler Testing')
    parser.add_argument('root_dir',
                        help='The root directory to run these compiler tests from (relative or absolute)')
    parser.add_argument('language',
                        help='Which language to test (L1, L2, L3, IR, LA, LB, LC, LD)')
    parser.add_argument('tests',
                        help='Test directory location (relative or absolute)')
    parser.add_argument('build_dir',
                        help='Directory to store compilation artifacts (relative or absolute)')
    parser.add_argument('runtime',
                        help='The location of our runtime to compile (relative or absolute)')
    parser.add_argument('-o', '--out', default=None,
                        help='Optional output directory location (relative or absolute)')
    parser.add_argument('--bin', default=None,
                        help='Directory to store binary files, None by default')
    parser.add_argument('-t', '--timeout', type=int, default=240,
                        help='Timeout per test in seconds (default: 240)')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Detailed runtime messages')
    parser.add_argument('-q', '--quiet', action='store_true',
                        help='Disable all non-essential script messages')

    args = parser.parse_args()
    assert args.language in compile.LANGUAGES, f'Invalid language choice {args.language}'

    utils.set_verbosity_from_args(args)

    # Absolute paths for consistency in calls
    root_dir = os.path.abspath(args.root_dir)
    assert os.path.exists(root_dir), f'Missing root directory {root_dir}'
    test_dir = os.path.abspath(args.tests)
    assert os.path.exists(test_dir), f'Missing test directory {root_dir}'

    build_dir = os.path.abspath(args.build_dir)
    runtime_location = os.path.abspath(args.runtime)
    output_dir = None if args.out is None else os.path.abspath(args.out)
    executable_save_dir = None if args.bin is None else os.path.abspath(
        args.bin)

    # Check invariants
    assert args.timeout > 0, f'Expected a timeout > 0, got {args.timeout}'

    # Make the build directory
    os.makedirs(build_dir, exist_ok=True)

    # Compile the runtime
    runtime_binary = compile.build_runtime(
        runtime_location, build_dir, args.timeout)

    # Run tests
    num_passing, failures = run_tests(root_dir,
                                      args.language,
                                      test_dir,
                                      build_dir,
                                      runtime_binary,
                                      args.timeout,
                                      output_dir,
                                      executable_save_dir)

    # Display a summary of results
    total_tests = num_passing + len(failures)
    utils.write_normal(
        f'{utils.as_green("TESTS PASSED:")} {num_passing}/{total_tests}')
    if len(failures) > 0:
        utils.write_normal(
            f'{utils.as_red("TESTS FAILED: ")} {len(failures)}/{total_tests}')
        for failure in failures:
            print(f'{utils.as_red("FAILED: ")} {failure}')


if __name__ == "__main__":
    main()
