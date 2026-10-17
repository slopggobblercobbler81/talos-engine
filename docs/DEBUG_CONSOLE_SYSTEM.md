# In-Editor Debug Console & Diagnostic Logger

Talos Engine includes a centralized diagnostic logging subsystem (`Core::Logger`) paired with an interactive **In-Editor Debug Console**. Every diagnostic entry records its classification level, local timestamp, human-readable message, and the exact **file and line number** from which it originated.

---

## 1. Message Classification (`Core::LogLevel`)

All diagnostic messages in Talos Engine are classified into three distinct severity levels:

| Type Badge | Enum Value | Purpose | Visual Style |
| :--- | :--- | :--- | :--- |
| **`[INFO]`** | `LogLevel::Info` | Normal engine lifecycle events, GPU texture uploads, scene loads/saves, Play Mode transitions, and Undo/Redo operations. | Cyan badge (`#55BEFF`), neutral dark row background. |
| **`[WARN]`** | `LogLevel::Warning` | Non-fatal anomalies or fallback behaviors (e.g., unrecognized keys inside a scene file, missing system font fallback). | Amber badge (`#FFC341`), warm amber-tinted row background. |
| **`[ERROR]`** | `LogLevel::Error` | Actionable failures such as missing texture files on disk, failed GPU uploads, or malformed scene serialization syntax. | Coral-red badge (`#FF5F5F`), crimson-tinted row background. |

---

## 2. Automatic Source File & Line Attribution (`[file:line]`)

Every entry stored in `Core::Logger` captures:
- **`level`**: Classification (`Info`, `Warning`, `Error`).
- **`timestamp`**: Local wall-clock time formatted as `HH:MM:SS`.
- **`file`**: Normalized source or asset identifier (automatically stripped of absolute machine prefixes).
- **`line`**: Exact 1-indexed line number where the diagnostic event occurred.
- **`message`**: Descriptive explanation of the event or failure.

### C++ Source Instrumentation Macros
Engine subsystems emit diagnostics through three convenience macros that automatically inject `__FILE__` and `__LINE__` at compile time:

```cpp
TALOS_LOG_INFO("Loaded texture into VRAM (64x64).");
TALOS_LOG_WARN("System UI font not found; falling back to bitmap font.");
TALOS_LOG_ERROR("Texture file not found on disk: 'missing.png'");
```

### Data-File Line Attribution
When parsing external data files (such as `.talos` scene files), `Core::Logger::Write` can also be invoked with an explicit data file identifier and the exact 1-indexed line number being parsed, pinpointing syntax errors directly inside the scene file alongside C++ call sites.

---

## 3. In-Editor Debug Console UI

The **Debug Console** is docked at the bottom of the central viewport column, preserving full vertical height for both the Left Entity List and the Right First-Level Inspector.

### Header Bar Controls
- **Severity Filter Toggles (`INFO: N`, `WARN: N`, `ERROR: N`)**: Live counters for each message classification. Clicking any badge toggles visibility of that category on or off in real time.
- **`Clear` Button**: Empties the log buffer and resets scroll position.
- **`Hide` / `Show` Button**: Collapses the console down to its compact 28px header bar to maximize 2D viewport space, or expands it back to its full height.

### Log List & Scrolling
- **Structured Row Layout**: Each row renders `[LEVEL]`, `HH:MM:SS`, `[file:line]`, and the diagnostic message text on a single scannable line.
- **Auto-Scroll on New Messages**: Whenever a new diagnostic message is emitted, the console automatically scrolls to the newest entry at the bottom.
- **Mouse Wheel Scrolling**: Hovering the mouse cursor over the console body and scrolling the wheel navigates through historical log entries (up to 500 buffered messages).
