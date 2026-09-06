$[]
# Astral

A feature tour, one slide at a time.

Run this deck with:

`astral demo.md`

$[align=left]
## Headings

Astral supports four levels of Markdown headings, each one a proportionally
smaller, bold heading.

## Level two

### Level three

#### Level four

A plain paragraph, for comparison against the headings above.

$[align=left]
## Inline styles

A sentence mixing **bold**, *italic*, ***bold and italic together***, and
~~strikethrough~~ — all working **inside** a single word too.

## A heading with *italic* and ~~strikethrough~~

A heading is always bold; adding italic on top switches to the bold-italic
font automatically.

---

A thematic break (`---`) renders as a horizontal rule, like the one above.

$[align=left,transition=slide]
## Per-slide styling

Every slide can set its own **background**, **text**, and **code
highlight** colors, plus horizontal **alignment**, using one marker line
right in the Markdown, right before the slide it applies to:

`align=left,bg-color=#1a2b4c,text-color=#ffd76e,block-color=#2f4770`

No theme file, no templating language needed.

$[align=left]
## Code blocks

Inline code like `npm install` or `git commit -m "message"` sits right in
a sentence.

Fenced blocks get their own highlighted background:

```
function fib(n) {
    if (n <= 1) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}
```

$[align=center]
## Mermaid ER diagram

```mermaid
erDiagram
  STUDENT {
    int id PK
    string name
    date dob
    string grade
  }
  TEACHER {
    int id PK
    string name
    string department
  }
  COURSE {
    int id PK
    string title
    int teacher_id FK
    int credits
  }
  ENROLLMENT {
    int id PK
    int student_id FK
    int course_id FK
    string semester
    float grade
  }
  TEACHER ||--o{ COURSE : teaches
  STUDENT ||--o{ ENROLLMENT : enrolled
  COURSE ||--o{ ENROLLMENT : has
```

$[align=center]
## graph TD

```mermaid
graph TD
  A[Start] --> B{Decision}
  B -->|Yes| C[Accept]
  B -->|No| D[Reject]
  C --> E[Done]
  D --> E
  linkStyle 0 stroke:#7aa2f7,stroke-width:3px
  linkStyle 1 stroke:#9ece6a,stroke-width:2px
  linkStyle 2 stroke:#f7768e,stroke-width:2px
  linkStyle default stroke:#565f89
```

$[align=center]
## and sequence diagrams

```mermaid
sequenceDiagram
  participant A as Alice
  participant B as Bob
  participant C as Charlie
  A->>B: Hello
  B->>C: Forward
  C-->>A: Reply
```

$[]
## XY diagram

```mermaid
xychart-beta horizontal
    title "Budget vs Actual"
    x-axis [Eng, Sales, Marketing, Product, Ops, HR, Finance, Legal]
    bar [500, 350, 250, 200, 150, 120, 100, 80]
    line [480, 380, 230, 180, 160, 110, 95, 75]
```

$[align=left]
## Lists

Unordered:

* Bullet one
* Bullet two
    * Nested bullet
    * Another nested bullet

Ordered:

1. First step
2. Second step
    1. Sub-step
    2. Sub-step

$[align=left]
## Tables

Markdown tables render with column alignment and inline styles inside
cells:

| Feature      | Status    | Notes             |
|:-------------|:---------:|------------------:|
| Live reload  | Active    | Watches the file  |
| Per-slide CSS-like styling | Active | No theme file needed |
| ***Transitions*** | Active | fade / slide / zoom |
| Screenshots  | Available | `--screenshot`     |

$[align=left,transition=fade]
## Blockquotes

A blockquote is set apart from the surrounding text, useful for callouts
or speaker notes rendered right on the slide.

> Astral: write the deck, not the tool.

$[]
## Image support

Astral renders images referenced from Markdown, resolved relative to the
deck's own directory:

![Mountain lake, generated placeholder art](mountain-lake.png)


$[]
## Emoji

Emoji render through an optional dedicated font, loaded only if the deck
actually uses one:

`astral demo.md --emoji-font emoji.ttf`

🎉 🚀 ❤️ 👍 🌍 😀 ✨

$[]
## CJK and other scripts

A second optional font extends coverage to Chinese, Japanese, and Korean
text:

`astral demo.md --asian-font unifont.ttf`

Chinese: 你好，世界！
Japanese (Hiragana): こんにちは
Korean (Hangul): 안녕하세요

$[align=left]
## Live reload

Save this very file while `astral` is running and the window updates
immediately — no restart, no manual "reload" command. This is what makes
Astral practical for writing a deck directly in an editor.

$[align=left]
## Overview mode

Hold **Ctrl** at any time to open a clickable grid of every slide in the
deck — useful for jumping around, or for picking a starting point before
presenting.

Start directly in this mode with:

`astral demo.md --force-overview`

$[bg-color=#006d77,text-color=#edf6f9,align=center,transition=zoom]
## Try it yourself

```
astral demo.md --slide 1 --transition fade
```

No install. No dependencies. Just a Markdown file and a single binary.
