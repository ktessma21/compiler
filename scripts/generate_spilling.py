import utils
import argparse
import subprocess
from typing import Union, List


class SpillData:
    """
    Stores parsed spill output
    Mostly used to be type-compatible with LivenessData and InterferenceData
    """
    _data: List[str]

    def __init__(self):
        self._data = []

    def add_line(self, line: str):
        """
        Adds a line of data
        Empty lines are skipped
        """
        line = line.strip()
        if len(line) > 0:
            self._data.append(line)

    def __str__(self) -> str:
        result = ''
        for line in self._data:
            if line.startswith('(') or line.startswith(')'):
                result += f'{line}\n'
            else:
                result += f'\t{line}\n'
        return result.strip()

    def __eq__(self, other):
        if not isinstance(other, SpillData):
            return False
        # Straightforward comparisons are fine I guess
        return self._data == other._data


def run_spilling(binary_location: str,
                     input_file: str,
                     timeout: int) -> Union[SpillData, str]:
    """
    Runs the compiler at binary_location with single-function spilling
        on input_file, piping the result to the function output
    Specifically provides `BIN -g 0 -s -O0 input_file` as compiler arguments
    Returns either an error (str) or the results from spilling (SpillData)
    """
    compiler_args = [f'{binary_location}',
                     '-g', '0',
                     '-s',
                     '-O0',
                     f'{input_file}']
    process = subprocess.run(compiler_args,
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE,
                             timeout=timeout)
    if len(process.stderr.strip()) > 0:  # non-empty error
        utils.write_error(process.stderr.decode().strip())
        return process.stderr
    return parse_spill_results(process.stdout)


def parse_spill_results(data: Union[str, bytes]) -> SpillData:
    """
    Parses and sorts spilling results line-by-line
    """
    result = SpillData()
    if type(data) == bytes:
        data = data.decode()
    for line in data.split('\n'):
        result.add_line(line)
    return result


def main():
    parser = argparse.ArgumentParser(description='Spilling Analysis')
    parser.add_argument('binary',
                        help='Binary location (relative or absolute)')
    parser.add_argument('input_file',
                        help='Input file location (relative or absolute)')
    parser.add_argument("-t", "--timeout", type=int, default=60,
                        help="Timeout per test in seconds (default: 60)")
    parser.add_argument('-o', '--out',
                        help='Optional output file location (relative or absolute)')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Detailed runtime messages')
    parser.add_argument('-q', '--quiet', action='store_true',
                        help='Disable all non-essential script messages')
    args = parser.parse_args()

    utils.set_verbosity_from_args(args)
    assert args.timeout > 0, f'Expected a timeout > 0, got {args.timeout}'
    result = run_spilling(args.binary, args.input_file, args.timeout)
    if args.out is None:
        # print output regardless of verbosity for this script
        print(result)
    else:
        utils.save(result, args.out)


if __name__ == '__main__':
    main()
