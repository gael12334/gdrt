# Generic Data Recovery Tool
Generic Data Recovery Tool - A simple tool to recover deleted files. Made to work on linux-based systems (including android, but requires root). This tool has 2 functionnality: Finder and Extractor.

## Finder
Tries to find MARKER (in hex, no spaces) first, then TRAILER (in hex, no spaces) if found, from INPUT (a path to a file), starting at OFFSET (integer) for LENGTH (integer) bytes, and saves results in OUTPUT (a path to a file). _Note: the OUTPUT file only contains references and not actual file contents. The OUTPUT file is the report used by the Extractor._

`gdrt -f INPUT OUTPUT MARKER TRAILER OFFSET LENGTH`

Shortcuts are available to launch Finder. There are 4 <ins>OPTION</ins>s: `-ij` = jpeg, `-ip` = png, `-vm` = mp3, `-dp` = pdf. 

`gdrt OPTION INPUT OUTPUT OFFSET LENGTH`

## Extractor
For a REPORT (a path to a file), save file contents to OUTPUT (a path to a file) of entry INDEX (an integer). _Note: the INDEX is displayed by Finder when a match has been found._

`gdrt -e REPORT OUTPUT INDEX`

## Help menu
To display the help menu, simply run the program without parameters.

`gdrt`

# Disclaimer
This is intended for my own personal use. The source code is intended to be part of my portofolio. If you desire to use this tool, be aware that only a minimal amount of tests have been performed. If a malfunction alters, corrupts or destroys data, the liability is on the person that downloaded and ran my program. 
