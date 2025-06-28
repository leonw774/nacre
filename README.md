# NacRE

NacRE is a lightweight, NFA-based regular expression engine that supports a subset of Perl compatible regex (PCRE) syntax.

## Supported Syntax

- Literals: `a`, `b`, `c`, etc.
- Escaped Characters: `\f`, `\n`, `\r`, `\t`, `\v`
- Wildcards: `.`, `\d`, `\D`, `\w`, `\W`, `\s`, `\S`, 
- Anchors: `^`, `$`, `\b`
- Quantifiers: `*`, `+`, `?`, `{n}`, `{n,}`, `{n,m}`
- Alternation: `|`
- Grouping: `()`
- Character Classes: `[abc]`, `[^abc]`, `[a-z]`, `[\d\D\w\W\s\S]`

## Installation

1. Clone the repository:

```sh
git clone {this repo}
cd nacre
```

2. Build the project:
   
```sh
make release
```

3. Run the executable:

```sh
./nacre
```

## Usage

```sh
./nacre [OPTIONS] PATTERN INPUT_FILE
```

The default match mode is find the first match from the start of file to the end of the file.

### Options:
- `-g`: Global matching (find all matches)
- `-m`: Multiline matching (process input line by line).

### Example:

```sh
./nacre -gm "a.*b" example.txt
```

This command matches all occurrences of the pattern `a.*b` in `example.txt` with global and multiline options enabled.
