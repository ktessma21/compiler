#!/usr/bin/env python3
import os
import argparse
import subprocess
import utils
from pathlib import Path
from typing import List, Optional

# Command line arguments to each compiler binary
ARGUMENTS = ['-g', '1', f'-O0']

# Languages in order of compilation (left to right)
LANGUAGES = ['LC', 'LB', 'LA', 'IR', 'L3', 'L2', 'L1']
# Relative path to the binary for a given language
RELATIVE_BINARY = 'bin'
# The name of the expected output file
OUTPUT_NAME = 'prog'
# The name of our runtime object file
RUNTIME_FILE = 'runtime.o'


def build_runtime(runtime_location: str,
                  build_dir: str,
                  timeout: int) -> str:
    """
    Builds our runtime to the given build directory
    Returns the runtime binary location if this succeeded,
      and an empty string otherwise
    """
    assert os.path.exists(runtime_location)
    runtime_binary = os.path.join(build_dir, RUNTIME_FILE)
    runtime_args = ['gcc', '-O2', '-c', '-g',
                    '-o', runtime_binary, runtime_location]
    runtime_result = subprocess.run(
        runtime_args,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout
    )

    if len(runtime_result.stderr) > 0:
        msg = f'Runtime building error\n{runtime_result.stderr.decode()}'
        utils.write_error(msg)
        return ''

    return runtime_binary


def run_intermediate_binary(binary_location: str,
                            arguments: List[str],
                            input_file: str,
                            build_dir: str,
                            timeout: int) -> bool:
    """
    Runs the given compiler binary with the provided arguments and input string
    Writes the result to build_dir (i.e. use build_dir as our working directory)
    Returns True if building succeeded, and False otherwise
    """
    compiler_args = [binary_location, input_file] + arguments
    comp_result = subprocess.run(
        compiler_args,
        cwd=build_dir,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout
    )

    if len(comp_result.stderr) > 0:
        error = comp_result.stderr.decode()
        msg = f'Binary {binary_location} failed to compile with error:\n{error}'
        utils.write_error(msg)
        return False

    return True


def compile_language(root_dir: str,
                     starting_language: str,
                     program_file: str,
                     build_dir: str,
                     runtime_binary: str,
                     timeout: int) -> str:
    """
    Builds the given language compiler in a local build directory
    Returns the location of the executable file
      Returns "" instead if any step failed
    """
    # Run through each compiler, starting from our current language
    for language in LANGUAGES[LANGUAGES.index(starting_language):]:
        language_file = program_file
        # If not the first language, update our input file
        if language != starting_language:
            language_file = os.path.join(
                build_dir, f'{OUTPUT_NAME}.{language}')
            assert os.path.exists(
                language_file), f'Missing intermediate file {language_file}'

        binary_location = os.path.join(root_dir, language,
                                       RELATIVE_BINARY, language)
        if not run_intermediate_binary(binary_location,
                                       ARGUMENTS,
                                       language_file,
                                       build_dir,
                                       timeout):
            return ''

    # This should have ended up producing an assembly file
    assembly_file = os.path.join(build_dir, f'{OUTPUT_NAME}.S')
    assert os.path.exists(assembly_file)

    # Assemble the resulting file
    assembled_file = os.path.join(build_dir, f'{OUTPUT_NAME}.o')
    assembler_args = ['as', assembly_file, '-o', assembled_file]
    assembler_result = subprocess.run(assembler_args,
                                      timeout=timeout,
                                      stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE)
    if len(assembler_result.stderr) > 0:
        msg = f'Assembler error\n{assembler_result.stderr.decode()}'
        utils.write_error(msg)
        return ""

    # Link our assembly file
    executable_file = os.path.join(build_dir, f'{OUTPUT_NAME}.out')
    # linker_args = ['gcc', '-no-pie', '-o',
    #                executable_file, assembled_file, runtime_binary]
    linker_args = ['gcc', '-no-pie', '-o',
                   executable_file, assembled_file, runtime_binary,
                   '-z', 'noexecstack']
    linker_result = subprocess.run(linker_args,
                                   timeout=timeout,
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE)
    if len(linker_result.stderr) > 0:
        msg = f'Linker error\n{linker_result.stderr.decode()}'
        utils.write_error(msg)
        return ""

    return executable_file


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
    parser.add_argument('-t', '--timeout', type=int, default=240,
                        help='Timeout per test in seconds (default: 240)')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Detailed runtime messages')
    parser.add_argument('-q', '--quiet', action='store_true',
                        help='Disable all non-essential script messages')

    args = parser.parse_args()
    assert args.language in LANGUAGES, f'Invalid language choice {args.language}'

    utils.set_verbosity_from_args(args)

    # Absolute paths for consistency in calls
    root_dir = os.path.abspath(args.root_dir)
    assert os.path.exists(root_dir), f'Missing root directory {root_dir}'
    build_dir = os.path.abspath(args.build_dir)
    program_file = os.path.abspath(args.filename)
    assert os.path.exists(program_file), f'Missing program file {program_file}'
    runtime_location = os.path.abspath(args.runtime)
    assert os.path.exists(
        runtime_location), f'Missing runtime {runtime_location}'

    # Make the build directory
    os.makedirs(build_dir, exist_ok=True)

    # Check invariants
    assert args.timeout > 0, f'Expected a timeout > 0, got {args.timeout}'

    # Compile the runtime
    runtime_binary = build_runtime(runtime_location, build_dir, args.timeout)

    # Compile our code
    executable_file = ''
    if runtime_location:
        # Run tests
        executable_file = compile_language(root_dir,
                                           args.language,
                                           program_file,
                                           build_dir,
                                           runtime_binary,
                                           args.timeout)

    if runtime_location and executable_file:
        print('Compilation Succeeded')
    else:
        print('Compilation Failed')


if __name__ == "__main__":
    main()
