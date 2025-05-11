# NacRE

NacRE is a lightweight, NFA-based regular expression engine that supports a subset of regex syntax. It is designed to parse and match patterns efficiently using NFA.

## Supported Syntax

- Literals: `a`, `b`, `c`, etc.
- Wildcards: `.`, `\d`, `\D`, `\w`, `\W`, `\s`, `\S`, 
- Anchors: `^`, `$`
- Quantifiers: `*`, `+`, `?`, `{n}`, `{n,}`, `{n,m}`
- Alternation: `|`
- Grouping: `()`
- Character Classes: `[abc]`, `[^abc]`, `[a-z]`, `[\d\w]`

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

### Options:
- `-g`: Global matching (find all matches).
- `-m`: Multiline matching (process input line by line).

### Example:

```sh
./nacre -gm "a.*b" example.txt
```

This command matches all occurrences of the pattern `a.*b` in `example.txt` with global and multiline options enabled.
