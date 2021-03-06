# Binpoke

Binpoke is a portable, console hex editor utility.  You can print out listings of sections of any binary file, which includes base-16 byte values as well as US-ASCII interpretations of the bytes.  You can also query and set the length of any binary file, as well as read and write any signed or unsigned integer value anywhere in the file.

## Compilation

Binpoke depends only on `libaksview`, and the dependency of that library, `aksmacro`.  You need to make sure that `aksmacro.h` and `aksview.h` are in the include path while compiling and that you link in the `libaksview` module, either by specifying `aksview.c` as one of the C source files you are compiling or by linking in a static library build.

On POSIX platforms (UNIX, UNIX-like, Linux, BSD, Solaris, OS X), you must define `_FILE_OFFSET_BITS=64` while compiling or you will get a compilation error from `libaksview`.

On Windows platforms, you must define both `UNICODE` and `_UNICODE` to enable Unicode mode while compiling or you will get a compilation error from Binpoke.  You should link Binpoke as a console application on Windows.

Provided that the `aksmacro.h` and `aksview.h` headers are in a folder named `/home/example_user/include` and that a static library build `libaksview.a` is in a folder named `/home/example_user/lib` the following is an example GCC invocation for building Binpoke on POSIX (everything should be on a single line):

    gcc
      -O2 -o binpoke
      -I/home/example_user/include
      -L/home/example_user/lib
      -D_FILE_OFFSET_BITS=64
      binpoke.c
      -laksview

## Syntax

The following are the invocation syntax styles for Binpoke:

    binpoke list [path] from [addr] for [count]
    binpoke read [path] at [addr] as [type]
    binpoke write [path] at [addr] as [type] with [value]
    binpoke query [path]
    binpoke resize [path] with [count]
    binpoke require [path]
    binpoke new [path]

The first parameter after the executable name must always be a _verb_ (`list` `read` `write` `query` `resize`) followed by `[path]`, which is the path to the binary file.  After the verb and path comes a sequence of one or more _phrases_.  Each phrase consists of a _preposition_ (`from` `for` `at` `as` `with`) followed by a _nominal_, which provides some kind of parameter value for the operation.  The invocation syntax list shown above defines exactly which phrases are required for each verb.  The phrases can be given in any order so long as the verb and path are first.

The `[addr]` nominals define a specific file offset within the binary file.  File offset zero is the first byte in the file, file offset one is the second byte in the file, and so forth.  The `[addr]` must refer to a byte that exists within the limits of the file.  Furthermore, for the `list` verb, the `[count]` added to the `[addr]` must not exceed the length of the file, and for the `read` and `write` verbs, no component byte of the integer may be beyond the file limits.  (The `[addr]` always gives the offset of the first byte of the integer for `read` and `write` verbs.)

The `[addr]` nominal may either be an unsigned decimal integer or an unsigned base-16 integer.  Unsigned base-16 integer values must have a prefix that is `0x` or `0X` (a zero, not the letter O) while decimal integer values must not have any prefix.  Whichever format is chosen, the resulting value must be in the range [0, `INT64_MAX`] where `INT64_MAX` is the maximum value of a signed 64-bit integer.

The `[count]` nominals must be unsigned decimal integers in the range [0, `INT64_MAX`] where `INT64_MAX` is the maximum value of a signed 64-bit integer.  For the `list` verb, the `[count]` gives the number of bytes that should be listed starting at `[addr]`, and it must be at least one and at most 65,536 (64K).  For the `resize` verb, the `[count]` is the new size of the file in bytes, and it must be at least zero and at most a maximum length limit defined by `libaksview` (which is so large it should never be an issue in practice).  Setting a gigantic file size might be a big problem in terms of immediately filling up the disk &mdash; or it might not be a problem at all if the file system supports "sparse" files.

