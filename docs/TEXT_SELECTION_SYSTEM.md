# Text Selection & Navigation System

## Overview

The **Text Selection & Navigation System** brings native text-editor ergonomics to all editable string fields in the **Talos Engine** Inspector (`Name:`, `Text:`, and `Image:`).

Creators can interact with text fields using standard desktop keyboard and mouse conventions:
1. **Mouse Click & Drag Selection:** Clicking anywhere inside a text box places the caret between the nearest characters based on exact TrueType glyph widths. Holding the left mouse button and dragging across the text highlights a contiguous character range.
2. **Keyboard Arrow Navigation (`Left` / `Right`):** Moves the caret one character at a time or collapses an active selection to its left or right boundary.
3. **Shift Selection (`Shift + Left` / `Shift + Right` / `Shift + Click`):** Holds the selection anchor fixed while moving the active caret, expanding or shrinking the highlighted text selection.
4. **Word-by-Word Navigation (`Ctrl + Left` / `Ctrl + Right`):** Jumps the caret across word boundaries (skipping spaces, slashes, underscores, dots, and hyphens). Combining **`Ctrl + Shift + Left/Right`** selects entire words at a time.
5. **Select All (`Ctrl + A`):** Instantly highlights the entire string inside the focused text box.
6. **Selection-Aware Editing & Word Deletion:**
   - Typing any printable character while text is highlighted replaces the entire selection in a single step.
   - Pressing `Backspace` or `Delete` while text is highlighted deletes the entire selection.
   - Pressing `Ctrl + Backspace` or `Ctrl + Delete` deletes the previous or next word up to the word boundary.
   - Every edit integrates with the **Undo / Redo System** (`Ctrl + Z` / `Ctrl + Y`), restoring both the string contents and the exact caret/selection positions.

---

## Architectural Design

### 1. Dual-Index Caret & Anchor Model

Instead of only appending characters at the end of a string, the Editor tracks three state variables for the focused text box:
- **`m_textCursorPos` (Active Caret):** The character index (`0 .. text.size()`) where the vertical caret bar is rendered and where new characters are inserted.
- **`m_textSelectionAnchor` (Selection Anchor):** The character index where the selection began.
  - When `m_textCursorPos == m_textSelectionAnchor`, no selection is active.
  - When `m_textCursorPos != m_textSelectionAnchor`, the selected substring spans `[std::min(cursor, anchor), std::max(cursor, anchor))`.
- **`m_isDraggingText`:** Tracks whether the user is actively holding the left mouse button to drag-select across the focused text box.

### 2. Pixel-Accurate Mouse Hit-Testing (`GetTextCharIndexAtMouseX`)

Because Talos Engine renders proportional TrueType fonts rather than fixed-width monospace characters, converting a mouse X coordinate into a character index requires measuring prefix widths:
- For each character index `i` from `1` to `text.size()`, the Editor queries `Renderer2D::MeasureTextWidth` on the prefix `text.substr(0, i)`.
- It computes the horizontal midpoint between the previous prefix width and the current prefix width:
  $$\text{charMidpoint} = \frac{w_{i-1} + w_i}{2}$$
- If the mouse's relative X coordinate lies to the left of `charMidpoint`, the caret snaps before character `i - 1`; otherwise it advances to the next character.

### 3. Word Boundary Detection (`IsWordChar` & `FindNextWordBoundary`)

To make `Ctrl + Left` and `Ctrl + Right` intuitive for both prose (`"Hello World"`) and asset paths (`"examples/assets/textures/hero.png"` or `"Object_1"`), characters are classified into alphanumeric word characters (`IsWordChar`) and non-word separators (spaces, `/`, `_`, `.`, `-`):
- **Jumping Left (`dir < 0`):** First skips any adjacent separators to the left of the caret, then skips all contiguous alphanumeric characters until reaching the start of the word.
- **Jumping Right (`dir > 0`):** First skips any adjacent separators to the right of the caret, then skips all contiguous alphanumeric characters until reaching the end of the word.

### 4. Visual Highlight & Caret Rendering (`DrawTextInputBox`)

During `Editor::DrawTextInputBox`, rendering proceeds in three layered passes inside the input box:
1. **Selection Highlight Pass:** If `m_textCursorPos != m_textSelectionAnchor`, the pixel X offsets of `selStart` and `selEnd` are measured via `Renderer2D::MeasureTextWidth`, and a translucent blue highlight rectangle is drawn behind the selected range.
2. **Text Glyph Pass:** The full `textBuffer` string is rendered with anti-aliased TrueType typography on top of the highlight rectangle.
3. **Caret Pass:** A 2-pixel-wide vertical bar is rendered at the measured X offset of `m_textCursorPos`.
