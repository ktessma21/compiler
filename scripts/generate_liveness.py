import utils
import argparse
import subprocess
from enum import Enum
from typing import Union, List


class LivenessData:
    """
    Stores parsed liveness output
    """
    _inputs: List[List[str]]
    _outputs: List[List[str]]

    def __init__(self):
        self._inputs = []
        self._outputs = []

    def add_input_line(self, line: List[str]):
        """
        Sorts the given line and adds it to our inputs
        """
        line.sort()
        self._inputs.append(line)

    def add_output_line(self, line: List[str]):
        """
        Sorts the given line and adds it to our outputs
        """
        line.sort()
        self._outputs.append(line)

    def __str__(self) -> str:
        result = '(\n'

        result += '\t(in\n'
        for line in self._inputs:
            result += f'\t\t({" ".join(line)})\n'
        result += '\t)\n'

        result += '\t(out\n'
        for line in self._inputs:
            result += f'\t\t({" ".join(line)})\n'
        result += '\t)\n'

        result += ')'
        return result

    def __eq__(self, other):
        if not isinstance(other, LivenessData):
            return False
        return self._inputs == other._inputs and \
            self._outputs == other._outputs


def run_liveness(binary_location: str,
                 input_file: str,
                 timeout: int) -> Union[LivenessData, str]:
    """
    Runs the compiler at binary_location with single-function liveness
        on input_file, piping the result to the function output
    Specifically provides `BIN -g 0 -l -O0 input_file` as compiler arguments
    Returns either an error (str) or the results from liveness (LivenessData)
    """
    compiler_args = [f'{binary_location}',
                     '-g', '0',
                     '-l',
                     '-O0',
                     f'{input_file}']
    process = subprocess.run(compiler_args,
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE,
                             timeout=timeout)
    if len(process.stderr.strip()) > 0:  # non-empty error
        utils.write_error(process.stderr.decode().strip())
        return process.stderr
    return parse_liveness_results(process.stdout)


def parse_liveness_results(data: Union[str, bytes]) -> LivenessData:
    """
    Parses and sorts liveness results line-by-line
    "Usual" outputs are sorted lexicographically
    Parsing is rather naive and brittle
    """
    class State(Enum):
        START = 0
        IN = 1
        OUT = 2
    state = State.START
    result = LivenessData()
    if type(data) == bytes:
        data = data.decode()
    for index, line in enumerate(data.split('\n')):
        line = line.strip()
        if line.startswith('(in'):
            assert (
                state == state.START), f'Line {index}: Expected a single "in" before any "out"'
            state = State.IN
        elif line.startswith('(out'):
            assert (
                state == state.IN), f'Line {index}: Expected a single "out" after an "in" line'
            state = State.OUT
        elif state != state.START:          # skip starting line(s)
            if line.startswith('('):        # ignore non-parenthesis lines
                assert line.endswith(
                    ')'), f'Line {index}: missing closing parenthesis'
                registers = line[1:-1].split()
                if state == State.IN:
                    result.add_input_line(registers)
                else:
                    result.add_output_line(registers)
    return result


def main():
    parser = argparse.ArgumentParser(description='Liveness Analysis')
    parser.add_argument('binary',
                        help='Binary location (relative or absolute)')
    parser.add_argument('input_file',
                        help='Input file location (relative or absolute)')
    parser.add_argument('-o', '--out',
                        help='Optional output file location (relative or absolute)')
    parser.add_argument('-t', '--timeout', type=int, default=60,
                        help='Timeout per test in seconds (default: 60)')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Detailed runtime messages')
    parser.add_argument('-q', '--quiet', action='store_true',
                        help='Disable all non-essential script messages')
    args = parser.parse_args()

    utils.set_verbosity_from_args(args)
    assert args.timeout > 0, f'Expected a timeout > 0, got {args.timeout}'
    result = run_liveness(args.binary, args.input_file, args.timeout)
    if args.out is None:
        # print output regardless of verbosity for this script
        print(result)
    else:
        utils.save(result, args.out)


if __name__ == '__main__':
    main()
