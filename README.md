# whereami - programmer's orientation tool

![whereami title graphics](https://github.com/edwinst/whereami/blob/master/whereami_title_304px.png?raw=true)

`whereami` takes a source file and a line number and prints a short string telling you where
the given line is in the indentation hierarchy of the source code. (It can also
print this information at once for each line in the whole file.) Currently
`whereami` should work for C, C++, and syntactically similar languages
(see 'Principle of operation').

The intended use is to call `whereami` from your editor in order to
help you orient yourself in large code files, especially after jumping
to a location via search.

# License

I put the source code of `whereami` in the public domain to allow unencumbered reuse and
adaptation. See the file `LICENSE` for the (un)license terms that apply.

# XXX TODO

* use
* make portable?
* example near the introduction

# Use

# Principle of operation

`whereami` saves time and complexity by not trying to understand the
syntax of your code. Instead, it simply looks at the indentation in order
to heuristically deduce the hierarchy of nested scopes, etc.

Once it has found the outer scopes of the given line which are implied by
indentation, `whereami` applies some simple heuristics for improving the
usefulness of the reported result. In particular it does the following:

* It skips 'boring' lines like those consisting only of an opening brace ('{').
  (see `line_is_boring` in `whereami.cpp`)

* It shortens the reported string by dropping the "namespace" keyword,
  shortening identifiers to the first few characters, etc.
  (see `maybe_skip_substr`, `print_context` in `whereami.cpp`)
  Different heuristics are applied to lines that look like function
  headers

* Lines starting with '#' like C preprocessor directives are currently
  ignored for purposes of context reporting.

* Context lines that are very close to the queried position are not
  reported. (The rationale here is that code within +/- 20 lines of
  your current position will typically be obvious to you on screen,
  anyway.)

The first step (indentation analysis) is mostly language-independent.
The second, heuristic, step, however, makes some assumptions about
C-like syntax for control-flow constructs and function declarations.
`whereami` will not complain about syntax it does not understand but
the information it provides will be suboptimal in such cases.

# Development

The original version of the program was written in a live stream on my
[twitch channel](https://www.twitch.tv/edwinst). You can watch the
[archived recording](https://youtu.be/vWOtwyDFxi8)
on [my YouTube channel](https://www.youtube.com/channel/UC2FDMyhLAoQM2HR8zY4m7hw).

# Limitations

* `whereami` reads your code from a file. Therefore, if you have unsaved changes
that modify indentation or move code, the printed information may be off.

* Tab stops are currently hard-coded to be at multiples of 8 characters. If you use
a different setting in your code, you will need to adapt the code or add an
option lest `whereami` misunderstand your indentation.

# Future directions

* `whereami` is very fast already but it could be made even more responsive by
  implementing a server mode in which the `whereami` process keeps running and
  the editor communicates with it via pipes or sockets (e.g. using vim's "channels"
  feature). In such a setup it would probably be responsive enough to
  invoke it after every cursor movement in the editor. The biggest challenge
  is how to handle modifications of the source code after the initial file or
  buffer contents have been loaded into `whereami`.
  Note: I did not find a great need to implement such advanced features as
  I usually only need to re-orient myself in the source code after using
  search functions to jump somewhere else and `whereami` is easily fast enough
  already to do that.
