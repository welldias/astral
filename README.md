# Astral

Astral is a minimalist presentation tool that turns plain Markdown files into
full-screen slide decks. Write your slides as `.md`, run one command, and
present — no slide editor, no cloud account, no build pipeline.

```
astral slides.md
```

## Why Astral

- **Write slides as text.** Headings, bold/italic, lists, tables, code
  blocks, blockquotes, and images all map directly to slide content, using
  the Markdown you already know.
- **Live reload while you write.** Astral watches the file and re-renders
  automatically the moment you save, so you can iterate on a deck in your
  editor and see the result instantly, side by side.
- **Per-slide styling, inline.** Background, text, and accent colors, plus
  alignment and transition effects, are set per slide with a simple
  `$[key=value,...]` marker right in the Markdown — no separate theme file
  or templating language.
- **Built for the terminal-first workflow.** Jump to a specific slide with
  `--slide`, capture a slide as a PNG with `--screenshot`, or open straight
  into a clickable grid overview of the whole deck — useful for scripting,
  CI previews, or picking where to start presenting.
- **Zoom in on the details.** Scroll (or `+`/`-`) to magnify the current
  slide and drag with the mouse to pan around while zoomed in; double-click,
  `=`, or Esc snaps back to normal size — useful for pointing out a detail
  without leaving the slide.
- **Handles real-world text.** Emoji and CJK/Hangul/Hiragana characters are
  supported via optional dedicated fonts (`--emoji-font`, `--asian-font`),
  loaded only when the deck actually needs them.
- **Bring your own fonts.** Astral picks up the operating system's default
  font automatically; `--regular-font`, `--italic-font`, `--bold-font`, and
  `--mono-font` override any of them individually with your own file, so a
  deck looks the same wherever it's presented.

## Advantages over other tools

Compared to slideware like PowerPoint or Keynote, and even other
Markdown-based presentation tools:

- **Simple.** One Markdown file in, one window out. There's no GUI editor
  to learn, no plugin ecosystem, no proprietary file format to fight with.
- **Lightweight and small.** Astral is a native C++ application rendered
  directly with the GPU (via raylib) — it starts instantly and stays out of
  your way, unlike browser-based tools (reveal.js, Slidev) that carry an
  entire Chromium runtime just to show text on a screen.
- **No dependencies to install.** Everything Astral needs is compiled into
  a single self-contained executable. There's no Node.js, no Electron, no
  browser, no runtime, and no package manager required on the machine
  running the presentation.
- **No installation step.** Download the binary and run it — that's the
  entire setup. No installer, no admin rights, no "creating your account"
  screen, no internet connection required to present.
- **Your slides stay files.** Decks are plain `.md` files you can version
  with git, diff, review in a pull request, and edit in any text editor —
  not binary blobs locked into one application.

## Usage

```
astral <file.md> [options]

  --slide <n>                 Open directly on slide n (1-based)
  --screenshot <path.png>     Render a slide to an image and exit
  --screenshot-delay <secs>   Delay before capturing the screenshot
  --transition <fade|slide|zoom|none>
                               Default transition between slides
  --emoji-font <path.ttf>     Font used for emoji glyphs
  --asian-font <path.ttf>     Font used for CJK/Hangul/Hiragana glyphs
  --regular-font <path.ttf>  Font used for regular text (default: system font)
  --italic-font <path.ttf>   Font used for italic text (default: system font)
  --bold-font <path.ttf>     Font used for bold text (default: system font)
  --mono-font <path.ttf>     Font used for code/monospace text (default: system font)
  --force-overview            Start in the grid overview mode
```

`--regular-font`/`--italic-font`/`--bold-font`/`--mono-font` are independent of
each other — pass only the ones you want to override, the rest keep using
the operating system's default. A path that doesn't exist is a hard error
(logged to stderr), not a silent fallback.

While presenting: the arrow keys move between slides, the mouse wheel (or
`+`/`-`) zooms into the current slide, dragging pans around it while zoomed,
and double-click / `=` / Esc resets the zoom — a second Esc (with nothing
left to reset) closes the window. Holding Ctrl opens the grid overview.

See `demo/` for sample decks demonstrating headings, inline styles,
lists, tables, code blocks, images, per-slide colors/alignment, and emoji
or CJK text. `demo/demo.md` is a full feature tour, one slide per
feature — run it with `astral demo/demo.md --emoji-font emoji.ttf --asian-font unifont.ttf`.

## Building from source

Astral uses CMake, which fetches its two dependencies (
[raylib](https://github.com/raysan5/raylib) for rendering and
[md4c](https://github.com/mity/md4c) for Markdown parsing) automatically:

```
cmake -B build
cmake --build build
```

The resulting `astral` executable in `build/` is ready to run — no install
step needed.
