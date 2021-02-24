# Frz, a simple utility for managing large files with Git

`frz` is a simple command-line utility that makes it possible to
manage large amounts of data with [Git](https://git-scm.com/) in a
completely peer-to-peer way. It can scan a repository to make sure
that all files are present and undamaged, and if some files are
missing or broken, it can look for good copies in arbitrary directory
trees.

## Overview

Frz replaces your files with symlinks that point to
[content-indexed][cas] files in a directory named `.frz`. Frz manages
the (large) files in there, and Git manages the (tiny) symlinks.

[cas]: https://en.wikipedia.org/wiki/Content-addressable_storage

You can use any Git workflow you like for the symlinks: sync with a
central repository, sync peer-to-peer, use tags, use branches, etc.
All you need to remember is to use `frz add` instead of `git add`.
Whenever a Git operation leaves you with symlinks that point to `.frz`
content files that you don’t yet have, you run `frz fill` to fetch the
missing content from a place that does have it.

`frz fill` scans the directory tree for symlinks whose targets are
missing, which is fast; there is also `frz repair`, which additionally
checks that the content files that you already have are not damaged.

## Selling points

* **You don’t need `frz` if you just want to read your data.** All
  `frz` does is write-protect your files and move them to a separate
  directory, leaving a directory tree with symlinks behind.
  
* **Frz never stores multiple copies of your content.** If you have a
  10 TB disk, you can use Frz and Git to manage very close to 10 TB of
  data.
  
* **Frz deduplicates your data** on a file level. If you have two
  files with the same content, Frz stores only one copy.

* **You can use the full suite of Git tools and workflows to manage
  the tree of symlinks.** Sync with a central repository, sync
  peer-to-peer, use tags, use branches, inspect the commit graph,
  restore old revisions, etc. Frz doesn’t get in the way; you just
  need to run `frz fill` whenever a Git operation leaves you with
  symlinks that point to `.frz` content files that you don’t yet have.
  
* **There’s no Frz server.** `frz fill` and `frz repair` fetch files
  from any directory tree, not even necessarily another Frz
  repository.
  
* **Content synchronisation, content error detection, and content
  repair are the same thing.** When you run `frz fill` to fetch
  content that you don’t yet have, Frz does exactly the same thing as
  when you run `frz repair`, except that it skips some expensive
  verification of the content files that you already have. This makes
  Frz simpler to use and simpler to develop, and reduces the amount of
  code (and the number of bugs) in seldom-used error paths.

## License

Frz is distributed under the Apache 2.0 license. See
[LICENSE.txt](LICENSE.txt) for details.

## Building from source

You need

  * [CMake](https://cmake.org/) 3.14 or later
  * [GCC](https://gcc.gnu.org/) 10.2 or later
  * [libgit2](https://libgit2.org/)

On Debian and Ubuntu, `sudo apt install cmake gcc libgit2-dev` should
install these.

Download the sources; then, in that directory,

```
$ mkdir build
$ cd build
$ cmake -D CMAKE_BUILD_TYPE=Release ..
$ make frz
$ ./frz --help  # run the new frz binary!
```

## Technical details

There’s a separate page with [technical details](technical-details.md)
for the curious.

## Contributing

Contributions are welcome! See the [developer
documentation](developer.md).

## Demo

### Create a directory with some files

We only have two modestly-sized files in this example, but Frz is
designed to work with terabytes of data and as many files as Git can
handle.

<pre>
$ <b>mkdir -p frz-test/foo</b>
$ <b>cd frz-test</b>
$ <b>head -c 1G /dev/urandom > one.bin</b>
$ <b>head -c 2G /dev/urandom > foo/two.bin</b>
</pre>

### Create the repo

One day, there will be a `frz init` command. Until then, we have to do
this by hand:

<pre>
$ <b>git init</b>
Initialized empty Git repository in /tmp/frz-test/.git/
$ <b>mkdir .frz</b>
$ <b>echo .frz >> .git/info/exclude</b>
</pre>

### Add the files

`frz add` will add all files in a specified directory tree. “`.`” is
the current directory, so this command will add everything:

<pre>
$ <b>frz add .</b>
+ foo/two.bin
+ one.bin

2 files successfully added
0 files successfully added and deduplicated
0 directory entries skipped because they weren't regular files
0 files skipped because of errors
</pre>

We can see that `frz add` replaced the files with symlinks, and that
the file contents is stored in the .frz directory:

<pre>
$ <b>ls -l one.bin</b>
lrwxrwxrwx 1 user user 72 Feb 21 13:59 one.bin -> .frz/blake3/nr/r6/ttns8389pbzhmdgk1bf319bh6m3hmukans2nhg4ze85h73q1000000
</pre>

Now commit it. `frz add` already ran `git add` for us, so we only need
to run `git commit`:

<pre>
$ <b>git commit -m "My first commit"</b>
 2 files changed, 2 insertions(+)
 create mode 120000 foo/two.bin
 create mode 120000 one.bin
</pre>

### Clone the repository

<pre>
$ <b>cd ..</b>
$ <b>git clone frz-test frz-test-clone</b>
Cloning into 'frz-test-clone'...
done.
$ <b>cd frz-test-clone</b>
$ <b>mkdir .frz</b>
$ <b>echo .frz >> .git/info/exclude</b>
</pre>

At this point, we have the whole git-controlled tree of symlinks, but
are missing the content files that the symlinks point to. `frz fill`
will fix that for us, if we point it to a directory tree where copies
of the required files can be found:

<pre>
$ <b>frz fill --copy-from ../frz-test</b>
Checking that referenced content is present... 
  Listing files in /tmp/frz-test... done (34 files)
  Hashing files... done (0 files, 2147483648 bytes)   
  Hashing files... done (0 files, 1073741824 bytes)   
Checking that referenced content is present... done (2 links)
Content files
  2 missing (restored)
  0 missing (not restored)
</pre>

### Pretend that our repository was damaged, and repair it

We remove the write protection from one of our files and overwrite the
first kilobyte. When we run `frz repair`, the checksum fails to match,
and `frz` looks for a good copy of the file in the directory we
specified.

<pre>
$ <b>chmod u+w one.bin</b>
$ <b>dd if=/dev/urandom of=one.bin bs=1k count=1 conv=notrunc</b>
1+0 records in
1+0 records out
1024 bytes (1.0 kB, 1.0 KiB) copied, 0.000210962 s, 4.9 MB/s
$ <b>frz repair --copy-from ../frz-test</b>
Checking index links and content files...                 
  Removing nrr6ttns8389pbzhmdgk1bf319bh6m3hmukans2nhg4ze85h73q1000000 from the index because it points to x8, which has the wrong hash (5jw4kk42pw8fxabdz10wzxyx5g1zd0j93txtfcze39gsrf30tnbh000000).
Checking index links and content files... done (2 links, 2 files)
Checking orphaned content files...                          
  Adding 5jw4kk42pw8fxabdz10wzxyx5g1zd0j93txtfcze39gsrf30tnbh000000 to the index, pointing to x8 (content was already present, but not indexed).
Checking orphaned content files... done (1 files, 1073741824 bytes)
Checking that referenced content is present... 
  Listing files in /tmp/frz-test... done (34 files)
  Hashing files... done (0 files, 1073741824 bytes)   
Checking that referenced content is present... done (2 links)
Index symlinks
  1 OK
  1 bad (removed)
  1 missing (recreated)
Content files
  0 duplicates (moved aside)
  1 missing (restored)
  0 missing (not restored)
</pre>
