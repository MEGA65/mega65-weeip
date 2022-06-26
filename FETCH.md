# Fetch -- the web-like browser for the MEGA65

## License

(C) Copyright Paul Gardner-Stephen, 2020-2022 -- see LICENSE for more details

## Overview

The MEGA65 is a retro-computer that is basically an improved Commodore 65, which
in turn is an improved Commodore 64.  For the purposes of this browser, the
important points are:

1.  40MHz 8-bit CPU
2.  384KB main RAM accessable via the video controller
3.  Optional 8MB or larger expansion RAM.  Only the CPU can see it, and its slower than the main RAM.  Handy for stashing files we are downloading, but not much else, for this project.
4.  100Mbit ethernet controller
5.  CC65 the C compiler we are using produces quite large code, and can only really use the first 64KB of RAM directly, so we have to chop the program up into several pieces that run each other as required.
6.  The MEGA65 provides a function that quickly loads a file from the FAT32 filesystem on the SD card, making (4) quite feasible.

## History and Motivation

The initial version just had one program that implemented the core of the browser all in one file.  To do this, it does some quite
horrible magic with a special .h65 file format that is basically a VIC-IV image to display, plus information about where hyperlinks
exist on the page.  This avoids the need for any kind of HTML parser in the browser, which is a good thing, given the constraints we have.  The result is quite good, with a simple markdown-to-h65 converter provided, that allows the use of images in pages.  More on the H65 format later.  Memory constraints for the single program means that it could only display pages, not download files.

The second stage of works being done at the moment, is to split out the network code into a separate helper, since it takes about 80% of the space available to a program compiled with CC65.  The intention is to do some very simple IPC via some agreed memory locations that we will preserve when switching between the programs.  This will allow us to have the browser program itself have more complexity, such as supporting downloading files, supporting simple H65-based forms, and if I get all excited, implementing a simple HTML parser to display pages from the "normal" web.  If we got it to the point where you could search google using it, then I think that would be a good achievement.

In terms of more interesting use-cases, the intention is to get fetch to the point where it supports browsing and downloading files from the MEGA65 Files facility (files.mega65.org), including easing the flashing of new bitstreams on the MEGA65.

## H65 format

The H65 file format is as previously described a very simple file format that contains a pre-rendered image for the VIC-IV video controller to display.  This consists of the screen RAM and colour RAM that define the display, as well as values for various registers that set the video mode, allowing considerable flexibility in how the pages look.  It also supports filling a large area of the graphics memory with custom gylphs, which can be used to display images, provide alternate fonts, and if someone gets around to making the pre-formatter support it, allow the use of nice looking proportional fonts, optionally with anti-aliasing, similar to what the 'MegaWAT!?' slide presentation software for the MEGA65 already does.

At the moment, however, only a simple markdown-to-h65 converter exists, which supports text, images, hyperlinks (including behind images), and some simple headings etc.  It lives in src/tools/md2h65.c in the github.com/mega65/mega65-tools repository.

## Planned Structure

The planned structure consists of the following modules:

1. fetch loader -- loads the default ASCII font, and then loads the main interactive part of fetch.
2. fetchm -- the main interactive browser program. Displays H65 pages, and allows scrolling, clicking on links, entering text into form fields etc, and interacting with the URL history etc.
3. fetchh65 -- Contains the network code, and downloads an H65 page directly into main memory.
4. fetchget -- Contains the network code, and downloads the requested URL into the 8MB expansion RAM area.
5. fetchsav -- Contains FAT32 filesystem handling code, and allows saving of downloaded files.

The fetch loader already exists, and fetchm and fetchh65 can be easily refactored out of the existing fetch program.
The fetchget program can also be easily derived from the existing fetch program.  Only fetchsav will require considerable work, but we can borrow much of the code from the mega65-fdisk program, that already implements FAT32 filesystem writing.  We will need to integrate some magic with the hypervisor so that it can ask the hypervisor for permission to do that, and otherwise deny writing to the FAT32 file system by normal programs.

It would be nice to also have a "fetch installer" that installs the native FAT32 files to the SD card, if they are not already there.

## Memory Layout

```
0x0000-0x0340 -- Standard C64 ZP, stack etc
0x0340-0x0380 -- Sprite for mouse pointer
0x0380-0x03ff -- fetch_shared_mem structure for passing control between modules
0x0400-0x07ff -- 40-column screen for progress messages
0x0800-0x9fff -- Code
0xa000-0xbfff -- Code ?
0xc000-0xc7ff -- 2KB TCP buffer
0xc800-0xcfff -- CC65 C stack?
0xd000-0xdfff -- Browser history
0xe000-0xefff --
0xf000-0xf7ff -- ASCII font
0xf800-0xffff -- Scratch memory for passing control between modules (eg passing URL elements)
0x10000-0x11fff -- C65 CBDOS buffers etc
0x12000-0x19fff -- 32KB Screen RAM for H65 page
0x1a000-0x1f7ff -- H65 custom glyphs ?
0x1f800-0x1ffff -- 2K colour RAM for 40 column display etc
0x20000-0x3ffff -- C65 ROM
0x40000-0x5ffff -- H65 custom gylphs
0xFF80800-0xFF87FFF -- 30KB colour RAM for H65 page
```
