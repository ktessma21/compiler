import utils
import argparse
import subprocess
from enum import Enum
from typing import Union, List, Dict


class InterferenceData:
    """
    Stores parsed liveness output
    """
    _var_mapping: Dict[str, List[str]]

    def __init__(self):
        self._var_mapping = {}

    def add_mapping(self, var: str, rest: List[str]):
        """
        Adds the given variable with a (sorted) rest map to our mapping
        """
        assert var not in self._var_mapping, f'Duplicate {var} found in interference graph'
        rest.sort()
        self._var_mapping[var] = rest

    def __str__(self) -> str:
        sorted_keys = list(self._var_mapping.keys())
        sorted_keys.sort()
        result = ''
        for key in sorted_keys:
            result += f'{key} {" ".join(self._var_mapping[key])}\n'
        return result

    def __eq__(self, other):
        if not isinstance(other, InterferenceData):
            return False
        for key in self._var_mapping:
            if key not in other._var_mapping:
                return False
            if self._var_mapping[key] != other._var_mapping[key]:
                return False
        return True


def run_interference(binary_location: str,
                     input_file: str,
                     timeout: int) -> Union[InterferenceData, str]:
    """
    Runs the compiler at binary_location with single-function interference
        on input_file, piping the result to the function output
    Specifically provides `BIN -g 0 -i -O0 input_file` as compiler arguments
    Returns either an error (str) or the results from interference (InterferenceData)
    """
    compiler_args = [f'{binary_location}',
                     '-g', '0',
                     '-i',
                     '-O0',
                     f'{input_file}']
    process = subprocess.run(compiler_args, 
                             stdout=subprocess.PIPE, 
                             stderr=subprocess.PIPE, 
                             timeout=timeout)
    if len(process.stderr.strip()) > 0:  # non-empty error
        utils.write_error(process.stderr.decode().strip())
        return process.stderr
    return parse_interference_results(process.stdout)


def parse_interference_results(data: Union[str, bytes]) -> InterferenceData:
    """
    Parses and sorts interference results line-by-line
    """
    result = InterferenceData()
    if type(data) == bytes:
        data = data.decode()
    for line in data.split('\n'):
        line = line.strip()
        if len(line) > 0:
            vars = line.split()
            result.add_mapping(vars[0], vars[1:])
    return result


def main():
    parser = argparse.ArgumentParser(description='Interference Analysis')
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
    result = run_interference(args.binary, args.input_file, args.timeout)
    if args.out is None:
        # print output regardless of verbosity for this script
        print(result)
    else:
        utils.save(result, args.out)


if __name__ == '__main__':
    main()
