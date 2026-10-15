"""Current header and archive contracts, independent of historical qualification."""
import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
CONTRACTS = ROOT / 'tools/ci/native-prefix'
SYMBOLS = (
    'JS_NewStringUTF16', 'JS_ToCStringLenUTF16', 'JS_FreeCStringRT',
    'JS_FreeCStringUTF16', 'JS_FreeCStringRT_UTF16', 'JS_NewTemporalInstant',
    'JS_GetTemporalInstantEpochNanoseconds', 'JS_SetIntlDefaultLocale',
    'JS_GetIntlDefaultLocale', 'JS_SetDefaultTimeZone', 'JS_GetDefaultTimeZone',
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default='gcc', choices=('gcc', 'clang'))
    args = parser.parse_args()
    cxx = 'g++' if args.cc == 'gcc' else 'clang++'
    common = ['-O2', '-Wall', '-Wextra', '-Wno-unused-parameter', '-Werror', '-Iinclude']

    def compile(compiler, language, standard, source, extra=()):
        subprocess.run([compiler, '-x', language, '-std=' + standard, *common,
                        *extra, '-fsyntax-only', str(CONTRACTS / source)], cwd=ROOT, check=True)

    for name in ('utf16', 'instant', 'default_time_zone'):
        for compiler, language, standard in ((args.cc, 'c', 'c11'), (cxx, 'c++', 'c++17')):
            for checked in ([], ['-DCONFIG_CHECK_JSVALUE']):
                compile(compiler, language, standard, name + '_abi_contract.c', checked)
    for name, macro in (('abi_contract.c', '-DCHECK_INTL_API'),
                        ('new_api_abi_contract.c', '-DCHECK_UTF16_API')):
        for checked in ([], ['-DCONFIG_CHECK_JSVALUE']):
            compile(args.cc, 'c', 'c11', name, [macro, *checked])
    for compiler, language, standard in ((args.cc, 'c', 'c99'), (cxx, 'c++', 'c++11')):
        for order in ('1', '2'):
            compile(compiler, language, standard, 'private_header_order.c',
                    ['-Isrc', '-Isrc/quickjs', '-DHEADER_ORDER=' + order])
    definitions = subprocess.check_output(
        ['nm', '-g', '--defined-only', 'libquickjs.a'], cwd=ROOT, text=True)
    for symbol in SYMBOLS:
        if len(re.findall(r'[ \t]' + re.escape(symbol) + r'$', definitions, re.M)) != 1:
            raise ValueError('Public API must have exactly one archive definition: ' + symbol)
    print('C/C++ checked signatures, private header order and unique public exports passed.')


if __name__ == '__main__':
    main()
