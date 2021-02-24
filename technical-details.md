# Technical details

## Hashes

Frz uses the 256-bit [Blake3][blake3] [cryptographic hash
function][chf] for [content addressing][cas] and [content
checksumming][csum]. To the 256 bits of the Blake3 hash, it appends
the size of the hashed file in bytes, represented with the minimum
number of bits possible but rounded up so that the number of bits is 4
[modulo][mod] 5. For example, a size of 5 bytes gives 0101, and a size of 20
bytes gives 000010100.

[blake3]: https://github.com/BLAKE3-team/BLAKE3
[chf]: https://en.wikipedia.org/wiki/Cryptographic_hash_function
[cas]: https://en.wikipedia.org/wiki/Content-addressable_storage
[csum]: https://en.wikipedia.org/wiki/Checksum
[mod]: https://en.wikipedia.org/wiki/Modular_arithmetic

Since 256 is 1 modulo 5, the Blake3 hash and the file size together is
1 + 4 = 0 modulo 5&emdash;that is, the number of bits is a multiple
of 5. Frz encodes them five at a time with this base-32 alphabet
(chosen so that letters easily mistaken for digits are omitted):

| Digit | Decimal | Binary |
|-------|---------|--------|
| 0     | 0       | 00000  |
| 1     | 1       | 00001  |
| 2     | 2       | 00010  |
| 3     | 3       | 00011  |
| 4     | 4       | 00100  |
| 5     | 5       | 00101  |
| 6     | 6       | 00110  |
| 7     | 7       | 00111  |
| 8     | 8       | 01000  |
| 9     | 9       | 01001  |
| a     | 10      | 01010  |
| b     | 11      | 01011  |
| c     | 12      | 01100  |
| d     | 13      | 01101  |
| e     | 14      | 01110  |
| f     | 15      | 01111  |
| g     | 16      | 10000  |
| h     | 17      | 10001  |
| j     | 18      | 10010  |
| k     | 19      | 10011  |
| m     | 20      | 10100  |
| n     | 21      | 10101  |
| p     | 22      | 10110  |
| q     | 23      | 10111  |
| r     | 24      | 11000  |
| s     | 25      | 11001  |
| t     | 26      | 11010  |
| u     | 27      | 11011  |
| w     | 28      | 11100  |
| x     | 29      | 11101  |
| y     | 30      | 11110  |
| z     | 31      | 11111  |

For example (with a 16-bit hash instead of 256 bits to make the
example manageable), if the hash is [0x][hex]a139 =
1010,0001,0011,1001 and the file size is 87 = 101,0111, we get

[hex]: https://en.wikipedia.org/wiki/Hexadecimal

| Hash                | Extra zeros | File size |
|---------------------|-------------|-----------|
| 1010,0001,0011,1001 | 00          | 101,0111  |

which we then group in fives and translate with the table:

| 1st   | 2nd   | 3rd   | 4th   | 5th   |
|-------|-------|-------|-------|-------|
| 10100 | 00100 | 11100 | 10010 | 10111 |
| m     | 4     | w     | j     | q     |

giving us a final hash string of “m4wjq”.

### Why include the file size?

The file size is not necessary to identify the correct byte sequence,
but when we scan an arbitrary directory tree looking for a file with a
specific hash (as `frz fill` and `frz repair` do), knowing the size of
the file we’re looking for allows us to skip all files that don’t have
this size, which speeds things up greatly (unless all files have the
same size).

### Why base-32?

Using a [base-64][base64] or [base-85][base85] encoding would have
produced slightly shorter file names; however, different filesystems
allow different alphabets in their file names, and some are not case
sensitive. The alphabet above, consisting of the digits and 22 of the
lowercase letters, is a good compromise that works everywhere, is easy
to read because there are no [homoglyphs][homoglyph], and is easy to
work with because its size is a power of two.

[base64]: https://en.wikipedia.org/wiki/Base64
[base85]: https://en.wikipedia.org/wiki/Ascii85
[homoglyph]: https://en.wikipedia.org/wiki/Homoglyph

## Filesystem layout

Here’s an example Frz repository with two files. Directories are
indicated with a trailing `/`, and symlinks with `->` followed by
their target.

```
.frz/
  content/
    mt
    ww
  unused-content/
  blake3/
    7x/
      x2/
        t0yu7yra5hgpshyb2qxgxjwdcf36nhz305508b3udztf3dyj000000 -> ../../../../content/ww
    nr/
      r6/
        ttns8389pbzhmdgk1bf319bh6m3hmukans2nhg4ze85h73q1000000 -> ../../../../content/mt
my-file -> .frz/blake3/nr/r6/ttns8389pbzhmdgk1bf319bh6m3hmukans2nhg4ze85h73q1000000
sub/
  .frz -> ../.frz
  my-other-file -> .frz/blake3/7x/x2/t0yu7yra5hgpshyb2qxgxjwdcf36nhz305508b3udztf3dyj000000
```

