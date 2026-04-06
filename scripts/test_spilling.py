import generate_spilling as spilling
import utils
import argparse
import os
from pathlib import Path
from typing import Optional, Tuple, List


def test_file(binary_location: str,
              input_file: Path,
              timeout: int,
              output_file: Optional[str] = None) -> bool:
    """
    Tests the given file, writing output to the given output_file
    Returns True if the test passed, and False otherwise
    """
    test_name = input_file.name
    try:
        result = spilling.run_spilling(binary_location,
                                       input_file,
                                       timeout)
        utils.save(result, output_file)

        # Read oracle if it exists
        oracle_name = input_file.with_suffix('.L2f.out')
        if not os.path.exists(oracle_name):
            utils.write_normal(
                f'{utils.as_red("ERROR:")} Missing oracle file {oracle_name}')
            return False
        with open(oracle_name, 'r') as ifile:
            expected = spilling.parse_spill_results(ifile.read())

        # Check if the test passing
        passed = result == expected
        if not passed:
            utils.write_normal(f'{utils.as_red("Failed:")} {test_name}')
        return passed

    except Exception as e:
        utils.write_error(f'Test {test_name} failed with error: {str(e)}')
        if output_file is not None:
            with open(output_file, 'w') as ofile:
                ofile.write(f'Error: {str(e)}')
        return test_name


def test_dir(binary_location: str,
             input_dir: str,
             timeout: int,
             output_dir: Optional[str] = None) -> Tuple[int, List[str]]:
    """
    Runs all tests in the given directory, 
        writing out to the given results directory
    If output_dir is none, does not write the results anywhere
    Returns the number of passing and failing tests (respectively)
    """
    assert os.path.exists(binary_location), f'{binary_location} does not exist'
    assert os.path.isdir(input_dir), f'{input_dir} not a dir'

    # Get each test file
    input_files = os.listdir(input_dir)
    num_passing = 0
    failing_tests = []
    if output_dir is not None:
        os.makedirs(output_dir, exist_ok=True)

    for filename in input_files:
        if filename.endswith('.L2f'):
            input_file = Path(input_dir) / Path(filename)
            test_name = Path(filename).name
            output_file = None
            if output_dir is not None:
                output_file = output_dir + test_name + '.result'
            passed = test_file(binary_location,
                               input_file,
                               timeout,
                               output_file)
            if passed:
                num_passing += 1
            else:
                failing_tests.append(test_name)
            test_count = num_passing + len(failing_tests)
            if test_count > 0 and test_count % 20 == 0:
                utils.write_normal(f'Running test #{test_count}')

    return num_passing, failing_tests


def main():
    parser = argparse.ArgumentParser(description='Liveness tests')
    parser.add_argument('binary',
                        help='Binary location (relative or absolute)')
    parser.add_argument('tests',
                        help='Test directory location (relative or absolute)')
    parser.add_argument('-o', '--out',
                        help='Optional output directory location (relative or absolute)')
    parser.add_argument("-t", "--timeout", type=int, default=30,
                        help="Timeout per test in seconds (default: 30)")
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Detailed runtime messages')
    parser.add_argument('-q', '--quiet', action='store_true',
                        help='Disable all non-essential script messages')
    args = parser.parse_args()

    utils.set_verbosity_from_args(args)
    assert args.timeout > 0, f'Expected a timeout > 0, got {args.timeout}'
    num_passing, failures = test_dir(args.binary,
                                     args.tests,
                                     args.timeout,
                                     args.out)

    total_tests = num_passing + len(failures)
    utils.write_normal(
        f'{utils.as_green("TESTS PASSED:")} {num_passing}/{total_tests}')
    if len(failures) > 0:
        utils.write_normal(
            f'{utils.as_red("TESTS FAILED:")} {len(failures)}/{total_tests}')

    # Formats output minimally for the autograder
    if utils.verbosity == utils.Verbosity.QUIET:
        if (len(failures) > 0):
            fail_string = '\n\t'.join(failures)
            print('Failed tests:\n\t' + fail_string + '\n')
        print(f'{num_passing}/{total_tests} tests passed')
        print(f'{len(failures)}/{total_tests} tests failed')


if __name__ == '__main__':
    main()
