# dmp_vlda
dmp\_lda is a simple tool to dump the binary output files from macxx and llf.

I've updated macxx and llf to build and run on both 32 and 64 Linux and Windows (under Mingw) operating systems. This tool was used to test and compare the binary outputs of
both old and new versions of macxx and llf to ensure they matched one another. The binary output of macxx is the .ob type file (made with macxx's -bin command line option) and
the binary output of llf is one of either a .vlda (made with llf's -bin or -vlda command line option) or .lb (made with llf's -rel and -bin or -vlda command line option).

Build this tool using one of the provided Makefiles. Run it with ./dmp\_vlda file. NOTE: It includes 5 header files from the macxx kit and expects to find them at ../macxx.

It outputs to stdout. An example output:

```FileSize=13116, fileExtent=65536
  0: 0x0049* 11(ID) err=0, warn=0, img="mac68k v12.04 (64 bit)", target=68000, "Jul 16 2025 09:59:14"
  1: 0x001D* 9(GSD) SEG: flags=0x001E, ssegm=".ABS.", ident=1, salign=1, dalign=1, base=0x0, max=0x0, offset=0x0
  2: 0x0007* 14(SLEN) ident=1, length=0
  ...
 27: 0x0014  9(GSD) SYM: flags=0x0001, symb="EEPROM", ident=14, value=0, noff=13, eoff=0, cnt=20:  <no expression>
 35: 0x001F* 9(GSD) SYM: flags=0x0013, symb="eer_bumph", ident=22, value=4118, noff=21, eoff=13, cnt=31:  Expr Terms=3, cnt=8 %13 4118 +
 60: 0x0004  10(ORG) Expr Terms=1, cnt=3 %13
558: 0x000A  12(EXPR) Expr Terms=4, cnt=9 %7 26 + :L
616: 0x01BB* 8(TXT) 00000074 -> 4E 65 77 20 47 61 6D 65 73 20 28 69 6E 20 73 65 63 6F +420 more
```

The FileSize is the size of the input file in bytes. The fileExtent is the size of the input buffer provided. This number is just for debugging. The first few
items on each line are:

* Record number
* Record length (an asterisk means the record has a padding byte on the end not included in the length)
* Record type followed by the symbolic name of the type (Could be any one of ABS, TXT. GSD, ORG, ID, EXPR, TPR, SLEN, XFER, TEST, DBGFILE, DBGSEG, BOFF or OOR)
* Other details are specific to the record type as described below:

* ABS - This will only appear in .vlda files. The 8 hex digit number is the physical address where the bytes are to be depositied.
* TXT - The 8 hex digit number is the relative address where the bytes are to be depositied.
* GSD - Global Symbol/Segment Definition. If a segment type, the keyword SEG will be present. If a symbol type the keyword SYM will be present. In the above examples, the symbol EEPROM is not defined where the symbol eer\_bumph is defined via the expression displayed at the end of the line.
* ORG - Sets the relative address (base address) where any subsequent bytes defined in TXT and EXPR records are to be deposited.
* ID  - This will always be the first record. There is only 1 of these per file. It contains, assembly errors and warnings made by the 'img', the target CPU and the timestamp of the assembly.
* EXPR - The bytes to deposit are computed according to the expression the laid in memory according to the tag (the last thing in the expression).
* TPR - Specifies a transparent record. The contents are not processed by either the assembler or the linker. They are passed through unchanged. LLF puts tekhex symbol records here.
* SLEN - Indicates the length of a segment. There will always be one of these for each segment defined.
* XFER - Indicates the address to which control is to be passed once the memory is loaded. Only present in vlda files.
* TEST - Specifies an expression that if resolves true, is to display the enclosed error message as an error.
* DBGFILE - Passes debug information
* DBGSEG - Passes debug information
* BOFF - An expression to be computed as a branch offset.
* OOR - An expression that tests for an out of range condition. The enclosed message is displayed if the out of range condition is met.