Let’s go through the various files and directories and see what they
are.

### `.frz/content/`

This directory contains the actual files, write-protected. File names
and subdirectory structure don’t matter; however, `frz` will create
short, random names and limit the number of files in each directory.

It is perfectly legal to manually add files to this directory (using
whatever file names and directory structure you like), or remove files
that were already here; just run `frz repair` afterwards.

### `.frz/unused-content/`

This is exactly like `.frz/content/`, except these are files we don't
need. This directory exists so that Frz operations that encounter
duplicate content files can move them here instead of deleting them
immediately.

It is perfectly legal to manually add files to this directory (using
whatever file names and directory structure you like), or remove files
that were already here. `frz repair` and `frz fill` will look here
first whenever there’s a content file missing.

### `.frz/blake3/`

This directory contains symlinks that point to the files in
`.frz/content/`; there’s supposed to be exactly one symlink per
content file, and the name of the symlink is the base-32 blake3 hash
and size of the content file. So as to not put too many files in each
directory, the first two base-32 digits are used to indicate a
subdirectory, the third and fourth base-32 digits are used to indicate
a second-level subdirectory, and the remaining digits form the
filename of the symlink.

This entire directory tree is just a cache; you can delete it
completely, and `frz repair` will simply regenerate it.

### `.frz`

At the root of the repository, `.frz` is a directory with
subdirectories for the content files and their index (see above). In
subdirectories, `.frz` is a symlink to the `.frz` directory in the
root; so, in our example, `sub/.frz` is a symlink that points to
`../.frz`, which is `.frz` in the repository root.

`frz add` creates these symlinks whenever it adds new files in a
directory, and `frz repair` recreates them if they are lost.

### User files (everything outside `.frz/`)

User files are symlinks that use the `.frz` symlinks and the
`.frz/blake3` symlink tree to point to the actual file contents. In
our example, `my-file` in the root directory points to
`.frz/blake3/nr/r6/ttns8389pbzhmdgk1bf319bh6m3hmukans2nhg4ze85h73q1000000`,
which in turn points to `.frz/content/mt`, which is the actual file.

Similarly, `sub/my-other-file` points to
`sub/.frz/blake3/7x/x2/t0yu7yra5hgpshyb2qxgxjwdcf36nhz305508b3udztf3dyj000000`;
since `sub/.frz` points to `.frz`, this successfully leads us to
`.frz/blake3/7x/x2/t0yu7yra5hgpshyb2qxgxjwdcf36nhz305508b3udztf3dyj000000`,
which points to `.frz/content/ww`, which is the actual file.

Because of the indirection through the `.frz` symlink in each
subdirectory, the user symlinks can be moved around without having to
change.

## Repair/fill algorithm

In this order,

  1. Remove bad `.frz/blake3/` symlinks. After this step, all
     remaining `.frz/blake3/` symlinks point to content files that
     exist and actually have the indicated hash.

     For each pre-existing `.frz/blake3/` symlink:

       1. Ensure that its syntax (file name and target) is correct.

       2. Ensure that it points to an existing file in
          `.frz/content/`.

       3. Check that the `.frz/content/` file has the expected size.

       4. Either read the first byte of the content file _[fast]_, or
          read the entire file and recompute the hash _[slow]_.

       5. As soon as any check fails, remove the current
          `.frz/blake3/` symlink and continue with the next one.

  2. Add missing `.frz/blake3/` symlinks. After this step, all content
     files are indexed by `.frz/blake3/` symlinks.

     For every `.frz/content/` file that we didn’t visit in step (1)
     because a `.frz/blake3/` symlink pointed to it:

       1. Compute the hash+size string.

       2. If a `.git/blake3/` symlink already exists for that
          hash+size, move the current `.frz/content/` file to
          `.frz/unused-content/`.

       3. Otherwise, create a `.frz/blake3/` symlink for it.

  3. Check that we have all content that we’re supposed to. After this
     step, we’ve either obtained each piece of missing content, or
     alerted the user about it.

     For every user file (symlink in the repository with the correct
     syntax):

       1. Ensure that there is a `.frz` symlink in its directory
          (unless it is in the repository’s root directory, where the
          `.frz/` directory is).

       2. If its `.frz/blake3/` target symlink doesn’t exist, look
          elsewhere for a file with that hash+size (first in
          `.frz/unused-content/`, then in external directories).

We can consider three levels of repair:

| Steps            | Frz command         |
|------------------|---------------------|
| 1 _[slow]_, 2, 3 | `frz repair`        |
| 1 _[fast]_, 2, 3 | `frz repair --fast` |
| 3                | `frz fill`          |

`frz repair` does the complete list of repair steps, but is slowest.
`frz fill` does only step 3, which is fastest but will fail to detect
some classes of errors.