The `[type]` nominals define a specific integer format to read and write.  The following table shows the supported values (case sensitive), whether the integer type is signed or unsigned, the bit width of the type, the endianness of the type, and the number of component bytes within integers of that type:

     Type  |  Signed  | Bit width | Endianness | Bytes
    =======+==========+===========+============+=======
     u8    | unsigned |     8     |    n/a     |   1
     s8    |  signed  |     8     |    n/a     |   1
    -------+----------+-----------+------------+-------
     u16le | unsigned |    16     |   little   |   2
     u16be | unsigned |    16     |    big     |   2
     s16le |  signed  |    16     |   little   |   2
     s16be |  signed  |    16     |    big     |   2
    -------+----------+-----------+------------+-------
     u32le | unsigned |    32     |   little   |   4
     u32be | unsigned |    32     |    big     |   4
     s32le |  signed  |    32     |   little   |   4
     s32be |  signed  |    32     |    big     |   4
    -------+----------+-----------+------------+-------
     u64le | unsigned |    64     |   little   |   8
     u64be | unsigned |    64     |    big     |   8
     s64le |  signed  |    64     |   little   |   8
     s64be |  signed  |    64     |    big     |   8

Binpoke always uses two's-complement format for signed integers.

The `[value]` nominal defines an integer value to write into the file.  If the value begins with a `-` or `+` sign then this sign must be followed by a sequence of decimal digits.  If the value begins with a `0x` or `0X` prefix (zero, not the letter O), then this prefix must be followed by a sequence of base-16 digits.  Otherwise, the value must be a sequence of decimal digits, which is equivalent to a value with a `+` sign in front of it.

Values given without any sign or prefix may be used with any type.  Values given with a `+` sign may also be used with any type.  Values given with a `-` sign may only be used with signed types.  Values given with a `0x` or `0X` prefix may be used with any type; however, if they are used with a signed type, Binpoke will act instead as if you provided the corresponding unsigned type (if you want a negative value in base-16, you must encode it in two's-complement yourself).

## Operation

The `list` verb will list a selected sequence of one or more bytes from within the binary file.  Each line in the listing has the following format:

    000023A0: 00 01 48 65 6c 6c 6f 20   77 6f 72 6c 64 21 fe fe | ..Hello world!..

The first field on each line is the file offset of first byte on the line, given in base-16.  A file offset of zero is the first byte in the file, a file offset of one is the second byte, and so forth.  Each line in the listing is always aligned to a 16-byte boundary, so the last digit of the base-16 address at the start of each line will always be zero.  If you are dealing with binary files of length greater than 4GB, then the first field will only be the 32 least significant bits of the address.

After the address field follows 16 unsigned byte values, each represented as two base-16 digits.  Single spaces are used to separate bytes, except that between the eighth and ninth bytes there is a larger gap of three spaces.  If the range of bytes chosen for listing does not cover the full 16-byte line in a listing, the missing byte values are replaced by two spaces.

The final field on the line is eight characters that give the US-ASCII interpretation of each byte value in the line.  Only byte values in printing range [0x20, 0x7E] will be displayed as ASCII characters; all other byte values will be replaced by a period character in the display.  (The period character is ambiguous, meaning either a byte value that is out of printing ASCII range or a byte value corresponding to the ASCII character for a period.)  If the range of bytes chosen for listing does not cover the full 16-byte line in a listing, the missing byte values are replaced by a space character in the character listing.

The `read` and `write` verbs allow you to read and write individual integer values within the file.  All component bytes of chosen integer locations must be within the file limits.  Resize the file first if you need to write an integer value beyond the current end of the file.

The `query` and `resize` verbs allow you to determine the current length in bytes of a file and change the current length in bytes of a file.

All verbs discussed so far require the indicated binary file to already exist.  The `require` verb, on the other hand, will do nothing if the binary file already exists and otherwise create a new binary file of length zero if the binary file does not already exist.  Finally, the `new` verb will create a new binary file of length zero, failing if the binary file already exists.
