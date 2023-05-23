# mytar
A (mostly) USTAR compliant tar implementation created for my CSC 357 (systems programming) class  at Cal Poly SLO.

Supports the creation, extraction, and listing of tar archives.

Usage: mytar [ctxvS]f tarfile [ path [ ... ] ]

Mytar is a subset of tar and only supports five options. One of ‘c’, ‘t’, or ‘x’ is required to be
present. Traditionally, ‘f’ is optional, but you are not required to support its being absent.
